/* a collection of watcom-specific hacks */

#ifndef WATCOM_HACKS
#define WATCOM_HACKS

#define __STDC_WANT_LIB_EXT1__ 1
#define __volatile__ /* nothing */
#define __attribute__(x) /*nothing*/

/* file system stuff */
#define fseeko (off_t) fseek
#define fchmod chmod
#define lchown chown

int mknod ( const char *filename,  int mode,  int dev);
int chown ( const char *filename,  int owner,  int group);
int link ( const char *oldname,  const char *newname);
int symlink ( const char *oldname,  const char *newname);
int fcntl(int fd, int cmd, ...);
long sysconf(int name);

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


/* system stuff */
#include <env.h>
#include <process.h>
int ftruncate ( int fildes, off_t length);
#include <stdio.h>
#define popen _popen
#define pclose _pclose

/* Windows does not have unix-style permissions */
#define mkdir(name,mode) mkdir(name)

/*
 * stdlib.h
 */
#define WIFEXITED(x) ((unsigned)(x) < 259)	/* STILL_ACTIVE */
#define WEXITSTATUS(x) ((x) & 0xff)
#define WIFSIGNALED(x) ((unsigned)(x) > 259)
#define WTERMSIG(x) ((x) & 0x7f)
#define WCOREDUMP(x) 0

typedef void (*sighandler_t)(int);

struct sigaction {
	sighandler_t sa_handler;
	unsigned sa_flags;
	int sa_mask;
};

int getpagesize(void);

/*
 * sys/ioctl.h
 */

#define TIOCGWINSZ 0x5413

/* #define procps_status_t.stime procps_status_t.p_stime
#define procps_status_t.utime procps_status_t.p_utime */

#include <tchar.h>
#include <windows.h> 
#include <windowsx.h>
#include <winnt.h>

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

/* time stuff */
#include <utime.h>
#define utimes utime
#define lutimes _utime
/* char *strptime(const char *restrict buf, const char *restrict format,
       struct tm *restrict tm) {
 return -1;
}*/ /* in win32 folder */
#define localtime_r localtime_s
#define stime (int) time

/*
 * sys/wait.h
 */
#define WNOHANG 1
#define WUNTRACED 2
int waitpid(pid_t pid, int *status, unsigned options);


/* group stuff */
int getgroups(int gidsetsize, gid_t grouplist[]);
gid_t getgid(void);
uid_t getuid(void);
#define geteuid getuid
#define getegid getgid

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

/*
 * grp.h
 */

struct group {
	char *gr_name;
	char *gr_passwd;
	gid_t gr_gid;
	char **gr_mem;
};

/*various */
#ifndef BB_VER
#define BB_VER "1.23.0.watcom2"
#endif

/* probably need to fix these ... */
#define SIGALRM 14
#define SIGWINCH 28
#define SIGCONT 18
#define SIGSTOP 19

#define _SC_CLK_TCK 2




#endif



