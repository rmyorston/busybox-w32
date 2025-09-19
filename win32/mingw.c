#include "libbb.h"
#include <userenv.h>
#include "lazyload.h"
#if ENABLE_FEATURE_EXTRA_FILE_DATA
#include <aclapi.h>
#endif
#include <ntdef.h>
#include <psapi.h>

#if defined(__MINGW64_VERSION_MAJOR)
#if ENABLE_GLOBBING
extern int _setargv(void);
int _setargv(void)
{
	extern int _dowildcard;
	char *glob;

	_dowildcard = -1;
	glob = getenv("BB_GLOBBING");
	if (glob) {
		if (strcmp(glob, "0") == 0)
			_dowildcard = 0;
	}
	else {
		setenv("BB_GLOBBING", "0", TRUE);
	}
	return 0;
}
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
static mode_t current_umask = DEFAULT_UMASK;

#pragma GCC optimize ("no-if-conversion")
int err_win_to_posix(void)
{
	int error = ENOSYS;
	switch(GetLastError()) {
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
	case ERROR_BAD_NET_NAME: error = ENOENT; break;
	case ERROR_BAD_NETPATH: error = ENOENT; break;
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
	case ERROR_CANT_RESOLVE_FILENAME: error = ELOOP; break;
	}
	return error;
}
#pragma GCC reset_options

#undef strerror
char *mingw_strerror(int errnum)
{
	if (errnum == ELOOP)
		return (char *)"Too many levels of symbolic links";
	return strerror(errnum);
}

char *strsignal(int sig)
{
	if (sig == SIGTERM)
		return (char *)"Terminated";
	else if (sig == SIGKILL)
		return (char *)"Killed";
	return (char *)get_signame(sig);
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
	if (filename && is_prefixed_with(filename, "/dev/"))
		return index_in_strings("null\0zero\0urandom\0", filename+5);

	return NOT_DEVICE;
}

void update_special_fd(int dev, int fd)
{
	if (dev == DEV_ZERO)
		zero_fd = fd;
	else if (dev == DEV_URANDOM)
		rand_fd = fd;
}

#define PREFIX_LEN (sizeof(DEV_FD_PREFIX)-1)
static int get_dev_fd(const char *filename)
{
	int fd;

	if (filename && is_prefixed_with(filename, DEV_FD_PREFIX)) {
		fd = bb_strtou(filename+PREFIX_LEN, NULL, 10);
		if (errno == 0 && (HANDLE)_get_osfhandle(fd) != INVALID_HANDLE_VALUE)
			return fd;
	}
	return -1;
}

