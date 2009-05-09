#include "libbb.h"
#include <windef.h>
#include <wincon.h>
#include "strbuf.h"
#include "cygwin_termios.h"

int tcgetattr(int fd, struct termios *t)
{
	t->c_lflag = ECHO;
	return 0;
}


int tcsetattr(int fd, int mode, const struct termios *t)
{
	return 0;
}

static int get_wincon_width_height(const int fd, int *width, int *height)
{
	HANDLE console;
	CONSOLE_SCREEN_BUFFER_INFO sbi;

	console = GetStdHandle(STD_OUTPUT_HANDLE);
	if (console == INVALID_HANDLE_VALUE || !console)
		return -1;

	GetConsoleScreenBufferInfo(console, &sbi);

	if (width)
		*width = sbi.srWindow.Right - sbi.srWindow.Left;
	if (height)
		*height = sbi.srWindow.Bottom - sbi.srWindow.Top;
	return 0;
}

int get_terminal_width_height(const int fd, int *width, int *height)
{
	return get_wincon_width_height(fd, width, height);
}

int wincon_read(int fd, char *buf, int size)
{
	static struct strbuf input = STRBUF_INIT;
	HANDLE cin = GetStdHandle(STD_INPUT_HANDLE);
	static int initialized = 0;

	if (fd != 0)
		die("wincon_read is for stdin only");
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
