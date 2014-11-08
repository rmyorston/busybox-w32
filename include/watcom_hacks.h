/* a collection of watcom-specific hacks */

#ifndef WATCOM_HACKS
#define WATCOM_HACKS

#define __STDC_WANT_LIB_EXT1__ 1
#define __volatile__ /* nothing */
#define __attribute__(x) /*nothing*/

/* file system stuff */
#define fseeko (off_t) fseek
#define fchmod chmod

/* copied from cpio sysdep.c */
int
mknod ( const char *filename __attribute__ ((unused)) ,  int mode __attribute__ ((unused)) ,  int dev __attribute__ ((unused)) )
{
  return -1;
}
int
chown ( const char *filename __attribute__ ((unused)) ,  int owner __attribute__ ((unused)) ,  int group __attribute__ ((unused)) )
{
  return -1;
}
int
link ( const char *oldname __attribute__ ((unused)) ,  const char *newname __attribute__ ((unused)) )
{
  return -1;
}
int
symlink ( const char *oldname __attribute__ ((unused)) ,  const char *newname __attribute__ ((unused)) )
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


#endif