static int mingw_is_directory(const char *path);
#undef open
int mingw_open (const char *filename, int oflags, ...)
{
	va_list args;
	int pmode, mode = 0666;
	int fd;
	int special = (oflags & O_SPECIAL);
	int dev = get_dev_type(filename);

	/* /dev/null is always allowed, others only if O_SPECIAL is set */
	if (dev == DEV_NULL || (special && dev != NOT_DEVICE)) {
		filename = "nul";
		oflags = O_RDWR;
	}
	else if ((fd=get_dev_fd(filename)) >= 0) {
		return fd;
	}

	if ((oflags & O_CREAT)) {
		va_start(args, oflags);
		mode = va_arg(args, int);
		va_end(args);
	}

	pmode = ((mode & S_IWUSR) ? _S_IWRITE : 0) |
					((mode & S_IRUSR) ? _S_IREAD : 0);

	fd = open(filename, oflags&~O_SPECIAL, pmode);
	if (fd >= 0) {
		update_special_fd(dev, fd);
	}
	else if ((oflags & O_ACCMODE) != O_RDONLY && errno == EACCES) {
		if (mingw_is_directory(filename))
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

ssize_t FAST_FUNC mingw_open_read_close(const char *fn, void *buf, size_t size)
{
	/* allow use of special devices */
	int fd = mingw_open(fn, O_RDONLY|O_SPECIAL);
	if (fd < 0)
		return fd;
	return read_close(fd, buf, size);
}

#undef fopen
FILE *mingw_fopen (const char *filename, const char *otype)
{
	int fd;
	FILE *stream;

	if (get_dev_type(filename) == DEV_NULL)
		filename = "nul";
	else if ((fd=get_dev_fd(filename)) >= 0)
		return fdopen(fd, otype);
	stream = fopen(filename, otype);
	if (stream == NULL && errno == EACCES && strcmp(otype, "r") == 0 &&
			mingw_is_directory(filename))
		errno = EISDIR;
	return stream;
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

static inline struct timespec filetime_to_timespec(const FILETIME *ft)
{
	struct timespec ts;
	long long winTime = filetime_to_hnsec(ft);

	ts.tv_sec = (time_t)(winTime / 10000000);
	ts.tv_nsec = (long)(winTime % 10000000) * 100;

	return ts;
}

static inline mode_t file_attr_to_st_mode(DWORD attr)
{
	mode_t fMode = S_IRUSR|S_IRGRP|S_IROTH;
	if (attr & FILE_ATTRIBUTE_DIRECTORY)
		fMode |= (S_IFDIR|S_IRWXU|S_IRWXG|S_IRWXO) & ~(current_umask & 0022);
	else if (attr & FILE_ATTRIBUTE_DEVICE)
		fMode |= S_IFCHR|S_IWUSR|S_IWGRP|S_IWOTH;
	else
		fMode |= S_IFREG;
	if (!(attr & (FILE_ATTRIBUTE_READONLY|FILE_ATTRIBUTE_DEVICE)))
		fMode |= (S_IWUSR|S_IWGRP|S_IWOTH) & ~(current_umask & 0022);
	return fMode;
}

static int get_file_attr(const char *fname, WIN32_FILE_ATTRIBUTE_DATA *fdata)
{
	char *want_dir;

	if (get_dev_type(fname) == DEV_NULL || get_dev_fd(fname) >= 0) {
		/* Fake attributes for special devices */
		/* Though not /dev/zero or /dev/urandom */
		FILETIME epoch = {0xd53e8000, 0x019db1de};	// Unix epoch as FILETIME
		fdata->dwFileAttributes = FILE_ATTRIBUTE_DEVICE;
		fdata->ftCreationTime = fdata->ftLastAccessTime =
				fdata->ftLastWriteTime = epoch;
		fdata->nFileSizeHigh = fdata->nFileSizeLow = 0;
		return 0;
	}

	want_dir = last_char_is_dir_sep(fname);
	if (GetFileAttributesExA(fname, GetFileExInfoStandard, fdata)) {
		if (!(fdata->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && want_dir)
			return ENOTDIR;
		fdata->dwFileAttributes &= ~FILE_ATTRIBUTE_DEVICE;
		return 0;
	}

	if (GetLastError() == ERROR_SHARING_VIOLATION) {
		HANDLE hnd;
		WIN32_FIND_DATA fd;

		if ((hnd=FindFirstFile(fname, &fd)) != INVALID_HANDLE_VALUE) {
			fdata->dwFileAttributes =
					fd.dwFileAttributes & ~FILE_ATTRIBUTE_DEVICE;
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
	case ERROR_INVALID_NAME:
		if (want_dir)
			return ENOTDIR;
	default:
		return ENOENT;
	}
}

#undef umask
mode_t mingw_umask(mode_t new_mode)
{
	mode_t tmp_mode;

	tmp_mode = current_umask;
	current_umask = new_mode & 0777;

	umask((new_mode & S_IWUSR) ? _S_IWRITE : 0);

	return tmp_mode;
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
	HANDLE fh;
	int fd = -1;
	ssize_t n;
	int sig;
	unsigned int offset;
	unsigned char buf[1024];

	/* special case: skip DLLs, there are thousands of them! */
	if (is_suffixed_with_case(name, ".dll"))
		return 0;

	/* Open file and try to avoid updating access time */
	fh = CreateFileA(name, GENERIC_READ | FILE_WRITE_ATTRIBUTES,
						FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (fh != INVALID_HANDLE_VALUE) {
		FILETIME last_access = { 0xffffffff, 0xffffffff };

		SetFileTime(fh, NULL, &last_access, NULL);
		fd = _open_osfhandle((intptr_t)fh, O_RDONLY);
	}

	if (fd < 0)
		n = open_read_close(name, buf, sizeof(buf));
	else
		n = read_close(fd, buf, sizeof(buf));

	if (n < 4) /* Need at least a few bytes and no error */
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
/* Convert four unsigned bytes to an unsigned int (little-endian) */
#define LE4(b, o) (((unsigned)b[o+3] << 24) + (b[o+2] << 16) + \
						(b[o+1] << 8) + b[o])

		/* Actually Portable Executable */
		/* See ape/ape.S at https://github.com/jart/cosmopolitan */
		const unsigned char *qFpD = (unsigned char *)"qFpD";
		if (n > 6 && LE4(buf, 2) == LE4(qFpD, 0))
			return 1;

		if (n > 0x3f) {
			offset = (buf[0x19] << 8) + buf[0x18];
			if (offset > 0x3f) {
				offset = LE4(buf, 0x3c);
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
							return (sig == 1 || sig == 2 || sig == 3
										|| sig == 7);
						}
					}
				}
			}
		}
	}

	return 0;
}

#if ENABLE_FEATURE_EXTRA_FILE_DATA
static uid_t file_owner(HANDLE fh, struct mingw_stat *buf)
{
	PSID pSidOwner;
	PACL pDACL;
	PSECURITY_DESCRIPTOR pSD;
	static PTOKEN_USER user = NULL;
	static HANDLE impersonate = INVALID_HANDLE_VALUE;
	static int initialised = 0;
	uid_t uid = 0;
	DWORD *ptr;
	unsigned char prefix[] = {
		0x01, 0x05, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x05,
		0x15, 0x00, 0x00, 0x00
	};
	unsigned char nullsid[] = {
		0x01, 0x01, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x01,
		0x00, 0x00, 0x00, 0x00
	};

	/*  get SID of current user */
	if (!initialised) {
		HANDLE token;
		DWORD ret = 0;

		initialised = 1;
		if (OpenProcessToken(GetCurrentProcess(),
				TOKEN_IMPERSONATE | TOKEN_QUERY | TOKEN_DUPLICATE |
					STANDARD_RIGHTS_READ, &token)) {
			GetTokenInformation(token, TokenUser, NULL, 0, &ret);
			if (ret <= 0 || (user=malloc(ret)) == NULL ||
					!GetTokenInformation(token, TokenUser, user, ret, &ret)) {
				free(user);
				user = NULL;
			}
			DuplicateToken(token, SecurityImpersonation, &impersonate);
			CloseHandle(token);
		}
	}

	if (user == NULL)
		return DEFAULT_UID;

	/* get SID of file's owner */
	if (GetSecurityInfo(fh, SE_FILE_OBJECT,
			OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION |
				DACL_SECURITY_INFORMATION,
			&pSidOwner, NULL, &pDACL, NULL, &pSD) != ERROR_SUCCESS)
		return 0;

	if (EqualSid(pSidOwner, user->User.Sid)) {
		uid = DEFAULT_UID;
	} else if (memcmp(pSidOwner, nullsid, sizeof(nullsid)) == 0) {
		uid = DEFAULT_UID;
	} else if (memcmp(pSidOwner, prefix, sizeof(prefix)) == 0) {
		/* for local or domain users use the RID as uid */
		ptr = (DWORD *)pSidOwner;
		if (ptr[6] >= 500 && ptr[6] < DEFAULT_UID)
			uid = (uid_t)ptr[6];
	}

	if (uid != DEFAULT_UID && impersonate != INVALID_HANDLE_VALUE &&
				getuid() != 0) {
		static GENERIC_MAPPING mapping = {
			FILE_GENERIC_READ, FILE_GENERIC_WRITE,
			FILE_GENERIC_EXECUTE, FILE_ALL_ACCESS
		};
		PRIVILEGE_SET privileges;
		DWORD grantedAccess;
		DWORD privilegesLength = sizeof(privileges);
		DWORD genericAccessRights = MAXIMUM_ALLOWED;
		BOOL result;

		if (AccessCheck(pSD, impersonate, genericAccessRights,
				&mapping, &privileges, &privilegesLength,
				&grantedAccess, &result)) {
			if (result && (grantedAccess & 0x1200af) == 0x1200af) {
				buf->st_mode |= (buf->st_mode & S_IRWXU) >> 6;
			}
		}
	}
	LocalFree(pSD);
	return uid;
}
#endif

static DWORD get_symlink_data(DWORD attr, const char *pathname,
					WIN32_FIND_DATAA *fbuf)
{
	if (attr & FILE_ATTRIBUTE_REPARSE_POINT) {
		HANDLE handle = FindFirstFileA(pathname, fbuf);
		if (handle != INVALID_HANDLE_VALUE) {
			FindClose(handle);
			if ((fbuf->dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
				switch (fbuf->dwReserved0) {
				case IO_REPARSE_TAG_SYMLINK:
				case IO_REPARSE_TAG_MOUNT_POINT:
				case IO_REPARSE_TAG_APPEXECLINK:
					return fbuf->dwReserved0;
				}
			}
		}
	}
	return 0;
}

static DWORD is_symlink(const char *pathname)
{
	WIN32_FILE_ATTRIBUTE_DATA fdata;
	WIN32_FIND_DATAA fbuf;

	if (!get_file_attr(pathname, &fdata))
		return get_symlink_data(fdata.dwFileAttributes, pathname, &fbuf);
	return 0;
}

static int mingw_is_directory(const char *path)
{
	WIN32_FILE_ATTRIBUTE_DATA fdata;

	return get_file_attr(path, &fdata) == 0 &&
				(fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
}

#if ENABLE_FEATURE_EXTRA_FILE_DATA
/*
 * By default we don't count subdirectories.  Counting can be enabled
 * in specific cases by calling 'count_subdirs(NULL)' before making
 * any calls to stat(2) or lstat(2) that require accurate values of
 * st_nlink for directories.
 */
int count_subdirs(const char *pathname)
{
	int count = 0;
	DIR *dirp;
	struct dirent *dp;
	static int do_count = FALSE;

	if (pathname == NULL) {
		do_count = TRUE;
		return 0;
	}

	if (do_count && (dirp = opendir(pathname))) {
		while ((dp = readdir(dirp)) != NULL) {
			if (dp->d_type == DT_DIR)
				count++;
		}
		closedir(dirp);
	} else {
		count = 2;
	}
	return count;
}
#endif

#ifndef FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS
# define FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS 0x00400000
#endif

/* If follow is true then act like stat() and report on the link
 * target. Otherwise report on the link itself.
 */
static int do_lstat(int follow, const char *file_name, struct mingw_stat *buf)
{
	int err;
	WIN32_FILE_ATTRIBUTE_DATA fdata;
	WIN32_FIND_DATAA findbuf;
	DWORD low, high;
	off64_t size;
	char *lname = NULL;

	while (!(err=get_file_attr(file_name, &fdata))) {
		buf->st_ino = 0;
		buf->st_uid = DEFAULT_UID;
		buf->st_gid = DEFAULT_GID;
		buf->st_dev = buf->st_rdev = 0;
		buf->st_attr = fdata.dwFileAttributes;
		buf->st_tag = get_symlink_data(buf->st_attr, file_name, &findbuf);

		if (buf->st_tag) {
			char *content;

			if (follow) {
				/* The file size and times are wrong when Windows follows
				 * a symlink.  Use the symlink target instead. */
				file_name = lname = xmalloc_follow_symlinks(file_name);
				if (!lname)
					return -1;
				continue;
			}

			/* Get the contents of a symlink, not its target. */
			buf->st_mode = S_IFLNK|S_IRWXU|S_IRWXG|S_IRWXO;
			content = xmalloc_readlink(file_name);
			buf->st_size = content ? strlen(content) : 0;
			free(content);
			buf->st_atim = filetime_to_timespec(&(findbuf.ftLastAccessTime));
			buf->st_mtim = filetime_to_timespec(&(findbuf.ftLastWriteTime));
			buf->st_ctim = filetime_to_timespec(&(findbuf.ftCreationTime));
		}
		else {
			/* The file is not a symlink. */
			buf->st_mode = file_attr_to_st_mode(fdata.dwFileAttributes);
			if (S_ISREG(buf->st_mode) &&
					(has_exe_suffix(file_name) ||
					(!(buf->st_attr & FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS) &&
						has_exec_format(file_name))))
				buf->st_mode |= S_IXUSR|S_IXGRP|S_IXOTH;
			buf->st_size = fdata.nFileSizeLow |
				(((off64_t)fdata.nFileSizeHigh)<<32);
			buf->st_atim = filetime_to_timespec(&(fdata.ftLastAccessTime));
			buf->st_mtim = filetime_to_timespec(&(fdata.ftLastWriteTime));
			buf->st_ctim = filetime_to_timespec(&(fdata.ftCreationTime));
		}
		buf->st_nlink = (buf->st_attr & FILE_ATTRIBUTE_DIRECTORY) ? 2 : 1;

#if ENABLE_FEATURE_EXTRA_FILE_DATA
		if (!(buf->st_attr &
			(FILE_ATTRIBUTE_DEVICE | FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS))) {
			DWORD flags;
			HANDLE fh;
			BY_HANDLE_FILE_INFORMATION hdata;

			flags = FILE_FLAG_BACKUP_SEMANTICS;
			if (S_ISLNK(buf->st_mode))
				flags |= FILE_FLAG_OPEN_REPARSE_POINT;
			fh = CreateFile(file_name, READ_CONTROL, 0, NULL,
								OPEN_EXISTING, flags, NULL);
			if (fh != INVALID_HANDLE_VALUE) {
				if (GetFileInformationByHandle(fh, &hdata)) {
					buf->st_dev = hdata.dwVolumeSerialNumber;
					buf->st_ino = hdata.nFileIndexLow |
							(((ino_t)hdata.nFileIndexHigh)<<32);
					buf->st_nlink = (buf->st_attr & FILE_ATTRIBUTE_DIRECTORY) ?
								count_subdirs(file_name) :
								hdata.nNumberOfLinks;
				}
				buf->st_uid = buf->st_gid = file_owner(fh, buf);
				CloseHandle(fh);
			} else {
				buf->st_uid = buf->st_gid = 0;
				buf->st_mode &= ~S_IRWXO;
			}
		}
#endif

		/* Get actual size of compressed/sparse files.  Only regular
		 * files need to be considered. */
		size = buf->st_size;
		if (S_ISREG(buf->st_mode)) {
			low = GetCompressedFileSize(file_name, &high);
			if (low != INVALID_FILE_SIZE || GetLastError() == NO_ERROR) {
				size = low | (((off64_t)high)<<32);
			}
		}

		/*
		 * Assume a block is 4096 bytes and calculate number of 512 byte
		 * sectors.
		 */
		buf->st_blksize = 4096;
		buf->st_blocks = ((size+4095)>>12)<<3;
		return 0;
	}
	free(lname);
	errno = err;
	return -1;
}

int mingw_lstat(const char *file_name, struct mingw_stat *buf)
{
	return do_lstat(0, file_name, buf);
}

int mingw_stat(const char *file_name, struct mingw_stat *buf)
{
	return do_lstat(1, file_name, buf);
}

#undef st_atime
#undef st_mtime
#undef st_ctime
int mingw_fstat(int fd, struct mingw_stat *buf)
{
	HANDLE fh = (HANDLE)_get_osfhandle(fd);
	BY_HANDLE_FILE_INFORMATION fdata;

	if (fh == INVALID_HANDLE_VALUE)
		goto fail;

	/* direct non-file handles to MS's fstat() */
	if (GetFileType(fh) != FILE_TYPE_DISK) {
		struct _stati64 buf64;

		if (_fstati64(fd, &buf64) != 0)
			return -1;

		buf->st_mode = (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)
							& ~(current_umask & 0022);
		buf->st_attr = FILE_ATTRIBUTE_NORMAL;
		buf->st_size = buf64.st_size;
		buf->st_atim.tv_sec = buf64.st_atime;
		buf->st_atim.tv_nsec = 0;
		buf->st_mtim.tv_sec = buf64.st_mtime;
		buf->st_mtim.tv_nsec = 0;
		buf->st_ctim.tv_sec = buf64.st_ctime;
		buf->st_ctim.tv_nsec = 0;
#if ENABLE_FEATURE_EXTRA_FILE_DATA
		buf->st_dev = 0;
		buf->st_ino = 0;
		buf->st_nlink = 1;
#endif
		goto success;
	}

	if (GetFileInformationByHandle(fh, &fdata)) {
		buf->st_mode = file_attr_to_st_mode(fdata.dwFileAttributes);
		buf->st_attr = fdata.dwFileAttributes;
		buf->st_size = fdata.nFileSizeLow |
			(((off64_t)fdata.nFileSizeHigh)<<32);
		buf->st_atim = filetime_to_timespec(&(fdata.ftLastAccessTime));
		buf->st_mtim = filetime_to_timespec(&(fdata.ftLastWriteTime));
		buf->st_ctim = filetime_to_timespec(&(fdata.ftCreationTime));
#if ENABLE_FEATURE_EXTRA_FILE_DATA
		buf->st_dev = fdata.dwVolumeSerialNumber;
		buf->st_ino = fdata.nFileIndexLow |
			(((uint64_t)fdata.nFileIndexHigh)<<32);
		buf->st_nlink = (buf->st_attr & FILE_ATTRIBUTE_DIRECTORY) ?
			2 : fdata.nNumberOfLinks;
#endif
 success:
#if !ENABLE_FEATURE_EXTRA_FILE_DATA
		buf->st_dev = 0;
		buf->st_ino = 0;
		buf->st_nlink = (buf->st_attr & FILE_ATTRIBUTE_DIRECTORY) ? 2 : 1;
#endif
		buf->st_tag = 0;
		buf->st_rdev = 0;
		buf->st_uid = DEFAULT_UID;
		buf->st_gid = DEFAULT_GID;
		buf->st_blksize = 4096;
		buf->st_blocks = ((buf->st_size+4095)>>12)<<3;
		return 0;
	}
 fail:
	errno = EBADF;
	return -1;
}

static inline void timespec_to_filetime(const struct timespec tv, FILETIME *ft)
{
	long long winTime = tv.tv_sec * 10000000LL + tv.tv_nsec / 100LL +
							116444736000000000LL;
	ft->dwLowDateTime = winTime;
	ft->dwHighDateTime = winTime >> 32;
}

static int hutimens(HANDLE fh, const struct timespec times[2])
{
	FILETIME now, aft, mft;
	FILETIME *pft[2] = {&aft, &mft};
	int i;

	GetSystemTimeAsFileTime(&now);

	if (times) {
		for (i = 0; i < 2; ++i) {
			if (times[i].tv_nsec == UTIME_NOW)
				*pft[i] = now;
			else if (times[i].tv_nsec == UTIME_OMIT)
				pft[i] = NULL;
			else if (times[i].tv_nsec >= 0 && times[i].tv_nsec < 1000000000L)
				timespec_to_filetime(times[i], pft[i]);
			else {
				errno = EINVAL;
				return -1;
			}
		}
	} else {
		aft = mft = now;
	}

	if (!SetFileTime(fh, NULL, pft[0], pft[1])) {
		errno = err_win_to_posix();
		return -1;
	}
	return 0;
}

int futimens(int fd, const struct timespec times[2])
{
	HANDLE fh;

	fh = (HANDLE)_get_osfhandle(fd);
	if (fh == INVALID_HANDLE_VALUE) {
		errno = EBADF;
		return -1;
	}

	return hutimens(fh, times);
}

int utimensat(int fd, const char *path, const struct timespec times[2],
                int flags)
{
	int rc = -1;
	HANDLE fh;
	DWORD cflag = FILE_FLAG_BACKUP_SEMANTICS;

	if (is_relative_path(path) && fd != AT_FDCWD) {
		errno = ENOSYS;	// partial implementation
		return rc;
	}

	if (flags & AT_SYMLINK_NOFOLLOW)
		cflag |= FILE_FLAG_OPEN_REPARSE_POINT;

	fh = CreateFile(path, FILE_WRITE_ATTRIBUTES, 0, NULL, OPEN_EXISTING,
					cflag, NULL);
	if (fh == INVALID_HANDLE_VALUE) {
		errno = err_win_to_posix();
		return rc;
	}

	rc = hutimens(fh, times);
	CloseHandle(fh);
	return rc;
}

int utimes(const char *file_name, const struct timeval tv[2])
{
	struct timespec ts[2];

	if (tv) {
		if (tv[0].tv_usec < 0 || tv[0].tv_usec >= 1000000 ||
				tv[1].tv_usec < 0 || tv[1].tv_usec >= 1000000) {
			errno = EINVAL;
			return -1;
		}
		ts[0].tv_sec = tv[0].tv_sec;
		ts[0].tv_nsec = tv[0].tv_usec * 1000;
		ts[1].tv_sec = tv[1].tv_sec;
		ts[1].tv_nsec = tv[1].tv_usec * 1000;
	}
	return utimensat(AT_FDCWD, file_name, tv ? ts : NULL, 0);
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

int clock_gettime(clockid_t clockid, struct timespec *tp)
{
	FILETIME ft;

	if (clockid != CLOCK_REALTIME) {
		errno = ENOSYS;
		return -1;
	}
	GetSystemTimeAsFileTime(&ft);
	*tp = filetime_to_timespec(&ft);
	return 0;
}

int clock_settime(clockid_t clockid, const struct timespec *tp)
{
	SYSTEMTIME st;
	FILETIME ft;

	if (clockid != CLOCK_REALTIME) {
		errno = ENOSYS;
		return -1;
	}

	timespec_to_filetime(*tp, &ft);
	if (FileTimeToSystemTime(&ft, &st) == 0) {
		errno = EINVAL;
		return -1;
	}

	if (SetSystemTime(&st) == 0) {
		errno = EPERM;
		return -1;
	}
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
	return bs_to_slash(ret);
}

#undef rename
int mingw_rename(const char *pold, const char *pnew)
{
	DWORD attrs;

	/*
	 * For non-symlinks, try native rename() first to get errno right.
	 * It is based on MoveFile(), which cannot overwrite existing files.
	 */
	if (!is_symlink(pold)) {
		if (!rename(pold, pnew))
			return 0;
		if (errno != EEXIST)
			return -1;
	}
	if (MoveFileEx(pold, pnew,
				MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED))
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
	DECLARE_PROC_ADDR(BOOL, GetUserProfileDirectoryA, HANDLE, LPSTR, LPDWORD);

	if (!buf) {
		DWORD len = PATH_MAX;
		HANDLE h;

		buf = xzalloc(len);
		if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &h)) {
			if (INIT_PROC_ADDR(userenv.dll, GetUserProfileDirectoryA)) {
				GetUserProfileDirectoryA(h, buf, &len);
				bs_to_slash(buf);
			}
			CloseHandle(h);
		}
	}
	return buf;
}

#define NAME_LEN 100
char *get_user_name(void)
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

/*
 * When 'drop' drops privileges TokenIsElevated is still TRUE.
 * Find out if we're really privileged by checking if the group
 * BUILTIN\Administrators is enabled.
 */
int
elevation_state(void)
{
	int elevated = FALSE;
	int enabled = TRUE;
	HANDLE h;
#if ENABLE_DROP || ENABLE_CDROP || ENABLE_PDROP
	BOOL admin_enabled = TRUE;
	unsigned char admin[16] = {
		0x01, 0x02, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x05,
		0x20, 0x00, 0x00, 0x00,
		0x20, 0x02, 0x00, 0x00
	};
#endif

	if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &h)) {
		TOKEN_ELEVATION elevation = { 0 };
		DWORD size;

		if (GetTokenInformation(h, TokenElevation, &elevation,
					sizeof(elevation), &size))
			elevated = elevation.TokenIsElevated != 0;
		CloseHandle(h);
	}

#if ENABLE_DROP || ENABLE_CDROP || ENABLE_PDROP
	if (CheckTokenMembership(NULL, (PSID)admin, &admin_enabled))
		enabled = admin_enabled != 0;
#endif

	return elevated | (enabled << 1);
}

