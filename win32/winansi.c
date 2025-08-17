/*
 * Copyright 2008 Peter Harris <git@peter.is-a-geek.org>
 */

#include "libbb.h"
#include <windows.h>
#include "lazyload.h"
#undef PACKED

static BOOL charToConBuffA(LPSTR s, DWORD len);
static BOOL charToConA(LPSTR s);

static int conv_fwriteCon(FILE *stream, char *buf, size_t siz);
static int conv_writeCon(int fd, char *buf, size_t siz);

/*
 Functions to be wrapped:
*/
#undef vfprintf
#undef vprintf
#undef printf
#undef fprintf
#undef fputs
#undef fputc
#undef putchar
#undef fwrite
#undef puts
#undef write
#undef read
#undef fread
#undef getc
#undef fgets

#define FOREGROUND_ALL (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE)
#define BACKGROUND_ALL (BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE)

static WORD plain_attr = 0xffff;
static WORD current_attr;

static HANDLE get_console(void)
{
	return GetStdHandle(STD_OUTPUT_HANDLE);
}

static WORD get_console_attr(void)
{
	CONSOLE_SCREEN_BUFFER_INFO sbi;

	if (GetConsoleScreenBufferInfo(get_console(), &sbi))
		return sbi.wAttributes;

	return FOREGROUND_ALL;
}

static int is_console(int fd)
{
	if (plain_attr == 0xffff)
		current_attr = plain_attr = get_console_attr();
	return isatty(fd) && get_console() != INVALID_HANDLE_VALUE;
}

static ALWAYS_INLINE int is_console_in(int fd)
{
	return isatty(fd) && GetStdHandle(STD_INPUT_HANDLE) != INVALID_HANDLE_VALUE;
}

static int is_wine(void)
{
	DECLARE_PROC_ADDR(const char *, wine_get_version, void);

	return INIT_PROC_ADDR(ntdll.dll, wine_get_version) != NULL;
}

#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

#ifndef DISABLE_NEWLINE_AUTO_RETURN
#define DISABLE_NEWLINE_AUTO_RETURN 0x0008
#endif

#ifndef ENABLE_VIRTUAL_TERMINAL_INPUT
#define ENABLE_VIRTUAL_TERMINAL_INPUT 0x0200
#endif

