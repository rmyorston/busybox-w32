#include "libbb.h"
#include <windef.h>
#include <wincon.h>
#include "strbuf.h"

#if ENABLE_FEATURE_CYGWIN_TTY
#include <ntdef.h>

/* NtCreateDirectoryObject */
#define DIRECTORY_QUERY			1
#define DIRECTORY_TRAVERSE		2
#define DIRECTORY_CREATE_OBJECT		4
#define DIRECTORY_CREATE_SUBDIRECTORY	8

#define CYG_SHARED_DIR_ACCESS	(DIRECTORY_QUERY		 \
				 | DIRECTORY_TRAVERSE		 \
				 | DIRECTORY_CREATE_SUBDIRECTORY \
				 | DIRECTORY_CREATE_OBJECT	 \
				 | READ_CONTROL)

typedef enum _OBJECT_INFORMATION_CLASS {
	ObjectNameInformation = 1,
} OBJECT_INFORMATION_CLASS;

typedef struct _OBJECT_NAME_INFORMATION {
	UNICODE_STRING Name;
} OBJECT_NAME_INFORMATION;

VOID NTAPI	RtlInitUnicodeString (PUNICODE_STRING, PCWSTR);
NTSTATUS NTAPI	NtOpenSection (PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES);
NTSTATUS NTAPI	NtCreateDirectoryObject (PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES);
NTSTATUS NTAPI	NtQueryObject (HANDLE, OBJECT_INFORMATION_CLASS, VOID *, ULONG, ULONG *);

/* Cygwin's shared_info */

struct tty_min {
	pid_t sid;	/* Session ID of tty */
	struct status_flags {
		unsigned initialized : 1; /* Set if tty is initialized */
		unsigned rstcons     : 1; /* Set if console needs to be set to "non-cooked" */
	} status;

	pid_t pgid;
	int output_stopped;
	int ntty;
	DWORD last_ctrl_c;	/* tick count of last ctrl-c */
	HWND hwnd;		/* Console window handle tty belongs to */

	struct termios ti;
	struct winsize winsize;

	/* ioctl requests buffer */
	int cmd;
	union {
		struct termios termios;
		struct winsize winsize;
		int value;
		pid_t pid;
	} arg;
	/* XXX_retval variables holds master's completion codes. Error are stored as
	 * -ERRNO
	 */
	int ioctl_retval;
	int write_error;
};

struct tty {
	struct tty_min tty_min;
	pid_t master_pid;	/* PID of tty master process */

	HANDLE from_master, to_master;

	int read_retval;
	int was_opened;		/* True if opened at least once. */
};

#define NTTYS		128
struct tty_list {
	struct tty ttys[NTTYS];
};

#define CYGWIN_PATH_MAX 4096
struct shared_info {
	DWORD version;
	DWORD cb;
	unsigned heap_chunk;
	int heap_slop_inited;
	unsigned heap_slop;
	DWORD sys_mount_table_counter;
	struct tty_list tty;
	LONG last_used_bindresvport;
	WCHAR installation_root[CYGWIN_PATH_MAX];
	DWORD obcaseinsensitive;
	/* mtinfo mt; */
};

/* Some magic to recognize shared_info */
#define CYGWIN_VERSION_SHARED_DATA	5
#define CYGWIN_VERSION_DLL_IDENTIFIER	L"cygwin1"
#define CYGWIN_VERSION_API_MAJOR	0
#define CYGWIN_VERSION_API_MINOR	210
#define SHARED_INFO_CB			39328
#define CURR_SHARED_MAGIC		0x22f9ff0bU

#define CYGWIN_VERSION_MAGIC(a, b) ((unsigned) ((((unsigned short) a) << 16) | (unsigned short) b))
#define SHARED_VERSION (unsigned)(CYGWIN_VERSION_API_MAJOR << 8 | CYGWIN_VERSION_API_MINOR)
#define SHARED_VERSION_MAGIC CYGWIN_VERSION_MAGIC (CURR_SHARED_MAGIC, SHARED_VERSION)

static struct shared_info *get_shared_info()
{
	WCHAR buf[32];
	HANDLE sh;
	WCHAR base[MAX_PATH];
	UNICODE_STRING uname;
	HANDLE dir;
	NTSTATUS status;
	OBJECT_ATTRIBUTES attr;
	static struct shared_info *si = NULL;

	if (si)
		return si;

	swprintf(base, L"\\BaseNamedObjects\\%sS%d",
		 CYGWIN_VERSION_DLL_IDENTIFIER,
		 CYGWIN_VERSION_SHARED_DATA);
	RtlInitUnicodeString (&uname, base);
	InitializeObjectAttributes (&attr, &uname, OBJ_INHERIT | OBJ_OPENIF, NULL, NULL);
	status = NtCreateDirectoryObject (&dir, CYG_SHARED_DIR_ACCESS, &attr);

