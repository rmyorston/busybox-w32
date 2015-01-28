#include <stdlib.h>
#include <stdio.h>
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

/* the following functions fail to link when debug build with CFLAGS -g2 is used */

void BUG_sizeof(void) {
}

void sed_free_and_close_stuff(void) {
}

void data_extract_to_command(void) {
}

void open_transformer(void) {
}

void check_errors_in_children(void) {
}








