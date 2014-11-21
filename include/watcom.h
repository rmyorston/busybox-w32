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
#include <wincon.h>

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

#define HAVE_CHSIZE 1
#define _GL_WINDOWS_64_BIT_OFF_T 1
#define REGEX_MALLOC 1

#define mingw_open open 
#define mingw_dup2 dup2 
#define mingw_fopen fopen


/* from Wine*/
#define REPARSE_DATA_BUFFER_HEADER_SIZE 8
#define MAXIMUM_REPARSE_DATA_BUFFER_SIZE  (16*1024)

/* what to do about signals? some namespace conflicts with signals < 12 */

#define lutimes utimes

/* eliminate cases where mingw.h conflicts with system headers */

/* some other re-definitions
if possible, we want to use native Watom functions 
and not MinGW replacement functions */
#define useconds_t unsigned long

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

#define NSIG 13

extern int ftruncate (int fd, off_t length);

/* explicitly link system libraries here
 * using -Wl, ... with spaces does not work that great
 * -l seems to make owcc want to look for .a libraries
 * By linking to the object files, I hope that the linker
 * will be less confused at the later stage */

#pragma library ("clib3r.lib")
#pragma library ("kernel32.lib")
#pragma library ("math387r.lib")
#pragma library ("crypt32.lib")
#pragma library ("userenv.lib")
#pragma library ("ws2_32.lib")
#pragma library ("wr7rdll.lib") /* for mktemp */





#endif /* WATCOM_HACKS */



