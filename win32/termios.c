#include "libbb.h"

int64_t FAST_FUNC read_key(int fd, char *buf UNUSED_PARAM, int timeout)
{
	HANDLE cin = GetStdHandle(STD_INPUT_HANDLE);
	INPUT_RECORD record;
	DWORD nevent_out, mode;
	int ret = -1;
#if ENABLE_FEATURE_EURO
	wchar_t uchar;
	char achar;
#else
	char *s;
#endif
	int alt_pressed = FALSE;
	DWORD state;

	if (fd != 0)
		bb_error_msg_and_die("read_key only works on stdin");
	if (cin == INVALID_HANDLE_VALUE)
		return -1;
	GetConsoleMode(cin, &mode);
	SetConsoleMode(cin, 0);

	while (1) {
		if (timeout > 0) {
			if (WaitForSingleObject(cin, timeout) != WAIT_OBJECT_0)
				goto done;
		}
#if ENABLE_FEATURE_EURO
		if (!ReadConsoleInputW(cin, &record, 1, &nevent_out))
#else
		if (!ReadConsoleInput(cin, &record, 1, &nevent_out))
#endif
			goto done;

		if (record.EventType != KEY_EVENT)
			continue;

		state = record.Event.KeyEvent.dwControlKeyState;
		if (!record.Event.KeyEvent.bKeyDown) {
			/* ignore all key up events except Alt */
			if (alt_pressed && !(state & LEFT_ALT_PRESSED))
				alt_pressed = FALSE;
			else
				continue;
		}
		else {
			alt_pressed = ((state & LEFT_ALT_PRESSED) != 0);
		}

#if ENABLE_FEATURE_EURO
		if (!record.Event.KeyEvent.uChar.UnicodeChar) {
#else
		if (!record.Event.KeyEvent.uChar.AsciiChar) {
#endif
			if (alt_pressed) {
				switch (record.Event.KeyEvent.wVirtualKeyCode) {
				case VK_MENU:
				case VK_INSERT:
				case VK_END:
				case VK_DOWN:
				case VK_NEXT:
				case VK_CLEAR:
				case VK_HOME:
				case VK_UP:
				case VK_PRIOR:
				case VK_KANA:
					continue;
				}
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
				if (state & (RIGHT_ALT_PRESSED|LEFT_ALT_PRESSED)) {
					ret = KEYCODE_ALT_RIGHT;
					goto done;
				}
				ret = KEYCODE_RIGHT;
				goto done;
			case VK_LEFT:
				if (state & (RIGHT_CTRL_PRESSED|LEFT_CTRL_PRESSED)) {
					ret = KEYCODE_CTRL_LEFT;
					goto done;
				}
				if (state & (RIGHT_ALT_PRESSED|LEFT_ALT_PRESSED)) {
					ret = KEYCODE_ALT_LEFT;
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
#if ENABLE_FEATURE_EURO
		uchar = record.Event.KeyEvent.uChar.UnicodeChar;
		achar = uchar & 0x7f;
		if (achar != uchar)
			WideCharToMultiByte(CP_ACP, 0, &uchar, 1, &achar, 1, NULL, NULL);
		ret = achar;
#else
		if ( (record.Event.KeyEvent.uChar.AsciiChar & 0x80) == 0x80 ) {
			s = &record.Event.KeyEvent.uChar.AsciiChar;
			OemToCharBuff(s, s, 1);
		}
		ret = record.Event.KeyEvent.uChar.AsciiChar;
#endif
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
