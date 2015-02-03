/* a collection of watcom-specific hacks */

#ifndef WATCOM_HACKS
#define WATCOM_HACKS 1

/*various */
#ifndef BB_VER
#define BB_VER "1.23.0.watcom2"
#endif

#define NOIMPL(name,...) static inline int name(__VA_ARGS__) { errno = ENOSYS; return -1; }
#define IMPL(name,ret,retval,...) static inline ret name(__VA_ARGS__) { return retval; }

#define ENABLE_USE_PORTABLE_CODE 1

/* enable various Windows and standard functions in Watcom libc */
#define __STDC_WANT_LIB_EXT1__ 1
#define HAVE_CHSIZE 1
#define _GL_WINDOWS_64_BIT_OFF_T 1
/* from Wine*/
#define REPARSE_DATA_BUFFER_HEADER_SIZE 8
#define MAXIMUM_REPARSE_DATA_BUFFER_SIZE  (16*1024)

#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0501 /* Windows XP */
#undef WINVER
#define WINVER 0x0501

#include <winsock2.h>
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

/* we try to keep as close as possible to MinGW port */
typedef long long off64_t;
#undef ENABLE_PLATFORM_MINGW32
#define ENABLE_PLATFORM_MINGW32 1
#undef IF_PLATFORM_MINGW32
#undef IF_NOT_PLATFORM_MINGW32
#define IF_PLATFORM_MINGW32(...) __VA_ARGS__

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

typedef void (__cdecl *sighandler_t)(int);
struct sigaction {
	sighandler_t sa_handler;
	unsigned sa_flags;
	int sa_mask;
};
#define sigemptyset(x) (void)0
#define SA_RESTART 0

int sigaction(int sig, struct sigaction *in, struct sigaction *out);


/* time */

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

/* File system and directories */

#undef mkdir
#define mkdir(name,mode) mkdir(name)
#define mktemp _mktemp
#undef popen
#undef pclose
#define popen _popen
#define pclose _pclose
#define fseeko(f,o,w) fseek(f,o,w)
#define lchown chown

IMPL(fchmod,int,0,int fildes UNUSED_PARAM, mode_t mode UNUSED_PARAM);
NOIMPL(fchown,int fd UNUSED_PARAM, uid_t uid UNUSED_PARAM, gid_t gid UNUSED_PARAM);
NOIMPL(symlink,const char *oldpath UNUSED_PARAM, const char *newpath UNUSED_PARAM);
NOIMPL(mknod,const char *name UNUSED_PARAM, mode_t mode UNUSED_PARAM, dev_t device UNUSED_PARAM);

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

/* users and permissions */

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

struct passwd {
	char *pw_name;
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

int fcntl(int fd, int cmd, ...);


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



