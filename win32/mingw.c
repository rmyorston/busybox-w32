#include "libbb.h"
#include <userenv.h>

#if defined(__MINGW64_VERSION_MAJOR)
#if ENABLE_GLOBBING
int _dowildcard = -1;
#else
int _dowildcard = 0;
#endif

#undef _fmode
int _fmode = _O_BINARY;
#endif

#if !defined(__MINGW64_VERSION_MAJOR)
#if ENABLE_GLOBBING
int _CRT_glob = 1;
#else
int _CRT_glob = 0;
#endif

unsigned int _CRT_fmode = _O_BINARY;
#endif

smallint bb_got_signal;

int err_win_to_posix(DWORD winerr)
{
	int error = ENOSYS;
	switch(winerr) {
	case ERROR_ACCESS_DENIED: error = EACCES; break;
	case ERROR_ACCOUNT_DISABLED: error = EACCES; break;
	case ERROR_ACCOUNT_RESTRICTION: error = EACCES; break;
	case ERROR_ALREADY_ASSIGNED: error = EBUSY; break;
	case ERROR_ALREADY_EXISTS: error = EEXIST; break;
	case ERROR_ARITHMETIC_OVERFLOW: error = ERANGE; break;
	case ERROR_BAD_COMMAND: error = EIO; break;
	case ERROR_BAD_DEVICE: error = ENODEV; break;
	case ERROR_BAD_DRIVER_LEVEL: error = ENXIO; break;
	case ERROR_BAD_EXE_FORMAT: error = ENOEXEC; break;
	case ERROR_BAD_FORMAT: error = ENOEXEC; break;
	case ERROR_BAD_LENGTH: error = EINVAL; break;
	case ERROR_BAD_PATHNAME: error = ENOENT; break;
	case ERROR_BAD_PIPE: error = EPIPE; break;
	case ERROR_BAD_UNIT: error = ENODEV; break;
	case ERROR_BAD_USERNAME: error = EINVAL; break;
	case ERROR_BROKEN_PIPE: error = EPIPE; break;
	case ERROR_BUFFER_OVERFLOW: error = ENAMETOOLONG; break;
	case ERROR_BUSY: error = EBUSY; break;
	case ERROR_BUSY_DRIVE: error = EBUSY; break;
	case ERROR_CALL_NOT_IMPLEMENTED: error = ENOSYS; break;
	case ERROR_CANNOT_MAKE: error = EACCES; break;
	case ERROR_CANTOPEN: error = EIO; break;
	case ERROR_CANTREAD: error = EIO; break;
	case ERROR_CANTWRITE: error = EIO; break;
	case ERROR_CRC: error = EIO; break;
	case ERROR_CURRENT_DIRECTORY: error = EACCES; break;
	case ERROR_DEVICE_IN_USE: error = EBUSY; break;
	case ERROR_DEV_NOT_EXIST: error = ENODEV; break;
	case ERROR_DIRECTORY: error = EINVAL; break;
	case ERROR_DIR_NOT_EMPTY: error = ENOTEMPTY; break;
	case ERROR_DISK_CHANGE: error = EIO; break;
	case ERROR_DISK_FULL: error = ENOSPC; break;
	case ERROR_DRIVE_LOCKED: error = EBUSY; break;
	case ERROR_ENVVAR_NOT_FOUND: error = EINVAL; break;
	case ERROR_EXE_MARKED_INVALID: error = ENOEXEC; break;
	case ERROR_FILENAME_EXCED_RANGE: error = ENAMETOOLONG; break;
	case ERROR_FILE_EXISTS: error = EEXIST; break;
	case ERROR_FILE_INVALID: error = ENODEV; break;
	case ERROR_FILE_NOT_FOUND: error = ENOENT; break;
	case ERROR_GEN_FAILURE: error = EIO; break;
	case ERROR_HANDLE_DISK_FULL: error = ENOSPC; break;
	case ERROR_INSUFFICIENT_BUFFER: error = ENOMEM; break;
	case ERROR_INVALID_ACCESS: error = EACCES; break;
	case ERROR_INVALID_ADDRESS: error = EFAULT; break;
	case ERROR_INVALID_BLOCK: error = EFAULT; break;
	case ERROR_INVALID_DATA: error = EINVAL; break;
	case ERROR_INVALID_DRIVE: error = ENODEV; break;
	case ERROR_INVALID_EXE_SIGNATURE: error = ENOEXEC; break;
	case ERROR_INVALID_FLAGS: error = EINVAL; break;
	case ERROR_INVALID_FUNCTION: error = ENOSYS; break;
	case ERROR_INVALID_HANDLE: error = EBADF; break;
	case ERROR_INVALID_LOGON_HOURS: error = EACCES; break;
	case ERROR_INVALID_NAME: error = EINVAL; break;
	case ERROR_INVALID_OWNER: error = EINVAL; break;
	case ERROR_INVALID_PARAMETER: error = EINVAL; break;
	case ERROR_INVALID_PASSWORD: error = EPERM; break;
	case ERROR_INVALID_PRIMARY_GROUP: error = EINVAL; break;
	case ERROR_INVALID_SIGNAL_NUMBER: error = EINVAL; break;
	case ERROR_INVALID_TARGET_HANDLE: error = EIO; break;
	case ERROR_INVALID_WORKSTATION: error = EACCES; break;
	case ERROR_IO_DEVICE: error = EIO; break;
	case ERROR_IO_INCOMPLETE: error = EINTR; break;
	case ERROR_LOCKED: error = EBUSY; break;
	case ERROR_LOCK_VIOLATION: error = EACCES; break;
	case ERROR_LOGON_FAILURE: error = EACCES; break;
	case ERROR_MAPPED_ALIGNMENT: error = EINVAL; break;
	case ERROR_META_EXPANSION_TOO_LONG: error = E2BIG; break;
	case ERROR_MORE_DATA: error = EPIPE; break;
	case ERROR_NEGATIVE_SEEK: error = ESPIPE; break;
	case ERROR_NOACCESS: error = EFAULT; break;
	case ERROR_NONE_MAPPED: error = EINVAL; break;
	case ERROR_NOT_ENOUGH_MEMORY: error = ENOMEM; break;
	case ERROR_NOT_READY: error = EAGAIN; break;
	case ERROR_NOT_SAME_DEVICE: error = EXDEV; break;
	case ERROR_NO_DATA: error = EPIPE; break;
	case ERROR_NO_MORE_SEARCH_HANDLES: error = EIO; break;
	case ERROR_NO_PROC_SLOTS: error = EAGAIN; break;
	case ERROR_NO_SUCH_PRIVILEGE: error = EACCES; break;
	case ERROR_OPEN_FAILED: error = EIO; break;
	case ERROR_OPEN_FILES: error = EBUSY; break;
	case ERROR_OPERATION_ABORTED: error = EINTR; break;
	case ERROR_OUTOFMEMORY: error = ENOMEM; break;
	case ERROR_PASSWORD_EXPIRED: error = EACCES; break;
	case ERROR_PATH_BUSY: error = EBUSY; break;
	case ERROR_PATH_NOT_FOUND: error = ENOENT; break;
	case ERROR_PIPE_BUSY: error = EBUSY; break;
	case ERROR_PIPE_CONNECTED: error = EPIPE; break;
	case ERROR_PIPE_LISTENING: error = EPIPE; break;
	case ERROR_PIPE_NOT_CONNECTED: error = EPIPE; break;
	case ERROR_PRIVILEGE_NOT_HELD: error = EACCES; break;
	case ERROR_READ_FAULT: error = EIO; break;
	case ERROR_SEEK: error = EIO; break;
	case ERROR_SEEK_ON_DEVICE: error = ESPIPE; break;
	case ERROR_SHARING_BUFFER_EXCEEDED: error = ENFILE; break;
	case ERROR_SHARING_VIOLATION: error = EACCES; break;
	case ERROR_STACK_OVERFLOW: error = ENOMEM; break;
	case ERROR_SWAPERROR: error = ENOENT; break;
	case ERROR_TOO_MANY_LINKS: error = EMLINK; break;
	case ERROR_TOO_MANY_MODULES: error = EMFILE; break;
	case ERROR_TOO_MANY_OPEN_FILES: error = EMFILE; break;
	case ERROR_UNRECOGNIZED_MEDIA: error = ENXIO; break;
	case ERROR_UNRECOGNIZED_VOLUME: error = ENODEV; break;
	case ERROR_WAIT_NO_CHILDREN: error = ECHILD; break;
	case ERROR_WRITE_FAULT: error = EIO; break;
	case ERROR_WRITE_PROTECT: error = EROFS; break;
	}
	return error;
}