int FAST_FUNC terminal_mode(int reset)
{
	static int mode = -1;

#if ENABLE_FEATURE_EURO
	if (mode < 0) {
		if (GetConsoleCP() == 850 && GetConsoleOutputCP() == 850) {
			SetConsoleCP(858);
			SetConsoleOutputCP(858);
		}
	}
#endif

	if (mode < 0 || reset) {
		HANDLE h;
		DWORD oldmode, newmode;
		const char *term = getenv(BB_TERMINAL_MODE);
		const char *skip = getenv(BB_SKIP_ANSI_EMULATION);

		if (term) {
			mode = atoi(term);
		} else if (skip) {
			mode = atoi(skip);
			if (mode == 2)
				mode = 5;
			else if (mode != 1)
				mode = 0;
		} else {
			mode = (getenv("CONEMUPID") != NULL || is_wine()) ? 0 :
						CONFIG_TERMINAL_MODE;
		}

		if (mode < 0 || mode > 5)
			mode = CONFIG_TERMINAL_MODE;

		if (is_console(STDOUT_FILENO)) {
			h = get_console();
			if (GetConsoleMode(h, &oldmode)) {
				// Try to recover from mode 0 induced by SSH.
				newmode = oldmode == 0 ? 3 : oldmode;
				// Turn off DISABLE_NEWLINE_AUTO_RETURN induced by Gradle?
				newmode &= ~DISABLE_NEWLINE_AUTO_RETURN;

				if ((mode & VT_OUTPUT)) {
					newmode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
				} else if (mode < 4) {
					newmode &= ~ENABLE_VIRTUAL_TERMINAL_PROCESSING;
				} else if ((oldmode & ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
					mode |= VT_OUTPUT;
				}

				if (newmode != oldmode) {
					if (!SetConsoleMode(h, newmode)) {
						if (mode >= 4)
							mode &= ~VT_OUTPUT;
						newmode &= ~ENABLE_VIRTUAL_TERMINAL_PROCESSING;
						SetConsoleMode(h, newmode);
					}
				}
			}
		}

		if (is_console_in(STDIN_FILENO)) {
			h = GetStdHandle(STD_INPUT_HANDLE);
			if (GetConsoleMode(h, &oldmode)) {
				// Try to recover from mode 0 induced by SSH.
				newmode = oldmode == 0 ? 0x1f7 : oldmode;

				if (mode < 4) {
					if ((mode & VT_INPUT))
						newmode |= ENABLE_VIRTUAL_TERMINAL_INPUT;
					else
						newmode &= ~ENABLE_VIRTUAL_TERMINAL_INPUT;
				} else if ((oldmode & ENABLE_VIRTUAL_TERMINAL_INPUT)) {
					mode |= VT_INPUT;
				}

				if (reset && newmode != oldmode) {
					if (!SetConsoleMode(h, newmode)) {
						if (mode >= 4)
							mode &= ~VT_INPUT;
						// Failure to set the new mode seems to leave
						// the flag set.  Forcibly unset it.
						newmode &= ~ENABLE_VIRTUAL_TERMINAL_INPUT;
						SetConsoleMode(h, newmode);
					}
				}
			}
		}
	}

	return mode;
}

void set_title(const char *str)
{
	SetConsoleTitle(str);
}

int get_title(char *buf, int len)
{
	return GetConsoleTitle(buf, len);
}

static HANDLE dup_handle(HANDLE h)
{
	HANDLE h2;

	if (!DuplicateHandle(GetCurrentProcess(), h, GetCurrentProcess(),
							&h2, 0, TRUE, DUPLICATE_SAME_ACCESS))
		return INVALID_HANDLE_VALUE;
	return h2;
}

static void use_alt_buffer(int flag)
{
	static HANDLE console_orig = INVALID_HANDLE_VALUE;
	HANDLE console, h;

	if (flag) {
		SECURITY_ATTRIBUTES sa;
		CONSOLE_SCREEN_BUFFER_INFO sbi;

		if (console_orig != INVALID_HANDLE_VALUE)
			return;

		console = get_console();
		console_orig = dup_handle(console);

		// handle should be inheritable
		memset(&sa, 0, sizeof(sa));
		sa.nLength = sizeof(sa);
		/* sa.lpSecurityDescriptor = NULL; - memset did it */
		sa.bInheritHandle = TRUE;

		// create new alternate buffer
		h = CreateConsoleScreenBuffer(GENERIC_READ|GENERIC_WRITE,
					FILE_SHARE_READ|FILE_SHARE_WRITE, &sa,
					CONSOLE_TEXTMODE_BUFFER, NULL);
		if (h == INVALID_HANDLE_VALUE)
			return;

		if (GetConsoleScreenBufferInfo(console, &sbi))
			SetConsoleScreenBufferSize(h, sbi.dwSize);
	}
	else {
		if (console_orig == INVALID_HANDLE_VALUE)
			return;

		// revert to original buffer
		h = dup_handle(console_orig);
		console_orig = INVALID_HANDLE_VALUE;
		if (h == INVALID_HANDLE_VALUE)
			return;
	}

	console = h;
	SetConsoleActiveScreenBuffer(console);
	close(STDOUT_FILENO);
	_open_osfhandle((intptr_t)console, O_RDWR|O_BINARY);
}

static void clear_buffer(DWORD len, COORD pos)
{
	HANDLE console = get_console();
	DWORD dummy;

	FillConsoleOutputCharacterA(console, ' ', len, pos, &dummy);
	FillConsoleOutputAttribute(console, plain_attr, len, pos, &dummy);
}

static void erase_in_line(void)
{
	HANDLE console = get_console();
	CONSOLE_SCREEN_BUFFER_INFO sbi;

	if (!GetConsoleScreenBufferInfo(console, &sbi))
		return;
	clear_buffer(sbi.dwSize.X - sbi.dwCursorPosition.X, sbi.dwCursorPosition);
}

static void erase_till_end_of_screen(void)
{
	HANDLE console = get_console();
	CONSOLE_SCREEN_BUFFER_INFO sbi;
	DWORD len;

	if(!GetConsoleScreenBufferInfo(console, &sbi))
		return;
	len = sbi.dwSize.X - sbi.dwCursorPosition.X +
			sbi.dwSize.X * (sbi.srWindow.Bottom - sbi.dwCursorPosition.Y);
	clear_buffer(len, sbi.dwCursorPosition);
}

void reset_screen(void)
{
	HANDLE console = get_console();
	CONSOLE_SCREEN_BUFFER_INFO sbi;
	COORD pos = { 0, 0 };

	/* move to start of screen buffer and clear it all */
	if (!GetConsoleScreenBufferInfo(console, &sbi))
		return;
	SetConsoleCursorPosition(console, pos);
	clear_buffer(sbi.dwSize.X * sbi.dwSize.Y, pos);
}

void move_cursor_row(int n)
{
	HANDLE console = get_console();
	CONSOLE_SCREEN_BUFFER_INFO sbi;

	if(!GetConsoleScreenBufferInfo(console, &sbi))
		return;
	sbi.dwCursorPosition.Y += n;
	SetConsoleCursorPosition(console, sbi.dwCursorPosition);
}

static void move_cursor_column(int n)
{
	HANDLE console = get_console();
	CONSOLE_SCREEN_BUFFER_INFO sbi;

	if (!GetConsoleScreenBufferInfo(console, &sbi))
		return;
	sbi.dwCursorPosition.X += n;
	SetConsoleCursorPosition(console, sbi.dwCursorPosition);
}

static void move_cursor(int x, int y)
{
	HANDLE console = get_console();
	COORD pos;
	CONSOLE_SCREEN_BUFFER_INFO sbi;

	if (!GetConsoleScreenBufferInfo(console, &sbi))
		return;
	pos.X = sbi.srWindow.Left + x;
	pos.Y = sbi.srWindow.Top + y;
	SetConsoleCursorPosition(console, pos);
}

static const unsigned char colour_1bit[16] = {
	/* Black */   0,
	/* Red   */   FOREGROUND_RED,
	/* Green */   FOREGROUND_GREEN,
	/* Yellow */  FOREGROUND_RED | FOREGROUND_GREEN,
	/* Blue */    FOREGROUND_BLUE,
	/* Magenta */ FOREGROUND_RED | FOREGROUND_BLUE,
	/* Cyan */    FOREGROUND_GREEN | FOREGROUND_BLUE,
	/* White */   FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
	/* ... and again but brighter */
	FOREGROUND_INTENSITY,
	FOREGROUND_RED | FOREGROUND_INTENSITY,
	FOREGROUND_GREEN | FOREGROUND_INTENSITY,
	FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY,
	FOREGROUND_BLUE | FOREGROUND_INTENSITY,
	FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
	FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
	FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY
};

#if !ENABLE_FEATURE_IMPROVED_COLOUR_MAPPING
static WORD rgb_to_console(int *rgb)
{
	int dark = 0, bright;
	WORD attr = 0;

	if (rgb[0] > 85)
		attr |= FOREGROUND_RED;
	else
		++dark;

	if (rgb[1] > 85)
		attr |= FOREGROUND_GREEN;
	else
		++dark;

	if (rgb[2] > 85)
		attr |= FOREGROUND_BLUE;
	else
		++dark;

	/* increase intensity if all components are either bright or
	 * dark and at least one is bright */
	bright = (rgb[0] > 171) + (rgb[1] > 171) + (rgb[2] > 171);
	if (bright + dark == 3 && dark != 3) {
		attr |= FOREGROUND_INTENSITY;
	}

	return attr;
}
#else
#include <math.h>

/* Standard console colours in LAB colour space */
static float colour_lab[16][3] = {
	{-0.000000, 0.000000, 0.000000},
	{25.530788, 48.055233, 38.059635},
	{46.228817, -51.699638, 49.897949},
	{51.868336, -12.930751, 56.677288},
	{12.975313, 47.507763, -64.704285},
	{29.782101, 58.939846, -36.497940},
	{48.256081, -28.841570, -8.481050},
	{77.704361, 0.004262, -0.008416},
	{53.585018, 0.003129, -0.006235},
	{53.232883, 80.109299, 67.220078},
	{87.737038, -86.184654, 83.181168},
	{97.138245, -21.555901, 94.482483},
	{32.302586, 79.196678, -107.863686},
	{60.319931, 98.254234, -60.842991},
	{91.116524, -48.079609, -14.138126},
	{100.000000, 0.005245, -0.010419},
};

/* Convert RGB to XYZ and XYZ to LAB.  See:
 * http://www.easyrgb.com/en/math.php#text1 */
static void rgb2lab(const int *rgb, float *lab)
{
	float var_RGB[3], var_XYZ[3];
	int i;

	for (i = 0; i < 3; ++i) {
		var_RGB[i] = rgb[i]/255.0f;
		if (var_RGB[i] > 0.04045f)
			var_RGB[i] = pow((var_RGB[i] + 0.055f) / 1.055f, 2.4f);
		else
			var_RGB[i] /= 12.92f;
	}

	/* use equal energy reference values */
	var_XYZ[0] = var_RGB[0]*0.4124f + var_RGB[1]*0.3576f + var_RGB[2]*0.1805f;
	var_XYZ[1] = var_RGB[0]*0.2126f + var_RGB[1]*0.7152f + var_RGB[2]*0.0722f;
	var_XYZ[2] = var_RGB[0]*0.0193f + var_RGB[1]*0.1192f + var_RGB[2]*0.9505f;

	for (i = 0; i < 3; ++i) {
		if (var_XYZ[i] > 0.008856f)
			var_XYZ[i] = pow(var_XYZ[i], 1.0f / 3.0f);
		else
			var_XYZ[i] = 7.787f * var_XYZ[i] + 16.0f / 116.0f;
	}

	lab[0] = 116.0f * var_XYZ[1] - 16.0f;
	lab[1] = 500.0f * (var_XYZ[0] - var_XYZ[1]);
	lab[2] = 200.0f * (var_XYZ[1] - var_XYZ[2]);
}

static WORD rgb_to_console(int *rgb)
{
	int i, imin = 0;
	float deltamin = 1.0e20;

	/* Use 1976 CIE deltaE to find closest console colour.  See:
	 * https://zschuessler.github.io/DeltaE/learn */
	for (i = 0; i < 16; ++i) {
		float lab[3], dl, da, db, delta;

		rgb2lab(rgb, lab);
		dl = colour_lab[i][0] - lab[0];
		da = colour_lab[i][1] - lab[1];
		db = colour_lab[i][2] - lab[2];
		delta = dl * dl + da * da + db *db;
		if (delta < deltamin) {
			imin = i;
			deltamin = delta;
		}
	}
	return colour_1bit[imin];
}
#endif

/* 24-bit colour */
static char *process_24bit(char *str, WORD *attr)
{
	int count;
	int rgb[3];

	for (count = 0; count < 3; ++count) {
		rgb[count] = strtol(str, (char **)&str, 10);
		if (*str == ';')
			++str;
	}

	*attr = rgb_to_console(rgb);

	return *(str - 1) == ';' ? str - 1 : str;
}

/* 8-bit colour */
static char *process_8bit(char *str, WORD *attr)
{
	int val = strtol(str, &str, 10);

	if (val < 16) {
		*attr = colour_1bit[val];
	}
	else if (val < 232) {
		int i, rgb[3];

		val -= 16;
		for (i = 2; i >= 0; --i) {
			rgb[i] = (val % 6) * 42 + 21;
			val /= 6;
		}

		*attr = rgb_to_console(rgb);
	}
	else if (val < 238) {
		/* black */
		*attr = 0;
	}
	else if (val < 244) {
		/* bright black */
		*attr = FOREGROUND_INTENSITY;
	}
	else if (val < 250) {
		/* white */
		*attr = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
	}
	else if (val < 256) {
		/* bright white */
		*attr = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE |
					FOREGROUND_INTENSITY;
	}

	return str;
}

static char *process_colour(char *str, WORD *attr)
{
	long val = strtol(str, (char **)&str, 10);

	*attr = 0xffff;	/* error return */
	switch (val) {
	case 2:
		str = process_24bit(str + 1, attr);
		break;
	case 5:
		str = process_8bit(str + 1, attr);
		break;
	default:
		break;
	}

	return str;
}

/* On input pos points to the start of a suspected escape sequence.
 * If a valid sequence is found return a pointer to the character
 * following it, otherwise return the original pointer. */
static char *process_escape(char *pos)
{
	char *str, *func;
	char *bel;
	size_t len;
	WORD t, attr = current_attr;
	static int reverse = 0;

	switch (pos[1]) {
	case '[':
		/* go ahead and process "\033[" sequence */
		break;
	case ']':
		if ((pos[2] == '0' || pos[2] == '2') && pos[3] == ';' &&
				(bel=strchr(pos+4, '\007')) && bel - pos < 260) {
			/* set console title */
			*bel++ = '\0';
			charToConA(pos+4);
			SetConsoleTitle(pos+4);
			return bel;
		}
		/* invalid "\033]" sequence, fall through */
	default:
		return pos;
	}

	str = pos + 2;
	len = strspn(str, "0123456789;");
	func = str + len;
	switch (*func) {
	case 'm':
		do {
			long val = strtol(str, (char **)&str, 10);
			switch (val) {
			case 0: /* reset */
				attr = plain_attr;
				reverse = 0;
				break;
			case 1: /* bold */
				attr |= FOREGROUND_INTENSITY;
				break;
			case 2:  /* faint */
			case 22: /* normal */
				attr &= ~FOREGROUND_INTENSITY;
				break;
			case 3:  /* italic */
				/* Unsupported */
				break;
			case 4:  /* underline */
			case 21: /* double underline */
				/* Wikipedia says this flag does nothing */
				/* Furthermore, mingw doesn't define this flag
				attr |= COMMON_LVB_UNDERSCORE; */
				break;
			case 24: /* no underline */
				/* attr &= ~COMMON_LVB_UNDERSCORE; */
				break;
			case 5:  /* slow blink */
			case 6:  /* fast blink */
				/* We don't have blink, but we do have
				   background intensity */
				attr |= BACKGROUND_INTENSITY;
				break;
			case 25: /* no blink */
				attr &= ~BACKGROUND_INTENSITY;
				break;
			case 7:  /* reverse video on */
				reverse = 1;
				break;
			case 27: /* reverse video off */
				reverse = 0;
				break;
			case 8:  /* conceal */
			case 9:  /* strike through */
			case 28: /* reveal */
				/* Unsupported */
				break;

			/* Foreground colours */
			case 30: /* Black */
			case 31: /* Red */
			case 32: /* Green */
			case 33: /* Yellow */
			case 34: /* Blue */
			case 35: /* Magenta */
			case 36: /* Cyan */
			case 37: /* White */
				attr &= ~FOREGROUND_ALL;
				attr |= colour_1bit[val - 30];
				break;
			case 38: /* 8/24 bit */
				str = process_colour(str + 1, &t);
				if (t != 0xffff) {
					attr &= ~(FOREGROUND_ALL|FOREGROUND_INTENSITY);
					attr |= t;
				}
				break;
			case 39: /* reset */
				attr &= ~FOREGROUND_ALL;
				attr |= (plain_attr & FOREGROUND_ALL);
				break;

			/* Background colours */
			case 40: /* Black */
			case 41: /* Red */
			case 42: /* Green */
			case 43: /* Yellow */
			case 44: /* Blue */
			case 45: /* Magenta */
			case 46: /* Cyan */
			case 47: /* White */
				attr &= ~BACKGROUND_ALL;
				attr |= colour_1bit[val - 40] << 4;
				break;
			case 48: /* 8/24 bit */
				str = process_colour(str + 1, &t);
				if (t != 0xffff) {
					attr &= ~(BACKGROUND_ALL|BACKGROUND_INTENSITY);
					attr |= t << 4;
				}
				break;
			case 49: /* reset */
				attr &= ~BACKGROUND_ALL;
				attr |= (plain_attr & BACKGROUND_ALL);
				break;

			default:
				/* Unsupported code */
				return pos;
			}
			str++;
		} while (str < func);

		current_attr = attr;
		if (reverse)
			attr = ((attr >> 4) & 0xf) | ((attr << 4) & 0xf0);
		SetConsoleTextAttribute(get_console(), attr);
		break;
	case 'A': /* up */
		move_cursor_row(-strtol(str, (char **)&str, 10));
		break;
	case 'B': /* down */
		move_cursor_row(strtol(str, (char **)&str, 10));
		break;
	case 'C': /* forward */
		move_cursor_column(strtol(str, (char **)&str, 10));
		break;
	case 'D': /* back */
		move_cursor_column(-strtol(str, (char **)&str, 10));
		break;
	case 'H':
		if (!len)
			move_cursor(0, 0);
		else {
			int row, col = 1;

			row = strtol(str, (char **)&str, 10);
			if (*str == ';') {
				col = strtol(str+1, (char **)&str, 10);
			}
			move_cursor(col > 0 ? col-1 : 0, row > 0 ? row-1 : 0);
		}
		break;
	case 'J':
		erase_till_end_of_screen();
		break;
	case 'K':
		erase_in_line();
		break;
	case '?':
		if (strncmp(str+1, "1049", 4) == 0 &&
				(str[5] == 'h' || str[5] == 'l') ) {
			use_alt_buffer(str[5] == 'h');
			func = str + 5;
			break;
		}
		/* fall through */
	default:
		/* Unsupported code */
		return pos;
	}

	return (char *)func + 1;
}

static BOOL charToConBuffA(LPSTR s, DWORD len)
{
	UINT acp = GetACP(), conocp = GetConsoleOutputCP();
	CPINFO acp_info, con_info;
	WCHAR *buf;

	if (acp == conocp)
		return TRUE;

	if (!s || !GetCPInfo(acp, &acp_info) || !GetCPInfo(conocp, &con_info) ||
			con_info.MaxCharSize > acp_info.MaxCharSize ||
			(len == 1 && acp_info.MaxCharSize != 1))
		return FALSE;

	terminal_mode(FALSE);
	buf = xmalloc(len*sizeof(WCHAR));
	MultiByteToWideChar(CP_ACP, 0, s, len, buf, len);
	WideCharToMultiByte(conocp, 0, buf, len, s, len, NULL, NULL);
	free(buf);
	return TRUE;
}

static BOOL charToConA(LPSTR s)
{
	if (!s)
		return FALSE;
	return charToConBuffA(s, strlen(s)+1);
}

BOOL conToCharBuffA(LPSTR s, DWORD len)
{
	UINT acp = GetACP(), conicp = GetConsoleCP();
	CPINFO acp_info, con_info;
	WCHAR *buf;

	if (acp == conicp
#if ENABLE_FEATURE_UTF8_INPUT
			// if acp is UTF8 then we got UTF8 via readConsoleInput_utf8
			|| acp == CP_UTF8
#endif
		)
		return TRUE;

	if (!s || !GetCPInfo(acp, &acp_info) || !GetCPInfo(conicp, &con_info) ||
			acp_info.MaxCharSize > con_info.MaxCharSize ||
			(len == 1 && con_info.MaxCharSize != 1))
		return FALSE;

	terminal_mode(FALSE);
	buf = xmalloc(len*sizeof(WCHAR));
	MultiByteToWideChar(conicp, 0, s, len, buf, len);
	WideCharToMultiByte(CP_ACP, 0, buf, len, s, len, NULL, NULL);
	free(buf);
	return TRUE;
}

static int ansi_emulate(const char *s, FILE *stream)
{
	int rv = 0;
	const unsigned char *t;
	char *pos, *str;
	size_t cur_len;
	static size_t max_len = 0;
	static char *mem = NULL;

	/* if no special treatment is required output the string as-is */
	for ( t=(unsigned char *)s; *t; ++t ) {
		if ( *t == '\033' || *t > 0x7f ) {
			break;
		}
	}

	if ( *t == '\0' ) {
		return fputs(s, stream) == EOF ? EOF : strlen(s);
	}

	/*
	 * Make a writable copy of the string and retain array for reuse.
	 * The test above guarantees that the string length won't be zero
	 * so the array will always be allocated.
	 */
	cur_len = strlen(s);
	if ( cur_len > max_len ) {
		free(mem);
		mem = xstrdup(s);
		max_len = cur_len;
	}
	else {
		strcpy(mem, s);
	}
	pos = str = mem;

	while (*pos) {
		pos = strchr(str, '\033');
		if (pos && !(terminal_mode(FALSE) & VT_OUTPUT)) {
			size_t len = pos - str;

			if (len) {
				if (conv_fwriteCon(stream, str, len) == EOF)
					return EOF;
				rv += len;
			}

			if (fflush(stream) == EOF)
				return EOF;

			str = process_escape(pos);
			if (str == pos) {
				if (fputc('\033', stream) == EOF)
					return EOF;
				++str;
			}
			rv += str - pos;
			pos = str;

			if (fflush(stream) == EOF)
				return EOF;

		} else {
			size_t len = strlen(str);
			rv += len;
			return conv_fwriteCon(stream, str, len) == EOF ? EOF : rv;
		}
	}
	return rv;
}

int winansi_putchar(int c)
{
	return winansi_fputc(c, stdout);
}

int winansi_puts(const char *s)
{
	return (winansi_fputs(s, stdout) == EOF || putchar('\n') == EOF) ? EOF : 0;
}

static sighandler_t sigpipe_handler = SIG_DFL;

#undef signal
sighandler_t winansi_signal(int signum, sighandler_t handler)
{
	sighandler_t old;

	if (signum == SIGPIPE) {
		old = sigpipe_handler;
		sigpipe_handler = handler;
		return old;
	}
	return signal(signum, handler);
}

static void check_pipe_fd(int fd)
{
	int error = GetLastError();

	if ((error == ERROR_NO_DATA &&
			GetFileType((HANDLE)_get_osfhandle(fd)) == FILE_TYPE_PIPE) ||
			error == ERROR_BROKEN_PIPE) {
		if (sigpipe_handler == SIG_DFL)
			exit(128+SIGPIPE);
		else /* SIG_IGN */
			errno = EPIPE;
	}
}

static void check_pipe(FILE *stream)
{
	int fd = fileno(stream);

	if (fd != -1 && ferror(stream)) {
		check_pipe_fd(fd);
	}
}

size_t winansi_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
	size_t lsize, lmemb, ret;
	char *str;
	int rv;

	lsize = MIN(size, nmemb);
	lmemb = MAX(size, nmemb);
	if (lsize != 1 || !is_console(fileno(stream))) {
		SetLastError(0);
		if ((ret=fwrite(ptr, size, nmemb, stream)) < nmemb)
			check_pipe(stream);
		return ret;
	}

	str = xmalloc(lmemb+1);
	memcpy(str, ptr, lmemb);
	str[lmemb] = '\0';

	rv = ansi_emulate(str, stream);
	free(str);

	return rv == EOF ? 0 : nmemb;
}

