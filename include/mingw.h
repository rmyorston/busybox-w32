
#define NOIMPL(name,...) static inline int name(__VA_ARGS__) { errno = ENOSYS; return -1; }
#define IMPL(name,ret,retval,...) static inline ret name(__VA_ARGS__) { return retval; }

/* Use 64-bit time on 32-bit platforms. */
#if !defined(_WIN64)
# define time_t __time64_t
# define ctime(t) _ctime64(t)
# define localtime(t) _localtime64(t)
# define time(t) _time64(t)
# define gmtime(t) _gmtime64(t)
# define mktime(t) _mktime64(t)
# define timespec _timespec64
#endif

/*
 * sys/types.h
 */
typedef int gid_t;
typedef int uid_t;

#define DEFAULT_UID 4095
#define DEFAULT_GID DEFAULT_UID

/*
 * arpa/inet.h
 */
static inline unsigned int git_ntohl(unsigned int x) { return (unsigned int)ntohl(x); }
#define ntohl git_ntohl
int inet_aton(const char *cp, struct in_addr *inp);
int inet_pton(int af, const char *src, void *dst);

/*
 * fcntl.h
 */
#define F_DUPFD 0
#define F_GETFD 1
#define F_SETFD 2
#define F_GETFL 3
#define F_SETFL 3
#define FD_CLOEXEC 0x1
#define O_NONBLOCK 0
#define O_NOFOLLOW 0
#define O_NOCTTY 0
#define O_DIRECT 0
#define O_SPECIAL 0x800000

#define AT_FDCWD -100
#define AT_SYMLINK_NOFOLLOW 0x100

/*
 * grp.h
 */

struct group {
	char *gr_name;
	char *gr_passwd;
	gid_t gr_gid;
	char **gr_mem;
};
IMPL(getgrnam,struct group *,NULL,const char *name UNUSED_PARAM);
struct group *getgrgid(gid_t gid);
NOIMPL(initgroups,const char *group UNUSED_PARAM,gid_t gid UNUSED_PARAM);
static inline void endgrent(void) {}
int getgrouplist(const char *user, gid_t group, gid_t *groups, int *ngroups);

/*
 * limits.h
 */
#define NAME_MAX 255
#define MAXSYMLINKS 20

#ifdef LONG_MAX
# if LONG_MAX == 2147483647
#  define LONG_BIT  32
# else
/* Safe assumption.  */
#  define LONG_BIT  64
# endif
#elif defined __LONG_MAX__
# if __LONG_MAX__ == 2147483647
#  define LONG_BIT  32
# else
/* Safe assumption.  */
#  define LONG_BIT  64
# endif
#endif

/*
 * netdb.h
 */

typedef int sa_family_t;

/*
 * linux/un.h
 */
struct sockaddr_un {
	sa_family_t sun_family;
	char sun_path[1]; /* to make compiler happy, don't bother */
};

/*
 * pwd.h
 */
struct passwd {
	char *pw_name;
	char *pw_passwd;
	char *pw_gecos;
	char *pw_dir;
	char *pw_shell;
	uid_t pw_uid;
	gid_t pw_gid;
};

struct passwd *getpwnam(const char *name);
struct passwd *getpwuid(uid_t uid);
static inline void setpwent(void) {}
static inline void endpwent(void) {}
IMPL(getpwent_r,int,ENOENT,struct passwd *pwbuf UNUSED_PARAM,char *buf UNUSED_PARAM,size_t buflen UNUSED_PARAM,struct passwd **pwbufp UNUSED_PARAM);
IMPL(getpwent,struct passwd *,NULL,void)

/*
 * signal.h
 */
#define SIGHUP  1
#define SIGQUIT 3
#define SIGKILL 9
#define SIGPIPE 13

#define SIG_UNBLOCK 1

typedef void (*sighandler_t)(int);
sighandler_t winansi_signal(int signum, sighandler_t handler);
#define signal(s, h) winansi_signal(s, h)

/*
 * stdio.h
 */
#undef fseeko
#define fseeko(f,o,w) fseek(f,o,w)