static int zero_fd = -1;
static int rand_fd = -1;

/*
 * Determine if 'filename' corresponds to one of the supported
 * device files.  Constants for these are defined as an enum
 * in mingw.h.
 */
int get_dev_type(const char *filename)
{
	int i;
	const char *devname[NOT_DEVICE] = { "null", "zero", "urandom" };

	if (filename && !strncmp(filename, "/dev/", 5)) {
		for (i=0; i<NOT_DEVICE; ++i ) {
			if (!strcmp(filename+5, devname[i])) {
				return i;
			}
		}
	}

	return NOT_DEVICE;
}

void update_dev_fd(int dev, int fd)
{
	if (dev == DEV_ZERO)
		zero_fd = fd;
	else if (dev == DEV_URANDOM)
		rand_fd = fd;
}

#undef open
int mingw_open (const char *filename, int oflags, ...)
{
	va_list args;
	unsigned mode;
	int fd;
	int special = (oflags & O_SPECIAL);
	int dev = get_dev_type(filename);

	/* /dev/null is always allowed, others only if O_SPECIAL is set */
	if (dev != NOT_DEVICE && (dev == DEV_NULL || special)) {
		filename = "nul";
		oflags = O_RDWR;
	}

	va_start(args, oflags);
	mode = va_arg(args, int);
	va_end(args);

	fd = open(filename, oflags&~O_SPECIAL, mode);
	if (fd >= 0) {
		update_dev_fd(dev, fd);
	}
	else if ((oflags & O_ACCMODE) != O_RDONLY && errno == EACCES) {
		DWORD attrs = GetFileAttributes(filename);
		if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY))
			errno = EISDIR;
	}
	return fd;
}