int getuid(void)
{
	return elevation_state() == (ELEVATED_PRIVILEGE | ADMIN_ENABLED) ?
				0 : DEFAULT_UID;
}

struct passwd *getpwnam(const char *name)
{
	const char *myname;

	if ( (myname=get_user_name()) != NULL &&
			strcmp(myname, name) == 0 ) {
		return getpwuid(DEFAULT_UID);
	}
	else if (strcmp(name, "root") == 0) {
		return getpwuid(0);
	}

	return NULL;
}

struct passwd *getpwuid(uid_t uid)
{
	static struct passwd p;

	if (uid == 0)
		p.pw_name = (char *)"root";
	else if (uid != DEFAULT_UID || (p.pw_name=get_user_name()) == NULL)
		return NULL;

	p.pw_dir = gethomedir();
	p.pw_passwd = (char *)"";
	p.pw_gecos = p.pw_name;
	p.pw_shell = NULL;
	p.pw_uid = uid;
	p.pw_gid = uid;

	return &p;
}

struct group *getgrgid(gid_t gid)
{
	static char *members[2] = { NULL, NULL };
	static struct group g;

	if (gid == 0) {
		g.gr_name = (char *)"root";
	}
	else if (gid != DEFAULT_GID || (g.gr_name=get_user_name()) == NULL) {
		return NULL;
	}
	g.gr_passwd = (char *)"";
	g.gr_gid = gid;
	members[0] = g.gr_name;
	g.gr_mem = members;

