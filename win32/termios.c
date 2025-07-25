#include "libbb.h"

int tcsetattr(int fd, int mode UNUSED_PARAM, const struct termios *t)
{
	HANDLE h = (HANDLE)_get_osfhandle(fd);
	if (!SetConsoleMode(h, t->w_mode)) {
		errno = err_win_to_posix();
		return -1;
	}

	return 0;
}

int tcgetattr(int fd, struct termios *t)
{
	HANDLE h = (HANDLE)_get_osfhandle(fd);
	if (!GetConsoleMode(h, &t->w_mode)) {
		errno = err_win_to_posix();
		return -1;
	}

	t->c_cc[VINTR] = 3;	// ctrl-c
	t->c_cc[VEOF] = 4;	// ctrl-d

	if (t->w_mode & ENABLE_ECHO_INPUT)
		t->c_lflag |= ECHO;
	else
		t->c_lflag &= ~ECHO;

	return 0;
}

int64_t FAST_FUNC windows_read_key(int fd, char *buf UNUSED_PARAM, int timeout)
{
	HANDLE cin = GetStdHandle(STD_INPUT_HANDLE);
	INPUT_RECORD record;
	DWORD nevent_out, mode;
	int ret = -1;
	DWORD alt_pressed = FALSE;
	DWORD state;

	if (fd != 0)
		bb_error_msg_and_die("read_key only works on stdin");
	if (cin == INVALID_HANDLE_VALUE)
		return -1;
	GetConsoleMode(cin, &mode);
	SetConsoleMode(cin, 0);

	while (1) {
		errno = 0;
		if (timeout > 0) {
			if (WaitForSingleObject(cin, timeout) != WAIT_OBJECT_0)
				goto done;
		}
		if (!readConsoleInput_utf8(cin, &record, 1, &nevent_out))
			goto done;

		if (record.EventType != KEY_EVENT)
			continue;

		state = record.Event.KeyEvent.dwControlKeyState;
		if (!record.Event.KeyEvent.bKeyDown) {
			/* ignore all key up events except Alt */
			if (!(alt_pressed && (state & LEFT_ALT_PRESSED) == 0 &&
					record.Event.KeyEvent.wVirtualKeyCode == VK_MENU))
				continue;
		}
		alt_pressed = state & LEFT_ALT_PRESSED;

		if (!record.Event.KeyEvent.uChar.AsciiChar) {
			if (alt_pressed && !(state & ENHANCED_KEY)) {
				/* keys on numeric pad used to enter character codes */
				switch (record.Event.KeyEvent.wVirtualKeyCode) {
				case VK_NUMPAD0: case VK_INSERT:
				case VK_NUMPAD1: case VK_END:
				case VK_NUMPAD2: case VK_DOWN:
				case VK_NUMPAD3: case VK_NEXT:
				case VK_NUMPAD4: case VK_LEFT:
				case VK_NUMPAD5: case VK_CLEAR:
				case VK_NUMPAD6: case VK_RIGHT:
				case VK_NUMPAD7: case VK_HOME:
				case VK_NUMPAD8: case VK_UP:
				case VK_NUMPAD9: case VK_PRIOR:
					continue;
				}
			}

			switch (record.Event.KeyEvent.wVirtualKeyCode) {
			case VK_DELETE: ret = KEYCODE_DELETE; break;
			case VK_INSERT: ret = KEYCODE_INSERT; break;
			case VK_UP: ret = KEYCODE_UP; break;
			case VK_DOWN: ret = KEYCODE_DOWN; break;
			case VK_RIGHT: ret = KEYCODE_RIGHT; break;
			case VK_LEFT: ret = KEYCODE_LEFT; break;
			case VK_HOME: ret = KEYCODE_HOME; break;
			case VK_END: ret = KEYCODE_END; break;
			case VK_PRIOR: ret = KEYCODE_PAGEUP; break;
			case VK_NEXT: ret = KEYCODE_PAGEDOWN; break;
			default:
				alt_pressed = FALSE;
				continue;
			}

			if (state & (RIGHT_ALT_PRESSED|LEFT_ALT_PRESSED))
				ret &= ~0x20;
			if (state & (RIGHT_CTRL_PRESSED|LEFT_CTRL_PRESSED))
				ret &= ~0x40;
			if (state & SHIFT_PRESSED)
				ret &= ~0x80;
			goto done;
		}
		if ( (record.Event.KeyEvent.uChar.AsciiChar & 0x80) == 0x80 ) {
			char *s = &record.Event.KeyEvent.uChar.AsciiChar;
			conToCharBuffA(s, 1);
		}
		ret = record.Event.KeyEvent.uChar.AsciiChar;
		if (state & (RIGHT_ALT_PRESSED|LEFT_ALT_PRESSED)) {
			switch (ret) {
			case '\b': ret = KEYCODE_ALT_BACKSPACE; goto done;
			case 'b': ret = KEYCODE_ALT_LEFT; goto done;
			case 'd': ret = KEYCODE_ALT_D; goto done;
			case 'f': ret = KEYCODE_ALT_RIGHT; goto done;
			}
		}
		break;
	}
 done:
	SetConsoleMode(cin, mode);
	return ret;
}