int winansi_fputs(const char *str, FILE *stream)
{
	int ret;

	if (!is_console(fileno(stream))) {
		SetLastError(0);
		if ((ret=fputs(str, stream)) == EOF)
			check_pipe(stream);
		return ret;
	}

	return ansi_emulate(str, stream) == EOF ? EOF : 0;
}

int winansi_fputc(int c, FILE *stream)
{
	int ret;
	char t = c;
	char *s = &t;

	if ((unsigned char)c <= 0x7f || !is_console(fileno(stream))) {
		SetLastError(0);
		if ((ret=fputc(c, stream)) == EOF)
			check_pipe(stream);
		return ret;
	}

	return conv_fwriteCon(stream, s, 1) == EOF ? EOF : (unsigned char )c;
}

#if !defined(__USE_MINGW_ANSI_STDIO) || !__USE_MINGW_ANSI_STDIO
/*
 * Prior to Windows 10 vsnprintf was incompatible with the C99 standard.
 * Implement a replacement using _vsnprintf.
 */
int winansi_vsnprintf(char *buf, size_t size, const char *format, va_list list)
{
	size_t len;
	va_list list2;

	va_copy(list2, list);
	len = _vsnprintf(NULL, 0, format, list2);
	va_end(list2);
	if (len < 0)
		return -1;

	_vsnprintf(buf, size, format, list);
	buf[size-1] = '\0';
	return len;
}
#endif

