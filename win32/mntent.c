/*
 * A simple WIN32 implementation of mntent routines.  It only handles
 * logical drives.
 */
#define MNTENT_PRIVATE
#include "libbb.h"

struct mntstate {
	DWORD drives;
	int index;
};

int fill_mntdata(struct mntdata *data, int index)
{
	UINT drive_type;
	char buf[PATH_MAX];

	// initialise pointers and scalar data
	data->me.mnt_fsname = data->mnt_fsname;
	data->me.mnt_dir = data->mnt_dir;
	data->me.mnt_type = data->mnt_type;
	data->me.mnt_opts = data->mnt_opts;
	data->me.mnt_freq = 0;
	data->me.mnt_passno = 0;

	// initialise strings
	data->mnt_fsname[0] = 'A' + index;
	data->mnt_fsname[1] = ':';
	data->mnt_fsname[2] = '\0';
	data->mnt_dir[0] = 'A' + index;
	data->mnt_dir[1] = ':';
	data->mnt_dir[2] = '/';
	data->mnt_dir[3] = '\0';
	data->mnt_type[0] = '\0';
	data->mnt_opts[0] = '\0';

	drive_type = GetDriveType(data->mnt_dir);
	if (drive_type == DRIVE_FIXED || drive_type == DRIVE_CDROM ||
			drive_type == DRIVE_REMOVABLE || drive_type == DRIVE_REMOTE) {
		if (!GetVolumeInformation(data->mnt_dir, NULL, 0, NULL, NULL,
						NULL, data->mnt_type, 100)) {
			return FALSE;
		}

		if (realpath(data->mnt_dir, buf) != NULL) {
			if (isalpha(buf[0]) && strcmp(buf+1, ":/") == 0)
				buf[2] = '\0';
			strcpy(data->mnt_fsname, buf);
		}
		return TRUE;
	}
	return FALSE;
}

FILE *mingw_setmntent(void)
{
	struct mntstate *state;

	if ( (state=malloc(sizeof(struct mntstate))) == NULL ) {
		return NULL;
	}

	state->drives = GetLogicalDrives();
	state->index = -1;

	return (FILE *)state;
}

struct mntent *getmntent(FILE *stream)
{
	struct mntstate *state = (struct mntstate *)stream;
	static struct mntdata *data = NULL;
	struct mntent *entry = NULL;

	while (++state->index < 26) {
		if ((state->drives & 1 << state->index) != 0) {
			if (data == NULL)
				data = xmalloc(sizeof(*data));

			if (fill_mntdata(data, state->index)) {
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
