/*
 * A simple WIN32 implementation of mntent routines.  It only handles
 * fixed logical drives.
 */
#include "libbb.h"

struct mntdata {
	DWORD flags;
	int index;
	struct mntent me;
	char mnt_fsname[4];
	char mnt_dir[4];
	char mnt_type[100];
	char mnt_opts[4];
};

FILE *setmntent(const char *file UNUSED_PARAM, const char *mode UNUSED_PARAM)
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
			data->mnt_dir[2] = '\\';
			data->mnt_dir[3] = '\0';
			data->mnt_type[0] = '\0';
			data->mnt_opts[0] = '\0';

			if ( GetDriveType(data->mnt_dir) == DRIVE_FIXED ) {
				if ( !GetVolumeInformation(data->mnt_dir, NULL, 0, NULL, NULL,
								NULL, data->mnt_type, 100) ) {
					data->mnt_type[0] = '\0';
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