int fdprintf(int fd, const char *format, ...);
FILE* mingw_fopen(const char *filename, const char *mode);
int mingw_rename(const char*, const char*);
#define fopen mingw_fopen
#define rename mingw_rename

FILE *mingw_popen(const char *cmd, const char *mode);
int mingw_popen_fd(const char *exe, const char *cmd, const char *mode,
					int fd0, pid_t *pid);
int mingw_pclose(FILE *fd);
pid_t mingw_fork_compressor(int fd, const char *compressor, const char *mode);
#undef popen
#undef pclose
#define popen mingw_popen
#define pclose mingw_pclose

IMPL(setlinebuf, void, ,FILE *fd UNUSED_PARAM)

/*
 * ANSI emulation wrappers
 */

BOOL conToCharBuffA(LPSTR d, DWORD len);
BOOL conToCharA(LPSTR d);

// same as ReadConsoleInputA, but delivers UTF8 regardless of console CP
BOOL readConsoleInput_utf8(HANDLE h, INPUT_RECORD *r, DWORD len, DWORD *got);

void set_title(const char *str);
int get_title(char *buf, int len);
void move_cursor_row(int n);
void reset_screen(void);
int winansi_putchar(int c);
int winansi_puts(const char *s);
size_t winansi_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int winansi_fputs(const char *str, FILE *stream);
int winansi_fputc(int c, FILE *stream);
int winansi_vsnprintf(char *buf, size_t size, const char *format, va_list list);
int winansi_vfprintf(FILE *stream, const char *format, va_list list);
int winansi_printf(const char *format, ...) __attribute__((format (printf, 1, 2)));
int winansi_fprintf(FILE *stream, const char *format, ...) __attribute__((format (printf, 2, 3)));
int winansi_write(int fd, const void *buf, size_t count);
int winansi_read(int fd, void *buf, size_t count);
size_t winansi_fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
int winansi_getc(FILE *stream);
int winansi_getchar(void);
char *winansi_fgets(char *s, int size, FILE *stream);
void console_write(const char *str, int len);

#define putchar winansi_putchar
#define puts winansi_puts
#define fwrite winansi_fwrite
#define fputs winansi_fputs
#define fputc winansi_fputc
#if !defined(__USE_MINGW_ANSI_STDIO) || !__USE_MINGW_ANSI_STDIO
#define vsnprintf(buf, size, ...) winansi_vsnprintf(buf, size,  __VA_ARGS__)
#endif
#define vfprintf(stream, ...) winansi_vfprintf(stream, __VA_ARGS__)
#define vprintf(...) winansi_vfprintf(stdout, __VA_ARGS__)
#define printf(...) winansi_printf(__VA_ARGS__)
#define fprintf(...) winansi_fprintf(__VA_ARGS__)
#define write winansi_write
#define read winansi_read
#define fread winansi_fread
#define getc winansi_getc
#define fgetc winansi_getc
#define getchar winansi_getchar
#define fgets winansi_fgets

/*
 * stdlib.h
 */
#define WTERMSIG(x) ((x) & 0x7f)
#define WIFEXITED(x) (WTERMSIG(x) == 0)
#define WEXITSTATUS(x) (((x) & 0xff00) >> 8)
#define WIFSIGNALED(x) (((signed char) (((x) & 0x7f) + 1) >> 1) > 0)
#define WCOREDUMP(x) 0
#define WIFSTOPPED(x) 0

int mingw_system(const char *cmd);
#define system mingw_system

int clearenv(void);
char *mingw_getenv(const char *name);
int mingw_putenv(const char *env);
char *mingw_mktemp(char *template);
int mkstemp(char *template);
char *realpath(const char *path, char *resolved_path);
int setenv(const char *name, const char *value, int replace);
int unsetenv(const char *env);

#define getenv mingw_getenv
#define putenv mingw_putenv
#define mktemp mingw_mktemp

/*
 * string.h
 */
char *strndup(char const *s, size_t n);
char *mingw_strerror(int errnum);
char *strsignal(int sig);
int strverscmp(const char *s1, const char *s2);