int mingw_xopen(const char *pathname, int flags)
{
	int ret;

	/* allow use of special devices */
	ret = mingw_open(pathname, flags|O_SPECIAL);
	if (ret < 0) {
		bb_perror_msg_and_die("can't open '%s'", pathname);
	}
	return ret;
}

#undef fopen
FILE *mingw_fopen (const char *filename, const char *otype)
{
	if (filename && !strcmp(filename, "/dev/null"))
		filename = "nul";
	return fopen(filename, otype);
}

#undef read
ssize_t mingw_read(int fd, void *buf, size_t count)
{
	if (fd == zero_fd) {
		memset(buf, 0, count);
		return count;
	}
	else if (fd == rand_fd) {
		return get_random_bytes(buf, count);
	}
	return read(fd, buf, count);
}

#undef close
int mingw_close(int fd)
{
	if (fd == zero_fd) {
		zero_fd = -1;
	}
	if (fd == rand_fd) {
		rand_fd = -1;
	}
	return close(fd);
}

#undef dup2
int mingw_dup2 (int fd, int fdto)
{
	int ret = dup2(fd, fdto);
	return ret != -1 ? fdto : -1;
}

/*
 * The unit of FILETIME is 100-nanoseconds since January 1, 1601, UTC.
 * Returns the 100-nanoseconds ("hekto nanoseconds") since the epoch.
 */
static inline long long filetime_to_hnsec(const FILETIME *ft)
{
	long long winTime = ((long long)ft->dwHighDateTime << 32) + ft->dwLowDateTime;
	/* Windows to Unix Epoch conversion */
	return winTime - 116444736000000000LL;
}

static inline time_t filetime_to_time_t(const FILETIME *ft)
{
	return (time_t)(filetime_to_hnsec(ft) / 10000000);
}

static inline mode_t file_attr_to_st_mode(DWORD attr)
{
	mode_t fMode = S_IRUSR|S_IRGRP|S_IROTH;
	if (attr & FILE_ATTRIBUTE_DIRECTORY)
		fMode |= S_IFDIR|S_IWUSR|S_IWGRP|S_IXUSR|S_IXGRP|S_IXOTH;
	else
		fMode |= S_IFREG;
	if (!(attr & FILE_ATTRIBUTE_READONLY))
		fMode |= S_IWUSR|S_IWGRP;
	return fMode;
}

static inline int get_file_attr(const char *fname, WIN32_FILE_ATTRIBUTE_DATA *fdata)
{
	if (GetFileAttributesExA(fname, GetFileExInfoStandard, fdata))
		return 0;

	if (GetLastError() == ERROR_SHARING_VIOLATION) {
		HANDLE hnd;
		WIN32_FIND_DATA fd;

		if ((hnd=FindFirstFile(fname, &fd)) != INVALID_HANDLE_VALUE) {
			fdata->dwFileAttributes = fd.dwFileAttributes;
			fdata->ftCreationTime = fd.ftCreationTime;
			fdata->ftLastAccessTime = fd.ftLastAccessTime;
			fdata->ftLastWriteTime = fd.ftLastWriteTime;
			fdata->nFileSizeHigh = fd.nFileSizeHigh;
			fdata->nFileSizeLow = fd.nFileSizeLow;
			FindClose(hnd);
			return 0;
		}
	}

	switch (GetLastError()) {
	case ERROR_ACCESS_DENIED:
	case ERROR_SHARING_VIOLATION:
	case ERROR_LOCK_VIOLATION:
	case ERROR_SHARING_BUFFER_EXCEEDED:
		return EACCES;
	case ERROR_BUFFER_OVERFLOW:
		return ENAMETOOLONG;
	case ERROR_NOT_ENOUGH_MEMORY:
		return ENOMEM;
	default:
		return ENOENT;
	}
}

/*
 * Examine a file's contents to determine if it can be executed.  This
 * should be a last resort:  in most cases it's much more efficient to
 * check the file extension.
 *
 * We look for two types of file:  shell scripts and binary executables.
 */