	return &g;
}

#if 0
int getgrouplist(const char *user UNUSED_PARAM, gid_t group,
					gid_t *groups, int *ngroups)
{
	if ( *ngroups == 0 ) {
		*ngroups = 1;
		return -1;
	}

	*ngroups = 1;
	groups[0] = group;
	return 1;
}

int getgroups(int n, gid_t *groups)
{
	if ( n == 0 ) {
		return 1;
	}

	groups[0] = getgid();
	return 1;
}
#endif

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
	memset(buf, 0, sizeof(*buf));
	return 0;
}

int link(const char *oldpath, const char *newpath)
{
	DECLARE_PROC_ADDR(BOOL, CreateHardLinkA, LPCSTR, LPCSTR,
						LPSECURITY_ATTRIBUTES);

	if (!INIT_PROC_ADDR(kernel32.dll, CreateHardLinkA)) {
		errno = ENOSYS;
		return -1;
	}
	if (!CreateHardLinkA(newpath, oldpath, NULL)) {
		errno = err_win_to_posix();
		return -1;
	}
	return 0;
}

#ifndef SYMBOLIC_LINK_FLAG_DIRECTORY
# define SYMBOLIC_LINK_FLAG_DIRECTORY (0x1)
#endif
#ifndef SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE
# define SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE (0x2)
#endif

int symlink(const char *target, const char *linkpath)
{
	DWORD flag = SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;
	DECLARE_PROC_ADDR(BOOLEAN, CreateSymbolicLinkA, LPCSTR, LPCSTR, DWORD);
	char *targ, *relative = NULL;

	if (!INIT_PROC_ADDR(kernel32.dll, CreateSymbolicLinkA)) {
		errno = ENOSYS;
		return -1;
	}

	if (is_relative_path(target) && has_path(linkpath)) {
		/* make target's path relative to current directory */
		const char *name = bb_get_last_path_component_nostrip(linkpath);
		relative = xasprintf("%.*s%s",
						(int)(name - linkpath), linkpath, target);
	}

	if (mingw_is_directory(relative ?: target))
		flag |= SYMBOLIC_LINK_FLAG_DIRECTORY;
	free(relative);

	targ = auto_string(strdup(target));
	slash_to_bs(targ);

 retry:
	if (!CreateSymbolicLinkA(linkpath, targ, flag)) {
		/* Old Windows versions see 'UNPRIVILEGED_CREATE' as an invalid
		 * parameter.  Retry without it. */
		if ((flag & SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE) &&
				GetLastError() == ERROR_INVALID_PARAMETER) {
			flag &= ~SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;
			goto retry;
		}
		errno = err_win_to_posix();
		return -1;
	}
	return 0;
}

