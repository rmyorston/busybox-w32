#ifndef MNTENT_H
#define MNTENT_H

#include <stdio.h>

struct mntent {
	char *mnt_fsname;   /* Device or server for filesystem.  */
	char *mnt_dir;      /* Directory mounted on.  */
	char *mnt_type;     /* Type of filesystem: ufs, nfs, etc.  */
	char *mnt_opts;     /* Comma-separated options for fs.  */
	int mnt_freq;       /* Dump frequency (in days).  */
	int mnt_passno;     /* Pass number for `fsck'.  */
};

extern FILE *mingw_setmntent(void);
extern struct mntent *getmntent(FILE *stream);
extern int endmntent(FILE *stream);

#define setmntent(f, m) mingw_setmntent()

#endif