static int has_exec_format(const char *name)
{
	int fd, n, sig;
	unsigned int offset;
	unsigned char buf[1024];

	/* special case: skip DLLs, there are thousands of them! */
	n = strlen(name);
	if (n > 4 && !strcasecmp(name+n-4, ".dll"))
		return 0;

	fd = open(name, O_RDONLY);
	if (fd < 0)
		return 0;
	n = read(fd, buf, sizeof(buf)-1);
	close(fd);
	if (n < 4)	/* at least '#!/x' and not error */
		return 0;

	/* shell script */
	if (buf[0] == '#' && buf[1] == '!') {
		return 1;
	}

	/*
	 * Poke about in file to see if it's a PE binary.  I've just copied
	 * the magic from the file command.
	 */
	if (buf[0] == 'M' && buf[1] == 'Z') {
		offset = (buf[0x19] << 8) + buf[0x18];
		if (offset > 0x3f) {
			offset = (buf[0x3f] << 24) + (buf[0x3e] << 16) +
						(buf[0x3d] << 8) + buf[0x3c];
			if (offset < sizeof(buf)-100) {
				if (memcmp(buf+offset, "PE\0\0", 4) == 0) {
					sig = (buf[offset+25] << 8) + buf[offset+24];
					if (sig == 0x10b || sig == 0x20b) {
						sig = (buf[offset+23] << 8) + buf[offset+22];
						if ((sig & 0x2000) != 0) {
							/* DLL */
							return 0;
						}
						sig = buf[offset+92];
						return (sig == 1 || sig == 2 || sig == 3 || sig == 7);
					}
				}
			}
		}
	}

	return 0;
}

/* We keep the do_lstat code in a separate function to avoid recursion.
 * When a path ends with a slash, the stat will fail with ENOENT. In
 * this case, we strip the trailing slashes and stat again.
 *
 * If follow is true then act like stat() and report on the link
 * target. Otherwise report on the link itself.
 */
static int do_lstat(int follow, const char *file_name, struct mingw_stat *buf)
{
	int err;
	WIN32_FILE_ATTRIBUTE_DATA fdata;

	if (!(err = get_file_attr(file_name, &fdata))) {
		buf->st_ino = 0;
		buf->st_uid = DEFAULT_UID;
		buf->st_gid = DEFAULT_GID;
		buf->st_nlink = 1;
		buf->st_mode = file_attr_to_st_mode(fdata.dwFileAttributes);
		if (S_ISREG(buf->st_mode) &&
				(has_exe_suffix(file_name) || has_exec_format(file_name)))
			buf->st_mode |= S_IXUSR|S_IXGRP|S_IXOTH;
		buf->st_size = fdata.nFileSizeLow |
			(((off64_t)fdata.nFileSizeHigh)<<32);
		buf->st_dev = buf->st_rdev = 0; /* not used by Git */
		buf->st_atime = filetime_to_time_t(&(fdata.ftLastAccessTime));
		buf->st_mtime = filetime_to_time_t(&(fdata.ftLastWriteTime));
		buf->st_ctime = filetime_to_time_t(&(fdata.ftCreationTime));
		if (fdata.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
			WIN32_FIND_DATAA findbuf;
			HANDLE handle = FindFirstFileA(file_name, &findbuf);
			if (handle != INVALID_HANDLE_VALUE) {
				if ((findbuf.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) &&
						(findbuf.dwReserved0 == IO_REPARSE_TAG_SYMLINK)) {
					if (follow) {
						char buffer[MAXIMUM_REPARSE_DATA_BUFFER_SIZE];
						buf->st_size = readlink(file_name, buffer, MAXIMUM_REPARSE_DATA_BUFFER_SIZE);
					} else {
						buf->st_mode = S_IFLNK;
					}
					buf->st_mode |= S_IRUSR|S_IRGRP|S_IROTH;
					if (!(findbuf.dwFileAttributes & FILE_ATTRIBUTE_READONLY))
						buf->st_mode |= S_IWUSR|S_IWGRP;
				}
				FindClose(handle);
			}
		}

		/*
		 * Assume a block is 4096 bytes and calculate number of 512 byte
		 * sectors.
		 */
		buf->st_blksize = 4096;
		buf->st_blocks = ((buf->st_size+4095)>>12)<<3;
		return 0;
	}
	errno = err;
	return -1;
}

/* We provide our own lstat/fstat functions, since the provided
 * lstat/fstat functions are so slow. These stat functions are
 * tailored for Git's usage (read: fast), and are not meant to be
 * complete. Note that Git stat()s are redirected to mingw_lstat()
 * too, since Windows doesn't really handle symlinks that well.
 */
static int do_stat_internal(int follow, const char *file_name, struct mingw_stat *buf)
{
	int namelen;
	char alt_name[PATH_MAX];

	if (!do_lstat(follow, file_name, buf))
		return 0;

	/* if file_name ended in a '/', Windows returned ENOENT;
	 * try again without trailing slashes
	 */
	if (errno != ENOENT)
		return -1;

	namelen = strlen(file_name);
	if (namelen && file_name[namelen-1] != '/')
		return -1;
	while (namelen && file_name[namelen-1] == '/')
		--namelen;
	if (!namelen || namelen >= PATH_MAX)
		return -1;

	memcpy(alt_name, file_name, namelen);
	alt_name[namelen] = 0;
	return do_lstat(follow, alt_name, buf);
}