/* Create a directory junction */
#define MRPB rptr->MountPointReparseBuffer
#if 0
static void print_junction(REPARSE_DATA_BUFFER *rptr)
{
	int i;
#define MRPB_HEADER_SIZE \
	(FIELD_OFFSET(REPARSE_DATA_BUFFER, MountPointReparseBuffer.PathBuffer) - \
	FIELD_OFFSET(REPARSE_DATA_BUFFER, MountPointReparseBuffer))

	fprintf(stderr, "---\n");
	fprintf(stderr, "Tag: %lx\n", rptr->ReparseTag);
	fprintf(stderr, "ReparseDataLength: %d (%d + %d + %d + %d + %d = %d)\n",
			rptr->ReparseDataLength, MRPB_HEADER_SIZE,
			MRPB.SubstituteNameLength, sizeof(WCHAR),
			MRPB.PrintNameLength, sizeof(WCHAR),
			MRPB_HEADER_SIZE + MRPB.SubstituteNameLength + sizeof(WCHAR) +
				MRPB.PrintNameLength + sizeof(WCHAR));
	fprintf(stderr, "Reserved: %d\n", rptr->Reserved);
	fprintf(stderr, "---\n");
	fprintf(stderr, "SubstituteNameOffset: %d\n", MRPB.SubstituteNameOffset);
	fprintf(stderr, "SubstituteNameLength: %d\n", MRPB.SubstituteNameLength);
	fprintf(stderr, "PrintNameOffset: %d\n", MRPB.PrintNameOffset);
	fprintf(stderr, "PrintNameLength: %d\n", MRPB.PrintNameLength);
	fprintf(stderr, "SubstituteName: ");
	for (i = 0; i < MRPB.SubstituteNameLength/sizeof(WCHAR); i++)
		fprintf(stderr, "%c",
			MRPB.PathBuffer[MRPB.SubstituteNameOffset/sizeof(WCHAR) + i]);
	fprintf(stderr, " (%x)",
		MRPB.PathBuffer[MRPB.SubstituteNameOffset/sizeof(WCHAR) + i]);
	fprintf(stderr, "\n");
	fprintf(stderr, "PrintName:      ");
	for (i = 0; i < MRPB.PrintNameLength/sizeof(WCHAR); i++)
		fprintf(stderr, "%c",
			MRPB.PathBuffer[MRPB.PrintNameOffset/sizeof(WCHAR) + i]);
	fprintf(stderr, " (%x)",
		MRPB.PathBuffer[MRPB.PrintNameOffset/sizeof(WCHAR) + i]);
	fprintf(stderr, "\n");
	fprintf(stderr, "---\n");
}
#endif

static REPARSE_DATA_BUFFER *make_junction_data_buffer(char *rpath)
{
	WCHAR pbuf[PATH_MAX];
	int plen, slen, rbufsize;
	REPARSE_DATA_BUFFER *rptr;

	/* We need two strings for the reparse data.  The PrintName is the
	 * target path in Win32 format, the SubstituteName is the same in
	 * NT format.
	 *
	 * The return value from MultiByteToWideChar includes the trailing
	 * L'\0' character.
	 */
	slash_to_bs(rpath);
	plen = MultiByteToWideChar(CP_ACP, 0, rpath, -1, pbuf, PATH_MAX);
	if (plen == 0) {
		errno = err_win_to_posix();
		return NULL;
	}
	slen = plen + 4;

	rbufsize = (slen + plen) * sizeof(WCHAR) +
		FIELD_OFFSET(REPARSE_DATA_BUFFER, MountPointReparseBuffer.PathBuffer);
	rptr = xzalloc(rbufsize);

	rptr->ReparseTag = IO_REPARSE_TAG_MOUNT_POINT;
	rptr->ReparseDataLength = rbufsize -
		FIELD_OFFSET(REPARSE_DATA_BUFFER, MountPointReparseBuffer);
	/* rptr->Reserved = 0; */
	/* MRPB.SubstituteNameOffset = 0; */
	MRPB.SubstituteNameLength = (slen - 1) * sizeof(WCHAR);
	MRPB.PrintNameOffset = MRPB.SubstituteNameLength + sizeof(WCHAR);
	MRPB.PrintNameLength = (plen - 1) * sizeof(WCHAR);

	wcscpy(MRPB.PathBuffer, L"\\??\\");
	wcscpy(MRPB.PathBuffer + 4, pbuf);
	wcscpy(MRPB.PathBuffer + slen, pbuf);
	return rptr;
}

int create_junction(const char *oldpath, const char *newpath)
{
	char rpath[PATH_MAX];
	struct stat statbuf;
	REPARSE_DATA_BUFFER *rptr = NULL;
	HANDLE h;
	int error = 0;
	DWORD bytes;

	if (realpath(oldpath, rpath) == NULL || stat(rpath, &statbuf) < 0)
		return -1;

	if (!has_dos_drive_prefix(rpath)) {
		errno = EINVAL;
		return -1;
	}

	if (!S_ISDIR(statbuf.st_mode)) {
		errno = ENOTDIR;
		return -1;
	}

	if (!(rptr = make_junction_data_buffer(rpath))) {
		return -1;
	}

	if (mkdir(newpath, 0777) < 0) {
		free(rptr);
		return -1;
	}

	h = CreateFileA(newpath, GENERIC_READ | GENERIC_WRITE, 0, NULL,
			OPEN_EXISTING,
			FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, NULL);
	if (h != INVALID_HANDLE_VALUE) {
		if (DeviceIoControl(h, FSCTL_SET_REPARSE_POINT, rptr,
				rptr->ReparseDataLength + REPARSE_DATA_BUFFER_HEADER_SIZE,
				NULL, 0, &bytes, NULL) != 0) {
			CloseHandle(h);
			free(rptr);
			return 0;
		}
		error = err_win_to_posix();
		CloseHandle(h);
	} else {
		error = err_win_to_posix();
	}

	rmdir(newpath);
	free(rptr);
	errno = error;
	return -1;
}

static char *normalize_ntpathA(char *buf)
{
	/* fix absolute path prefixes */
	if (buf[0] == '\\') {
		/* strip NT namespace prefixes */
		if (is_prefixed_with(buf, "\\??\\") ||
				is_prefixed_with(buf, "\\\\?\\"))
			buf += 4;
		else if (is_prefixed_with_case(buf, "\\DosDevices\\"))
			buf += 12;
		/* replace remaining '...UNC\' with '\\' */
		if (is_prefixed_with_case(buf, "UNC\\")) {
			buf += 2;
			*buf = '\\';
		}
	}
	return buf;
}

static char *resolve_symlinks(char *path)
{
	HANDLE h;
	DWORD status;
	char *ptr = NULL;
	DECLARE_PROC_ADDR(DWORD, GetFinalPathNameByHandleA, HANDLE,
						LPSTR, DWORD, DWORD);
	char *resolve = NULL;

	if (GetFileAttributesA(path) & FILE_ATTRIBUTE_REPARSE_POINT) {
		resolve = xmalloc_follow_symlinks(path);
		if (!resolve)
			return NULL;
	}

	/* need a file handle to resolve symlinks */
	h = CreateFileA(resolve ?: path, 0, 0, NULL, OPEN_EXISTING,
				FILE_FLAG_BACKUP_SEMANTICS, NULL);
	if (h != INVALID_HANDLE_VALUE) {
		if (!INIT_PROC_ADDR(kernel32.dll, GetFinalPathNameByHandleA)) {
			if (resolve)
				strcpy(path, resolve);
			ptr = path;
			goto end;
		}

		/* normalize the path and return it on success */
		status = GetFinalPathNameByHandleA(h, path, MAX_PATH,
							FILE_NAME_NORMALIZED|VOLUME_NAME_DOS);
		if (status != 0 && status < MAX_PATH) {
			ptr = normalize_ntpathA(path);
			goto end;
		} else if (err_win_to_posix() == ENOSYS) {
			if (resolve)
				strcpy(path, resolve);
			ptr = path;
			goto end;
		}
	}

	errno = err_win_to_posix();
 end:
	CloseHandle(h);
	free(resolve);
	return ptr;
}

/*
 * Emulate realpath in two stages:
 *
 * - _fullpath handles './', '../' and extra '/' characters.  The
 *   resulting path may not refer to an actual file.
 *
 * - resolve_symlinks checks that the file exists (by opening it) and
 *   resolves symlinks by calling GetFinalPathNameByHandleA.
 */
char *realpath(const char *path, char *resolved_path)
{
	char buffer[MAX_PATH];
	char *real_path, *p;

	/* enforce glibc pre-2.3 behaviour */
	if (path == NULL || resolved_path == NULL) {
		errno = EINVAL;
		return NULL;
	}

	if (_fullpath(buffer, path, MAX_PATH) &&
			(real_path=resolve_symlinks(buffer))) {
		bs_to_slash(strcpy(resolved_path, real_path));
		p = last_char_is(resolved_path, '/');
		if (p && p > resolved_path && p[-1] != ':')
			*p = '\0';
		return resolved_path;
	}
	return NULL;
}

