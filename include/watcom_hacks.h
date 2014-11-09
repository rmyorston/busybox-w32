/* a collection of watcom-specific hacks */

#ifndef WATCOM_HACKS
#define WATCOM_HACKS

#define __STDC_WANT_LIB_EXT1__ 1
#define __volatile__ /* nothing */
#define __attribute__(x) /*nothing*/

/* file system stuff */
#define fseeko (off_t) fseek
#define fchmod chmod

/* note to self: perhaps the stub functions should be placed somewhere else... */
/* copied from cpio sysdep.c */
int
mknod ( const char *filename,  int mode,  int dev)
{
  return -1;
}
int
chown ( const char *filename,  int owner,  int group)
{
  return -1;
}
int
link ( const char *oldname,  const char *newname)
{
  return -1;
}
int
symlink ( const char *oldname,  const char *newname)
{
  return -1;
}
/* end of copied section */

/* system stuff */
#include <env.h>
#include <process.h>
int ftruncate ( int fildes, off_t length)
{
 return -1;
}
#include <stdio.h>
#define popen _popen
#define pclose _pclose



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


/* group stuff */
int getgroups(int gidsetsize, gid_t grouplist[]) {
 return 0;
}
gid_t getgid(void) {
  return 0;
}
uid_t getuid(void) {
 return 0;
}
#define geteuid getuid
#define getegid getgid

/* annoying things with INIT_ stuff that I do not understand */
// xzalloc in libbb.h
// barrier() played tricks, need to look into that

#ifndef BB_VER
#define BB_VER "1.23.0.watcom2"
#endif


#endif