int winansi_vfprintf(FILE *stream, const char *format, va_list list)
{
	int len, rv;
	char small_buf[256];
	char *buf = small_buf;
	va_list cp;

	if (!is_console(fileno(stream)))
		goto abort;

	va_copy(cp, list);
	len = vsnprintf(small_buf, sizeof(small_buf), format, cp);
	va_end(cp);

	if (len > sizeof(small_buf) - 1) {
		buf = xmalloc(len + 1);
		va_copy(cp, list);
		len = vsnprintf(buf, len + 1, format, cp);
		va_end(cp);
	}

	if (len == -1)
		goto abort;

	rv = ansi_emulate(buf, stream);

	if (buf != small_buf)
		free(buf);
	return rv;

abort:
	SetLastError(0);
	if ((rv=vfprintf(stream, format, list)) == EOF || ferror(stream) != 0)
		check_pipe(stream);
	return rv;
}

int winansi_fprintf(FILE *stream, const char *format, ...)
{
	va_list list;
	int rv;

	va_start(list, format);
	rv = winansi_vfprintf(stream, format, list);
	va_end(list);

	return rv;
}

int winansi_printf(const char *format, ...)
{
	va_list list;
	int rv;

	va_start(list, format);
	rv = winansi_vfprintf(stdout, format, list);
	va_end(list);

	return rv;
}