static wchar_t *normalize_ntpath(wchar_t *wbuf)
{
	int i;
	/* fix absolute path prefixes */
	if (wbuf[0] == '\\') {
		/* strip NT namespace prefixes */
		if (!wcsncmp(wbuf, L"\\??\\", 4) ||
		    !wcsncmp(wbuf, L"\\\\?\\", 4))
			wbuf += 4;
		else if (!wcsnicmp(wbuf, L"\\DosDevices\\", 12))
			wbuf += 12;
		/* replace remaining '...UNC\' with '\\' */
		if (!wcsnicmp(wbuf, L"UNC\\", 4)) {
			wbuf += 2;
			*wbuf = '\\';
		}
	}
	/* convert backslashes to slashes */
	for (i = 0; wbuf[i]; i++)
		if (wbuf[i] == '\\')
			wbuf[i] = '/';
	return wbuf;
}

/*
 * This is the stucture required for reparse points with the tag
 * IO_REPARSE_TAG_APPEXECLINK.  The Buffer member contains four
 * NUL-terminated, concatentated strings:
 *
 *  package id, entry point, executable path and application type.
 *
 *  https://www.tiraniddo.dev/2019/09/overview-of-windows-execution-aliases.html
 */
typedef struct {
	DWORD	ReparseTag;
	USHORT	ReparseDataLength;
	USHORT	Reserved;
	ULONG	Version;
	WCHAR	Buffer[1];
} APPEXECLINK_BUFFER;

