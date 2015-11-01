#ifndef _SYS_STATFS_H
#define _SYS_STATFS_H 1

#include <stdint.h>

struct statfs {
	int f_type;
	uint64_t f_bsize;
	uint64_t f_frsize;
	uint64_t f_blocks;
	uint64_t f_bfree;
	uint64_t f_bavail;
	uint64_t f_files;
	uint64_t f_ffree;
	uint64_t f_fsid;
	uint64_t f_flag;
	uint64_t f_namelen;
};

extern int statfs(const char *file, struct statfs *buf);

#endif
