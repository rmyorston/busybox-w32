/* a collection of watcom-specific hacks */

#ifndef WATCOM_HACKS
#define WATCOM_HACKS 1

/*various */
#define NOIMPL(name,...) static inline int name(__VA_ARGS__) { errno = ENOSYS; return -1; }
#define IMPL(name,ret,retval,...) static inline ret name(__VA_ARGS__) { return retval; }

#define ENABLE_USE_PORTABLE_CODE 1
#define _SC_CLK_TCK 2

/* enable various Windows and standard functions in Watcom libc */
#define __STDC_WANT_LIB_EXT1__ 1
#define HAVE_CHSIZE 1
#define _GL_WINDOWS_64_BIT_OFF_T 1
/* from Wine*/
#define REPARSE_DATA_BUFFER_HEADER_SIZE 8
#define MAXIMUM_REPARSE_DATA_BUFFER_SIZE  (16*1024)
#define __USE_GNU	1

/*other headers*/
#include <tchar.h>
#include <windows.h>
#include <windowsx.h>
#include <winnt.h>
#include <stdio.h>
#include <stdlib.h>
#include <utime.h>
#include <process.h>
#include <wincon.h>
#include <alloca.h>

/* getting rid of GCC-isms */
#undef __volatile__
#define __volatile__ /* nothing */
#undef __attribute__
#define __attribute__(x) /*nothing*/

#if !defined(LOAD_LIBRARY_SEARCH_SYSTEM32)
#define LOAD_LIBRARY_SEARCH_SYSTEM32   0x00000800
#endif

/* we try to keep as close as possible to MinGW port */
typedef long long off64_t;
#undef IF_PLATFORM_MINGW32
#define IF_PLATFORM_MINGW32(...) __VA_ARGS__

#define WIFEXITED(x) ((unsigned)(x) < 259)	/* STILL_ACTIVE */
#define WEXITSTATUS(x) ((x) & 0xff)
#define WIFSIGNALED(x) ((unsigned)(x) > 259)
#define WTERMSIG(x) ((x) & 0x7f)
#define WCOREDUMP(x) 0

intptr_t FAST_FUNC mingw_spawn_proc(const char **argv);
pid_t FAST_FUNC mingw_spawn(char **argv);
#define spawn mingw_spawn


/* signals */
#define SIGHUP SIGTERM
#define SIGQUIT SIGABRT
#define SIGKILL SIGTERM
#define SIGPIPE SIG_IGN
#define SIGALRM SIG_IGN
#define SIGCHLD SIG_IGN
#define SIGCONT SIGUSR2
#define SIGSTOP SIGUSR1
#define SIGTSTP SIGUSR1
#define SIGTTIN SIGUSR1
#define SIGTTOU SIGUSR1
#define SIGXCPU SIG_IGN
#define SIGXFSZ SIG_IGN
#define SIGVTALRM SIG_IGN
#define SIGWINCH SIG_IGN

#define SIG_UNBLOCK SIGUSR3

#define NSIG 13

typedef void (*sighandler_t)(int);
struct sigaction {
	sighandler_t sa_handler;
	unsigned sa_flags;
	int sa_mask;
};
#define __cdecl /*nothing*/
#define sigemptyset(x) (void)0
#define SA_RESTART 0

int sigaction(int sig, struct sigaction *in, struct sigaction *out);
NOIMPL(FAST_FUNC sigprocmask_allsigs, int how UNUSED_PARAM);

#define WINDOWS_SOCKETS 1
#define SHUT_WR SD_SEND
#define hstrerror strerror

#define EAFNOSUPPORT ENXIO

/* time */

#define _timezone timezone
#define _tzset tzset
#define clock_t long

#define TICKS_PER_SECOND 100
#define MS_PER_TICK 10
#define HNSEC_PER_TICK 100000

struct tms {
	clock_t tms_utime;		/* user CPU time */
	clock_t tms_stime;		/* system CPU time */
	clock_t tms_cutime;		/* user CPU time of children */
	clock_t tms_cstime;		/* system CPU time of children */
};

clock_t times(struct tms *buf);

#define lutimes utimes
#define P_NOWAIT 1
#define useconds_t unsigned long

struct tm *localtime_r(const time_t *timep, struct tm *result);
int utimes(const char *file_name, const struct timeval times[2]);
int stime(time_t *t);
char *strptime(const char *s, const char *format, struct tm *tm);

struct timespec {
	time_t tv_sec;
	long int tv_nsec;
};

#define WNOHANG 1
#define WUNTRACED 2
int waitpid(pid_t pid, int *status, int options);

/* File system, io and directories */