#define SRPB rptr->SymbolicLinkReparseBuffer
char * FAST_FUNC xmalloc_readlink(const char *pathname)
{
	HANDLE h;
	char *buf;
	int bufsiz;

	h = CreateFile(pathname, 0, 0, NULL, OPEN_EXISTING,
				FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_BACKUP_SEMANTICS, NULL);
	if (h != INVALID_HANDLE_VALUE) {
		DWORD nbytes;
		BYTE rbuf[MAXIMUM_REPARSE_DATA_BUFFER_SIZE];
		PREPARSE_DATA_BUFFER rptr = (PREPARSE_DATA_BUFFER)rbuf;
		APPEXECLINK_BUFFER *aptr = (APPEXECLINK_BUFFER *)rptr;
		BOOL status;
		size_t len;
		WCHAR *name = NULL, *str[4], *s;
		int i;

		status = DeviceIoControl(h, FSCTL_GET_REPARSE_POINT, NULL, 0,
					rptr, sizeof(rbuf), &nbytes, NULL);
		CloseHandle(h);

		if (status && rptr->ReparseTag == IO_REPARSE_TAG_SYMLINK) {
			len = SRPB.SubstituteNameLength/sizeof(WCHAR);
			name = SRPB.PathBuffer + SRPB.SubstituteNameOffset/sizeof(WCHAR);
		} else if (status && rptr->ReparseTag == IO_REPARSE_TAG_MOUNT_POINT) {
			len = MRPB.SubstituteNameLength/sizeof(WCHAR);
			name = MRPB.PathBuffer + MRPB.SubstituteNameOffset/sizeof(WCHAR);
		} else if (status && rptr->ReparseTag == IO_REPARSE_TAG_APPEXECLINK) {
			// We only need the executable path but we determine all of
			// the strings as a sanity check.
			i = 0;
			s = aptr->Buffer;
			do {
				str[i] = s;
				while (*s++)
					;
			} while (++i < 4);

			if (s - aptr->Buffer < MAXIMUM_REPARSE_DATA_BUFFER_SIZE) {
				len = wcslen(str[2]);
				name = str[2];
			}
		}

		if (name) {
			name[len] = 0;
			name = normalize_ntpath(name);
			bufsiz = WideCharToMultiByte(CP_ACP, 0, name, -1, NULL, 0, 0, 0);
			if (bufsiz) {
				buf = xmalloc(bufsiz);
				if (WideCharToMultiByte(CP_ACP, 0, name, -1, buf, bufsiz, 0, 0))
					return buf;
				free(buf);
			}
		}
	}
	errno = err_win_to_posix();
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
		bs_to_slash(path);
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

#undef chdir
int mingw_chdir(const char *dirname)
{
	int ret = -1;
	char *realdir;

	if (is_symlink(dirname))
		realdir = xmalloc_realpath(dirname);
	else
		realdir = xstrdup(dirname);

	if (realdir) {
		fix_path_case(realdir);
		ret = chdir(realdir);
	}
	free(realdir);

	return ret;
}

#undef chmod
int mingw_chmod(const char *path, int mode)
{
	if (mingw_is_directory(path))
		mode |= 0222;

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
#undef rmdir
int mingw_unlink(const char *pathname)
{
	int ret;

	/* read-only files cannot be removed */
	chmod(pathname, 0666);

	ret = unlink(pathname);
	if (ret == -1 && errno == EACCES) {
		/* a symlink to a directory needs to be removed by calling rmdir */
		/* (the *real* Windows rmdir, not mingw_rmdir) */
		if (is_symlink(pathname)) {
			return rmdir(pathname);
		}
	}
	return ret;
}

struct pagefile_info {
	SIZE_T total;
	SIZE_T in_use;
};

static BOOL CALLBACK
pagefile_cb(LPVOID context, PENUM_PAGE_FILE_INFORMATION info,
				LPCSTR name UNUSED_PARAM)
{
	struct pagefile_info *pfinfo = (struct pagefile_info *)context;

	pfinfo->total += info->TotalSize;
	pfinfo->in_use += info->TotalInUse;
	return TRUE;
}

int sysinfo(struct sysinfo *info)
{
	PERFORMANCE_INFORMATION perf;
	struct pagefile_info pfinfo;
	DECLARE_PROC_ADDR(BOOL, GetPerformanceInfo, PPERFORMANCE_INFORMATION,
						DWORD);
	DECLARE_PROC_ADDR(BOOL, EnumPageFilesA, PENUM_PAGE_FILE_CALLBACKA, LPVOID);

	memset((void *)info, 0, sizeof(struct sysinfo));
	memset((void *)&perf, 0, sizeof(PERFORMANCE_INFORMATION));
	memset((void *)&pfinfo, 0, sizeof(struct pagefile_info));
	info->mem_unit = 4096;

	if (INIT_PROC_ADDR(psapi.dll, GetPerformanceInfo)) {
		perf.cb = sizeof(PERFORMANCE_INFORMATION);
		GetPerformanceInfo(&perf, perf.cb);
	}

	if (INIT_PROC_ADDR(psapi.dll, EnumPageFilesA)) {
		EnumPageFilesA((PENUM_PAGE_FILE_CALLBACK)pagefile_cb, (LPVOID)&pfinfo);
	}

	info->totalram = perf.PhysicalTotal * perf.PageSize / 4096;
	info->bufferram = perf.SystemCache * perf.PageSize / 4096;
	if (perf.PhysicalAvailable > perf.SystemCache)
		info->freeram = perf.PhysicalAvailable * perf.PageSize / 4096 -
							info->bufferram;
	info->totalswap = pfinfo.total * perf.PageSize / 4096;
	info->freeswap = (pfinfo.total - pfinfo.in_use) * perf.PageSize / 4096;

	info->uptime = GetTickCount64() / 1000;
	info->procs = perf.ProcessCount;

	return 0;
}

#undef strftime
size_t mingw_strftime(char *buf, size_t max, const char *format, const struct tm *tm)
{
	size_t ret;
	char buffer[64];
	const char *replace;
	char *t;
	char *fmt;
	struct tm tm2;

	/*
	 * Emulate some formats that Windows' strftime lacks.
	 * - '%e' day of the month with space padding
	 * - '%s' number of seconds since the Unix epoch
	 * - '%T' same as %H:%M:%S
	 * - '%z' timezone offset
	 * Also, permit the '-' modifier to omit padding.  Windows uses '#'.
	 */
	fmt = xstrdup(format);
	for ( t=fmt; *t; ++t ) {
		if ( *t == '%' ) {
			replace = NULL;
			if ( t[1] == 'e' ) {
				if ( tm->tm_mday >= 0 && tm->tm_mday <= 99 ) {
					sprintf(buffer, "%2d", tm->tm_mday);
				}
				else {
					strcpy(buffer, "  ");
				}
				replace = buffer;
			}
			else if ( t[1] == 's' ) {
				tm2 = *tm;
				sprintf(buffer, "%"LL_FMT"d", (long long)mktime(&tm2));
				replace = buffer;
			}
			else if ( t[1] == 'T' ) {
				replace = "%H:%M:%S";
			}
			else if ( t[1] == 'z' ) {
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
				else {
					buffer[0] = '\0';
				}
				replace = buffer;
			}
			else if ( t[1] == '-' && t[2] != '\0' &&
						strchr("dHIjmMSUwWyY", t[2]) ) {
				/* Microsoft uses '#' rather than '-' to remove padding */
				t[1] = '#';
			}
			else if ( t[1] != '\0' ) {
				++t;
			}

			if (replace) {
				int m;
				char *newfmt;

				*t = '\0';
				m = t - fmt;
				newfmt = xasprintf("%s%s%s", fmt, replace, t+2);
				free(fmt);
				t = newfmt + m + strlen(replace) - 1;
				fmt = newfmt;
			}
		}
	}

	ret = strftime(buf, max, fmt, tm);
	free(fmt);

	return ret;
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

	if (!mingw_stat(name, &s)) {
		if ((s.st_mode&S_IXUSR)) {
			return 0;
		}
		errno = EACCES;
	}

	return -1;
}

int mingw_rmdir(const char *path)
{
	/* On Linux rmdir(2) doesn't remove symlinks */
	if (is_symlink(path)) {
		errno = ENOTDIR;
		return -1;
	}

	/* read-only directories cannot be removed */
	chmod(path, 0666);
	return rmdir(path);
}

void mingw_sync(void)
{
	HANDLE h;
	FILE *mnt;
	struct mntent *entry;
	char name[] = "\\\\.\\C:";

	mnt = setmntent(bb_path_mtab_file, "r");
	if (mnt) {
		while ((entry=getmntent(mnt)) != NULL) {
			name[4] = entry->mnt_dir[0];
			h = CreateFile(name, GENERIC_READ | GENERIC_WRITE,
						FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
						OPEN_EXISTING, 0, NULL);
			if (h != INVALID_HANDLE_VALUE) {
				FlushFileBuffers(h);
				CloseHandle(h);
			}
		}
		endmntent(mnt);
	}
}

#define NUMEXT 5
static const char win_suffix[NUMEXT][4] = { "com", "exe", "sh", "bat", "cmd" };

static int has_win_suffix(const char *name, int start)
{
	const char *dot = strrchr(bb_basename(name), '.');
	int i;

	if (dot != NULL && strlen(dot) < 5) {
		for (i=start; i<NUMEXT; ++i) {
			if (!strcasecmp(dot+1, win_suffix[i])) {
				return 1;
			}
		}
	}
	return 0;
}

int has_bat_suffix(const char *name)
{
	return has_win_suffix(name, 3);
}

int has_exe_suffix(const char *name)
{
	return has_win_suffix(name, 0);
}

int has_exe_suffix_or_dot(const char *name)
{
	return last_char_is(name, '.') || has_win_suffix(name, 0);
}

/* Copy path to an allocated string long enough to allow a file extension
 * to be added. */
char *alloc_ext_space(const char *path)
{
	char *s = xmalloc(strlen(path) + 5);
	strcpy(s, path);
	return s;
}

/* Check if path is an executable or can be made into one by adding
 * a suffix.  The suffix is added to the end of the argument which
 * must be long enough to allow this.
 *
 * If the return value is TRUE the argument contains the new path,
 * if FALSE the argument is unchanged.
 */
int
add_win32_extension(char *p)
{
	if (file_is_executable(p))
		return TRUE;

	if (!has_exe_suffix_or_dot(p)) {
		int i, len = strlen(p);

		p[len] = '.';
		for (i = 0; i < NUMEXT; ++i) {
			strcpy(p + len + 1, win_suffix[i]);
			if (file_is_executable(p))
				return TRUE;
		}
		p[len] = '\0';
	}
	return FALSE;
}

/*
 * Determine if a path represents a WIN32 executable, adding a suffix
 * if necessary.  Returns an allocated string if it does, NULL if not.
 */
char *
file_is_win32_exe(const char *name)
{
	char *path = alloc_ext_space(name);

	if (add_win32_extension(path))
		return path;

	free(path);
	return NULL;
}

char * FAST_FUNC bs_to_slash(char *str)
{
	char *p;

	for (p=str; *p; ++p) {
		if ( *p == '\\' ) {
			*p = '/';
		}
	}
	return str;
}

#if ENABLE_UNICODE_SUPPORT
MINGW_BB_WCHAR_T * FAST_FUNC bs_to_slash_u(MINGW_BB_WCHAR_T *str)
{
	MINGW_BB_WCHAR_T *p;

	for (p=str; *p; ++p) {
		if ( *p == '\\' ) {
			*p = '/';
		}
	}
	return str;
}
#endif

void FAST_FUNC slash_to_bs(char *p)
{
	for (; *p; ++p) {
		if ( *p == '/' ) {
			*p = '\\';
		}
	}
}

/* Windows strips trailing dots and spaces from the last component of
 * a file path.  This routine emulates that behaviour so we can preempt
 * Windows if necessary. */
void FAST_FUNC strip_dot_space(char *p)
{
	char *start = (char *)bb_basename(p);
	char *end = start + strlen(start);

	while (end > start && (end[-1] == '.' || end[-1] == ' ')) {
		*--end = '\0';
	}

	// Strip trailing slash, but not from a drive root (C:/)
	if (--end != start && (*end == '/' || *end == '\\') &&
			!(end == p + 2 && root_len(p) == 2))
		*end = '\0';
}

size_t FAST_FUNC remove_cr(char *p, size_t len)
{
	ssize_t i, j;

	for (i=j=0; i<len; ++i) {
		if (p[i] == '\r' && i < len - 1 && p[i+1] == '\n')
			continue;
		p[j++] = p[i];
	}
	return j;
}

off_t mingw_lseek(int fd, off_t offset, int whence)
{
	DWORD ftype;
	HANDLE h = (HANDLE)_get_osfhandle(fd);
	if (h == INVALID_HANDLE_VALUE) {
		errno = EBADF;
		return -1;
	}
	ftype = GetFileType(h);
	if (ftype != FILE_TYPE_DISK && ftype != FILE_TYPE_CHAR) {
		errno = ESPIPE;
		return -1;
	}
	return _lseeki64(fd, offset, whence);
}

#undef GetTickCount64
ULONGLONG CompatGetTickCount64(void)
{
	DECLARE_PROC_ADDR(ULONGLONG, GetTickCount64, void);

	if (!INIT_PROC_ADDR(kernel32.dll, GetTickCount64)) {
		return (ULONGLONG)GetTickCount();
	}

	return GetTickCount64();
}

#if ENABLE_FEATURE_INSTALLER
/*
 * Enumerate the names of all hard links to a file.  The first call
 * provides the file name as the first argument; subsequent calls must
 * set the first argument to NULL.  Returns 0 on error or when there are
 * no more links.
 */
int enumerate_links(const char *file, char *name)
{
	static HANDLE h = INVALID_HANDLE_VALUE;
	char aname[PATH_MAX];
	wchar_t wname[PATH_MAX];
	DWORD length = PATH_MAX;
	DECLARE_PROC_ADDR(HANDLE, FindFirstFileNameW, LPCWSTR, DWORD, LPDWORD,
						PWSTR);
	DECLARE_PROC_ADDR(BOOL, FindNextFileNameW, HANDLE, LPDWORD, PWSTR);

	if (!INIT_PROC_ADDR(kernel32.dll, FindFirstFileNameW) ||
			!INIT_PROC_ADDR(kernel32.dll, FindNextFileNameW))
		return 0;

	if (file != NULL) {
		wchar_t wfile[PATH_MAX];
		MultiByteToWideChar(CP_ACP, 0, file, -1, wfile, PATH_MAX);
		h = FindFirstFileNameW(wfile, 0, &length, wname);
		if (h == INVALID_HANDLE_VALUE)
			return 0;
	}
	else if (!FindNextFileNameW(h, &length, wname)) {
		FindClose(h);
		h = INVALID_HANDLE_VALUE;
		return 0;
	}
	WideCharToMultiByte(CP_ACP, 0, wname, -1, aname, PATH_MAX, NULL, NULL);
	realpath(aname, name);
	return 1;
}
#endif

/* Return the length of the root of a UNC path, i.e. the '//host/share'
 * component, or 0 if the path doesn't look like that. */
int FAST_FUNC unc_root_len(const char *dir)
{
	const char *s = dir + 2;
	int len;

	if (!is_unc_path(dir))
		return 0;
	len = strcspn(s, "/\\");
	if (len == 0)
		return 0;
	s += len + 1;
	len = strcspn(s, "/\\");
	if (len == 0)
		return 0;
	s += len;

	return s - dir;
}

/* Return the length of the root of a path, i.e. either the drive or
 * UNC '//host/share', or 0 if the path doesn't look like that. */
int FAST_FUNC root_len(const char *path)
{
	if (path == NULL)
		return 0;
	if (isalpha(*path) && path[1] == ':')
		return 2;
	return unc_root_len(path);
}

const char * FAST_FUNC get_system_drive(void)
{
	static const char *drive = NULL;
	char sysdir[PATH_MAX];
	int len;

	if (drive == NULL) {
		UINT ret = GetSystemDirectory(sysdir, PATH_MAX);
		if ((ret != 0 && ret < PATH_MAX) && (len=root_len(sysdir)))
			drive = xstrndup(sysdir, len);
		else
			drive = "";
	}

	return getenv(BB_SYSTEMROOT) ?: drive;
}

int chdir_system_drive(void)
{
	const char *sd = get_system_drive();
	int ret = -1;

	if (*sd)
		ret = chdir(auto_string(concat_path_file(sd, "")));
	return ret;
}

/*
 * This function is used to make relative paths absolute before a call
 * to chdir_system_drive().  It's unlikely to be useful in other cases.
 *
 * If the argument is an absolute path return 'path', otherwise return
 * an allocated string containing the resolved path.  Die on failure,
 * which is most likely because the file doesn't exist.
 */
char * FAST_FUNC xabsolute_path(char *path)
{
	char *rpath;

	if (root_len(path) != 0)
		return path;	// absolute path
	rpath = xmalloc_realpath(path);
	if (rpath)
		return rpath;
	bb_perror_msg_and_die("can't open '%s'", path);
}

char * FAST_FUNC get_drive_cwd(const char *path, char *buffer, int size)
{
	char drive[3] = { *path, ':', '\0' };
	DWORD ret;

	ret = GetFullPathName(drive, size, buffer, NULL);
	if (ret == 0 || ret > size)
		return NULL;
	return bs_to_slash(buffer);
}

void FAST_FUNC fix_path_case(char *path)
{
	char resolved[PATH_MAX];
	int len;

	// Canonicalise path: for physical drives this makes case match
	// what's stored on disk.  For mapped drives, not so much.
	if (realpath(path, resolved) && strcasecmp(path, resolved) == 0)
		strcpy(path, resolved);

	// make drive letter or UNC hostname uppercase
	len = root_len(path);
	if (len == 2) {
		*path = toupper(*path);
	}
	else if (len != 0) {
		for (path+=2; !is_dir_sep(*path); ++path) {
			*path = toupper(*path);
		}
	}
}

void FAST_FUNC make_sparse(int fd, off_t start, off_t end)
{
	DWORD dwTemp;
	HANDLE fh;
	FILE_ZERO_DATA_INFORMATION fzdi;

	if ((fh=(HANDLE)_get_osfhandle(fd)) == INVALID_HANDLE_VALUE)
		return;

	DeviceIoControl(fh, FSCTL_SET_SPARSE, NULL, 0, NULL, 0, &dwTemp, NULL);

	fzdi.FileOffset.QuadPart = start;
	fzdi.BeyondFinalZero.QuadPart = end;
	DeviceIoControl(fh, FSCTL_SET_ZERO_DATA, &fzdi, sizeof(fzdi),
					 NULL, 0, &dwTemp, NULL);
}

void *get_proc_addr(const char *dll, const char *function,
					struct proc_addr *proc)
{
	/* only do this once */
	if (!proc->initialized) {
		HANDLE hnd = LoadLibraryExA(dll, NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);

		/* The documentation for LoadLibraryEx says the above may fail
		 * on Windows 7.  If it does, retry using LoadLibrary with an
		 * explicit, backslash-separated path. */
		if (!hnd) {
			char buf[PATH_MAX];
			UINT ret = GetSystemDirectory(buf, PATH_MAX);
			if (ret != 0 && ret < PATH_MAX) {
				char *path = concat_path_file(buf, dll);
				slash_to_bs(path);
				hnd = LoadLibrary(path);
				free(path);
			}
		}

		if (hnd)
			proc->pfunction = GetProcAddress(hnd, function);
		proc->initialized = 1;
	}
	return proc->pfunction;
}

int FAST_FUNC unix_path(const char *path)
{
	int i;
	char *p = xstrdup(path);

#define UNIX_PATHS "/bin\0/usr/bin\0/sbin\0/usr/sbin\0"
	i = index_in_strings(UNIX_PATHS, dirname(p));
	free(p);
	return i >= 0;
}

/* Return true if file is referenced using a path.  This means a path
 * look-up isn't required. */
int FAST_FUNC has_path(const char *file)
{
	return strchr(file, '/') || strchr(file, '\\') ||
				has_dos_drive_prefix(file);
}

/*
 * Test whether a path is relative to a known location (usually the
 * current working directory or a symlink).  On Unix this is a path
 * that doesn't start with a slash but on Windows it also includes
 * paths that don't start with a backslash or a drive letter.
 *
 * Paths of the form /dir/file or c:dir/file aren't relative by this
 * definition.
 */
int FAST_FUNC is_relative_path(const char *path)
{
	return !is_dir_sep(path[0]) && !has_dos_drive_prefix(path);
}

#if ENABLE_FEATURE_SH_STANDALONE
/*
 * In standalone shell mode it's possible there's no binary file
 * corresponding to an applet name.  There's one case where it's
 * easy to determine the corresponding binary:  if the applet name
 * matches the file name from bb_busybox_exec_path (with appropriate
 * allowance for 'busybox*.exe').
 */
const char * FAST_FUNC applet_to_exe(const char *name)
{
	const char *exefile = bb_basename(bb_busybox_exec_path);
	const char *exesuff = is_prefixed_with_case(exefile, name);

	if (exesuff && (strcmp(name, "busybox") == 0 ||
						strcasecmp(exesuff, ".exe") == 0)) {
		return bb_busybox_exec_path;
	}
	return name;
}
#endif

/*
 * Append a word to a space-separated string of words.  The first
 * call should use a NULL pointer for str, subsequent calls should
 * pass an allocated string which will be freed.
 */
char * FAST_FUNC xappendword(const char *str, const char *word)
{
	char *newstr = str ? xasprintf("%s %s", str, word) : xstrdup(word);
	free((void *)str);
	return newstr;
}

/*
 * Detect if the environment contains certain mixed-case names:
 *
 *   Path          is present in a standard Windows environment
 *   ComSpec       is present in WINE
 *   ProgramData   is present in Cygwin/MSYS2
 */
int
windows_env(void)
{
	const char *names = "PATH=\0""COMSPEC=\0""PROGRAMDATA=\0";
	const char *n;

	for (char **envp = environ; envp && *envp; envp++) {
		for (n = names; *n; ) {
			if (is_prefixed_with_case(*envp, n) &&
						!is_prefixed_with(*envp, n)) {
				return TRUE;
			}
			while (*n++)
				;
		}
	}
	return FALSE;
}

void FAST_FUNC
change_critical_error_dialogs(const char *newval)
{
	SetErrorMode(newval && newval[0] == '1' && newval[1] == '\0' ?
					0 : SEM_FAILCRITICALERRORS);
}

char * FAST_FUNC exe_relative_path(const char *tail)
{
	char *exepath = xstrdup(bb_busybox_exec_path);
	char *relpath = concat_path_file(dirname(exepath), tail);
	free(exepath);
	return relpath;
}

int mingw_shell_execute(SHELLEXECUTEINFO *info)
{
	DECLARE_PROC_ADDR(BOOL, ShellExecuteExA, SHELLEXECUTEINFOA *);
	char *lpath;
	int ret;

	if (!INIT_PROC_ADDR(shell32.dll, ShellExecuteExA)) {
		errno = ENOSYS;
		return FALSE;
	}

	// ShellExecuteEx() needs backslash as separator in UNC paths.
	lpath = xstrdup(info->lpFile);
	slash_to_bs(lpath);
	info->lpFile = lpath;

	ret = ShellExecuteExA(info);

	free(lpath);
	return ret;
}

#if ENABLE_FEATURE_USE_CNG_API
void mingw_die_if_error(NTSTATUS status, const char *function_name) {
	if (!NT_SUCCESS(status)) {
		bb_error_msg_and_die("call to %s failed: 0x%08lX",
								function_name, (unsigned long)status);
	}
}
#endif
