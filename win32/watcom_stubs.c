#include <stdlib.h>
#include <errno.h>

typedef int gid_t;
typedef int uid_t;

#undef mknod
int
mknod ( const char *filename,  int mode,  int dev)
{
  errno = ENOSYS;
  return -1;
}

#undef chown
int
chown ( const char *filename,  int owner,  int group)
{
  errno = ENOSYS;
  return -1;
}
/* int
link ( const char *oldname,  const char *newname)
{
  return -1;
} */ /* implemented in win32/mingw.c */

#undef symlink
int
symlink ( const char *oldname,  const char *newname)
{
  errno = ENOSYS;
  return -1;
}

#undef getgroups
int getgroups(int gidsetsize, gid_t grouplist[]) {
 errno = ENOSYS;
 return 0;
}

#undef getgid
gid_t getgid(void) {
  errno = ENOSYS;
  return 0;
}

#undef getuid
uid_t getuid(void) {
 errno = ENOSYS;
 return 0;
}








