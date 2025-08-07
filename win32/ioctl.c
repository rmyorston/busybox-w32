#include "libbb.h"
#include "lazyload.h"

#if ENABLE_STTY || ENABLE_TTYSIZE
static int mingw_get_terminal_width_height(struct winsize *win)
{
	int fd;
	HANDLE handle;
	CONSOLE_SCREEN_BUFFER_INFO sbi;

	win->ws_row = 0;
	win->ws_col = 0;

	for (fd=STDOUT_FILENO; fd<=STDERR_FILENO; ++fd) {
		handle = (HANDLE)_get_osfhandle(fd);
		if (handle != INVALID_HANDLE_VALUE &&
				GetConsoleScreenBufferInfo(handle, &sbi) != 0) {
			win->ws_row = sbi.srWindow.Bottom - sbi.srWindow.Top + 1;
			win->ws_col = sbi.srWindow.Right - sbi.srWindow.Left + 1;
			return 0;
		}
	}

	return -1;
}
#endif

#if ENABLE_STTY
static int mingw_set_terminal_width_height(struct winsize *win)
{
	BOOL ret;
	DECLARE_PROC_ADDR(BOOL, GetConsoleScreenBufferInfoEx, HANDLE,
				PCONSOLE_SCREEN_BUFFER_INFOEX);
	DECLARE_PROC_ADDR(BOOL, SetConsoleScreenBufferInfoEx, HANDLE,
				PCONSOLE_SCREEN_BUFFER_INFOEX);

	if (!INIT_PROC_ADDR(kernel32.dll, GetConsoleScreenBufferInfoEx))
		return -1;
	if (!INIT_PROC_ADDR(kernel32.dll, SetConsoleScreenBufferInfoEx))
		return -1;

	for (int fd = STDOUT_FILENO; fd <= STDERR_FILENO; ++fd) {
		CONSOLE_SCREEN_BUFFER_INFOEX sbi;
		HANDLE handle = (HANDLE)_get_osfhandle(fd);

		sbi.cbSize = sizeof(sbi);
		if (handle != INVALID_HANDLE_VALUE &&
				(ret=GetConsoleScreenBufferInfoEx(handle, &sbi)) != 0) {
			if (sbi.dwSize.X != win->ws_col) {
				sbi.dwSize.X = win->ws_col;
			}
			if (sbi.dwSize.Y < win->ws_row) {
				sbi.dwSize.Y = win->ws_row;
			}
			sbi.srWindow.Bottom = sbi.srWindow.Top + win->ws_row;
			sbi.srWindow.Right = sbi.srWindow.Left + win->ws_col;
			ret = SetConsoleScreenBufferInfoEx(handle, &sbi);
			break;
		}
	}

	return ret ? 0 : -1;
}
#endif

int ioctl(int fd UNUSED_PARAM, int code, ...)
{
	va_list ap;
	void *arg;
	int ret = -1;

	va_start(ap, code);

	switch (code) {
#if ENABLE_STTY || ENABLE_TTYSIZE
	case TIOCGWINSZ:
		arg = va_arg(ap, void *);
		ret = mingw_get_terminal_width_height((struct winsize *)arg);
		break;
#endif
#if ENABLE_STTY
	case TIOCSWINSZ:
		arg = va_arg(ap, void *);
		ret = mingw_set_terminal_width_height((struct winsize *)arg);
		break;
#endif
	default:
		ret = -1;
		errno = EINVAL;
		break;
	}

	va_end(ap);
	return ret;
}
