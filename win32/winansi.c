/*
 * Copyright 2008 Peter Harris <git@peter.is-a-geek.org>
 */

#include "libbb.h"
#include <windows.h>
#undef PACKED

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
#undef getc

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

static int is_console_in(int fd)
{
	return isatty(fd) && GetStdHandle(STD_INPUT_HANDLE) != INVALID_HANDLE_VALUE;
}

#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

#ifndef DISABLE_NEWLINE_AUTO_RETURN
#define DISABLE_NEWLINE_AUTO_RETURN 0x0008
#endif

int skip_ansi_emulation(int reset)
{
	static int skip = -1;

	if (skip < 0 || reset) {
		const char *var = getenv(bb_skip_ansi_emulation);
		skip = var == NULL ? CONFIG_SKIP_ANSI_EMULATION_DEFAULT : atoi(var);
		if (skip < 0 || skip > 2)
			skip = 0;

		if (is_console(STDOUT_FILENO)) {
			HANDLE h = get_console();
			DWORD mode;

			if (GetConsoleMode(h, &mode)) {
				if (skip)
					mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
				else
					mode &= ~ENABLE_VIRTUAL_TERMINAL_PROCESSING;
				mode &= ~DISABLE_NEWLINE_AUTO_RETURN;
				if (!SetConsoleMode(h, mode) && skip == 2)
					skip = 0;
			}
		}
	}

	return skip;
}

void set_title(const char *str)
{
	if (is_console(STDOUT_FILENO))
		SetConsoleTitle(str);
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
	const char *var;
	HANDLE console, h;

	var = getenv("BB_ALT_BUFFER");
	if (var && strcmp(var, "0") == 0) {
		reset_screen();
		return;
	}

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
	int count = 0;
	int rgb[3] = {0, 0, 0};

	do {
		rgb[count++] = strtol(str, (char **)&str, 10);
		++str;
	} while (*(str-1) == ';' && count < 3);

	*attr = rgb_to_console(rgb);

	return str;
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

	*attr = -1;	/* error return */
	switch (val) {
	case 2:
		str = process_24bit(++str, attr);
		break;
	case 5:
		str = process_8bit(++str, attr);
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
			CharToOem(pos+4, pos+4);
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
				str = process_colour(++str, &t);
				if (t != -1) {
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
				str = process_colour(++str, &t);
				if (t != -1) {
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
		} while (*(str-1) == ';');

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

#if ENABLE_FEATURE_EURO
void init_codepage(void)
{
	if (GetConsoleCP() == 850 && GetConsoleOutputCP() == 850) {
		SetConsoleCP(858);
		SetConsoleOutputCP(858);
	}
}

static BOOL winansi_CharToOemBuff(LPCSTR s, LPSTR d, DWORD len)
{
	WCHAR *buf;
	int i;

	if (!s || !d)
		return FALSE;

	buf = xmalloc(len*sizeof(WCHAR));
	MultiByteToWideChar(CP_ACP, 0, s, len, buf, len);
	WideCharToMultiByte(CP_OEMCP, 0, buf, len, d, len, NULL, NULL);
	if (GetConsoleOutputCP() == 858) {
		for (i=0; i<len; ++i) {
			if (buf[i] == 0x20ac) {
				d[i] = 0xd5;
			}
		}
	}
	free(buf);
	return TRUE;
}

static BOOL winansi_CharToOem(LPCSTR s, LPSTR d)
{
	if (!s || !d)
		return FALSE;
	return winansi_CharToOemBuff(s, d, strlen(s)+1);
}

static BOOL winansi_OemToCharBuff(LPCSTR s, LPSTR d, DWORD len)
{
	WCHAR *buf;
	int i;

	if (!s || !d)
		return FALSE;

	buf = xmalloc(len*sizeof(WCHAR));
	MultiByteToWideChar(CP_OEMCP, 0, s, len, buf, len);
	WideCharToMultiByte(CP_ACP, 0, buf, len, d, len, NULL, NULL);
	if (GetConsoleOutputCP() == 858) {
		for (i=0; i<len; ++i) {
			if (buf[i] == 0x0131) {
				d[i] = 0x80;
			}
		}
	}
	free(buf);
	return TRUE;
}

# undef CharToOemBuff
# undef CharToOem
# undef OemToCharBuff
# define CharToOemBuff winansi_CharToOemBuff
# define CharToOem winansi_CharToOem
# define OemToCharBuff winansi_OemToCharBuff
#endif

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
		if (pos && !skip_ansi_emulation(FALSE)) {
			size_t len = pos - str;

			if (len) {
				*pos = '\0';	/* NB, '\033' has been overwritten */
				CharToOem(str, str);
				if (fputs(str, stream) == EOF)
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
			rv += strlen(str);
			CharToOem(str, str);
			return fputs(str, stream) == EOF ? EOF : rv;
		}
	}
	return rv;
}

int winansi_putchar(int c)
{
	char t = c;
	char *s = &t;

	if (!is_console(STDOUT_FILENO))
		return putchar(c);

	CharToOemBuff(s, s, 1);
	return putchar(t) == EOF ? EOF : (unsigned char)c;
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

	if (!is_console(fileno(stream))) {
		SetLastError(0);
		if ((ret=fputc(c, stream)) == EOF)
			check_pipe(stream);
		return ret;
	}

	CharToOemBuff(s, s, 1);
	return fputc(t, stream) == EOF ? EOF : (unsigned char )c;
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
		if (pos && !skip_ansi_emulation(FALSE)) {
			len = pos - str;

			if (len) {
				CharToOemBuff(str, str, len);
				out_len = write(fd, str, len);
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
			CharToOem(str, str);
			out_len = write(fd, str, len);
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
		OemToCharBuff(buf, buf, rv);
	}

	return rv;
}

int winansi_getc(FILE *stream)
{
	int rv;

	rv = getc(stream);
	if (!is_console_in(fileno(stream)))
		return rv;

	if ( rv != EOF ) {
		unsigned char c = (unsigned char)rv;
		char *s = (char *)&c;
		OemToCharBuff(s, s, 1);
		rv = (int)c;
	}

	return rv;
}

/* Ensure that isatty(fd) returns 0 for the NUL device */
int mingw_isatty(int fd)
{
	int result = _isatty(fd);

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