#define strerror mingw_strerror

/*
 * strings.h
 */
#if !defined(__GNUC__)
int ffs(int i);
#else
# define ffs(i) __builtin_ffs(i)
#endif

/*
 * sys/ioctl.h
 */

#define TIOCGWINSZ 0x5413
#define TIOCSWINSZ 0x5414

int ioctl(int fd, int code, ...);

/*
 * sys/socket.h
 */
#define hstrerror strerror

#define SHUT_WR SD_SEND

int mingw_socket(int domain, int type, int protocol);
int mingw_connect(int sockfd, const struct sockaddr *sa, size_t sz);
int mingw_bind(int sockfd, struct sockaddr *sa, size_t sz);
int mingw_setsockopt(int sockfd, int lvl, int optname, void *optval, int optlen);
int mingw_shutdown(int sockfd, int how);
int mingw_listen(int sockfd, int backlog);
int mingw_accept(int sockfd1, struct sockaddr *sa, socklen_t *sz);
int mingw_select (int nfds, fd_set *rfds, fd_set *wfds, fd_set *xfds,
            struct timeval *timeout);
int mingw_getpeername(int fd, struct sockaddr *sa, socklen_t *sz);
int mingw_gethostname(char *host, int namelen);
int mingw_getaddrinfo(const char *node, const char *service,
			const struct addrinfo *hints, struct addrinfo **res);
struct hostent *mingw_gethostbyaddr(const void *addr, socklen_t len, int type);

#define socket mingw_socket
#define connect mingw_connect
#define listen mingw_listen
#define bind mingw_bind
#define setsockopt mingw_setsockopt
#define shutdown mingw_shutdown
#define accept mingw_accept
#define select mingw_select
#define getpeername mingw_getpeername
#define gethostname mingw_gethostname
#define getaddrinfo mingw_getaddrinfo
#define gethostbyaddr mingw_gethostbyaddr

/*
 * sys/time.h
 */
#ifndef _TIMESPEC_DEFINED
#define _TIMESPEC_DEFINED
struct timespec {
	time_t tv_sec;
	long int tv_nsec;
};
#endif

typedef int clockid_t;
#define CLOCK_REALTIME 0

time_t timegm(struct tm *tm);

int nanosleep(const struct timespec *req, struct timespec *rem);
int clock_gettime(clockid_t clockid, struct timespec *tp);
int clock_settime(clockid_t clockid, const struct timespec *tp);

/*
 * sys/stat.h
 */
#define S_ISUID 04000
#define S_ISGID 02000
#define S_ISVTX 01000
#ifndef S_IRWXU
#define S_IRWXU (S_IRUSR | S_IWUSR | S_IXUSR)
#endif
#define S_IRWXG (S_IRWXU >> 3)
#define S_IRWXO (S_IRWXG >> 3)

#define S_IFSOCK 0140000
#define S_IFLNK    0120000 /* Symbolic link */
#define S_ISLNK(x) (((x) & S_IFMT) == S_IFLNK)
#define S_ISSOCK(x) 0

#define S_IRGRP (S_IRUSR >> 3)
#define S_IWGRP (S_IWUSR >> 3)
#define S_IXGRP (S_IXUSR >> 3)
#define S_IROTH (S_IRGRP >> 3)
#define S_IWOTH (S_IWGRP >> 3)
#define S_IXOTH (S_IXGRP >> 3)

mode_t mingw_umask(mode_t mode);
#define umask mingw_umask

#define DEFAULT_UMASK 0002

IMPL(fchmod,int,0,int fildes UNUSED_PARAM, mode_t mode UNUSED_PARAM);
NOIMPL(fchown,int fd UNUSED_PARAM, uid_t uid UNUSED_PARAM, gid_t gid UNUSED_PARAM);
int mingw_mkdir(const char *path, int mode);
int mingw_chdir(const char *path);
int mingw_chmod(const char *path, int mode);

#define mkdir mingw_mkdir
#define chdir mingw_chdir
#define chmod mingw_chmod

