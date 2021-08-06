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
#endif
	DWORD alt_pressed = FALSE;
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
			if (!(alt_pressed && (state & LEFT_ALT_PRESSED) == 0 &&
					record.Event.KeyEvent.wVirtualKeyCode == VK_MENU))
				continue;
		}
		alt_pressed = state & LEFT_ALT_PRESSED;

#if ENABLE_FEATURE_EURO
		if (!record.Event.KeyEvent.uChar.UnicodeChar) {
#else
		if (!record.Event.KeyEvent.uChar.AsciiChar) {
#endif
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
#if ENABLE_FEATURE_EURO
		uchar = record.Event.KeyEvent.uChar.UnicodeChar;
		achar = uchar & 0x7f;
		if (achar != uchar)
			WideCharToMultiByte(CP_ACP, 0, &uchar, 1, &achar, 1, NULL, NULL);
		ret = achar;
#else
		if ( (record.Event.KeyEvent.uChar.AsciiChar & 0x80) == 0x80 ) {
			char *s = &record.Event.KeyEvent.uChar.AsciiChar;
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
