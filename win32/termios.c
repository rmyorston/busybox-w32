#include "libbb.h"

int tcsetattr(int fd UNUSED_PARAM, int mode UNUSED_PARAM,  const struct termios *t UNUSED_PARAM)
{
	return -1;
}

int tcgetattr(int fd UNUSED_PARAM, struct termios *t UNUSED_PARAM)
{
	return -1;
}

int64_t FAST_FUNC read_key(int fd, char *buf UNUSED_PARAM, int timeout)
{
	HANDLE cin = GetStdHandle(STD_INPUT_HANDLE);
	INPUT_RECORD record;
	DWORD nevent_out, mode;
	int ret = -1;
	char *s;

	if (fd != 0)
		bb_error_msg_and_die("read_key only works on stdin");
	if (cin == INVALID_HANDLE_VALUE)
		return -1;
	GetConsoleMode(cin, &mode);
	SetConsoleMode(cin, 0);

	if (timeout > 0) {
		if (WaitForSingleObject(cin, timeout) != WAIT_OBJECT_0)
			goto done;
	}
	while (1) {
		if (!ReadConsoleInput(cin, &record, 1, &nevent_out))
			goto done;
		if (record.EventType != KEY_EVENT || !record.Event.KeyEvent.bKeyDown)
			continue;
		if (!record.Event.KeyEvent.uChar.AsciiChar) {
			DWORD state = record.Event.KeyEvent.dwControlKeyState;

			if (state & (RIGHT_CTRL_PRESSED|LEFT_CTRL_PRESSED) &&
			    (record.Event.KeyEvent.wVirtualKeyCode >= 'A' &&
			     record.Event.KeyEvent.wVirtualKeyCode <= 'Z')) {
				ret = record.Event.KeyEvent.wVirtualKeyCode & ~0x40;
				break;
			}

			switch (record.Event.KeyEvent.wVirtualKeyCode) {
			case VK_DELETE: ret = KEYCODE_DELETE; goto done;
			case VK_INSERT: ret = KEYCODE_INSERT; goto done;
			case VK_UP: ret = KEYCODE_UP; goto done;
			case VK_DOWN: ret = KEYCODE_DOWN; goto done;
			case VK_RIGHT:
				if (state & (RIGHT_CTRL_PRESSED|LEFT_CTRL_PRESSED)) {
					ret = KEYCODE_CTRL_RIGHT;
					goto done;
				}
				ret = KEYCODE_RIGHT;
				goto done;
			case VK_LEFT:
				if (state & (RIGHT_CTRL_PRESSED|LEFT_CTRL_PRESSED)) {
					ret = KEYCODE_CTRL_LEFT;
					goto done;
				}
				ret = KEYCODE_LEFT;
				goto done;
			case VK_HOME: ret = KEYCODE_HOME; goto done;
			case VK_END: ret = KEYCODE_END; goto done;
			case VK_PRIOR: ret = KEYCODE_PAGEUP; goto done;
			case VK_NEXT: ret = KEYCODE_PAGEDOWN; goto done;
			}
			continue;
		}
		if ( (record.Event.KeyEvent.uChar.AsciiChar & 0x80) == 0x80 ) {
			s = &record.Event.KeyEvent.uChar.AsciiChar;
			OemToCharBuff(s, s, 1);
		}
		ret = record.Event.KeyEvent.uChar.AsciiChar;
		break;
	}
 done:
	SetConsoleMode(cin, mode);
	return ret;
}
