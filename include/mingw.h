
#define NOIMPL(name,...) static inline int name(__VA_ARGS__) { errno = ENOSYS; return -1; }
#define IMPL(name,ret,retval,...) static inline ret name(__VA_ARGS__) { return retval; }

/*
 * sys/types.h
 */
typedef int gid_t;
typedef int uid_t;
#ifndef _WIN64
typedef int pid_t;
#else
typedef __int64 pid_t;
#endif

#define DEFAULT_UID 1000
#define DEFAULT_GID 1000

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
#define SIGHUP 1
#define SIGQUIT 3
#define SIGKILL 9
#define SIGUSR1 10
#define SIGUSR2 12
#define SIGPIPE 13
#define SIGALRM 14
#define SIGCHLD 17
#define SIGCONT 18
#define SIGSTOP 19
#define SIGTSTP 20
#define SIGTTIN 21
#define SIGTTOU 22
#define SIGXCPU 24
#define SIGXFSZ 25
#define SIGVTALRM 26
#define SIGWINCH 28

#define SIG_UNBLOCK 1

typedef void (__cdecl *sighandler_t)(int);
struct sigaction {
	sighandler_t sa_handler;
	unsigned sa_flags;
	int sa_mask;
};
#define sigemptyset(x) (void)0
#define SA_RESTART 0

NOIMPL(sigaction,int sig UNUSED_PARAM, struct sigaction *in UNUSED_PARAM, struct sigaction *out UNUSED_PARAM);
NOIMPL(sigfillset,int *mask UNUSED_PARAM);
NOIMPL(FAST_FUNC sigprocmask_allsigs, int how UNUSED_PARAM);
NOIMPL(FAST_FUNC sigaction_set,int signo UNUSED_PARAM, const struct sigaction *sa UNUSED_PARAM);

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
int mingw_popen_fd(const char *cmd, const char *mode, int fd0, pid_t *pid);
int mingw_pclose(FILE *fd);
#undef popen
#undef pclose
#define popen mingw_popen
#define pclose mingw_pclose

#define setlinebuf(fd) setvbuf(fd, (char *) NULL, _IOLBF, 0)

/*
 * ANSI emulation wrappers
 */

void move_cursor_row(int n);
void reset_screen(void);
int winansi_putchar(int c);
int winansi_puts(const char *s);
size_t winansi_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int winansi_fputs(const char *str, FILE *stream);
int winansi_vfprintf(FILE *stream, const char *format, va_list list);
int winansi_printf(const char *format, ...) __attribute__((format (printf, 1, 2)));
int winansi_fprintf(FILE *stream, const char *format, ...) __attribute__((format (printf, 2, 3)));
int winansi_write(int fd, const void *buf, size_t count);
int winansi_read(int fd, void *buf, size_t count);
int winansi_getc(FILE *stream);
#define putchar winansi_putchar
#define puts winansi_puts
#define fwrite winansi_fwrite
#define fputs winansi_fputs
#define vfprintf(stream, ...) winansi_vfprintf(stream, __VA_ARGS__)
#define vprintf(...) winansi_vfprintf(stdout, __VA_ARGS__)
#define printf(...) winansi_printf(__VA_ARGS__)
#define fprintf(...) winansi_fprintf(__VA_ARGS__)
#define write winansi_write
#define read winansi_read
#define getc winansi_getc

int winansi_get_terminal_width_height(struct winsize *win);

/*
 * stdlib.h
 */
#define WTERMSIG(x) ((x) & 0x7f)
#define WIFEXITED(x) (WTERMSIG(x) == 0)
#define WEXITSTATUS(x) (((x) & 0xff00) >> 8)
#define WIFSIGNALED(x) (((signed char) (((x) & 0x7f) + 1) >> 1) > 0)
#define WCOREDUMP(x) 0

int mingw_system(const char *cmd);
#define system mingw_system

int clearenv(void);
char *mingw_getenv(const char *name);
int mingw_putenv(const char *env);
char *mingw_mktemp(char *template);
int mkstemp(char *template);
char *realpath(const char *path, char *resolved_path);
int setenv(const char *name, const char *value, int replace);
#if ENABLE_SAFE_ENV
int unsetenv(const char *env);
#else
void unsetenv(const char *env);
#endif

