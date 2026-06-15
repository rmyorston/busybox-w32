/*
 * A WIN32 implementation of mntent routines.
 */
#define MNTENT_PRIVATE
#include "libbb.h"

struct mntstate {
	DWORD drives;
	int index;
	HANDLE volh;
	char vol_name[MAX_PATH];
	char *mount_list;
	char *mount_ptr;
	char *drive_vol_name[26];
	struct mntdata data;
};

void FAST_FUNC init_mntdata(struct mntdata *data)
{
	// initialise pointers
	data->me.mnt_fsname = data->mnt_fsname;
	data->me.mnt_volname = data->mnt_volname;
	data->me.mnt_dir = data->mnt_dir;
	data->me.mnt_type = data->mnt_type;
}

static int fill_mntdata(struct mntdata *data, int index)
{
	char buf[PATH_MAX], drive[4] = "A:\\";

	init_mntdata(data);
	if (index < 26) {
		/* Device is specified by drive letter */
		drive[0] = 'A' + index;
		data->mnt_fsname[0] = 'A' + index;
		data->mnt_fsname[1] = ':';
		data->mnt_fsname[2] = '\0';
		data->mnt_volname[0] = '\0';
		data->mnt_dir[0] = 'A' + index;
		data->mnt_dir[1] = ':';
		data->mnt_dir[2] = '/';
		data->mnt_dir[3] = '\0';

		switch (GetDriveType(data->mnt_dir)) {
		case DRIVE_FIXED:
		case DRIVE_CDROM:
		case DRIVE_REMOVABLE:
		case DRIVE_REMOTE:
			if (realpath(data->mnt_dir, buf) != NULL) {
				if (isalpha(buf[0]) && strcmp(buf+1, ":/") == 0)
					buf[2] = '\0';
				strcpy(data->mnt_fsname, buf);

				GetVolumeNameForVolumeMountPoint(drive, data->mnt_volname, 50);
			}
			break;
		default:
			return FALSE;
		}
	}

	/* Device was obtained by querying volumes */
	if (!GetVolumeInformation(data->mnt_dir, NULL, 0, NULL, NULL,
					NULL, data->mnt_type, 100)) {
		return FALSE;
	}
	return TRUE;
}

FILE *mingw_setmntent(void)
{
	struct mntstate *state;

	state = xzalloc(sizeof(struct mntstate));

	state->drives = GetLogicalDrives();
	state->index = -1;
	state->volh = INVALID_HANDLE_VALUE;
	/* Everything else is initialised by xzalloc() */
	/* state->vol_name[0] = '\0'; */
	/* state->mount_list = NULL; */
	/* state->mount_ptr = NULL; */
	/* for (int i = 0; i < 26; ++i) */
	/*	state->drive_vol_name[i] = NULL; */

	return (FILE *)state;
}

struct mntent *getmntent(FILE *stream)
{
	struct mntstate *state = (struct mntstate *)stream;
	struct mntent *entry = NULL;

	/* First we query each drive letter */
	while (++state->index < 26) {
		if ((state->drives & 1 << state->index) != 0) {
			if (fill_mntdata(&state->data, state->index)) {
				/* Record volume name, if any */
				if (state->data.mnt_volname[0] != '\0') {
					state->drive_vol_name[state->index] =
							xstrdup(state->data.mnt_volname);
				}
				return &state->data.me;
			}
		}
	}

	/* Here we iterate through each volume and their mount points */
	if (state->volh == INVALID_HANDLE_VALUE) {
		state->volh = FindFirstVolume(state->vol_name, MAX_PATH);
		if (state->volh == INVALID_HANDLE_VALUE)
			return NULL;
	}

	/* If we're at the end of a mount point list we need the next volume */
	if (state->mount_list && state->mount_ptr[0] == '\0') {
 next_vol:
		free(state->mount_list);
		state->mount_list = state->mount_ptr = NULL;
		if (!FindNextVolume(state->volh, state->vol_name, MAX_PATH)) {
			return NULL;
		}
	}

	/* If we don't have a mount point list we need to get one */
	if (state->mount_list == NULL) {
		DWORD need = 0;

		/* Mount point list is a single string array which may contain
		 * many items separated by NUL, terminated by two NULs */
		GetVolumePathNamesForVolumeName(state->vol_name, NULL, 0, &need);
		state->mount_list = state->mount_ptr = xmalloc(need);
		GetVolumePathNamesForVolumeName(state->vol_name, state->mount_list,
											need, &need);
		if (state->mount_ptr[0] == '\0')
			goto next_vol;
	}

	/* Skip mount points like 'C:/', we've already done them */
	if (strlen(state->mount_ptr) == 3 &&
			state->drives & 1 << (toupper(state->mount_ptr[0]) - 'A')) {
		state->mount_ptr += 4;
		if (state->mount_ptr[0] == '\0')
			goto next_vol;
	}

	strcpy(state->data.mnt_fsname, state->vol_name);
	/* If volume name matches one recorded for a drive letter, use
	 * the letter, it looks prettier in 'df' output */
	for (int i = 0; i < 26; ++i) {
		if (state->drive_vol_name[i] &&
				strcmp(state->drive_vol_name[i], state->vol_name) == 0) {
			state->data.mnt_fsname[0] = 'A' + i;
			state->data.mnt_fsname[1] = ':';
			state->data.mnt_fsname[2] = '\0';
			break;
		}
	}

	/* Return volume name/mount point details */
	strcpy(state->data.mnt_volname, state->vol_name);
	strcpy(state->data.mnt_dir, state->mount_ptr);
	bs_to_slash(state->data.mnt_dir);
	if (fill_mntdata(&state->data, state->index)) {
		entry = &state->data.me;
	}
	/* move to next item in mount point list */
	state->mount_ptr += strlen(state->mount_ptr) + 1;

	return entry;
}

int endmntent(FILE *stream)
{
	struct mntstate *state = (struct mntstate *)stream;

	if (state->volh != INVALID_HANDLE_VALUE)
		FindVolumeClose(state->volh);
	for (int i = 0; i < 26; ++i)
		free(state->drive_vol_name[i]);
	free(state->mount_list);
	free(stream);
	return 0;
}