int mingw_lstat(const char *file_name, struct mingw_stat *buf)
{
	return do_stat_internal(0, file_name, buf);
}
int mingw_stat(const char *file_name, struct mingw_stat *buf)
{
	return do_stat_internal(1, file_name, buf);
}

int mingw_fstat(int fd, struct mingw_stat *buf)
{
	HANDLE fh = (HANDLE)_get_osfhandle(fd);
	BY_HANDLE_FILE_INFORMATION fdata;

	if (fh == INVALID_HANDLE_VALUE) {
		errno = EBADF;
		return -1;
	}
	/* direct non-file handles to MS's fstat() */
	if (GetFileType(fh) != FILE_TYPE_DISK) {
		struct _stati64 buf64;

		if ( _fstati64(fd, &buf64) != 0 )  {
			return -1;
		}
		buf->st_mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH;
		buf->st_size = buf64.st_size;
		buf->st_atime = buf64.st_atime;
		buf->st_mtime = buf64.st_mtime;
		buf->st_ctime = buf64.st_ctime;
		buf->st_blocks = ((buf64.st_size+4095)>>12)<<3;
		goto success;
	}

	if (GetFileInformationByHandle(fh, &fdata)) {
		buf->st_mode = file_attr_to_st_mode(fdata.dwFileAttributes);
		buf->st_size = fdata.nFileSizeLow |
			(((off64_t)fdata.nFileSizeHigh)<<32);
		buf->st_atime = filetime_to_time_t(&(fdata.ftLastAccessTime));
		buf->st_mtime = filetime_to_time_t(&(fdata.ftLastWriteTime));
		buf->st_ctime = filetime_to_time_t(&(fdata.ftCreationTime));
		buf->st_blocks = ((buf->st_size+4095)>>12)<<3;
 success:
		buf->st_dev = buf->st_rdev = 0;
		buf->st_ino = 0;
		buf->st_uid = DEFAULT_UID;
		buf->st_gid = DEFAULT_GID;
		/* could use fdata.nNumberOfLinks but it's inconsistent with stat */
		buf->st_nlink = 1;
		buf->st_blksize = 4096;
		return 0;
	}

	errno = EBADF;
	return -1;
}

static inline void timeval_to_filetime(const struct timeval tv, FILETIME *ft)
{
	long long winTime = ((tv.tv_sec * 1000000LL) + tv.tv_usec) * 10LL + 116444736000000000LL;
	ft->dwLowDateTime = winTime;
	ft->dwHighDateTime = winTime >> 32;
}

int utimes(const char *file_name, const struct timeval tims[2])
{
	FILETIME mft, aft;
	HANDLE fh;
	DWORD flags, attrs;
	int rc;

	flags = FILE_ATTRIBUTE_NORMAL;

	/* must have write permission */
	attrs = GetFileAttributes(file_name);
	if ( attrs != INVALID_FILE_ATTRIBUTES ) {
	    if ( attrs & FILE_ATTRIBUTE_READONLY ) {
			/* ignore errors here; open() will report them */
			SetFileAttributes(file_name, attrs & ~FILE_ATTRIBUTE_READONLY);
		}

	    if ( attrs & FILE_ATTRIBUTE_DIRECTORY ) {
			flags = FILE_FLAG_BACKUP_SEMANTICS;
		}
	}

	fh = CreateFile(file_name, GENERIC_READ|GENERIC_WRITE,
				FILE_SHARE_READ|FILE_SHARE_WRITE,
				NULL, OPEN_EXISTING, flags, NULL);
	if ( fh == INVALID_HANDLE_VALUE ) {
		errno = err_win_to_posix(GetLastError());
		rc = -1;
		goto revert_attrs;
	}

	if (tims) {
		timeval_to_filetime(tims[0], &aft);
		timeval_to_filetime(tims[1], &mft);
	}
	else {
		GetSystemTimeAsFileTime(&mft);
		aft = mft;
	}
	if (!SetFileTime(fh, NULL, &aft, &mft)) {
		errno = EINVAL;
		rc = -1;
	} else
		rc = 0;
	CloseHandle(fh);

revert_attrs:
	if (attrs != INVALID_FILE_ATTRIBUTES &&
	    (attrs & FILE_ATTRIBUTE_READONLY)) {
		/* ignore errors again */
		SetFileAttributes(file_name, attrs);
	}
	return rc;
}

unsigned int sleep (unsigned int seconds)
{
	Sleep(seconds*1000);
	return 0;
}

int nanosleep(const struct timespec *req, struct timespec *rem)
{
	if (req->tv_nsec < 0 || 1000000000 <= req->tv_nsec) {
		errno = EINVAL;
		return -1;
	}

	Sleep(req->tv_sec*1000 + req->tv_nsec/1000000);

	/* Sleep is not interruptible.  So there is no remaining delay.  */
	if (rem != NULL) {
		rem->tv_sec = 0;
		rem->tv_nsec = 0;
	}

	return 0;
}

/*
 * Windows' mktemp returns NULL on error whereas POSIX always returns the
 * template and signals an error by making it an empty string.
 */