#define getenv mingw_getenv
#if ENABLE_SAFE_ENV
#define putenv mingw_putenv
#endif
#define mktemp mingw_mktemp

/*
 * string.h
 */
void *mempcpy(void *dest, const void *src, size_t n);

/*
 * strings.h
 */
int ffs(int i);

/*
 * sys/ioctl.h
 */

#define TIOCGWINSZ 0x5413

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

NOIMPL(mingw_sendto,SOCKET s UNUSED_PARAM, const char *buf UNUSED_PARAM, int len UNUSED_PARAM, int flags UNUSED_PARAM, const struct sockaddr *sa UNUSED_PARAM, int salen UNUSED_PARAM);

#define socket mingw_socket
#define connect mingw_connect
#define sendto mingw_sendto
#define listen mingw_listen
#define bind mingw_bind
#define setsockopt mingw_setsockopt
#define shutdown mingw_shutdown
#define accept mingw_accept
#define select mingw_select

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

IMPL(fchmod,int,0,int fildes UNUSED_PARAM, mode_t mode UNUSED_PARAM);
NOIMPL(fchown,int fd UNUSED_PARAM, uid_t uid UNUSED_PARAM, gid_t gid UNUSED_PARAM);
int mingw_mkdir(const char *path, int mode);
int mingw_chmod(const char *path, int mode);

#define mkdir mingw_mkdir
#define chmod mingw_chmod

#if ENABLE_LFS && !defined(__MINGW64_VERSION_MAJOR)
# define off_t off64_t
#endif

typedef int nlink_t;
typedef int blksize_t;
typedef off_t blkcnt_t;

struct mingw_stat {
	dev_t     st_dev;
	ino_t     st_ino;
	mode_t    st_mode;
	nlink_t   st_nlink;
	uid_t     st_uid;
	gid_t     st_gid;
	dev_t     st_rdev;
	off_t     st_size;
	time_t    st_atime;
	time_t    st_mtime;
	time_t    st_ctime;
	blksize_t st_blksize;
	blkcnt_t  st_blocks;
};

int mingw_lstat(const char *file_name, struct mingw_stat *buf);
int mingw_stat(const char *file_name, struct mingw_stat *buf);
int mingw_fstat(int fd, struct mingw_stat *buf);
#undef lstat
#undef stat
#undef fstat
#define lstat mingw_lstat
#define stat mingw_stat
#define fstat mingw_fstat

/*
 * sys/sysmacros.h
 */
#define makedev(a,b) 0*(a)*(b) /* avoid unused warning */
#define minor(x) 0
#define major(x) 0

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

int nanosleep(const struct timespec *req, struct timespec *rem);

/*
 * sys/wait.h
 */
#define WNOHANG 1
#define WUNTRACED 2
int waitpid(pid_t pid, int *status, int options);

/*
 * time.h
 */
struct tm *gmtime_r(const time_t *timep, struct tm *result);
struct tm *localtime_r(const time_t *timep, struct tm *result);
char *strptime(const char *s, const char *format, struct tm *tm);
size_t mingw_strftime(char *buf, size_t max, const char *format, const struct tm *tm);
int stime(time_t *t);

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

IMPL(alarm,unsigned int,0,unsigned int seconds UNUSED_PARAM);
IMPL(chown,int,0,const char *path UNUSED_PARAM, uid_t uid UNUSED_PARAM, gid_t gid UNUSED_PARAM);
NOIMPL(chroot,const char *root UNUSED_PARAM);
NOIMPL(fchdir,int fd UNUSED_PARAM);
int mingw_dup2 (int fd, int fdto);
char *mingw_getcwd(char *pointer, int len);
off_t mingw_lseek(int fd, off_t offset, int whence);