#if ENABLE_LFS && !defined(__MINGW64_VERSION_MAJOR)
# define off_t off64_t
#endif

typedef int nlink_t;
typedef int blksize_t;
typedef off_t blkcnt_t;
#if ENABLE_FEATURE_EXTRA_FILE_DATA
#define ino_t uint64_t
#endif

struct mingw_stat {
	dev_t     st_dev;
	ino_t     st_ino;
	mode_t    st_mode;
	nlink_t   st_nlink;
	uid_t     st_uid;
	gid_t     st_gid;
	dev_t     st_rdev;
	off_t     st_size;
	struct timespec st_atim;
	struct timespec st_mtim;
	struct timespec st_ctim;
	blksize_t st_blksize;
	blkcnt_t  st_blocks;
	DWORD     st_attr;
	DWORD     st_tag;
};
#define st_atime st_atim.tv_sec
#define st_mtime st_mtim.tv_sec
#define st_ctime st_ctim.tv_sec

int count_subdirs(const char *pathname);
int mingw_lstat(const char *file_name, struct mingw_stat *buf);
int mingw_stat(const char *file_name, struct mingw_stat *buf);
int mingw_fstat(int fd, struct mingw_stat *buf);
#undef lstat
#undef stat
#undef fstat
#define lstat mingw_lstat
#define stat mingw_stat
#define fstat mingw_fstat

#define UTIME_NOW  ((1l << 30) - 1l)
#define UTIME_OMIT ((1l << 30) - 2l)

int utimensat(int fd, const char *path, const struct timespec times[2],
				int flags);
int futimens(int fd, const struct timespec times[2]);

/*
 * sys/sysinfo.h
 */
struct sysinfo {
	long uptime;             /* Seconds since boot */
	unsigned long loads[3];  /* 1, 5, and 15 minute load averages */
	unsigned long totalram;  /* Total usable main memory size */
	unsigned long freeram;   /* Available memory size */
	unsigned long sharedram; /* Amount of shared memory */
	unsigned long bufferram; /* Memory used by buffers */
	unsigned long totalswap; /* Total swap space size */
	unsigned long freeswap;  /* Swap space still available */
	unsigned short procs;    /* Number of current processes */
	unsigned long totalhigh; /* Total high memory size */
	unsigned long freehigh;  /* Available high memory size */
	unsigned int mem_unit;   /* Memory unit size in bytes */
};

int sysinfo(struct sysinfo *info);

/*
 * sys/sysmacros.h
 */
#define makedev(a,b) 0*(a)*(b) /* avoid unused warning */
#define minor(x) 0
#define major(x) 0

/*
 * sys/wait.h
 */
#define WNOHANG 1
#define WUNTRACED 2
pid_t waitpid(pid_t pid, int *status, int options);
pid_t mingw_wait3(pid_t pid, int *status, int options, struct rusage *rusage);

/*
 * time.h
 */
struct tm *gmtime_r(const time_t *timep, struct tm *result);
struct tm *localtime_r(const time_t *timep, struct tm *result);
char *strptime(const char *s, const char *format, struct tm *tm);
char *mingw_strptime(const char *s, const char *format, struct tm *tm, long *gmt);
size_t mingw_strftime(char *buf, size_t max, const char *format, const struct tm *tm);

#define strftime mingw_strftime

/*
 * times.h
 */
#define clock_t long

struct tms {
	clock_t tms_utime;		/* user CPU time */
	clock_t tms_stime;		/* system CPU time */
	clock_t tms_cutime;		/* user CPU time of children */
	clock_t tms_cstime;		/* system CPU time of children */
};

clock_t times(struct tms *buf);

/*
 * unistd.h
 */
#define PIPE_BUF 8192

#define _SC_CLK_TCK 2

#define TICKS_PER_SECOND 100
#define MS_PER_TICK 10
#define HNSEC_PER_TICK 100000

