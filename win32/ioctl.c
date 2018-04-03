#include "libbb.h"

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

int ioctl(int fd UNUSED_PARAM, int code, ...)
{
	va_list ap;
	void *arg;
	int ret = -1;

	va_start(ap, code);

	switch (code) {
	case TIOCGWINSZ:
		arg = va_arg(ap, void *);
		ret = mingw_get_terminal_width_height((struct winsize *)arg);
		break;
	default:
		ret = -1;
		errno = EINVAL;
		break;
	}

	va_end(ap);
	return ret;
}