#undef mktemp
char *mingw_mktemp(char *template)
{
	if ( mktemp(template) == NULL ) {
		template[0] = '\0';
	}

	return template;
}

int mkstemp(char *template)
{
	char *filename = mktemp(template);
	if (filename == NULL)
		return -1;
	return open(filename, O_RDWR | O_CREAT, 0600);
}

int gettimeofday(struct timeval *tv, void *tz UNUSED_PARAM)
{
	FILETIME ft;
	long long hnsec;

	GetSystemTimeAsFileTime(&ft);
	hnsec = filetime_to_hnsec(&ft);
	tv->tv_sec = hnsec / 10000000;
	tv->tv_usec = (hnsec % 10000000) / 10;
	return 0;
}

int pipe(int filedes[2])
{
	if (_pipe(filedes, PIPE_BUF, 0) < 0)
		return -1;
	return 0;
}

struct tm *gmtime_r(const time_t *timep, struct tm *result)
{
	/* gmtime() in MSVCRT.DLL is thread-safe, but not reentrant */
	memcpy(result, gmtime(timep), sizeof(struct tm));
	return result;
}

struct tm *localtime_r(const time_t *timep, struct tm *result)
{
	/* localtime() in MSVCRT.DLL is thread-safe, but not reentrant */
	memcpy(result, localtime(timep), sizeof(struct tm));
	return result;
}

#undef getcwd
char *mingw_getcwd(char *pointer, int len)
{
	char *ret = getcwd(pointer, len);
	if (!ret)
		return ret;
	convert_slashes(ret);
	return ret;
}

#undef rename
int mingw_rename(const char *pold, const char *pnew)
{
	DWORD attrs;

	/*
	 * Try native rename() first to get errno right.
	 * It is based on MoveFile(), which cannot overwrite existing files.
	 */
	if (!rename(pold, pnew))
		return 0;
	if (errno != EEXIST)
		return -1;
	if (MoveFileEx(pold, pnew, MOVEFILE_REPLACE_EXISTING))
		return 0;
	/* TODO: translate more errors */
	if (GetLastError() == ERROR_ACCESS_DENIED &&
	    (attrs = GetFileAttributes(pnew)) != INVALID_FILE_ATTRIBUTES) {
		if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
			errno = EISDIR;
			return -1;
		}
		if ((attrs & FILE_ATTRIBUTE_READONLY) &&
		    SetFileAttributes(pnew, attrs & ~FILE_ATTRIBUTE_READONLY)) {
			if (MoveFileEx(pold, pnew, MOVEFILE_REPLACE_EXISTING))
				return 0;
			/* revert file attributes on failure */
			SetFileAttributes(pnew, attrs);
		}
	}
	errno = EACCES;
	return -1;
}

static char *gethomedir(void)
{
	static char *buf = NULL;
	DWORD len = PATH_MAX;
	HANDLE h;

	if (!buf)
		buf = xzalloc(PATH_MAX);

	if (buf[0])
		return buf;

	if ( !OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &h) )
		return buf;

	GetUserProfileDirectory(h, buf, &len);
	CloseHandle(h);
	convert_slashes(buf);

	return buf;
}

#define NAME_LEN 100
static char *get_user_name(void)
{
	static char *user_name = NULL;
	char *s;
	DWORD len = NAME_LEN;

	if ( user_name == NULL ) {
		user_name = xzalloc(NAME_LEN);
	}

	if ( user_name[0] != '\0' ) {
		return user_name;
	}

	if ( !GetUserName(user_name, &len) ) {
		return NULL;
	}

	for ( s=user_name; *s; ++s ) {
		if ( *s == ' ' ) {
			*s = '_';
		}
	}

	return user_name;
}

struct passwd *getpwnam(const char *name)
{
	const char *myname;

	if ( (myname=get_user_name()) != NULL &&
			strcmp(myname, name) == 0 ) {
		return getpwuid(DEFAULT_UID);
	}

	return NULL;
}

struct passwd *getpwuid(uid_t uid UNUSED_PARAM)
{
	static struct passwd p;

	if ( (p.pw_name=get_user_name()) == NULL ) {
		return NULL;
	}
	p.pw_passwd = (char *)"secret";
	p.pw_gecos = (char *)"unknown";
	p.pw_dir = gethomedir();
	p.pw_shell = NULL;
	p.pw_uid = DEFAULT_UID;
	p.pw_gid = DEFAULT_GID;

	return &p;
}

struct group *getgrgid(gid_t gid UNUSED_PARAM)
{
	static char *members[2] = { NULL, NULL };
	static struct group g;

	if ( (g.gr_name=get_user_name()) == NULL ) {
		return NULL;
	}
	g.gr_passwd = (char *)"secret";
	g.gr_gid = DEFAULT_GID;
	members[0] = g.gr_name;
	g.gr_mem = members;

	return &g;
}