#undef mkdir
#define mkdir(name,mode) mkdir(name)
#define mktemp _mktemp
#undef popen
#undef pclose
#define popen _popen
#define pclose _pclose
#define fseeko(f,o,w) fseek(f,o,w)
#define lchown chown
#define mingw_read read

IMPL(fchmod,int,0,int fildes UNUSED_PARAM, mode_t mode UNUSED_PARAM);
NOIMPL(fchown,int fd UNUSED_PARAM, uid_t uid UNUSED_PARAM, gid_t gid UNUSED_PARAM);
NOIMPL(symlink,const char *oldpath UNUSED_PARAM, const char *newpath UNUSED_PARAM);
NOIMPL(readlink,const char *path UNUSED_PARAM, char *buf UNUSED_PARAM, size_t bufsiz UNUSED_PARAM);
NOIMPL(mknod,const char *name UNUSED_PARAM, mode_t mode UNUSED_PARAM, dev_t device UNUSED_PARAM);
#define is_absolute_path(path) ((path)[0] == '/' || has_dos_drive_prefix(path))
#define has_dos_drive_prefix(path) 0

extern int ftruncate (int fd, off_t length);
int link(const char *oldpath, const char *newpath);

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

int ioctl(int fd, int code, ...);

/* window/terminal size */

#define TIOCGWINSZ 0x5413
int winansi_get_terminal_width_height(struct winsize *win);


#define PIPE_BUF 8192

/* users and permissions */

struct group {
	char *gr_name;
	char *gr_passwd;
	gid_t gr_gid;
	char **gr_mem;
};

#define DEFAULT_UID 1000
#define DEFAULT_GID 1000

IMPL(getgrnam,struct group *,NULL,const char *name UNUSED_PARAM);
IMPL(getgrgid,struct group *,NULL,gid_t gid UNUSED_PARAM);
IMPL(getegid,int,1,void);
IMPL(geteuid,int,1,void);
IMPL(getuid,int,DEFAULT_UID,void);
NOIMPL(getsid,pid_t pid UNUSED_PARAM);
NOIMPL(setsid,void);
NOIMPL(setgid,gid_t gid UNUSED_PARAM);
NOIMPL(setuid,uid_t gid UNUSED_PARAM);
NOIMPL(setegid,gid_t gid UNUSED_PARAM);
NOIMPL(seteuid,uid_t gid UNUSED_PARAM);
IMPL(getpwnam,struct passwd *,NULL,const char *name UNUSED_PARAM);
static inline void setpwent(void) {}
NOIMPL(initgroups,const char *group UNUSED_PARAM,gid_t gid UNUSED_PARAM);
static inline void endgrent(void) {}
IMPL(alarm,unsigned int,0,unsigned int seconds UNUSED_PARAM);
NOIMPL(chroot,const char *root UNUSED_PARAM);
NOIMPL(fchdir,int fd UNUSED_PARAM);
static inline void endpwent(void) {}
IMPL(getpwent_r,int,ENOENT,struct passwd *pwbuf UNUSED_PARAM,char *buf UNUSED_PARAM,size_t buflen UNUSED_PARAM,struct passwd **pwbufp UNUSED_PARAM);
IMPL(getpwent,struct passwd *,NULL,void)
IMPL(getppid,int,1,void);

struct passwd {
	char *pw_name;
	char *pw_passwd;
	char *pw_gecos;
	char *pw_dir;
	char *pw_shell;
	uid_t pw_uid;
	gid_t pw_gid;
};

/* memory and process handling */
NOIMPL(vfork,void);

#define BB_MMU 1

#define F_DUPFD 0
#define F_GETFD 1
#define F_SETFD 2
#define F_GETFL 3
#define F_SETFL 3
#define FD_CLOEXEC 0x1
#define O_NONBLOCK 04000
#define O_NOFOLLOW 0

int fcntl(int fd, int cmd, ...);

typedef int sa_family_t;
struct sockaddr_un {
	sa_family_t sun_family;
	char sun_path[1]; /* to make compiler happy, don't bother */
};

#define fork() -1
NOIMPL(ttyname_r,int fd UNUSED_PARAM, char *buf UNUSED_PARAM, int sz UNUSED_PARAM);

/*
 * arpa/inet.h
 */
static inline unsigned int git_ntohl(unsigned int x) { return (unsigned int)ntohl(x); }
#define ntohl git_ntohl
int inet_aton(const char *cp, struct in_addr *inp);
int inet_pton(int af, const char *src, void *dst);

/*
 * helpers
 */

char **copy_environ(const char *const *env);
void free_environ(char **env);
char **env_setenv(char **env, const char *name);

const char *get_busybox_exec_path(void);
void init_winsock(void);

char *file_is_win32_executable(const char *p);

#endif /* WATCOM_HACKS */



