#include <libbb.h>

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
/* int
link ( const char *oldname,  const char *newname)
{
  return -1;
} */ /* implemented in win32/mingw.c */
int
symlink ( const char *oldname,  const char *newname)
{
  return -1;
}
int getgroups(int gidsetsize, gid_t grouplist[]) {
 return 0;
}
gid_t getgid(void) {
  return 0;
}
uid_t getuid(void) {
 return 0;
}
int ftruncate ( int fildes, off_t length)
{
 return -1;
}

/* from http://www.genesys-e.org/jwalter/mix4win.htm */
/* getpagesize for windows */
long getpagesize (void) {
    static long g_pagesize = 0;
    if (! g_pagesize) {
        SYSTEM_INFO system_info;
        GetSystemInfo (&system_info);
        g_pagesize = system_info.dwPageSize;
    }
    return g_pagesize;
}
long getregionsize (void) {
    static long g_regionsize = 0;
    if (! g_regionsize) {
        SYSTEM_INFO system_info;
        GetSystemInfo (&system_info);
        g_regionsize = system_info.dwAllocationGranularity;
    }
    return g_regionsize;
}






