/*
 * A simple WIN32 implementation of mntent routines.  It only handles
 * logical drives.
 */
#include "libbb.h"

struct mntdata {
	DWORD flags;
	int index;
	struct mntent me;
	char mnt_fsname[PATH_MAX];
	char mnt_dir[4];
	char mnt_type[100];
	char mnt_opts[4];
};

FILE *mingw_setmntent(void)
{
	struct mntdata *data;

	if ( (data=malloc(sizeof(struct mntdata))) == NULL ) {
		return NULL;
	}

	data->flags = GetLogicalDrives();
	data->index = -1;

	return (FILE *)data;
}

struct mntent *getmntent(FILE *stream)
{
	struct mntdata *data = (struct mntdata *)stream;
	struct mntent *entry;
	UINT drive_type;
	char buf[PATH_MAX];

	data->me.mnt_fsname = data->mnt_fsname;
	data->me.mnt_dir = data->mnt_dir;
	data->me.mnt_type = data->mnt_type;
	data->me.mnt_opts = data->mnt_opts;
	data->me.mnt_freq = 0;
	data->me.mnt_passno = 0;

	entry = NULL;
	while ( ++data->index < 26 ) {
		if ( (data->flags & 1<<data->index) != 0 ) {
			data->mnt_fsname[0] = 'A' + data->index;
			data->mnt_fsname[1] = ':';
			data->mnt_fsname[2] = '\0';
			data->mnt_dir[0] = 'A' + data->index;
			data->mnt_dir[1] = ':';
			data->mnt_dir[2] = '/';
			data->mnt_dir[3] = '\0';
			data->mnt_type[0] = '\0';
			data->mnt_opts[0] = '\0';

			drive_type = GetDriveType(data->mnt_dir);
			if ( drive_type == DRIVE_FIXED || drive_type == DRIVE_CDROM ||
						drive_type == DRIVE_REMOVABLE ||
						drive_type == DRIVE_REMOTE ) {
				if ( !GetVolumeInformation(data->mnt_dir, NULL, 0, NULL, NULL,
								NULL, data->mnt_type, 100) ) {
					continue;
				}

				if (realpath(data->mnt_dir, buf) != NULL) {
					if (isalpha(buf[0]) && strcmp(buf+1, ":/") == 0)
						buf[2] = '\0';
					strcpy(data->mnt_fsname, buf);
				}

				entry = &data->me;
				break;
			}
		}
	}

	return entry;
}

int endmntent(FILE *stream)
{
	free(stream);
	return 0;
}
