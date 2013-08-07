#include <sys/utime.h>

#define NOIMPL(name,...) static inline int name(__VA_ARGS__) { errno = ENOSYS; return -1; }
#define IMPL(name,ret,retval,...) static inline ret name(__VA_ARGS__) { return retval; }

/*
 * sys/types.h
 */
typedef int gid_t;
typedef int uid_t;
#ifdef _WIN64
typedef __int64 pid_t;
#else
typedef int pid_t;
#endif

/*
 * arpa/inet.h
 */
static inline unsigned int git_ntohl(unsigned int x) { return (unsigned int)ntohl(x); }
#define ntohl git_ntohl
int inet_aton(const char *cp, struct in_addr *inp);

/*
 * fcntl.h
 */
#define F_DUPFD 0
#define F_GETFD 1
#define F_SETFD 2
#define F_GETFL 3
#define F_SETFL 3
#define FD_CLOEXEC 0x1
#define O_NONBLOCK 04000

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
IMPL(getgrgid,struct group *,NULL,gid_t gid UNUSED_PARAM);
NOIMPL(initgroups,const char *group UNUSED_PARAM,gid_t gid UNUSED_PARAM);
static inline void endgrent(void) {}

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
	char *pw_gecos;
	char *pw_dir;
	char *pw_shell;
	uid_t pw_uid;
	gid_t pw_gid;
};

IMPL(getpwnam,struct passwd *,NULL,const char *name UNUSED_PARAM);
struct passwd *getpwuid(int uid);

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

int sigaction(int sig, struct sigaction *in, struct sigaction *out);
sighandler_t mingw_signal(int sig, sighandler_t handler);
NOIMPL(sigfillset,int *mask UNUSED_PARAM);
NOIMPL(FAST_FUNC sigprocmask_allsigs, int how UNUSED_PARAM);
NOIMPL(FAST_FUNC sigaction_set,int signo UNUSED_PARAM, const struct sigaction *sa UNUSED_PARAM);

#define signal mingw_signal
/*
 * stdio.h
 */
#define fseeko(f,o,w) fseek(f,o,w)

int fdprintf(int fd, const char *format, ...);
FILE* mingw_fopen(const char *filename, const char *mode);
int mingw_rename(const char*, const char*);
#define fopen mingw_fopen
#define rename mingw_rename

FILE *mingw_popen(const char *cmd, const char *mode);
int mingw_pclose(FILE *fd);
#undef popen
#undef pclose
#define popen mingw_popen
#define pclose mingw_pclose

#define setlinebuf(fd) setvbuf(fd, (char *) NULL, _IOLBF, 0)

/*
 * ANSI emulation wrappers
 */

int winansi_fputs(const char *str, FILE *stream);
int winansi_printf(const char *format, ...) __attribute__((format (printf, 1, 2)));
int winansi_fprintf(FILE *stream, const char *format, ...) __attribute__((format (printf, 2, 3)));
#define fputs winansi_fputs
#define printf(...) winansi_printf(__VA_ARGS__)
#define fprintf(...) winansi_fprintf(__VA_ARGS__)

int winansi_get_terminal_width_height(struct winsize *win);

/*
 * stdlib.h
 */
#define WIFEXITED(x) ((unsigned)(x) < 259)	/* STILL_ACTIVE */
#define WEXITSTATUS(x) ((x) & 0xff)
#define WIFSIGNALED(x) ((unsigned)(x) > 259)
#define WTERMSIG(x) ((x) & 0x7f)
#define WCOREDUMP(x) 0

int mingw_system(const char *cmd);
#define system mingw_system

int clearenv(void);
char *mingw_getenv(const char *name);
int mkstemp(char *template);
char *realpath(const char *path, char *resolved_path);
int setenv(const char *name, const char *value, int replace);
void unsetenv(const char *env);

#define getenv mingw_getenv

/*
 * sys/ioctl.h
 */

#define TIOCGWINSZ 0x5413

int ioctl(int fd, int code, ...);

/*
 * sys/socket.h
 */
#define hstrerror strerror

#ifdef CONFIG_WIN32_NET
int mingw_socket(int domain, int type, int protocol);
int mingw_connect(int sockfd, struct sockaddr *sa, size_t sz);

# define socket mingw_socket
# define connect mingw_connect
#endif
NOIMPL(mingw_sendto,SOCKET s UNUSED_PARAM, const char *buf UNUSED_PARAM, int len UNUSED_PARAM, int flags UNUSED_PARAM, const struct sockaddr *sa UNUSED_PARAM, int salen UNUSED_PARAM);
NOIMPL(mingw_listen,SOCKET s UNUSED_PARAM,int backlog UNUSED_PARAM);
NOIMPL(mingw_bind,SOCKET s UNUSED_PARAM,const struct sockaddr* sa UNUSED_PARAM,int salen UNUSED_PARAM);