static int ansi_emulate_write(int fd, const void *buf, size_t count)
{
	int rv = 0, i;
	int special = FALSE, has_null = FALSE;
	const unsigned char *s = (const unsigned char *)buf;
	char *pos, *str;
	size_t len, out_len;
	static size_t max_len = 0;
	static char *mem = NULL;

	for ( i=0; i<count; ++i ) {
		if ( s[i] == '\033' || s[i] > 0x7f ) {
			special = TRUE;
		}
		else if ( !s[i] ) {
			has_null = TRUE;
		}
	}

	/*
	 * If no special treatment is required or the data contains NUL
	 * characters output the string as-is.
	 */
	if ( !special || has_null ) {
		return write(fd, buf, count);
	}

	/* make a writable copy of the data and retain array for reuse */
	if ( count > max_len ) {
		free(mem);
		mem = malloc(count+1);
		max_len = count;
	}
	memcpy(mem, buf, count);
	mem[count] = '\0';
	pos = str = mem;

	/* we've checked the data doesn't contain any NULs */
	while (*pos) {
		pos = strchr(str, '\033');
		if (pos && !(terminal_mode(FALSE) & VT_OUTPUT)) {
			len = pos - str;

			if (len) {
				out_len = conv_writeCon(fd, str, len);
				if (out_len == -1)
					return -1;
				rv += out_len;
			}

			str = process_escape(pos);
			if (str == pos) {
				if (write(fd, pos, 1) == -1)
					return -1;
				++str;
			}
			rv += str - pos;
			pos = str;
		} else {
			len = strlen(str);
			out_len = conv_writeCon(fd, str, len);
			return (out_len == -1) ? -1 : rv+out_len;
		}
	}
	return rv;
}