IMPL(alarm,unsigned int,0,unsigned int seconds UNUSED_PARAM);
IMPL(chown,int,0,const char *path UNUSED_PARAM, uid_t uid UNUSED_PARAM, gid_t gid UNUSED_PARAM);
NOIMPL(chroot,const char *root UNUSED_PARAM);
NOIMPL(fchdir,int fd UNUSED_PARAM);
int mingw_dup2 (int fd, int fdto);
char *mingw_getcwd(char *pointer, int len);
off_t mingw_lseek(int fd, off_t offset, int whence);


int getuid(void);
#define getgid getuid
#define geteuid getuid
#define getegid getuid
int getgroups(int n, gid_t *groups);
pid_t getppid(void);
NOIMPL(getsid,pid_t pid UNUSED_PARAM);
int getlogin_r(char *buf, size_t len);
int fcntl(int fd, int cmd, ...);
int fsync(int fd);
int kill(pid_t pid, int sig);
int link(const char *oldpath, const char *newpath);
NOIMPL(mknod,const char *name UNUSED_PARAM, mode_t mode UNUSED_PARAM, dev_t device UNUSED_PARAM);
/* order of devices must match that in get_dev_type */
enum {DEV_NULL, DEV_ZERO, DEV_URANDOM, NOT_DEVICE = -1};
int get_dev_type(const char *filename);
void update_special_fd(int dev, int fd);
int mingw_open (const char *filename, int oflags, ...);

/* functions which add O_SPECIAL to open(2) to allow access to devices */
int mingw_xopen(const char *filename, int oflags);
ssize_t mingw_open_read_close(const char *fn, void *buf, size_t size) FAST_FUNC;

#ifndef IO_REPARSE_TAG_APPEXECLINK
# define IO_REPARSE_TAG_APPEXECLINK 0x8000001b
#endif

ssize_t mingw_read(int fd, void *buf, size_t count);
int mingw_close(int fd);
int pipe(int filedes[2]);
NOIMPL(setgid,gid_t gid UNUSED_PARAM);
NOIMPL(setegid,gid_t gid UNUSED_PARAM);
NOIMPL(setsid,void);
NOIMPL(setuid,uid_t gid UNUSED_PARAM);
NOIMPL(seteuid,uid_t gid UNUSED_PARAM);
unsigned int sleep(unsigned int seconds);
int symlink(const char *target, const char *linkpath);
int create_junction(const char *oldpath, const char *newpath);
long sysconf(int name);
IMPL(getpagesize,int,4096,void);
NOIMPL(ttyname_r,int fd UNUSED_PARAM, char *buf UNUSED_PARAM, int sz UNUSED_PARAM);
int mingw_unlink(const char *pathname);
int mingw_access(const char *name, int mode);
int mingw_rmdir(const char *name);
void mingw_sync(void);
int mingw_isatty(int fd);

#define dup2 mingw_dup2
#define getcwd mingw_getcwd
#define lchown chown
#define open mingw_open
#define close mingw_close
#define unlink mingw_unlink
#define rmdir mingw_rmdir
#define sync mingw_sync
#undef lseek
#define lseek mingw_lseek

#undef access
#define access mingw_access
#define isatty mingw_isatty

/*
 * utime.h
 */
int utimes(const char *file_name, const struct timeval times[2]);

/*
 * Functions with different prototypes in BusyBox and WIN32
 */
#define itoa bb_itoa
#define strrev bb_strrev

/*
 * MinGW specific
 */
#define is_dir_sep(c) ((c) == '/' || (c) == '\\')
#define is_unc_path(x) (strlen(x) > 4 && is_dir_sep(x[0]) && \
							is_dir_sep(x[1]) && !is_dir_sep(x[2]))

typedef struct {
	char *path;
	char *name;
	char *opts;
	char buf[100];
} interp_t;

