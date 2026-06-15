#ifndef MNTENT_H
#define MNTENT_H

#include <stdio.h>

struct mntent {
	char *mnt_fsname;   /* Device or server for filesystem.  */
	char *mnt_volname;  /* Windows volume name */
	char *mnt_dir;      /* Directory mounted on.  */
	char *mnt_type;     /* Type of filesystem: ufs, nfs, etc.  */
};

extern FILE *mingw_setmntent(void);
extern struct mntent *getmntent(FILE *stream);
extern int endmntent(FILE *stream);

# if defined(MNTENT_PRIVATE)
struct mntdata {
	struct mntent me;
	char mnt_fsname[PATH_MAX];
	char mnt_volname[50];
	char mnt_dir[PATH_MAX];
	char mnt_type[100];
};

void init_mntdata(struct mntdata *data) FAST_FUNC;
# endif

#define setmntent(f, m) mingw_setmntent()

#endif
