#include "libbb.h"

#define LONG_PATH_MAX 32800

static wchar_t *wide_path_buf;
static wchar_t *wide_pattern_buf;

struct DIR {
	struct dirent dd_dir;
	HANDLE dd_handle;     /* FindFirstFile handle */
	int not_first;
	int got_dot;
	int got_dotdot;
	int use_wide;
};

static inline unsigned char get_dtype(DWORD attr, DWORD reserved0)
{
	if ((attr & FILE_ATTRIBUTE_REPARSE_POINT) &&
			(reserved0 == IO_REPARSE_TAG_SYMLINK ||
			reserved0 == IO_REPARSE_TAG_MOUNT_POINT ||
			reserved0 == IO_REPARSE_TAG_APPEXECLINK))
		return DT_LNK;
	if (attr & FILE_ATTRIBUTE_DIRECTORY)
		return DT_DIR;
	return DT_REG;
}

static inline void finddata2dirent(struct dirent *ent, WIN32_FIND_DATAA *fdata)
{
	/* copy file name from WIN32_FIND_DATA to dirent */
	strcpy(ent->d_name, fdata->cFileName);
	ent->d_type = get_dtype(fdata->dwFileAttributes, fdata->dwReserved0);
}

static inline void finddata2dirent_w(struct dirent *ent, WIN32_FIND_DATAW *fdata)
{
	WideCharToMultiByte(CP_ACP, 0, fdata->cFileName, -1,
			ent->d_name, PATH_MAX, NULL, NULL);
	ent->d_type = get_dtype(fdata->dwFileAttributes, fdata->dwReserved0);
}

DIR *opendir(const char *name)
{
	char pattern[MAX_PATH];
	WIN32_FIND_DATAA fdata;
	WIN32_FIND_DATAW fdataw;
	HANDLE h;
	int len, use_wide = 0;
	DIR *dir;

	/* check that name is not NULL */
	if (!name) {
		errno = EINVAL;
		return NULL;
	}

	len = strlen(name);

	/* use wide API for long paths - need headroom for child names */
	if (len + 40 >= MAX_PATH) {
		DWORD wlen;

		if (!wide_path_buf) {
			wide_path_buf = xmalloc(LONG_PATH_MAX * sizeof(wchar_t));
			wide_pattern_buf = xmalloc(LONG_PATH_MAX * sizeof(wchar_t));
		}

		if (MultiByteToWideChar(CP_ACP, 0, name, -1, wide_path_buf, LONG_PATH_MAX) == 0) {
			errno = EINVAL;
			return NULL;
		}

		/* build \\?\<absolute_path>\* pattern using GetFullPathNameW */
		memcpy(wide_pattern_buf, L"\\\\?\\", 4 * sizeof(wchar_t));
		wlen = GetFullPathNameW(wide_path_buf, LONG_PATH_MAX - 8, wide_pattern_buf + 4, NULL);
		if (wlen == 0 || wlen >= LONG_PATH_MAX - 8) {
			errno = ENAMETOOLONG;
			return NULL;
		}
		wlen += 4;
		if (wide_pattern_buf[wlen - 1] != L'\\' && wide_pattern_buf[wlen - 1] != L'/')
			wide_pattern_buf[wlen++] = L'\\';
		wide_pattern_buf[wlen++] = L'*';
		wide_pattern_buf[wlen] = 0;

		h = FindFirstFileW(wide_pattern_buf, &fdataw);
		use_wide = 1;
		goto check_handle;
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

 check_handle:
	if (h == INVALID_HANDLE_VALUE) {
		DWORD err = GetLastError();
		errno = (err == ERROR_DIRECTORY) ? ENOTDIR : err_win_to_posix();
		return NULL;
	}

	/* initialize DIR structure and copy first dir entry */
	dir = xzalloc(sizeof(DIR));
	dir->dd_handle = h;
	dir->use_wide = use_wide;
	/* dir->not_first = 0; */
	/* dir->got_dot = 0; */
	/* dir->got_dotdot = 0; */
	if (use_wide)
		finddata2dirent_w(&dir->dd_dir, &fdataw);
	else
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
		int found = 0;

		if (dir->use_wide) {
			WIN32_FIND_DATAW fdataw;
			if (FindNextFileW(dir->dd_handle, &fdataw)) {
				finddata2dirent_w(&dir->dd_dir, &fdataw);
				found = 1;
			}
		} else {
			WIN32_FIND_DATAA fdata;
			if (FindNextFileA(dir->dd_handle, &fdata)) {
				finddata2dirent(&dir->dd_dir, &fdata);
				found = 1;
			}
		}

		if (!found) {
			if (!dir->got_dot) {
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
