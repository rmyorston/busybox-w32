/* a collection of watcom-specific hacks */

#ifndef WATCOM_HACKS
#define WATCOM_HACKS 1

#define _POSIX_SOURCE 1
#define _LINUX_SOURCE 1

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#define ENABLE_USE_PORTABLE_CODE 1

#include <sys/socket.h>
#include <netdb.h> /*h_error*/
#include <netinet/in.h> /* sockaddr_in*/
#include <arpa/inet.h> /* inet_aton */

/* hacks */
#include <musl-netdb.h>


/* getting rid of GCC-isms */
#undef __volatile__
#define __volatile__ /* nothing */
#undef __attribute__
#define __attribute__(x) /*nothing*/

/* try to use platform POSIX */
#undef ENABLE_PLATFORM_POSIX
#define ENABLE_PLATFORM_POSIX 1
#undef IF_PLATFORM_POSIX
#undef IF_NOT_PLATFORM_POSIX
#define IF_PLATFORM_POSIX(...) __VA_ARGS__

#define NOIMPL(name,...) static inline int name(__VA_ARGS__) { errno = ENOSYS; return -1; }
#define IMPL(name,ret,retval,...) static inline ret name(__VA_ARGS__) { return retval; }

/*various */
#ifndef BB_VER
#define BB_VER "1.23.0.watcom2"
#endif

#define stime p_stime
#define utime p_utime

#define ULONGLONG unsigned long long
#define DWORD long

#define NSIG 12

#include <sys/wait.h>
#define WCOREDUMP __WCOREDUMP

int utimes(const char, struct timeval *);
typedef unsigned char* RE_TRANSLATE_TYPE;
typedef void (*sighandler_t)(int);

#endif /* WATCOM_HACKS */