IMPL(getgid,int,DEFAULT_GID,void);
int getgroups(int n, gid_t *groups);
IMPL(getppid,int,1,void);
IMPL(getegid,int,DEFAULT_GID,void);
IMPL(geteuid,int,DEFAULT_UID,void);
NOIMPL(getsid,pid_t pid UNUSED_PARAM);
IMPL(getuid,int,DEFAULT_UID,void);
int getlogin_r(char *buf, size_t len);
int fcntl(int fd, int cmd, ...);
#define fork() -1
IMPL(fsync,int,0,int fd UNUSED_PARAM);
int kill(pid_t pid, int sig);
int link(const char *oldpath, const char *newpath);
NOIMPL(mknod,const char *name UNUSED_PARAM, mode_t mode UNUSED_PARAM, dev_t device UNUSED_PARAM);
int mingw_open (const char *filename, int oflags, ...);
int pipe(int filedes[2]);
NOIMPL(readlink,const char *path UNUSED_PARAM, char *buf UNUSED_PARAM, size_t bufsiz UNUSED_PARAM);
NOIMPL(setgid,gid_t gid UNUSED_PARAM);
NOIMPL(setegid,gid_t gid UNUSED_PARAM);
NOIMPL(setsid,void);
NOIMPL(setuid,uid_t gid UNUSED_PARAM);
NOIMPL(seteuid,uid_t gid UNUSED_PARAM);
unsigned int sleep(unsigned int seconds);
NOIMPL(symlink,const char *oldpath UNUSED_PARAM, const char *newpath UNUSED_PARAM);
static inline void sync(void) {}
long sysconf(int name);
IMPL(getpagesize,int,4096,void);
NOIMPL(ttyname_r,int fd UNUSED_PARAM, char *buf UNUSED_PARAM, int sz UNUSED_PARAM);
int mingw_unlink(const char *pathname);
NOIMPL(vfork,void);
int mingw_access(const char *name, int mode);
int mingw_rmdir(const char *name);

#define dup2 mingw_dup2
#define getcwd mingw_getcwd
#define lchown chown
#define open mingw_open
#define unlink mingw_unlink
#define rmdir mingw_rmdir
#undef lseek
#define lseek mingw_lseek

#undef access
#define access mingw_access

/*
 * utime.h
 */
int utimes(const char *file_name, const struct timeval times[2]);

/*
 * dirent.h
 */
DIR *mingw_opendir(const char *path);
#define opendir mingw_opendir

/*
 * MinGW specific
 */
#define is_dir_sep(c) ((c) == '/' || (c) == '\\')

pid_t FAST_FUNC mingw_spawn(char **argv);
intptr_t FAST_FUNC mingw_spawn_proc(const char **argv);
int mingw_execv(const char *cmd, char *const *argv);
int mingw_execvp(const char *cmd, char *const *argv);
int mingw_execve(const char *cmd, char *const *argv, char *const *envp);
#define spawn mingw_spawn
#define execvp mingw_execvp
#define execve mingw_execve
#define execv mingw_execv

const char * next_path_sep(const char *path);
#define has_dos_drive_prefix(path) (isalpha(*(path)) && (path)[1] == ':')
#define is_absolute_path(path) ((path)[0] == '/' || (path)[0] == '\\' || has_dos_drive_prefix(path))

#define find_mount_point(n, s) find_mount_point(n)
#define add_to_ino_dev_hashtable(s, n) (void)0

/* Ensure that isatty(fd) returns 0 for the NUL device */
static inline int mingw_isatty(int fd)
{
	int result = _isatty(fd);

	if (result) {
		HANDLE handle = (HANDLE) _get_osfhandle(fd);
		CONSOLE_SCREEN_BUFFER_INFO sbi;
		DWORD mode;

	        if (handle == INVALID_HANDLE_VALUE)
	                return 0;

	        /* check if its a device (i.e. console, printer, serial port) */
	        if (GetFileType(handle) != FILE_TYPE_CHAR)
	                return 0;

		if (!fd) {
			if (!GetConsoleMode(handle, &mode))
				return 0;
		} else if (!GetConsoleScreenBufferInfo(handle, &sbi))
			return 0;
	}

	return result;
}
#define isatty mingw_isatty

/*
 * helpers
 */

char **env_setenv(char **env, const char *name);

const char *get_busybox_exec_path(void);
void init_winsock(void);

char *file_is_win32_executable(const char *p);

int err_win_to_posix(DWORD winerr);