int winansi_write(int fd, const void *buf, size_t count)
{
	if (!is_console(fd)) {
		int ret;

		SetLastError(0);
		if ((ret=write(fd, buf, count)) == -1) {
			check_pipe_fd(fd);
		}
		return ret;
	}

	return ansi_emulate_write(fd, buf, count);
}

int winansi_read(int fd, void *buf, size_t count)
{
	int rv;

	rv = mingw_read(fd, buf, count);
	if (!is_console_in(fd))
		return rv;

	if ( rv > 0 ) {
		conToCharBuffA(buf, rv);
	}

	return rv;
}

size_t winansi_fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
	int rv;

	rv = fread(ptr, size, nmemb, stream);
	if (!is_console_in(fileno(stream)))
		return rv;

	if (rv > 0)
		conToCharBuffA(ptr, rv * size);

	return rv;
}

int winansi_getc(FILE *stream)
{
	int rv;

	rv = _getc_nolock(stream);
	if (!is_console_in(fileno(stream)))
		return rv;

	if ( rv != EOF ) {
		unsigned char c = (unsigned char)rv;
		char *s = (char *)&c;
		conToCharBuffA(s, 1);
		rv = (int)c;
	}

	return rv;
}

int winansi_getchar(void)
{
	return winansi_getc(stdin);
}

char *winansi_fgets(char *s, int size, FILE *stream)
{
	char *rv;

	rv = fgets(s, size, stream);
	if (!is_console_in(fileno(stream)))
		return rv;

	if (rv)
		conToCharBuffA(s, strlen(s));

	return rv;
}

/* Ensure that isatty(fd) returns 0 for the NUL device */
int mingw_isatty(int fd)
{
	int result = _isatty(fd) != 0;

	if (result) {
		HANDLE handle = (HANDLE) _get_osfhandle(fd);
		DWORD mode;

		if (handle == INVALID_HANDLE_VALUE)
			return 0;

		/* check if its a device (i.e. console, printer, serial port) */
		if (GetFileType(handle) != FILE_TYPE_CHAR)
			return 0;

		if (!GetConsoleMode(handle, &mode))
			return 0;
	}

	return result;
}

#if ENABLE_FEATURE_UTF8_INPUT
// intentionally also converts invalid values (surrogate halfs, too big)
static int toutf8(DWORD cp, unsigned char *buf) {
	if (cp <= 0x7f) {
		*buf = cp;
		return 1;
	}
	if (cp <= 0x7ff) {
		*buf++ = 0xc0 |  (cp >>  6);
		*buf   = 0x80 |  (cp        & 0x3f);
		return 2;
	}
	if (cp <= 0xffff) {
		*buf++ = 0xe0 |  (cp >> 12);
		*buf++ = 0x80 | ((cp >>  6) & 0x3f);
		*buf   = 0x80 |  (cp        & 0x3f);
		return 3;
	}
	if (cp <= 0x10ffff) {
		*buf++ = 0xf0 |  (cp >> 18);
		*buf++ = 0x80 | ((cp >> 12) & 0x3f);
		*buf++ = 0x80 | ((cp >>  6) & 0x3f);
		*buf   = 0x80 |  (cp        & 0x3f);
		return 4;
	}
	// invalid. returning 0 works in our context because it's delivered
	// as a key event, where 0 values are typically ignored by the caller
	*buf = 0;
	return 1;
}

// peek into the console input queue and try to find a key-up event of
// a surrugate-2nd-half, at which case eat the console events up to this
// one (excluding), and combine the pair values into *ph1
static void maybeEatUpto2ndHalfUp(HANDLE h, DWORD *ph1)
{
	// Peek into the queue arbitrary 16 records deep
	INPUT_RECORD r[16];
	DWORD got;
	int i;

	if (!PeekConsoleInputW(h, r, 16, &got))
		return;

	// we're conservative, and abort the search on anything which
	// seems out of place, like non-key event, non-2nd-half, etc.
	// search from 1 because i==0 is still the 1st half down record.
	for (i = 1; i < got; ++i) {
		DWORD h2;
		int is2nd, isdown;

		if (r[i].EventType != KEY_EVENT)
			return;

		isdown = r[i].Event.KeyEvent.bKeyDown;
		h2 = r[i].Event.KeyEvent.uChar.UnicodeChar;
		is2nd = h2 >= 0xDC00 && h2 <= 0xDFFF;

		// skip 0 values, keyup of 1st half, and keydown of a 2nd half, if any
		if (!h2 || (h2 == *ph1 && !isdown) || (is2nd && isdown))
			continue;

		if (!is2nd)
			return;

		// got 2nd-half-up. eat the events up to this, combine the values
		ReadConsoleInputW(h, r, i, &got);
		*ph1 = 0x10000 + (((*ph1 & ~0xD800) << 10) | (h2 & ~0xDC00));
		return;
	}
}

// if the codepoint is a key-down event, remember it, else if
// it's a key-up event with matching prior down - forget the down,
// else (up without matching prior key-down) - change it to down.
// We remember few prior key-down events so that a sequence
// like X-down Y-down X-up Y-up won't trigger this hack for Y-up.
// When up is changed into down there won't be further key-up event,
// but that's OK because the caller ignores key-up events anyway.
static void maybe_change_up_to_down(wchar_t key, BOOL *isdown)
{
	#define DOWN_BUF_SIZ 8
	static wchar_t downbuf[DOWN_BUF_SIZ] = {0};
	static int pos = 0;

	if (*isdown) {
		downbuf[pos++] = key;
		pos = pos % DOWN_BUF_SIZ;
		return;
	}

	// the missing-key-down issue was only observed with unicode values,
	// so limit this hack to non-ASCII-7 values.
	// also, launching a new shell/read process from CLI captures
	// an ENTER-up event without prior down at this new process, which
	// would otherwise change it to down - creating a wrong ENTER keypress.
	if (key <= 127)
		return;

	// key up, try to match a prior down
	for (int i = 0; i < DOWN_BUF_SIZ; ++i) {
		if (downbuf[i] == key) {
			downbuf[i] = 0;  // "forget" this down
			return;
		}
	}

	// no prior key-down - replace the up with down
	*isdown = TRUE;
}

