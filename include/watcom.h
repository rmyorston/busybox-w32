/* a collection of watcom-specific hacks */

#ifndef WATCOM_HACKS
#define WATCOM_HACKS

/* enable various Windows and standard functions in Watcom libc */
#define __STDC_WANT_LIB_EXT1__ 1
#include <winsock2.h>
#include <tchar.h>
#include <windows.h> 
#include <windowsx.h>
#include <winnt.h>
#include <stdio.h>
#include <stdlib.h>
#include <utime.h>
#include <process.h>

/* getting rid of GCC-isms */
#undef __volatile__
#define __volatile__ /* nothing */
#undef __attribute__
#define __attribute__(x) /*nothing*/

/* we try to keep as close as possible to MinGW port */
typedef long long off64_t;
#undef SIGUSR1
#undef SIGUSR2
#include "mingw.h"
#undef ENABLE_PLATFORM_MINGW32
#define ENABLE_PLATFORM_MINGW32 1
#undef IF_PLATFORM_MINGW32
#undef IF_NOT_PLATFORM_MINGW32
#define IF_PLATFORM_MINGW32(...) __VA_ARGS__

/* from Wine*/
#define REPARSE_DATA_BUFFER_HEADER_SIZE 8
#define MAXIMUM_REPARSE_DATA_BUFFER_SIZE  (16*1024)

/* what to do about signals? some namespace conflicts with signals < 12 */

#define lutimes utimes
#define P_NOWAIT 1

/* eliminate cases where mingw.h conflicts with system headers */

/* some other re-definitions
if possible, we want to use native Watom functions 
and not MinGW replacement functions */

/* Windows does not have unix-style file permissions */
#undef mkdir
#define mkdir(name,mode) mkdir(name)
#define mktemp _mktemp

/* popen and _popen seem functionally equivalent */
#undef popen
#undef pclose
#define popen _popen
#define pclose _pclose

/*various */
#ifndef BB_VER
#define BB_VER "1.23.0.watcom2"
#endif

#define BB_MMU 1

#define NSIG 13


#endif /* WATCOM_HACKS */



