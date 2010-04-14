#include <sys/utime.h>

#define NOIMPL(name,...) static inline int name(__VA_ARGS__) { errno = ENOSYS; return -1; }
#define IMPL(name,ret,retval,...) static inline ret name(__VA_ARGS__) { return retval; }

/*
 * sys/types.h
 */
typedef int gid_t;
typedef int uid_t;
typedef int pid_t;

/*
 * arpa/inet.h
 */
static inline unsigned int git_ntohl(unsigned int x) { return (unsigned int)ntohl(x); }
#define ntohl git_ntohl

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
 * poll.h
 */
struct pollfd {
	int fd;           /* file descriptor */
	short events;     /* requested events */
	short revents;    /* returned events */
};
typedef unsigned long nfds_t;
#define POLLIN 1
#define POLLHUP 2

int poll(struct pollfd *ufds, unsigned int nfds, int timeout);

/*
 * pwd.h
 */
struct passwd {
	char *pw_name;
	char *pw_gecos;
	char *pw_dir;
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

#define signal mingw_signal
/*
 * stdio.h
 */
#define fseeko(f,o,w) fseek(f,o,w)

int fdprintf(int fd, const char *format, ...);

/*
 * stdlib.h
 */
#define WIFEXITED(x) ((unsigned)(x) < 259)	/* STILL_ACTIVE */
#define WEXITSTATUS(x) ((x) & 0xff)
#define WIFSIGNALED(x) ((unsigned)(x) > 259)
#define WTERMSIG(x) ((x) & 0x7f)

NOIMPL(clearenv,void);
IMPL(mingw_getenv,char*,NULL,const char *name UNUSED_PARAM);
int mkstemp(char *template);
char *realpath(const char *path, char *resolved_path);
NOIMPL(setenv,const char *name UNUSED_PARAM, const char *value UNUSED_PARAM, int replace UNUSED_PARAM);
IMPL(unsetenv,void,,const char *env UNUSED_PARAM);

#define getenv mingw_getenv
/*
 * string.h
 */
char *strsep(char **stringp, const char *delim);

/*
 * sys/ioctl.h
 */

#define TIOCGWINSZ 0x5413

NOIMPL(ioctl,int fd UNUSED_PARAM, int code UNUSED_PARAM,...);

/*
 * sys/socket.h
 */
#define hstrerror strerror

NOIMPL(mingw_socket,int domain UNUSED_PARAM, int type UNUSED_PARAM, int protocol UNUSED_PARAM);
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

NOIMPL(fchmod,int fildes UNUSED_PARAM, mode_t mode UNUSED_PARAM);
NOIMPL(fchown,int fd UNUSED_PARAM, uid_t uid UNUSED_PARAM, gid_t gid UNUSED_PARAM);
int mingw_mkdir(const char *path, int mode);

#define mkdir mingw_mkdir
#define lstat stat

/*
 * sys/sysmacros.h
 */
#define makedev(a,b) 0*(a)*(b) /* avoid unused warning */
#define minor(x) 0
#define major(x) 0

/*
 * sys/time.h
 */
struct itimerval {
	struct timeval it_value, it_interval;
};
#define ITIMER_REAL 0

int setitimer(int type, struct itimerval *in, struct itimerval *out);

/*
 * sys/wait.h
 */
#define WNOHANG 1
int waitpid(pid_t pid, int *status, unsigned options);

/*
 * time.h
 */
struct tm *gmtime_r(const time_t *timep, struct tm *result);
struct tm *localtime_r(const time_t *timep, struct tm *result);
IMPL(strptime,char*,NULL,const char *s UNUSED_PARAM, const char *format UNUSED_PARAM, struct tm *tm UNUSED_PARAM);

/*
 * unistd.h
 */
#define PIPE_BUF 8192

IMPL(alarm,unsigned int,0,unsigned int seconds UNUSED_PARAM);
NOIMPL(chown,const char *path UNUSED_PARAM, uid_t uid UNUSED_PARAM, gid_t gid UNUSED_PARAM);
NOIMPL(chroot,const char *root UNUSED_PARAM);
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
NOIMPL(kill,pid_t pid UNUSED_PARAM, int sig UNUSED_PARAM);
int link(const char *oldpath, const char *newpath);
NOIMPL(mknod,const char *name UNUSED_PARAM, mode_t mode UNUSED_PARAM, dev_t device UNUSED_PARAM);
int pipe(int filedes[2]);
NOIMPL(readlink,const char *path UNUSED_PARAM, char *buf UNUSED_PARAM, size_t bufsiz UNUSED_PARAM);
NOIMPL(setgid,gid_t gid UNUSED_PARAM);
NOIMPL(setsid,void);
NOIMPL(setuid,uid_t gid UNUSED_PARAM);
unsigned int sleep(unsigned int seconds);
NOIMPL(symlink,const char *oldpath UNUSED_PARAM, const char *newpath UNUSED_PARAM);
static inline void sync(void) {}
NOIMPL(vfork,void);

#define getcwd mingw_getcwd
#define lchown(a,b,c) chown(a,b,c)

/*
 * utime.h
 */
int mingw_utime(const char *file_name, const struct utimbuf *times);
NOIMPL(utimes,const char *filename UNUSED_PARAM, const struct timeval times[2] UNUSED_PARAM);

#define utime mingw_utime

/*
 * MinGW specific
 */
#define has_dos_drive_prefix(path) (isalpha(*(path)) && (path)[1] == ':')
#define is_dir_sep(c) ((c) == '/' || (c) == '\\')
#define PRIuMAX "I64u"

/*
 * helpers
 */

char **copy_environ(const char *const *env);
void free_environ(char **env);
char **env_setenv(char **env, const char *name);

const char *get_busybox_exec_path(void);