/*
 * readConsoleInput_utf8 behaves similar enough to ReadConsoleInputA when
 * the console (input) CP is UTF8, but addressed two issues:
 * - It depend on the console CP, while we use ReadConsoleInputW internally.
 * - ReadConsoleInputA with Console CP of UTF8 (65001) is buggy:
 *   - Doesn't work on Windows 7 (reads 0 or '?' for non-ASCII codepoints).
 *   - When used at the cmd.exe console - but not Windows Terminal:
 *     sometimes only key-up events arrive without the expected prior key-down.
 *     Seems to depend both on the console CP and the entered/pasted codepoint.
 *   - If reading one record at a time (which is how we use it), then input
 *     codepoints of U+0800 or higher crash the console/terminal window.
 *     (tested on Windows 10.0.19045.3086: console and Windows Terminal 1.17)
 *     Example: U+0C80 (UTF8: 0xE0 0xB2 0x80): "ಀ"
 *     Example: U+1F600 (UTF8: 0xF0 0x9F 0x98 0x80): "😀"
 *   - If reading more than one record at a time:
 *     - Unknown whether it can still crash in some cases (was not observed).
 *     - Codepoints above U+FFFF are broken, and arrive as
 *       U+FFFD REPLACEMENT CHARACTER "�"
 * - Few more codepoints to test the issues above (and below):
 *   - U+0500 (UTF8: 0xD4, 0x80): "Ԁ"  (OK in UTF8 CP, else maybe no key-down)
 *   - U+07C0 (UTF8: 0xDF, 0x80): "߀"  (might exhibit missing key-down)
 *
 * So this function uses ReadConsoleInputW and then delivers it as UTF8:
 * - Works with any console CP, in Windows terminal and Windows 7/10 console.
 * - Surrogate pairs are combined and delivered as a single UTF8 codepoint.
 *   - Ignore occasional intermediate control events between the halfs.
 *   - If we can't find the 2nd half, or if for some reason we get a 2nd half
 *     wiithout the 1st, deliver the half we got as UTF8 (a-la WTF8).
 * - The "sometimes key-down is missing" issue at the cmd.exe console happens
 *   also when using ReadConsoleInputW (for U+0080 or higher), so handle it.
 *   This can also happen with surrogate pairs.
 * - Up to 4-bytes state is maintained for a single UTF8 codepoint buffer.
 *
 * Gotchas (could be solved, but currently there's no need):
 * - We support reading one record at a time, else fail - to make it obvious.
 * - We have a state which is hidden from PeekConsoleInput - so not in sync.
 * - We don't deliver key-up events in some cases: when working around
 *   the "missing key-down" issue, and with combined surrogate halfs value.
 */
BOOL readConsoleInput_utf8(HANDLE h, INPUT_RECORD *r, DWORD len, DWORD *got)
{
	static unsigned char u8buf[4];  // any single codepoint in UTF8
	static int u8pos = 0, u8len = 0;
	static INPUT_RECORD srec;

	if (len != 1)
		return FALSE;

	// if ACP is UTF8 then we read UTF8 regardless of console (in) CP
	if (GetConsoleCP() != CP_UTF8 && GetACP() != CP_UTF8)
		return ReadConsoleInput(h, r, len, got);

	if (u8pos == u8len) {
		DWORD codepoint;

		// wait-and-peek rather than read to keep the last processed record
		// at the console queue until we deliver all of its products, so
		// that external WaitForSingleObject(h) shows there's data ready.
		if (WaitForSingleObject(h, INFINITE) != WAIT_OBJECT_0)
			return FALSE;
		if (!PeekConsoleInputW(h, r, 1, got))
			return FALSE;
		if (*got == 0)
			return TRUE;
		if (r->EventType != KEY_EVENT)
			return ReadConsoleInput(h, r, 1, got);

		srec = *r;
		codepoint = srec.Event.KeyEvent.uChar.UnicodeChar;

		// Observed when pasting unicode at cmd.exe console (but not
		// windows terminal), we sometimes get key-up event without
		// a prior matching key-down (or with key-down codepoint 0),
		// so this call would change the up into down in such case.
		// E.g. pastes fixed by this hack: U+1F600 "😀", or U+0C80 "ಀ"
		if (codepoint)
			maybe_change_up_to_down(codepoint, &srec.Event.KeyEvent.bKeyDown);

		// if it's a 1st (high) surrogate pair half, try to eat upto and
		// excluding the 2nd (low) half, and combine them into codepoint.
		// this does not interfere with the missing-key-down workaround
		// (no issue if the down-buffer has 1st-half-down without up).
		if (codepoint >= 0xD800 && codepoint <= 0xDBFF)
			maybeEatUpto2ndHalfUp(h, &codepoint);

		u8len = toutf8(codepoint, u8buf);
		u8pos = 0;
	}

	*r = srec;
	r->Event.KeyEvent.uChar.AsciiChar = (char)u8buf[u8pos++];
	if (u8pos == u8len)  // consume the record which generated this buffer
		ReadConsoleInputW(h, &srec, 1, got);
	*got = 1;
	return TRUE;
}
#else
/*
 * In Windows 10 and 11 using ReadConsoleInputA() with a console input
 * code page of CP_UTF8 can crash the console/terminal.  Avoid this by
 * using ReadConsoleInputW() in that case.
 */
BOOL readConsoleInput_utf8(HANDLE h, INPUT_RECORD *r, DWORD len, DWORD *got)
{
	if (GetConsoleCP() != CP_UTF8)
		return ReadConsoleInput(h, r, len, got);

	if (ReadConsoleInputW(h, r, len, got)) {
		wchar_t uchar = r->Event.KeyEvent.uChar.UnicodeChar;
		char achar = uchar & 0x7f;
		if (achar != uchar)
			achar = '?';
		r->Event.KeyEvent.uChar.AsciiChar = achar;
		return TRUE;
	}
	return FALSE;
}
#endif