int FAST_FUNC parse_interpreter(const char *cmd, interp_t *interp);
char ** FAST_FUNC grow_argv(char **argv, int n);
pid_t FAST_FUNC mingw_spawn(char **argv);
intptr_t FAST_FUNC mingw_spawn_detach(char **argv);
intptr_t FAST_FUNC mingw_spawn_proc(const char **argv);
int mingw_execv(const char *cmd, char *const *argv);
int httpd_execv_detach(const char *cmd, char *const *argv);
int mingw_execvp(const char *cmd, char *const *argv);
int mingw_execve(const char *cmd, char *const *argv, char *const *envp);
#define spawn mingw_spawn
#define execvp mingw_execvp
#define execve mingw_execve
#define execv mingw_execv
#define HTTPD_DETACH (8)

#define has_dos_drive_prefix(path) (isalpha(*(path)) && (path)[1] == ':')

BOOL WINAPI kill_child_ctrl_handler(DWORD dwCtrlType);
int FAST_FUNC is_valid_signal(int number);
int exit_code_to_wait_status(DWORD win_exit_code);
int exit_code_to_posix(DWORD win_exit_code);

#define find_mount_point(n, s) find_mount_point(n)

char *is_prefixed_with_case(const char *string, const char *key) FAST_FUNC;
char *is_suffixed_with_case(const char *string, const char *key) FAST_FUNC;

#define VT_OUTPUT	1
#define VT_INPUT	2

/*
 * helpers
 */

const char *get_busybox_exec_path(void);
void init_winsock(void);

int has_bat_suffix(const char *p);
int has_exe_suffix(const char *p);
int has_exe_suffix_or_dot(const char *name);
char *alloc_ext_space(const char *path);
int add_win32_extension(char *p);
char *file_is_win32_exe(const char *name);

#if ENABLE_UNICODE_SUPPORT
/*
 * windows wchar_t is 16 bit, while linux (and busybox expectation) is 32.
 * so when (busybox) unicode.h is included, wchar_t is 32 bit.
 * Without unicode.h, MINGW_BB_WCHAR_T is busybox wide char (32),
 * and wchar_t is Windows wide char (16).
 */
#define MINGW_BB_WCHAR_T uint32_t  /* keep in sync with unicode.h */

MINGW_BB_WCHAR_T *bs_to_slash_u(MINGW_BB_WCHAR_T *p) FAST_FUNC;
#endif

char *bs_to_slash(char *p) FAST_FUNC;
void slash_to_bs(char *p) FAST_FUNC;
void strip_dot_space(char *p) FAST_FUNC;
size_t remove_cr(char *p, size_t len) FAST_FUNC;

int err_win_to_posix(void);

ULONGLONG CompatGetTickCount64(void);
#define GetTickCount64 CompatGetTickCount64

ssize_t get_random_bytes(void *buf, ssize_t count);
int enumerate_links(const char *file, char *name);

int unc_root_len(const char *dir) FAST_FUNC;
int root_len(const char *path) FAST_FUNC;
const char *get_system_drive(void) FAST_FUNC;
int chdir_system_drive(void);
char *xabsolute_path(char *path) FAST_FUNC;
char *get_drive_cwd(const char *path, char *buffer, int size) FAST_FUNC;
void fix_path_case(char *path) FAST_FUNC;
void make_sparse(int fd, off_t start, off_t end) FAST_FUNC;
int terminal_mode(int reset) FAST_FUNC;
int unix_path(const char *path) FAST_FUNC;
int has_path(const char *file) FAST_FUNC;
int is_relative_path(const char *path) FAST_FUNC;
char *get_last_slash(const char *path) FAST_FUNC;
const char *applet_to_exe(const char *name) FAST_FUNC;
char *get_user_name(void);
char *quote_arg(const char *arg) FAST_FUNC;
char *find_first_executable(const char *name) FAST_FUNC;
char *xappendword(const char *str, const char *word) FAST_FUNC;
int windows_env(void);
void change_critical_error_dialogs(const char *newval) FAST_FUNC;
char *exe_relative_path(const char *tail) FAST_FUNC;
enum {
	ELEVATED_PRIVILEGE = 1,
	ADMIN_ENABLED = 2
};
int elevation_state(void);
void set_interp(int i) FAST_FUNC;
int mingw_shell_execute(SHELLEXECUTEINFO *info);
void mingw_die_if_error(NTSTATUS status, const char *function_name);
