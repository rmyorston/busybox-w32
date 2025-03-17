#include "libbb.h"

struct DIR {
	struct dirent dd_dir;
	HANDLE dd_handle;     /* FindFirstFile handle */
	int not_first;
	int got_dot;
	int got_dotdot;
};

static inline void finddata2dirent(struct dirent *ent, WIN32_FIND_DATAA *fdata)
{
	/* copy file name from WIN32_FIND_DATA to dirent */
	strcpy(ent->d_name, fdata->cFileName);

	if ((fdata->dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) &&
			(fdata->dwReserved0 == IO_REPARSE_TAG_SYMLINK ||
			fdata->dwReserved0 ==  IO_REPARSE_TAG_MOUNT_POINT ||
			fdata->dwReserved0 ==  IO_REPARSE_TAG_APPEXECLINK))
		ent->d_type = DT_LNK;
	else if (fdata->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		ent->d_type = DT_DIR;
	else
		ent->d_type = DT_REG;
}

DIR *opendir(const char *name)
{
	char pattern[MAX_PATH];
	WIN32_FIND_DATAA fdata;
	HANDLE h;
	int len;
	DIR *dir;

	/* check that name is not NULL */
	if (!name) {
		errno = EINVAL;
		return NULL;
	}
	/* check that the pattern won't be too long for FindFirstFileA */
	len = strlen(name);
	if (len + 2 >= MAX_PATH) {
		errno = ENAMETOOLONG;
		return NULL;
	}
	/* copy name to temp buffer */
	strcpy(pattern, name);

	/* append optional '/' and wildcard '*' */
	if (len && !is_dir_sep(pattern[len - 1]))
		pattern[len++] = '/';
	pattern[len++] = '*';
	pattern[len] = 0;

	/* open find handle */
	h = FindFirstFileA(pattern, &fdata);
	if (h == INVALID_HANDLE_VALUE) {
		DWORD err = GetLastError();
		errno = (err == ERROR_DIRECTORY) ? ENOTDIR : err_win_to_posix();
		return NULL;
	}

	/* initialize DIR structure and copy first dir entry */
	dir = xzalloc(sizeof(DIR));
	dir->dd_handle = h;
	/* dir->not_first = 0; */
	/* dir->got_dot = 0; */
	/* dir->got_dotdot = 0; */
	finddata2dirent(&dir->dd_dir, &fdata);
	return dir;
}

struct dirent *readdir(DIR *dir)
{
	if (!dir) {
		errno = EBADF; /* No set_errno for mingw */
		return NULL;
	}

	/* if first entry, dirent has already been set up by opendir */
	if (dir->not_first) {
		/* get next entry and convert from WIN32_FIND_DATA to dirent */
		WIN32_FIND_DATAA fdata;
		if (FindNextFileA(dir->dd_handle, &fdata)) {
			finddata2dirent(&dir->dd_dir, &fdata);
		} else if (!dir->got_dot) {
			strcpy(dir->dd_dir.d_name, ".");
			dir->dd_dir.d_type = DT_DIR;
		} else if (!dir->got_dotdot) {
			strcpy(dir->dd_dir.d_name, "..");
			dir->dd_dir.d_type = DT_DIR;
		} else {
			DWORD lasterr = GetLastError();
			/* POSIX says you shouldn't set errno when readdir can't
			   find any more files; so, if another error we leave it set. */
			if (lasterr != ERROR_NO_MORE_FILES)
				errno = err_win_to_posix();
			return NULL;
		}
	}

	/* Have we seen '.' or '..'? */
	if (dir->dd_dir.d_name[0] == '.') {
		if (dir->dd_dir.d_name[1] == '\0')
			dir->got_dot = TRUE;
		else if (dir->dd_dir.d_name[1] == '.' && dir->dd_dir.d_name[2] == '\0')
			dir->got_dotdot = TRUE;
	}

	dir->not_first = TRUE;
	return &dir->dd_dir;
}

int closedir(DIR *dir)
{
	if (!dir) {
		errno = EBADF;
		return -1;
	}

	FindClose(dir->dd_handle);
	free(dir);
	return 0;
}
