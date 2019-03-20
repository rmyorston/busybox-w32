#include <sys/statfs.h>
#include "libbb.h"

/*
 * Code from libguestfs (with addition of GetVolumeInformation call)
 */
int statfs(const char *file, struct statfs *buf)
{
	ULONGLONG free_bytes_available;         /* for user - similar to bavail */
	ULONGLONG total_number_of_bytes;
	ULONGLONG total_number_of_free_bytes;   /* for everyone - bfree */
	DWORD serial, namelen, flags;
	char fsname[100];
	struct mntent *mnt;

	if ( (mnt=find_mount_point(file, 0)) == NULL ) {
		return -1;
	}

	file = mnt->mnt_dir;
	if ( !GetDiskFreeSpaceEx(file, (PULARGE_INTEGER) &free_bytes_available,
			(PULARGE_INTEGER) &total_number_of_bytes,
			(PULARGE_INTEGER) &total_number_of_free_bytes) ) {
		errno = err_win_to_posix();
		return -1;
	}

	if ( !GetVolumeInformation(file, NULL, 0, &serial, &namelen, &flags,
								fsname, 100) ) {
		errno = err_win_to_posix();
		return -1;
	}

	/* XXX I couldn't determine how to get block size.  MSDN has a
	 * unhelpful hard-coded list here:
	 * http://support.microsoft.com/kb/140365
	 * but this depends on the filesystem type, the size of the disk and
	 * the version of Windows.  So this code assumes the disk is NTFS
	 * and the version of Windows is >= Win2K.
	 */
	if (total_number_of_bytes < UINT64_C(16) * 1024 * 1024 * 1024 * 1024)
		buf->f_bsize = 4096;
	else if (total_number_of_bytes < UINT64_C(32) * 1024 * 1024 * 1024 * 1024)
		buf->f_bsize = 8192;
	else if (total_number_of_bytes < UINT64_C(64) * 1024 * 1024 * 1024 * 1024)
		buf->f_bsize = 16384;
	else if (total_number_of_bytes < UINT64_C(128) * 1024 * 1024 * 1024 * 1024)
		buf->f_bsize = 32768;
	else
		buf->f_bsize = 65536;

	/*
	 * Valid filesystem names don't seem to be documented.  The following
	 * are present in Wine (dlls/kernel32/volume.c).
	 */
	if ( strcmp(fsname, "NTFS") == 0 ) {
		buf->f_type = 0x5346544e;
	}
	else if ( strcmp(fsname, "FAT") == 0 || strcmp(fsname, "FAT32") == 0 ) {
		buf->f_type = 0x4006;
	}
	else if ( strcmp(fsname, "CDFS") == 0 ) {
		buf->f_type = 0x9660;
	}
	else if ( strcmp(fsname, "UDF") == 0 ) {
		buf->f_type = 0x15013346;
	}
	else {
		buf->f_type = 0;
	}

	buf->f_frsize = buf->f_bsize;
	buf->f_blocks = total_number_of_bytes / buf->f_bsize;
	buf->f_bfree = total_number_of_free_bytes / buf->f_bsize;
	buf->f_bavail = free_bytes_available / buf->f_bsize;
	buf->f_files = 0;
	buf->f_ffree = 0;
	buf->f_fsid = serial;
	buf->f_flag = 0;
	buf->f_namelen = namelen;

	return 0;
}