#if ENABLE_FEATURE_UTF8_OUTPUT
// Write u8buf as if the console output CP is UTF8 - regardless of the CP.
// fd should be associated with a console output.
// Return: 0 on successful write[s], else -1 (e.g. if fd is not a console).
//
// Up to 3 bytes of an incomplete codepoint may be buffered from prior call[s].
// All the completed codepoints in one call are written using WriteConsoleW.
// Bad sequence of any length (till ASCII7 or UTF8 lead) prints 1 subst wchar.
//
// note: one console is assumed, and the (3 bytes) buffer is shared regardless
//       of the original output stream (stdout/err), or even if the handle is
//       of a different console. This can result in invalid codepoints output
//       if streams are multiplexed mid-codepoint (same as elsewhere?)
static int writeCon_utf8(int fd, const char *u8buf, size_t u8siz)
{
	static int state = 0;  // -1: bad, 0-3: remaining cp bytes (0: done/new)
	static uint32_t codepoint = 0;  // accumulated from up to 4 UTF8 bytes

	// not a state, only avoids re-alloc on every call
	static const int wbufwsiz = 4096;
	static wchar_t *wbuf = 0;

	HANDLE h = (HANDLE)_get_osfhandle(fd);
	int wlen = 0;

	if (!wbuf)
		wbuf = xmalloc(wbufwsiz * sizeof(wchar_t));

	// ASCII7 uses least logic, then UTF8 continuations, UTF8 lead, errors
	while (u8siz--) {
		unsigned char c = *u8buf++;
		int topbits = 0;

		while (c & (0x80 >> topbits))
			++topbits;

		if (state == 0 && topbits == 0) {
			// valid ASCII7, state remains 0
			codepoint = c;

		} else if (state > 0 && topbits == 1) {
			// valid continuation byte
			codepoint = (codepoint << 6) | (c & 0x3f);
			if (--state)
				continue;

		} else if (state == 0 && topbits >= 2 && topbits <= 4) {
			// valid UTF8 lead of 2/3/4 bytes codepoint
			codepoint = c & (0x7f >> topbits);
			state = topbits - 1;  // remaining bytes after lead
			continue;

		} else {
			// already bad (state<0), or unexpected c at state 0-3.
			// placeholder is added only at the 1st (state>=0).
			// regardless, c may be valid to reprocess as state 0
			// (even when it's the 1st unexpected in state 1/2/3)
			int placeholder_done = state < 0;

			if (topbits < 5 && topbits != 1) {
				--u8buf;  // valid for state 0, reprocess
				++u8siz;
				state = 0;
			} else {
				state = -1;  // set/keep bad state
			}

			if (placeholder_done)
				continue;

			// 1st unexpected char, add placeholder
			codepoint = CONFIG_SUBST_WCHAR;
		}

		// codepoint is complete
		// we don't reject surrogate halves, reserved, etc
		if (codepoint < 0x10000) {
			wbuf[wlen++] = codepoint;
		} else {
			// generate a surrogates pair (wbuf has room for 2+)
			codepoint -= 0x10000;
			wbuf[wlen++] = 0xd800 | (codepoint >> 10);
			wbuf[wlen++] = 0xdc00 | (codepoint & 0x3ff);
		}

		// flush if we have less than two empty spaces
		if (wlen > wbufwsiz - 2) {
			if (!WriteConsoleW(h, wbuf, wlen, 0, 0))
				return -1;
			wlen = 0;
		}
	}

	if (wlen && !WriteConsoleW(h, wbuf, wlen, 0, 0))
		return -1;
	return 0;
}
#endif

void console_write(const char *str, int len)
{
	char *buf = xmemdup(str, len);
	int fd = _open("CONOUT$", _O_WRONLY);
	conv_writeCon(fd, buf, len);
	close(fd);
	free(buf);
}

// LC_ALL=C disables console output conversion, so that the source
// data is interpreted only by the console according to its output CP.
static int conout_conv_enabled(void)
{
	static int enabled, tested;  /* = 0 */

	if (!tested) {
		// keep in sync with [re]init_unicode at libbb/unicode.c
		char *s = getenv("LC_ALL");
		if (!s) s = getenv("LC_CTYPE");
		if (!s) s = getenv("LANG");

		enabled = !(s && s[0] == 'C' && s[1] == 0);
		tested = 1;
	}

	return enabled;
}

// TODO: improvements:
//
// 1. currently conv_[f]writeCon modify buf inplace, which means the caller
// typically has to make a writable copy first just for this.
// Sometimes it allocates a big copy once, and calls us with substrings.
// Instead, we could make a writable copy here - it's not used later anyway.
// To avoid the performance hit of many small allocations, we could use
// a local buffer for short strings, and allocate only if it doesn't fit
// (or maybe just reuse the local buffer with substring iterations).
//
// 2. Instead of converting from ACP to the console out CP - which guarantees
// potential data-loss if they differ, we could convert it to wchar_t and
// write it using WriteConsoleW. This should prevent all output data-loss.
// care should be taken with DBCS codepages (e.g. 936) or other multi-byte
// because then converting on arbitrary substring boundaries can fail.

// convert buf inplace from ACP to console out CP and write it to stream
// returns EOF on error, 0 on success
static int conv_fwriteCon(FILE *stream, char *buf, size_t siz)
{
	if (conout_conv_enabled()) {
#if ENABLE_FEATURE_UTF8_OUTPUT
		if (GetConsoleOutputCP() != CP_UTF8) {
			fflush(stream);  // writeCon_utf8 is unbuffered
			return writeCon_utf8(fileno(stream), buf, siz) ? EOF : 0;
		}
#else
		charToConBuffA(buf, siz);
#endif
	}
	return fwrite(buf, 1, siz, stream) < siz ? EOF : 0;
}

// similar to above, but using lower level write
// returns -1 on error, actually-written bytes on suceess
static int conv_writeCon(int fd, char *buf, size_t siz)
{
	if (conout_conv_enabled()) {
#if ENABLE_FEATURE_UTF8_OUTPUT
		if (GetConsoleOutputCP() != CP_UTF8)
			return writeCon_utf8(fd, buf, siz) ? -1 : siz;
#else
		charToConBuffA(buf, siz);
#endif
	}
	return write(fd, buf, siz);
}