/* Windows declaration is different */
#define socket mingw_socket
#define sendto mingw_sendto
#define listen mingw_listen
#define bind mingw_bind

/*
 * sys/stat.h
 */
typedef int blkcnt_t;
typedef int nlink_t;

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
#define mkdir mingw_mkdir

/* Use mingw_lstat()/mingw_stat() instead of lstat()/stat() and
 * mingw_fstat() instead of fstat() on Windows.
 */
#if ENABLE_LFS
# define off_t off64_t
#endif
#define lseek _lseeki64
#define stat _stat64
int mingw_lstat(const char *file_name, struct stat *buf);
int mingw_stat(const char *file_name, struct stat *buf);
int mingw_fstat(int fd, struct stat *buf);
#define fstat mingw_fstat
#define lstat mingw_lstat
#define _stati64(x,y) mingw_stat(x,y)

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
struct itimerval {
	struct timeval it_value, it_interval;
};
#define ITIMER_REAL 0

int setitimer(int type, struct itimerval *in, struct itimerval *out);

/*
 * sys/wait.h
 */
#define WNOHANG 1
#define WUNTRACED 2
int waitpid(pid_t pid, int *status, unsigned options);

/*
 * time.h
 */
struct tm *gmtime_r(const time_t *timep, struct tm *result);
struct tm *localtime_r(const time_t *timep, struct tm *result);
char *strptime(const char *s, const char *format, struct tm *tm);

/*
 * unistd.h
 */
#define PIPE_BUF 8192

IMPL(alarm,unsigned int,0,unsigned int seconds UNUSED_PARAM);
NOIMPL(chown,const char *path UNUSED_PARAM, uid_t uid UNUSED_PARAM, gid_t gid UNUSED_PARAM);
NOIMPL(chroot,const char *root UNUSED_PARAM);
int mingw_dup2 (int fd, int fdto);
char *mingw_getcwd(char *pointer, int len);


#ifdef USE_WIN32_MMAP
int mingw_getpagesize(void);
#define getpagesize mingw_getpagesize
#else
int getpagesize(void);	/* defined in MinGW's libgcc.a */
#endif

IMPL(getgid,int,1,void);
NOIMPL(getgroups,int n UNUSED_PARAM,gid_t *groups UNUSED_PARAM);
IMPL(getppid,int,1,void);
IMPL(getegid,int,1,void);
IMPL(geteuid,int,1,void);
NOIMPL(getsid,pid_t pid UNUSED_PARAM);
IMPL(getuid,int,1,void);
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
NOIMPL(setsid,void);
NOIMPL(setuid,uid_t gid UNUSED_PARAM);
unsigned int sleep(unsigned int seconds);
NOIMPL(symlink,const char *oldpath UNUSED_PARAM, const char *newpath UNUSED_PARAM);
static inline void sync(void) {}
NOIMPL(ttyname_r,int fd UNUSED_PARAM, char *buf UNUSED_PARAM, int sz UNUSED_PARAM);
int mingw_unlink(const char *pathname);
NOIMPL(vfork,void);
int mingw_access(const char *name, int mode);

#define dup2 mingw_dup2
#define getcwd mingw_getcwd
#define lchown chown
#define open mingw_open
#define unlink mingw_unlink

#undef access
#define access mingw_access

/*
 * utime.h
 */
int mingw_utime(const char *file_name, const struct utimbuf *times);
int utimes(const char *file_name, const struct timeval times[2]);

#define utime mingw_utime

/*
 * MinGW specific
 */
#define is_dir_sep(c) ((c) == '/' || (c) == '\\')
#define PRIuMAX "I64u"

pid_t mingw_spawn(char **argv);
int mingw_execv(const char *cmd, const char *const *argv);
int mingw_execvp(const char *cmd, const char *const *argv);
int mingw_execve(const char *cmd, const char *const *argv, const char *const *envp);
pid_t mingw_spawn_applet(int mode, const char *applet, const char *const *argv, const char *const *envp);
pid_t mingw_spawn_1(int mode, const char *cmd, const char *const *argv, const char *const *envp);
#define execvp mingw_execvp
#define execve mingw_execve
#define execv mingw_execv

const char * next_path_sep(const char *path);
#define has_dos_drive_prefix(path) (isalpha(*(path)) && (path)[1] == ':')
#define is_absolute_path(path) ((path)[0] == '/' || has_dos_drive_prefix(path))

/*
 * helpers
 */

char **copy_environ(const char *const *env);
void free_environ(char **env);
char **env_setenv(char **env, const char *name);

const char *get_busybox_exec_path(void);
void init_winsock(void);

char *win32_execable_file(const char *p);