int getgrouplist(const char *user UNUSED_PARAM, gid_t group UNUSED_PARAM,
					gid_t *groups, int *ngroups)
{
	if ( *ngroups == 0 ) {
		*ngroups = 1;
		return -1;
	}

	*ngroups = 1;
	groups[0] = DEFAULT_GID;
	return 1;
}

int getgroups(int n, gid_t *groups)
{
	if ( n == 0 ) {
		return 1;
	}

	groups[0] = DEFAULT_GID;
	return 1;
}

int getlogin_r(char *buf, size_t len)
{
	char *name;

	if ( (name=get_user_name()) == NULL ) {
		return -1;
	}

	if ( strlen(name) >= len ) {
		errno = ERANGE;
		return -1;
	}

	strcpy(buf, name);
	return 0;
}

long sysconf(int name)
{
	if ( name == _SC_CLK_TCK ) {
		return TICKS_PER_SECOND;
	}
	errno = EINVAL;
	return -1;
}

clock_t times(struct tms *buf)
{
	buf->tms_utime = 0;
	buf->tms_stime = 0;
	buf->tms_cutime = 0;
	buf->tms_cstime = 0;

	return 0;
}

int link(const char *oldpath, const char *newpath)
{
	typedef BOOL (WINAPI *T)(const char*, const char*, LPSECURITY_ATTRIBUTES);
	static T create_hard_link = NULL;
	if (!create_hard_link) {
		create_hard_link = (T) GetProcAddress(
			GetModuleHandle("kernel32.dll"), "CreateHardLinkA");
		if (!create_hard_link)
			create_hard_link = (T)-1;
	}
	if (create_hard_link == (T)-1) {
		errno = ENOSYS;
		return -1;
	}
	if (!create_hard_link(newpath, oldpath, NULL)) {
		errno = err_win_to_posix(GetLastError());
		return -1;
	}
	return 0;
}

char *realpath(const char *path, char *resolved_path)
{
	if (_fullpath(resolved_path, path, PATH_MAX)) {
		convert_slashes(resolved_path);
		return resolved_path;
	}
	return NULL;
}

const char *get_busybox_exec_path(void)
{
	static char *path = NULL;

	if (!path) {
		path = xzalloc(PATH_MAX);
	}

	if (!*path) {
		GetModuleFileName(NULL, path, PATH_MAX);
		convert_slashes(path);
	}
	return path;
}

#undef mkdir
int mingw_mkdir(const char *path, int mode UNUSED_PARAM)
{
	int ret;
	struct stat st;
	int lerrno = 0;

	if ( (ret=mkdir(path)) < 0 ) {
		lerrno = errno;
		if ( lerrno == EACCES && stat(path, &st) == 0 ) {
			ret = 0;
			lerrno = 0;
		}
	}

	errno = lerrno;
	return ret;
}

#undef chmod
int mingw_chmod(const char *path, int mode)
{
	WIN32_FILE_ATTRIBUTE_DATA fdata;

	if ( get_file_attr(path, &fdata) == 0 &&
			fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) {
		mode |= 0222;
	}

	return chmod(path, mode);
}

int fcntl(int fd, int cmd, ...)
{
	va_list arg;
	int result = -1;
	char *fds;
	int target, i, newfd;

	va_start(arg, cmd);

	switch (cmd) {
	case F_GETFD:
	case F_SETFD:
	case F_GETFL:
		/*
		 * Our fake F_GETFL won't matter if the return value is used as
		 *    fcntl(fd, F_SETFL, ret|something);
		 * because F_SETFL isn't supported either.
		 */
		result = 0;
		break;
	case F_DUPFD:
		target = va_arg(arg, int);
		fds = xzalloc(target);
		while ((newfd = dup(fd)) < target && newfd >= 0) {
			fds[newfd] = 1;
		}
		for (i = 0; i < target; ++i) {
			if (fds[i]) {
				close(i);
			}
		}
		free(fds);
		result = newfd;
		break;
	default:
		errno = ENOSYS;
		break;
	}

	va_end(arg);
	return result;
}

#undef unlink
int mingw_unlink(const char *pathname)
{
	/* read-only files cannot be removed */
	chmod(pathname, 0666);
	return unlink(pathname);
}