	swprintf(buf, L"%s.%d", L"shared", CYGWIN_VERSION_SHARED_DATA);
	RtlInitUnicodeString(&uname, buf);
	InitializeObjectAttributes (&attr, &uname, OBJ_CASE_INSENSITIVE, dir, NULL);
	status = NtOpenSection (&sh, FILE_MAP_READ | FILE_MAP_WRITE, &attr);
	si = MapViewOfFileEx(sh, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 0, NULL);
	//if (si->version == SHARED_VERSION_MAGIC && si->cb == SHARED_INFO_CB)
	return si;
}

static int fd_to_tty(int fd)
{
	ULONG len = 0;
	OBJECT_NAME_INFORMATION dummy_oni, *ntfn;
	HANDLE h = _get_osfhandle(fd);
	char buf[PATH_MAX];
	int tty;

	NtQueryObject (h, ObjectNameInformation, &dummy_oni, sizeof (dummy_oni), &len);
	ntfn = malloc (len + sizeof (WCHAR));
	NtQueryObject (h, ObjectNameInformation, ntfn, len, NULL);
	wcstombs(buf, ntfn->Name.Buffer, 100);
	tty = -1;
	swscanf(ntfn->Name.Buffer, L"\\Device\\NamedPipe\\cygwin-tty%d-from-master", &tty);
	free(ntfn);
	return tty;
}

int is_cygwin_tty(int fd)
{
	return fd_to_tty(fd) >= 0 && get_shared_info() != NULL;
}

static int cygwin_tcgetattr(int fd, struct termios *t)
{
	int tty = fd_to_tty(fd);
	struct shared_info *si = get_shared_info();
	if (tty < 0 || !si)
		return -1;
	*t = si->tty.ttys[tty].tty_min.ti;
	return 0;
}


static int cygwin_tcsetattr(int fd, int mode, const struct termios *t)
{
	int tty = fd_to_tty(fd);
	struct shared_info *si = get_shared_info();
	if (tty < 0 || !si)
		return -1;
	si->tty.ttys[tty].tty_min.ti = *t;
	return 0;
}

#else

int is_cygwin_tty(int fd)
{
	return 0;
}

#define cygwin_tcgetattr(fd,t) -1
#define cygwin_tcsetattr(fd,mode,t) -1

#endif

#undef isatty
int mingw_isatty(int fd)
{
	return isatty(fd) || is_cygwin_tty(fd);
}

int tcgetattr(int fd, struct termios *t)
{
	int ret = cygwin_tcgetattr(fd, t);
	if (ret >= 0)
		return ret;
	/* not cygwin tty, likely windows console */
	t->c_lflag = ECHO;
	return 0;
}


int tcsetattr(int fd, int mode, const struct termios *t)
{
	return cygwin_tcsetattr(fd, mode, t);
}

static int get_wincon_width_height(const int fd, int *width, int *height)
{
	HANDLE console;
	CONSOLE_SCREEN_BUFFER_INFO sbi;

	console = GetStdHandle(STD_OUTPUT_HANDLE);
	if (console == INVALID_HANDLE_VALUE || !console || !GetConsoleScreenBufferInfo(console, &sbi))
		return -1;

	if (width)
		*width = sbi.srWindow.Right - sbi.srWindow.Left;
	if (height)
		*height = sbi.srWindow.Bottom - sbi.srWindow.Top;
	return 0;
}

int FAST_FUNC get_terminal_width_height(int fd, unsigned *width, unsigned *height)
{
#if ENABLE_FEATURE_CYGWIN_TTY
	int tty = fd_to_tty(fd);
	struct shared_info *si = get_shared_info();

	if (tty >= 0 && si) {
		if (width)
			*width = si->tty.ttys[tty].tty_min.winsize.ws_col;
		if (height)
			*height = si->tty.ttys[tty].tty_min.winsize.ws_row;
		return 0;
	}
#endif
	return get_wincon_width_height(fd, width, height);
}

int wincon_read(int fd, char *buf, int size)
{
	static struct strbuf input = STRBUF_INIT;
	HANDLE cin = GetStdHandle(STD_INPUT_HANDLE);
	static int initialized = 0;

	if (fd != 0)
		bb_error_msg_and_die("wincon_read is for stdin only");
	if (cin == INVALID_HANDLE_VALUE || is_cygwin_tty(fd))
		return safe_read(fd, buf, size);
	if (!initialized) {
		SetConsoleMode(cin, ENABLE_ECHO_INPUT);
		initialized = 1;
	}
	while (input.len < size) {
		INPUT_RECORD record;
		DWORD nevent_out;
		int ch;

		if (!ReadConsoleInput(cin, &record, 1, &nevent_out))
			return -1;
		if (record.EventType != KEY_EVENT || !record.Event.KeyEvent.bKeyDown)
			continue;
		ch = record.Event.KeyEvent.uChar.AsciiChar;
		/* Ctrl-X is handled by ReadConsoleInput, Alt-X is not needed anyway */
		strbuf_addch(&input, ch);
	}
	memcpy(buf, input.buf, size);
	memcpy(input.buf, input.buf+size, input.len-size+1);
	strbuf_setlen(&input, input.len-size);
	return size;
}
