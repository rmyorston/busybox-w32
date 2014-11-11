#include <libbb.h>
#include <errno.h>

int
mknod ( const char *filename,  int mode,  int dev)
{
  errno = ENOSYS;
  return -1;
}
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
int
symlink ( const char *oldname,  const char *newname)
{
  errno = ENOSYS;
  return -1;
}
int getgroups(int gidsetsize, gid_t grouplist[]) {
 errno = ENOSYS;
 return 0;
}
gid_t getgid(void) {
  errno = ENOSYS;
  return 0;
}
uid_t getuid(void) {
 errno = ENOSYS;
 return 0;
}