#undef strftime
size_t mingw_strftime(char *buf, size_t max, const char *format, const struct tm *tm)
{
	size_t ret;
	char day[3];
	char *t;
	char *fmt, *newfmt;
	struct tm tm2;
	int m;

	/*
	 * Emulate the some formats that Windows' strftime lacks.
	 * - '%e' day of the month with space padding
	 * - '%s' number of seconds since the Unix epoch
	 * - '%z' timezone offset
	 * Also, permit the '-' modifier to omit padding.  Windows uses '#'.
	 */
	fmt = xstrdup(format);
	for ( t=fmt; *t; ++t ) {
		if ( *t == '%' ) {
			if ( t[1] == 'e' ) {
				if ( tm->tm_mday >= 0 && tm->tm_mday <= 99 ) {
					sprintf(day, "%2d", tm->tm_mday);
				}
				else {
					strcpy(day, "  ");
				}
				memcpy(t++, day, 2);
			}
			else if ( t[1] == 's' ) {
				*t = '\0';
				m = t - fmt;
				tm2 = *tm;
				newfmt = xasprintf("%s%d%s", fmt, (int)mktime(&tm2), t+2);
				free(fmt);
				t = newfmt + m + 1;
				fmt = newfmt;
			}
			else if ( t[1] == 'z' ) {
				char buffer[16] = "";

				*t = '\0';
				m = t - fmt;
				_tzset();
				if ( tm->tm_isdst >= 0 ) {
					int offset = (int)_timezone - (tm->tm_isdst > 0 ? 3600 : 0);
					int hr, min;

					if ( offset > 0 ) {
						buffer[0] = '-';
					}
					else {
						buffer[0] = '+';
						offset = -offset;
					}

					hr = offset / 3600;
					min = (offset % 3600) / 60;
					sprintf(buffer+1, "%04d", hr*100 + min);
				}
				newfmt = xasprintf("%s%s%s", fmt, buffer, t+2);
				free(fmt);
				t = newfmt + m + 1;
				fmt = newfmt;
			}
			else if ( t[1] == '-' && t[2] != '\0' &&
						strchr("dHIjmMSUwWyY", t[2]) ) {
				/* Microsoft uses '#' rather than '-' to remove padding */
				t[1] = '#';
			}
			else if ( t[1] != '\0' ) {
				++t;
			}
		}
	}

	ret = strftime(buf, max, fmt, tm);
	free(fmt);

	return ret;
}

int stime(time_t *t UNUSED_PARAM)
{
	errno = EPERM;
	return -1;
}

#undef access
int mingw_access(const char *name, int mode)
{
	int ret;
	struct stat s;

	/* Windows can only handle test for existence, read or write */
	if (mode == F_OK || (mode & ~X_OK)) {
		ret = _access(name, mode & ~X_OK);
		if (ret < 0 || !(mode & X_OK)) {
			return ret;
		}
	}

	if (!mingw_stat(name, &s) && S_ISREG(s.st_mode) && (s.st_mode&S_IXUSR)) {
		return 0;
	}

	return -1;
}

#undef rmdir
int mingw_rmdir(const char *path)
{
	/* read-only directories cannot be removed */
	chmod(path, 0666);
	return rmdir(path);
}

const char win_suffix[4][4] = { "com", "exe", "bat", "cmd" };

static int has_win_suffix(const char *name, int start)
{
	int i, len = strlen(name);

	if (len > 4 && name[len-4] == '.') {
		for (i=start; i<4; ++i) {
			if (!strcasecmp(name+len-3, win_suffix[i])) {
				return 1;
			}
		}
	}
	return 0;
}

int has_bat_suffix(const char *name)
{
	return has_win_suffix(name, 2);
}

int has_exe_suffix(const char *name)
{
	return has_win_suffix(name, 0);
}

int has_exe_suffix_or_dot(const char *name)
{
	return last_char_is(name, '.') || has_win_suffix(name, 0);
}

/* check if path can be made into an executable by adding a suffix;
 * return an allocated string containing the path if it can;
 * return NULL if not.
 *
 * if path already has a suffix don't even bother trying
 */
char *add_win32_extension(const char *p)
{
	char *path;
	int i, len;

	if (has_exe_suffix_or_dot(p)) {
		return NULL;
	}

	len = strlen(p);
	path = xasprintf("%s.com", p);

	for (i=0; i<4; ++i) {
		memcpy(path+len+1, win_suffix[i], 4);
		if (file_is_executable(path)) {
			return path;
		}
	}

	free(path);

	return NULL;
}

void FAST_FUNC convert_slashes(char *p)
{
	for (; *p; ++p) {
		if ( *p == '\\' ) {
			*p = '/';
		}
	}
}

#undef opendir
DIR *mingw_opendir(const char *path)
{
	char name[4];

	if (isalpha(path[0]) && path[1] == ':' && path[2] == '\0') {
		strcpy(name, path);
		name[2] = '/';
		name[3] = '\0';
		path = name;
	}

	return opendir(path);
}

off_t mingw_lseek(int fd, off_t offset, int whence)
{
	HANDLE h = (HANDLE)_get_osfhandle(fd);
	if (h == INVALID_HANDLE_VALUE) {
		errno = EBADF;
		return -1;
	}
	if (GetFileType(h) != FILE_TYPE_DISK) {
		errno = ESPIPE;
		return -1;
	}
	return _lseeki64(fd, offset, whence);
}

#if ENABLE_FEATURE_PS_TIME || ENABLE_FEATURE_PS_LONG
#undef GetTickCount64
#include "lazyload.h"

ULONGLONG CompatGetTickCount64(void)
{
	DECLARE_PROC_ADDR(kernel32.dll, ULONGLONG, GetTickCount64, void);

	if (!INIT_PROC_ADDR(GetTickCount64)) {
		return (ULONGLONG)GetTickCount();
	}

	return GetTickCount64();
}
#endif
