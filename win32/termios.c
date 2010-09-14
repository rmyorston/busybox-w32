#include "libbb.h"

int tcsetattr(int fd UNUSED_PARAM, int mode UNUSED_PARAM,  const struct termios *t UNUSED_PARAM)
{
	return -1;
}

int tcgetattr(int fd UNUSED_PARAM, struct termios *t UNUSED_PARAM)
{
	return -1;
}

int64_t FAST_FUNC read_key(int fd, char *buf, int timeout)
{
	static int initialized = 0;
	HANDLE cin = GetStdHandle(STD_INPUT_HANDLE);
	INPUT_RECORD record;
	DWORD nevent_out;

	if (fd != 0)
		bb_error_msg_and_die("read_key only works on stdin");
	if (cin == INVALID_HANDLE_VALUE)
		return -1;
	if (!initialized) {
		SetConsoleMode(cin, ENABLE_ECHO_INPUT);
		initialized = 1;
	}

	if (timeout > 0) {
		if (WaitForSingleObject(cin, timeout) != WAIT_OBJECT_0)
			goto done;
	}
	while (1) {
		if (!ReadConsoleInput(cin, &record, 1, &nevent_out))
			return -1;
		if (record.EventType != KEY_EVENT || !record.Event.KeyEvent.bKeyDown)
			continue;
		return record.Event.KeyEvent.uChar.AsciiChar;
	}
}
