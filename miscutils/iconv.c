/*
 * iconv implementation using Win32 API to convert.
 *
 * This file is placed in the public domain.
 */

/*
 * This code was obtained from:
 *
 *    https://github.com/win-iconv/win-iconv
 *
 * Modified for busybox-w32 by Ronald M Yorston.  These modifications
 * are also dedicated to the public domain.
 */

//config:config ICONV
//config:   bool "iconv (11.4 kb)"
//config:   default y
//config:   depends on PLATFORM_MINGW32
//config:   help
//config:   'iconv' converts text between character encodings.

//applet:IF_ICONV(APPLET(iconv, BB_DIR_USR_BIN, BB_SUID_DROP))

//kbuild:lib-$(CONFIG_ICONV) += iconv.o

//usage:#define iconv_trivial_usage
//usage:       "[-lc] [-o outfile] [-f from-enc] [-t to-enc] [FILE]..."
//usage:#define iconv_full_usage "\n\n"
//usage:       "Convert text between character encodings\n"
//usage:     "\n    -l  List all known character encodings"
//usage:     "\n    -c  Silently discard characters that cannot be converted"
//usage:     "\n    -o  Use outfile for output"
//usage:     "\n    -f  Use from-enc for input characters"
//usage:     "\n    -t  Use to-enc for output characters"

#include "libbb.h"

/* WORKAROUND: */
#define GetProcAddressA GetProcAddress

#define MB_CHAR_MAX 16

#define UNICODE_MODE_BOM_DONE   1
#define UNICODE_MODE_SWAPPED    2

#define FLAG_USE_BOM            1
#define FLAG_TRANSLIT           2 /* //TRANSLIT */
#define FLAG_IGNORE             4 /* //IGNORE */

typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;

typedef void* iconv_t;

static iconv_t iconv_open(const char *tocode, const char *fromcode);
static int iconv_close(iconv_t cd);
static size_t iconv(iconv_t cd, const char **inbuf, size_t *inbytesleft, char **outbuf, size_t *outbytesleft);

typedef struct compat_t compat_t;
typedef struct csconv_t csconv_t;
typedef struct rec_iconv_t rec_iconv_t;

typedef int (*f_mbtowc)(csconv_t *cv, const uchar *buf, int bufsize, ushort *wbuf, int *wbufsize);
typedef int (*f_wctomb)(csconv_t *cv, ushort *wbuf, int wbufsize, uchar *buf, int bufsize);
typedef int (*f_mblen)(csconv_t *cv, const uchar *buf, int bufsize);
typedef int (*f_flush)(csconv_t *cv, uchar *buf, int bufsize);

#define COMPAT_IN   1
#define COMPAT_OUT  2

/* unicode mapping for compatibility with other conversion table. */
struct compat_t {
	uint in;
	uint out;
	uint flag;
};

struct csconv_t {
	int codepage;
	int flags;
	f_mbtowc mbtowc;
	f_wctomb wctomb;
	f_mblen mblen;
	f_flush flush;
	DWORD mode;
	compat_t *compat;
};

struct rec_iconv_t {
	iconv_t cd;
	csconv_t from;
	csconv_t to;
};

static int load_mlang(void);
static int make_csconv(const char *name, csconv_t *cv);
static int name_to_codepage(const char *name);
static uint utf16_to_ucs4(const ushort *wbuf);
static void ucs4_to_utf16(uint wc, ushort *wbuf, int *wbufsize);
static int mbtowc_flags(int codepage);
static int must_use_null_useddefaultchar(int codepage);
static int seterror(int err);

static int sbcs_mblen(csconv_t *cv, const uchar *buf, int bufsize);
static int dbcs_mblen(csconv_t *cv, const uchar *buf, int bufsize);
static int mbcs_mblen(csconv_t *cv, const uchar *buf, int bufsize);
static int utf8_mblen(csconv_t *cv, const uchar *buf, int bufsize);
static int eucjp_mblen(csconv_t *cv, const uchar *buf, int bufsize);

static int kernel_mbtowc(csconv_t *cv, const uchar *buf, int bufsize, ushort *wbuf, int *wbufsize);
static int kernel_wctomb(csconv_t *cv, ushort *wbuf, int wbufsize, uchar *buf, int bufsize);
static int mlang_mbtowc(csconv_t *cv, const uchar *buf, int bufsize, ushort *wbuf, int *wbufsize);
static int mlang_wctomb(csconv_t *cv, ushort *wbuf, int wbufsize, uchar *buf, int bufsize);
static int utf16_mbtowc(csconv_t *cv, const uchar *buf, int bufsize, ushort *wbuf, int *wbufsize);
static int utf16_wctomb(csconv_t *cv, ushort *wbuf, int wbufsize, uchar *buf, int bufsize);
static int utf32_mbtowc(csconv_t *cv, const uchar *buf, int bufsize, ushort *wbuf, int *wbufsize);
static int utf32_wctomb(csconv_t *cv, ushort *wbuf, int wbufsize, uchar *buf, int bufsize);
static int iso2022jp_mbtowc(csconv_t *cv, const uchar *buf, int bufsize, ushort *wbuf, int *wbufsize);
static int iso2022jp_wctomb(csconv_t *cv, ushort *wbuf, int wbufsize, uchar *buf, int bufsize);
static int iso2022jp_flush(csconv_t *cv, uchar *buf, int bufsize);

#define CP_ALIAS_LIST \
	CP_ALIAS(65001, "CP65001") \
	CP_ALIAS(65001, "UTF8") \
	CP_ALIAS(65001, "UTF-8") \
\
	CP_ALIAS(1200, "CP1200") \
	CP_ALIAS(1200, "UTF16LE") \
	CP_ALIAS(1200, "UTF-16LE") \
	CP_ALIAS(1200, "UCS2LE") \
	CP_ALIAS(1200, "UCS-2LE") \
	CP_ALIAS(1200, "UCS-2-INTERNAL") \
\
	CP_ALIAS(1201, "CP1201") \
	CP_ALIAS(1201, "UTF16BE") \
	CP_ALIAS(1201, "UTF-16BE") \
	CP_ALIAS(1201, "UCS2BE") \
	CP_ALIAS(1201, "UCS-2BE") \
	CP_ALIAS(1201, "unicodeFFFE") \
\
	CP_ALIAS(12000, "CP12000") \
	CP_ALIAS(12000, "UTF32LE") \
	CP_ALIAS(12000, "UTF-32LE") \
	CP_ALIAS(12000, "UCS4LE") \
	CP_ALIAS(12000, "UCS-4LE") \
\
	CP_ALIAS(12001, "CP12001") \
	CP_ALIAS(12001, "UTF32BE") \
	CP_ALIAS(12001, "UTF-32BE") \
	CP_ALIAS(12001, "UCS4BE") \
	CP_ALIAS(12001, "UCS-4BE") \
\
	/* Default is little endian, because the platform is */ \
	CP_ALIAS(1200, "UTF16") \
	CP_ALIAS(1200, "UTF-16") \
	CP_ALIAS(1200, "UCS2") \
	CP_ALIAS(1200, "UCS-2") \
	CP_ALIAS(12000, "UTF32") \
	CP_ALIAS(12000, "UTF-32") \
	CP_ALIAS(12000, "UCS4") \
	CP_ALIAS(12000, "UCS-4") \
\
	/* copy from libiconv `iconv -l` */ \
	/* !IsValidCodePage(367) */ \
	CP_ALIAS(20127, "ANSI_X3.4-1968") \
	CP_ALIAS(20127, "ANSI_X3.4-1986") \
	CP_ALIAS(20127, "ASCII") \
	CP_ALIAS(20127, "CP367") \
	CP_ALIAS(20127, "IBM367") \
	CP_ALIAS(20127, "ISO-IR-6") \
	CP_ALIAS(20127, "ISO646-US") \
	CP_ALIAS(20127, "ISO_646.IRV:1991") \
	CP_ALIAS(20127, "US") \
	CP_ALIAS(20127, "US-ASCII") \
	CP_ALIAS(20127, "CSASCII") \
\
	/* !IsValidCodePage(819) */ \
	CP_ALIAS(1252, "CP819") \
	CP_ALIAS(1252, "IBM819") \
	CP_ALIAS(28591, "ISO-8859-1") \
	CP_ALIAS(28591, "ISO-IR-100") \
	CP_ALIAS(28591, "ISO8859-1") \
	CP_ALIAS(28591, "ISO_8859-1") \
	CP_ALIAS(28591, "ISO_8859-1:1987") \
	CP_ALIAS(28591, "L1") \
	CP_ALIAS(28591, "LATIN1") \
	CP_ALIAS(28591, "CSISOLATIN1") \
\
	CP_ALIAS(1250, "CP1250") \
	CP_ALIAS(1250, "MS-EE") \
	CP_ALIAS(1250, "WINDOWS-1250") \
\
	CP_ALIAS(1251, "CP1251") \
	CP_ALIAS(1251, "MS-CYRL") \
	CP_ALIAS(1251, "WINDOWS-1251") \
\
	CP_ALIAS(1252, "CP1252") \
	CP_ALIAS(1252, "MS-ANSI") \
	CP_ALIAS(1252, "WINDOWS-1252") \
\
	CP_ALIAS(1253, "CP1253") \
	CP_ALIAS(1253, "MS-GREEK") \
	CP_ALIAS(1253, "WINDOWS-1253") \
\
	CP_ALIAS(1254, "CP1254") \
	CP_ALIAS(1254, "MS-TURK") \
	CP_ALIAS(1254, "WINDOWS-1254") \
\
	CP_ALIAS(1255, "CP1255") \
	CP_ALIAS(1255, "MS-HEBR") \
	CP_ALIAS(1255, "WINDOWS-1255") \
\
	CP_ALIAS(1256, "CP1256") \
	CP_ALIAS(1256, "MS-ARAB") \
	CP_ALIAS(1256, "WINDOWS-1256") \
\
	CP_ALIAS(1257, "CP1257") \
	CP_ALIAS(1257, "WINBALTRIM") \
	CP_ALIAS(1257, "WINDOWS-1257") \
\
	CP_ALIAS(1258, "CP1258") \
	CP_ALIAS(1258, "WINDOWS-1258") \
\
	CP_ALIAS(850, "850") \
	CP_ALIAS(850, "CP850") \
	CP_ALIAS(850, "IBM850") \
	CP_ALIAS(850, "CSPC850MULTILINGUAL") \
\
	/* !IsValidCodePage(862) */ \
	CP_ALIAS(862, "862") \
	CP_ALIAS(862, "CP862") \
	CP_ALIAS(862, "IBM862") \
	CP_ALIAS(862, "CSPC862LATINHEBREW") \
\
	CP_ALIAS(866, "866") \
	CP_ALIAS(866, "CP866") \
	CP_ALIAS(866, "IBM866") \
	CP_ALIAS(866, "CSIBM866") \
\
	/* !IsValidCodePage(154) */ \
	CP_ALIAS(154, "CP154") \
	CP_ALIAS(154, "CYRILLIC-ASIAN") \
	CP_ALIAS(154, "PT154") \
	CP_ALIAS(154, "PTCP154") \
	CP_ALIAS(154, "CSPTCP154") \
\
	/* !IsValidCodePage(1133) */ \
	CP_ALIAS(1133, "CP1133") \
	CP_ALIAS(1133, "IBM-CP1133") \
\
	CP_ALIAS(874, "CP874") \
	CP_ALIAS(874, "WINDOWS-874") \
\
	/* !IsValidCodePage(51932) */ \
	CP_ALIAS(51932, "CP51932") \
	CP_ALIAS(51932, "MS51932") \
	CP_ALIAS(51932, "WINDOWS-51932") \
	CP_ALIAS(51932, "EUC-JP") \
\
	CP_ALIAS(932, "CP932") \
	CP_ALIAS(932, "MS932") \
	CP_ALIAS(932, "SHIFFT_JIS") \
	CP_ALIAS(932, "SHIFFT_JIS-MS") \
	CP_ALIAS(932, "SJIS") \
	CP_ALIAS(932, "SJIS-MS") \
	CP_ALIAS(932, "SJIS-OPEN") \
	CP_ALIAS(932, "SJIS-WIN") \
	CP_ALIAS(932, "WINDOWS-31J") \
	CP_ALIAS(932, "WINDOWS-932") \
	CP_ALIAS(932, "CSWINDOWS31J") \
\
	CP_ALIAS(50221, "CP50221") \
	CP_ALIAS(50221, "ISO-2022-JP") \
	CP_ALIAS(50221, "ISO-2022-JP-MS") \
	CP_ALIAS(50221, "ISO2022-JP") \
	CP_ALIAS(50221, "ISO2022-JP-MS") \
	CP_ALIAS(50221, "MS50221") \
	CP_ALIAS(50221, "WINDOWS-50221") \
\
	CP_ALIAS(936, "CP936") \
	CP_ALIAS(936, "GBK") \
	CP_ALIAS(936, "MS936") \
	CP_ALIAS(936, "WINDOWS-936") \
\
	CP_ALIAS(950, "CP950") \
	CP_ALIAS(950, "BIG5") \
	CP_ALIAS(950, "BIG5HKSCS") \
	CP_ALIAS(950, "BIG5-HKSCS") \
\
	CP_ALIAS(949, "CP949") \
	CP_ALIAS(949, "UHC") \
	CP_ALIAS(949, "EUC-KR") \
\
	CP_ALIAS(1361, "CP1361") \
	CP_ALIAS(1361, "JOHAB") \
\
	CP_ALIAS(437, "437") \
	CP_ALIAS(437, "CP437") \
	CP_ALIAS(437, "IBM437") \
	CP_ALIAS(437, "CSPC8CODEPAGE437") \
\
	CP_ALIAS(737, "CP737") \
\
	CP_ALIAS(775, "CP775") \
	CP_ALIAS(775, "IBM775") \
	CP_ALIAS(775, "CSPC775BALTIC") \
\
	CP_ALIAS(852, "852") \
	CP_ALIAS(852, "CP852") \
	CP_ALIAS(852, "IBM852") \
	CP_ALIAS(852, "CSPCP852") \
\
	/* !IsValidCodePage(853) */ \
	CP_ALIAS(853, "CP853") \
\
	CP_ALIAS(855, "855") \
	CP_ALIAS(855, "CP855") \
	CP_ALIAS(855, "IBM855") \
	CP_ALIAS(855, "CSIBM855") \
\
	CP_ALIAS(857, "857") \
	CP_ALIAS(857, "CP857") \
	CP_ALIAS(857, "IBM857") \
	CP_ALIAS(857, "CSIBM857") \
\
	/* !IsValidCodePage(858) */ \
	CP_ALIAS(858, "CP858") \
\
	CP_ALIAS(860, "860") \
	CP_ALIAS(860, "CP860") \
	CP_ALIAS(860, "IBM860") \
	CP_ALIAS(860, "CSIBM860") \
\
	CP_ALIAS(861, "861") \
	CP_ALIAS(861, "CP-IS") \
	CP_ALIAS(861, "CP861") \
	CP_ALIAS(861, "IBM861") \
	CP_ALIAS(861, "CSIBM861") \
\
	CP_ALIAS(863, "863") \
	CP_ALIAS(863, "CP863") \
	CP_ALIAS(863, "IBM863") \
	CP_ALIAS(863, "CSIBM863") \
\
	CP_ALIAS(864, "CP864") \
	CP_ALIAS(864, "IBM864") \
	CP_ALIAS(864, "CSIBM864") \
\
	CP_ALIAS(865, "865") \
	CP_ALIAS(865, "CP865") \
	CP_ALIAS(865, "IBM865") \
	CP_ALIAS(865, "CSIBM865") \
\
	CP_ALIAS(869, "869") \
	CP_ALIAS(869, "CP-GR") \
	CP_ALIAS(869, "CP869") \
	CP_ALIAS(869, "IBM869") \
	CP_ALIAS(869, "CSIBM869") \
\
	/* !IsValidCodePage(1152) */ \
	CP_ALIAS(1125, "CP1125") \
\
	/* \
	 * Code Page Identifiers \
	 * http://msdn2.microsoft.com/en-us/library/ms776446.aspx \
	 */ \
	CP_ALIAS(37, "IBM037") /* IBM EBCDIC US-Canada */ \
	CP_ALIAS(437, "IBM437") /* OEM United States */ \
	CP_ALIAS(500, "IBM500") /* IBM EBCDIC International */ \
	CP_ALIAS(708, "ASMO-708") /* Arabic (ASMO 708) */ \
	/* 709 		Arabic (ASMO-449+, BCON V4) */ \
	/* 710 		Arabic - Transparent Arabic */ \
	CP_ALIAS(720, "DOS-720") /* Arabic (Transparent ASMO); Arabic (DOS) */ \
	CP_ALIAS(737, "ibm737") /* OEM Greek (formerly 437G); Greek (DOS) */ \
	CP_ALIAS(775, "ibm775") /* OEM Baltic; Baltic (DOS) */ \
	CP_ALIAS(850, "ibm850") /* OEM Multilingual Latin 1; Western European (DOS) */ \
	CP_ALIAS(852, "ibm852") /* OEM Latin 2; Central European (DOS) */ \
	CP_ALIAS(855, "IBM855") /* OEM Cyrillic (primarily Russian) */ \
	CP_ALIAS(857, "ibm857") /* OEM Turkish; Turkish (DOS) */ \
	CP_ALIAS(858, "IBM00858") /* OEM Multilingual Latin 1 + Euro symbol */ \
	CP_ALIAS(860, "IBM860") /* OEM Portuguese; Portuguese (DOS) */ \
	CP_ALIAS(861, "ibm861") /* OEM Icelandic; Icelandic (DOS) */ \
	CP_ALIAS(862, "DOS-862") /* OEM Hebrew; Hebrew (DOS) */ \
	CP_ALIAS(863, "IBM863") /* OEM French Canadian; French Canadian (DOS) */ \
	CP_ALIAS(864, "IBM864") /* OEM Arabic; Arabic (864) */ \
	CP_ALIAS(865, "IBM865") /* OEM Nordic; Nordic (DOS) */ \
	CP_ALIAS(866, "cp866") /* OEM Russian; Cyrillic (DOS) */ \
	CP_ALIAS(869, "ibm869") /* OEM Modern Greek; Greek, Modern (DOS) */ \
	CP_ALIAS(870, "IBM870") /* IBM EBCDIC Multilingual/ROECE (Latin 2); IBM EBCDIC Multilingual Latin 2 */ \
	CP_ALIAS(874, "windows-874") /* ANSI/OEM Thai (same as 28605, ISO 8859-15); Thai (Windows) */ \
	CP_ALIAS(875, "cp875") /* IBM EBCDIC Greek Modern */ \
	CP_ALIAS(932, "shift_jis") /* ANSI/OEM Japanese; Japanese (Shift-JIS) */ \
	CP_ALIAS(932, "shift-jis") /* alternative name for it */ \
	CP_ALIAS(936, "gb2312") /* ANSI/OEM Simplified Chinese (PRC, Singapore); Chinese Simplified (GB2312) */ \
	CP_ALIAS(949, "ks_c_5601-1987") /* ANSI/OEM Korean (Unified Hangul Code) */ \
	CP_ALIAS(950, "big5") /* ANSI/OEM Traditional Chinese (Taiwan; Hong Kong SAR, PRC); Chinese Traditional (Big5) */ \
	CP_ALIAS(950, "big5hkscs") /* ANSI/OEM Traditional Chinese (Hong Kong SAR); Chinese Traditional (Big5-HKSCS) */ \
	CP_ALIAS(950, "big5-hkscs") /* alternative name for it */ \
	CP_ALIAS(1026, "IBM1026") /* IBM EBCDIC Turkish (Latin 5) */ \
	CP_ALIAS(1047, "IBM01047") /* IBM EBCDIC Latin 1/Open System */ \
	CP_ALIAS(1140, "IBM01140") /* IBM EBCDIC US-Canada (037 + Euro symbol); IBM EBCDIC (US-Canada-Euro) */ \
	CP_ALIAS(1141, "IBM01141") /* IBM EBCDIC Germany (20273 + Euro symbol); IBM EBCDIC (Germany-Euro) */ \
	CP_ALIAS(1142, "IBM01142") /* IBM EBCDIC Denmark-Norway (20277 + Euro symbol); IBM EBCDIC (Denmark-Norway-Euro) */ \
	CP_ALIAS(1143, "IBM01143") /* IBM EBCDIC Finland-Sweden (20278 + Euro symbol); IBM EBCDIC (Finland-Sweden-Euro) */ \
	CP_ALIAS(1144, "IBM01144") /* IBM EBCDIC Italy (20280 + Euro symbol); IBM EBCDIC (Italy-Euro) */ \
	CP_ALIAS(1145, "IBM01145") /* IBM EBCDIC Latin America-Spain (20284 + Euro symbol); IBM EBCDIC (Spain-Euro) */ \
	CP_ALIAS(1146, "IBM01146") /* IBM EBCDIC United Kingdom (20285 + Euro symbol); IBM EBCDIC (UK-Euro) */ \
	CP_ALIAS(1147, "IBM01147") /* IBM EBCDIC France (20297 + Euro symbol); IBM EBCDIC (France-Euro) */ \
	CP_ALIAS(1148, "IBM01148") /* IBM EBCDIC International (500 + Euro symbol); IBM EBCDIC (International-Euro) */ \
	CP_ALIAS(1149, "IBM01149") /* IBM EBCDIC Icelandic (20871 + Euro symbol); IBM EBCDIC (Icelandic-Euro) */ \
	CP_ALIAS(1250, "windows-1250") /* ANSI Central European; Central European (Windows) */ \
	CP_ALIAS(1251, "windows-1251") /* ANSI Cyrillic; Cyrillic (Windows) */ \
	CP_ALIAS(1252, "windows-1252") /* ANSI Latin 1; Western European (Windows) */ \
	CP_ALIAS(1253, "windows-1253") /* ANSI Greek; Greek (Windows) */ \
	CP_ALIAS(1254, "windows-1254") /* ANSI Turkish; Turkish (Windows) */ \
	CP_ALIAS(1255, "windows-1255") /* ANSI Hebrew; Hebrew (Windows) */ \
	CP_ALIAS(1256, "windows-1256") /* ANSI Arabic; Arabic (Windows) */ \
	CP_ALIAS(1257, "windows-1257") /* ANSI Baltic; Baltic (Windows) */ \
	CP_ALIAS(1258, "windows-1258") /* ANSI/OEM Vietnamese; Vietnamese (Windows) */ \
	CP_ALIAS(1361, "Johab") /* Korean (Johab) */ \
	CP_ALIAS(10000, "macintosh") /* MAC Roman; Western European (Mac) */ \
	CP_ALIAS(10001, "x-mac-japanese") /* Japanese (Mac) */ \
	CP_ALIAS(10002, "x-mac-chinesetrad") /* MAC Traditional Chinese (Big5); Chinese Traditional (Mac) */ \
	CP_ALIAS(10003, "x-mac-korean") /* Korean (Mac) */ \
	CP_ALIAS(10004, "x-mac-arabic") /* Arabic (Mac) */ \
	CP_ALIAS(10005, "x-mac-hebrew") /* Hebrew (Mac) */ \
	CP_ALIAS(10006, "x-mac-greek") /* Greek (Mac) */ \
	CP_ALIAS(10007, "x-mac-cyrillic") /* Cyrillic (Mac) */ \
	CP_ALIAS(10008, "x-mac-chinesesimp") /* MAC Simplified Chinese (GB 2312); Chinese Simplified (Mac) */ \
	CP_ALIAS(10010, "x-mac-romanian") /* Romanian (Mac) */ \
	CP_ALIAS(10017, "x-mac-ukrainian") /* Ukrainian (Mac) */ \
	CP_ALIAS(10021, "x-mac-thai") /* Thai (Mac) */ \
	CP_ALIAS(10029, "x-mac-ce") /* MAC Latin 2; Central European (Mac) */ \
	CP_ALIAS(10079, "x-mac-icelandic") /* Icelandic (Mac) */ \
	CP_ALIAS(10081, "x-mac-turkish") /* Turkish (Mac) */ \
	CP_ALIAS(10082, "x-mac-croatian") /* Croatian (Mac) */ \
	CP_ALIAS(20000, "x-Chinese_CNS") /* CNS Taiwan; Chinese Traditional (CNS) */ \
	CP_ALIAS(20001, "x-cp20001") /* TCA Taiwan */ \
	CP_ALIAS(20002, "x_Chinese-Eten") /* Eten Taiwan; Chinese Traditional (Eten) */ \
	CP_ALIAS(20003, "x-cp20003") /* IBM5550 Taiwan */ \
	CP_ALIAS(20004, "x-cp20004") /* TeleText Taiwan */ \
	CP_ALIAS(20005, "x-cp20005") /* Wang Taiwan */ \
	CP_ALIAS(20105, "x-IA5") /* IA5 (IRV International Alphabet No. 5, 7-bit); Western European (IA5) */ \
	CP_ALIAS(20106, "x-IA5-German") /* IA5 German (7-bit) */ \
	CP_ALIAS(20107, "x-IA5-Swedish") /* IA5 Swedish (7-bit) */ \
	CP_ALIAS(20108, "x-IA5-Norwegian") /* IA5 Norwegian (7-bit) */ \
	CP_ALIAS(20127, "us-ascii") /* US-ASCII (7-bit) */ \
	CP_ALIAS(20261, "x-cp20261") /* T.61 */ \
	CP_ALIAS(20269, "x-cp20269") /* ISO 6937 Non-Spacing Accent */ \
	CP_ALIAS(20273, "IBM273") /* IBM EBCDIC Germany */ \
	CP_ALIAS(20277, "IBM277") /* IBM EBCDIC Denmark-Norway */ \
	CP_ALIAS(20278, "IBM278") /* IBM EBCDIC Finland-Sweden */ \
	CP_ALIAS(20280, "IBM280") /* IBM EBCDIC Italy */ \
	CP_ALIAS(20284, "IBM284") /* IBM EBCDIC Latin America-Spain */ \
	CP_ALIAS(20285, "IBM285") /* IBM EBCDIC United Kingdom */ \
	CP_ALIAS(20290, "IBM290") /* IBM EBCDIC Japanese Katakana Extended */ \
	CP_ALIAS(20297, "IBM297") /* IBM EBCDIC France */ \
	CP_ALIAS(20420, "IBM420") /* IBM EBCDIC Arabic */ \
	CP_ALIAS(20423, "IBM423") /* IBM EBCDIC Greek */ \
	CP_ALIAS(20424, "IBM424") /* IBM EBCDIC Hebrew */ \
	CP_ALIAS(20833, "x-EBCDIC-KoreanExtended") /* IBM EBCDIC Korean Extended */ \
	CP_ALIAS(20838, "IBM-Thai") /* IBM EBCDIC Thai */ \
	CP_ALIAS(20866, "koi8-r") /* Russian (KOI8-R); Cyrillic (KOI8-R) */ \
	CP_ALIAS(20871, "IBM871") /* IBM EBCDIC Icelandic */ \
	CP_ALIAS(20880, "IBM880") /* IBM EBCDIC Cyrillic Russian */ \
	CP_ALIAS(20905, "IBM905") /* IBM EBCDIC Turkish */ \
	CP_ALIAS(20924, "IBM00924") /* IBM EBCDIC Latin 1/Open System (1047 + Euro symbol) */ \
	CP_ALIAS(20932, "EUC-JP") /* Japanese (JIS 0208-1990 and 0121-1990) */ \
	CP_ALIAS(20936, "x-cp20936") /* Simplified Chinese (GB2312); Chinese Simplified (GB2312-80) */ \
	CP_ALIAS(20949, "x-cp20949") /* Korean Wansung */ \
	CP_ALIAS(21025, "cp1025") /* IBM EBCDIC Cyrillic Serbian-Bulgarian */ \
	/* 21027 		(deprecated) */ \
	CP_ALIAS(21866, "koi8-u") /* Ukrainian (KOI8-U); Cyrillic (KOI8-U) */ \
	CP_ALIAS(28591, "iso-8859-1") /* ISO 8859-1 Latin 1; Western European (ISO) */ \
	CP_ALIAS(28591, "iso8859-1") /* ISO 8859-1 Latin 1; Western European (ISO) */ \
	CP_ALIAS(28591, "iso_8859-1") \
	CP_ALIAS(28591, "iso_8859_1") \
	CP_ALIAS(28592, "iso-8859-2") /* ISO 8859-2 Central European; Central European (ISO) */ \
	CP_ALIAS(28592, "iso8859-2") /* ISO 8859-2 Central European; Central European (ISO) */ \
	CP_ALIAS(28592, "iso_8859-2") \
	CP_ALIAS(28592, "iso_8859_2") \
	CP_ALIAS(28593, "iso-8859-3") /* ISO 8859-3 Latin 3 */ \
	CP_ALIAS(28593, "iso8859-3") /* ISO 8859-3 Latin 3 */ \
	CP_ALIAS(28593, "iso_8859-3") \
	CP_ALIAS(28593, "iso_8859_3") \
	CP_ALIAS(28594, "iso-8859-4") /* ISO 8859-4 Baltic */ \
	CP_ALIAS(28594, "iso8859-4") /* ISO 8859-4 Baltic */ \
	CP_ALIAS(28594, "iso_8859-4") \
	CP_ALIAS(28594, "iso_8859_4") \
	CP_ALIAS(28595, "iso-8859-5") /* ISO 8859-5 Cyrillic */ \
	CP_ALIAS(28595, "iso8859-5") /* ISO 8859-5 Cyrillic */ \
	CP_ALIAS(28595, "iso_8859-5") \
	CP_ALIAS(28595, "iso_8859_5") \
	CP_ALIAS(28596, "iso-8859-6") /* ISO 8859-6 Arabic */ \
	CP_ALIAS(28596, "iso8859-6") /* ISO 8859-6 Arabic */ \
	CP_ALIAS(28596, "iso_8859-6") \
	CP_ALIAS(28596, "iso_8859_6") \
	CP_ALIAS(28597, "iso-8859-7") /* ISO 8859-7 Greek */ \
	CP_ALIAS(28597, "iso8859-7") /* ISO 8859-7 Greek */ \
	CP_ALIAS(28597, "iso_8859-7") \
	CP_ALIAS(28597, "iso_8859_7") \
	CP_ALIAS(28598, "iso-8859-8") /* ISO 8859-8 Hebrew; Hebrew (ISO-Visual) */ \
	CP_ALIAS(28598, "iso8859-8") /* ISO 8859-8 Hebrew; Hebrew (ISO-Visual) */ \
	CP_ALIAS(28598, "iso_8859-8") \
	CP_ALIAS(28598, "iso_8859_8") \
	CP_ALIAS(28599, "iso-8859-9") /* ISO 8859-9 Turkish */ \
	CP_ALIAS(28599, "iso8859-9") /* ISO 8859-9 Turkish */ \
	CP_ALIAS(28599, "iso_8859-9") \
	CP_ALIAS(28599, "iso_8859_9") \
	CP_ALIAS(28603, "iso-8859-13") /* ISO 8859-13 Estonian */ \
	CP_ALIAS(28603, "iso8859-13") /* ISO 8859-13 Estonian */ \
	CP_ALIAS(28603, "iso_8859-13") \
	CP_ALIAS(28603, "iso_8859_13") \
	CP_ALIAS(28605, "iso-8859-15") /* ISO 8859-15 Latin 9 */ \
	CP_ALIAS(28605, "iso8859-15") /* ISO 8859-15 Latin 9 */ \
	CP_ALIAS(28605, "iso_8859-15") \
	CP_ALIAS(28605, "iso_8859_15") \
	CP_ALIAS(29001, "x-Europa") /* Europa 3 */ \
	CP_ALIAS(38598, "iso-8859-8-i") /* ISO 8859-8 Hebrew; Hebrew (ISO-Logical) */ \
	CP_ALIAS(38598, "iso8859-8-i") /* ISO 8859-8 Hebrew; Hebrew (ISO-Logical) */ \
	CP_ALIAS(38598, "iso_8859-8-i") \
	CP_ALIAS(38598, "iso_8859_8-i") \
	CP_ALIAS(50220, "iso-2022-jp") /* ISO 2022 Japanese with no halfwidth Katakana; Japanese (JIS) */ \
	CP_ALIAS(50221, "csISO2022JP") /* ISO 2022 Japanese with halfwidth Katakana; Japanese (JIS-Allow 1 byte Kana) */ \
	CP_ALIAS(50222, "iso-2022-jp") /* ISO 2022 Japanese JIS X 0201-1989; Japanese (JIS-Allow 1 byte Kana - SO/SI) */ \
	CP_ALIAS(50225, "iso-2022-kr") /* ISO 2022 Korean */ \
	CP_ALIAS(50225, "iso2022-kr") /* ISO 2022 Korean */ \
	CP_ALIAS(50227, "x-cp50227") /* ISO 2022 Simplified Chinese; Chinese Simplified (ISO 2022) */ \
	/* 50229 		ISO 2022 Traditional Chinese */ \
	/* 50930 		EBCDIC Japanese (Katakana) Extended */ \
	/* 50931 		EBCDIC US-Canada and Japanese */ \
	/* 50933 		EBCDIC Korean Extended and Korean */ \
	/* 50935 		EBCDIC Simplified Chinese Extended and Simplified Chinese */ \
	/* 50936 		EBCDIC Simplified Chinese */ \
	/* 50937 		EBCDIC US-Canada and Traditional Chinese */ \
	/* 50939 		EBCDIC Japanese (Latin) Extended and Japanese */ \
	CP_ALIAS(51932, "euc-jp") /* EUC Japanese */ \
	CP_ALIAS(51936, "EUC-CN") /* EUC Simplified Chinese; Chinese Simplified (EUC) */ \
	CP_ALIAS(51949, "euc-kr") /* EUC Korean */ \
	/* 51950 		EUC Traditional Chinese */ \
	CP_ALIAS(52936, "hz-gb-2312") /* HZ-GB2312 Simplified Chinese; Chinese Simplified (HZ) */ \
	CP_ALIAS(54936, "GB18030") /* Windows XP and later: GB18030 Simplified Chinese (4 byte); Chinese Simplified (GB18030) */ \
	CP_ALIAS(57002, "x-iscii-de") /* ISCII Devanagari */ \
	CP_ALIAS(57003, "x-iscii-be") /* ISCII Bengali */ \
	CP_ALIAS(57004, "x-iscii-ta") /* ISCII Tamil */ \
	CP_ALIAS(57005, "x-iscii-te") /* ISCII Telugu */ \
	CP_ALIAS(57006, "x-iscii-as") /* ISCII Assamese */ \
	CP_ALIAS(57007, "x-iscii-or") /* ISCII Oriya */ \
	CP_ALIAS(57008, "x-iscii-ka") /* ISCII Kannada */ \
	CP_ALIAS(57009, "x-iscii-ma") /* ISCII Malayalam */ \
	CP_ALIAS(57010, "x-iscii-gu") /* ISCII Gujarati */ \
	CP_ALIAS(57011, "x-iscii-pa") /* ISCII Punjabi */

#define CP_ALIAS(codepage, alias) codepage,
static const int cp_codepage[] = {
	CP_ALIAS_LIST
};
#undef CP_ALIAS

#define CP_ALIAS(codepage, alias) alias"\0"
static const char cp_alias[] ALIGN1 =
	CP_ALIAS_LIST;
#undef CP_ALIAS

/*
 * SJIS SHIFTJIS table              CP932 table
 * ---- --------------------------- --------------------------------
 *   5C U+00A5 YEN SIGN             U+005C REVERSE SOLIDUS
 *   7E U+203E OVERLINE             U+007E TILDE
 * 815C U+2014 EM DASH              U+2015 HORIZONTAL BAR
 * 815F U+005C REVERSE SOLIDUS      U+FF3C FULLWIDTH REVERSE SOLIDUS
 * 8160 U+301C WAVE DASH            U+FF5E FULLWIDTH TILDE
 * 8161 U+2016 DOUBLE VERTICAL LINE U+2225 PARALLEL TO
 * 817C U+2212 MINUS SIGN           U+FF0D FULLWIDTH HYPHEN-MINUS
 * 8191 U+00A2 CENT SIGN            U+FFE0 FULLWIDTH CENT SIGN
 * 8192 U+00A3 POUND SIGN           U+FFE1 FULLWIDTH POUND SIGN
 * 81CA U+00AC NOT SIGN             U+FFE2 FULLWIDTH NOT SIGN
 *
 * EUC-JP and ISO-2022-JP should be compatible with CP932.
 *
 * Kernel and MLang have different Unicode mapping table.  Make sure
 * which API is used.
 */
static compat_t cp932_compat[] = {
	{0x00A5, 0x005C, COMPAT_OUT},
	{0x203E, 0x007E, COMPAT_OUT},
	{0x2014, 0x2015, COMPAT_OUT},
	{0x301C, 0xFF5E, COMPAT_OUT},
	{0x2016, 0x2225, COMPAT_OUT},
	{0x2212, 0xFF0D, COMPAT_OUT},
	{0x00A2, 0xFFE0, COMPAT_OUT},
	{0x00A3, 0xFFE1, COMPAT_OUT},
	{0x00AC, 0xFFE2, COMPAT_OUT},
	{0, 0, 0}
};

static compat_t cp20932_compat[] = {
	{0x00A5, 0x005C, COMPAT_OUT},
	{0x203E, 0x007E, COMPAT_OUT},
	{0x2014, 0x2015, COMPAT_OUT},
	{0xFF5E, 0x301C, COMPAT_OUT|COMPAT_IN},
	{0x2225, 0x2016, COMPAT_OUT|COMPAT_IN},
	{0xFF0D, 0x2212, COMPAT_OUT|COMPAT_IN},
	{0xFFE0, 0x00A2, COMPAT_OUT|COMPAT_IN},
	{0xFFE1, 0x00A3, COMPAT_OUT|COMPAT_IN},
	{0xFFE2, 0x00AC, COMPAT_OUT|COMPAT_IN},
	{0, 0, 0}
};

static compat_t *cp51932_compat = cp932_compat;

/* cp20932_compat for kernel.  cp932_compat for mlang. */
static compat_t *cp5022x_compat = cp932_compat;

typedef HRESULT (WINAPI *CONVERTINETMULTIBYTETOUNICODE)(
	LPDWORD lpdwMode,
	DWORD dwSrcEncoding,
	LPCSTR lpSrcStr,
	LPINT lpnMultiCharCount,
	LPWSTR lpDstStr,
	LPINT lpnWideCharCount
);

typedef HRESULT (WINAPI *CONVERTINETUNICODETOMULTIBYTE)(
	LPDWORD lpdwMode,
	DWORD dwEncoding,
	LPCWSTR lpSrcStr,
	LPINT lpnWideCharCount,
	LPSTR lpDstStr,
	LPINT lpnMultiCharCount
);

static CONVERTINETMULTIBYTETOUNICODE ConvertINetMultiByteToUnicode;
static CONVERTINETUNICODETOMULTIBYTE ConvertINetUnicodeToMultiByte;

static int
load_mlang(void)
{
	HMODULE h;
	if (ConvertINetMultiByteToUnicode != NULL)
		return TRUE;
	h = LoadLibrary(TEXT("mlang.dll"));
	if (!h)
		return FALSE;
	ConvertINetMultiByteToUnicode = (CONVERTINETMULTIBYTETOUNICODE)GetProcAddressA(h, "ConvertINetMultiByteToUnicode");
	ConvertINetUnicodeToMultiByte = (CONVERTINETUNICODETOMULTIBYTE)GetProcAddressA(h, "ConvertINetUnicodeToMultiByte");
	return TRUE;
}

static iconv_t
iconv_open(const char *tocode, const char *fromcode)
{
	rec_iconv_t *cd;

	cd = (rec_iconv_t *)xzalloc(sizeof(rec_iconv_t));

	/* reset the errno to prevent reporting wrong error code.
	 * 0 for unsorted error. */
	errno = 0;
	if (make_csconv(fromcode, &cd->from) && make_csconv(tocode, &cd->to)) {
		cd->cd = (iconv_t)cd;
		return (iconv_t)cd;
	}

	free(cd);
	return (iconv_t)(-1);
}

static int
iconv_close(iconv_t _cd)
{
	free(_cd);
	return 0;
}

static size_t
iconv(iconv_t _cd, const char **inbuf, size_t *inbytesleft, char **outbuf, size_t *outbytesleft)
{
	rec_iconv_t *cd = (rec_iconv_t *)_cd;
	ushort wbuf[MB_CHAR_MAX]; /* enough room for one character */
	int insize;
	int outsize;
	int wsize;
	DWORD frommode;
	DWORD tomode;
	uint wc;
	compat_t *cp;
	int i;

	if (inbuf == NULL || *inbuf == NULL)
	{
		if (outbuf != NULL && *outbuf != NULL && cd->to.flush != NULL)
		{
			tomode = cd->to.mode;
			outsize = cd->to.flush(&cd->to, (uchar *)*outbuf, *outbytesleft);
			if (outsize == -1)
			{
				if ((cd->to.flags & FLAG_IGNORE) && errno != E2BIG)
				{
					outsize = 0;
				}
				else
				{
					cd->to.mode = tomode;
					return (size_t)(-1);
				}
			}
			*outbuf += outsize;
			*outbytesleft -= outsize;
		}
		cd->from.mode = 0;
		cd->to.mode = 0;
		return 0;
	}

	while (*inbytesleft != 0)
	{
		frommode = cd->from.mode;
		tomode = cd->to.mode;
		wsize = MB_CHAR_MAX;

		insize = cd->from.mbtowc(&cd->from, (const uchar *)*inbuf, *inbytesleft, wbuf, &wsize);
		if (insize == -1)
		{
			if (cd->to.flags & FLAG_IGNORE)
			{
				cd->from.mode = frommode;
				insize = 1;
				wsize = 0;
			}
			else
			{
				cd->from.mode = frommode;
				return (size_t)(-1);
			}
		}

		if (wsize == 0)
		{
			*inbuf += insize;
			*inbytesleft -= insize;
			continue;
		}

		if (cd->from.compat != NULL)
		{
			wc = utf16_to_ucs4(wbuf);
			cp = cd->from.compat;
			for (i = 0; cp[i].in != 0; ++i)
			{
				if ((cp[i].flag & COMPAT_IN) && cp[i].out == wc)
				{
					ucs4_to_utf16(cp[i].in, wbuf, &wsize);
					break;
				}
			}
		}

		if (cd->to.compat != NULL)
		{
			wc = utf16_to_ucs4(wbuf);
			cp = cd->to.compat;
			for (i = 0; cp[i].in != 0; ++i)
			{
				if ((cp[i].flag & COMPAT_OUT) && cp[i].in == wc)
				{
					ucs4_to_utf16(cp[i].out, wbuf, &wsize);
					break;
				}
			}
		}

		outsize = cd->to.wctomb(&cd->to, wbuf, wsize, (uchar *)*outbuf, *outbytesleft);
		if (outsize == -1)
		{
			if ((cd->to.flags & FLAG_IGNORE) && errno != E2BIG)
			{
				cd->to.mode = tomode;
				outsize = 0;
			}
			else
			{
				cd->from.mode = frommode;
				cd->to.mode = tomode;
				return (size_t)(-1);
			}
		}

		*inbuf += insize;
		*outbuf += outsize;
		*inbytesleft -= insize;
		*outbytesleft -= outsize;
	}

	return 0;
}

static int
make_csconv(const char *_name, csconv_t *cv)
{
	CPINFO cpinfo;
	int use_compat = TRUE;
	int flag = 0;
	char *name;
	char *p, *s;

	name = xstrdup(_name);

	/* check for option "enc_name//opt1//opt2" */
	while ((p = strrstr(name, "//")) != NULL)
	{
		for (s = p + 2; *s; ++s)
			*s = tolower(*s);
		switch (index_in_strings("nocompat\0translit\0ignore\0", p + 2)) {
		case 0:
			use_compat = FALSE;
			break;
		case 1:
			flag |= FLAG_TRANSLIT;
			break;
		case 2:
			flag |= FLAG_IGNORE;
			break;
		}
		*p = 0;
	}

	cv->mode = 0;
	cv->flags = flag;
	cv->mblen = NULL;
	cv->flush = NULL;
	cv->compat = NULL;
	cv->codepage = name_to_codepage(name);
	if (cv->codepage == 1200 || cv->codepage == 1201)
	{
		cv->mbtowc = utf16_mbtowc;
		cv->wctomb = utf16_wctomb;
		if (_stricmp(name, "UTF-16") == 0 || _stricmp(name, "UTF16") == 0 ||
		  _stricmp(name, "UCS-2") == 0 || _stricmp(name, "UCS2") == 0 ||
	  _stricmp(name,"UCS-2-INTERNAL") == 0)
			cv->flags |= FLAG_USE_BOM;
	}
	else if (cv->codepage == 12000 || cv->codepage == 12001)
	{
		cv->mbtowc = utf32_mbtowc;
		cv->wctomb = utf32_wctomb;
		if (_stricmp(name, "UTF-32") == 0 || _stricmp(name, "UTF32") == 0 ||
		  _stricmp(name, "UCS-4") == 0 || _stricmp(name, "UCS4") == 0)
			cv->flags |= FLAG_USE_BOM;
	}
	else if (cv->codepage == 65001)
	{
		cv->mbtowc = kernel_mbtowc;
		cv->wctomb = kernel_wctomb;
		cv->mblen = utf8_mblen;
	}
	else if ((cv->codepage == 50220 || cv->codepage == 50221 || cv->codepage == 50222) && load_mlang())
	{
		cv->mbtowc = iso2022jp_mbtowc;
		cv->wctomb = iso2022jp_wctomb;
		cv->flush = iso2022jp_flush;
	}
	else if (cv->codepage == 51932 && load_mlang())
	{
		cv->mbtowc = mlang_mbtowc;
		cv->wctomb = mlang_wctomb;
		cv->mblen = eucjp_mblen;
	}
	else if (IsValidCodePage(cv->codepage)
				&& GetCPInfo(cv->codepage, &cpinfo) != 0)
	{
		cv->mbtowc = kernel_mbtowc;
		cv->wctomb = kernel_wctomb;
		if (cpinfo.MaxCharSize == 1)
			cv->mblen = sbcs_mblen;
		else if (cpinfo.MaxCharSize == 2)
			cv->mblen = dbcs_mblen;
		else
			cv->mblen = mbcs_mblen;
	}
	else
	{
		/* not supported */
		free(name);
		errno = EINVAL;
		return FALSE;
	}

	if (use_compat)
	{
		switch (cv->codepage)
		{
		case 932: cv->compat = cp932_compat; break;
		case 20932: cv->compat = cp20932_compat; break;
		case 51932: cv->compat = cp51932_compat; break;
		case 50220: case 50221: case 50222: cv->compat = cp5022x_compat; break;
		}
	}

	free(name);

	return TRUE;
}

static int
name_to_codepage(const char *name)
{
	int i;
	const char *alias;

	if (*name == '\0' || strcmp(name, "char") == 0)
		return GetACP();
	else if (strcmp(name, "wchar_t") == 0)
		return 1200;
	else if (_strnicmp(name, "cp", 2) == 0)
		return atoi(name + 2); /* CP123 */
	else if ('0' <= name[0] && name[0] <= '9')
		return atoi(name);     /* 123 */
	else if (_strnicmp(name, "xx", 2) == 0)
		return atoi(name + 2); /* XX123 for debug */

	i = 0;
	alias = cp_alias;
	while (*alias) {
		if (_stricmp(alias, name) == 0) {
			return cp_codepage[i];
		}
		alias += strlen(alias) + 1;
		++i;
	}
	return -1;
}

/*
 * http://www.faqs.org/rfcs/rfc2781.html
 */
static uint
utf16_to_ucs4(const ushort *wbuf)
{
	uint wc = wbuf[0];
	if (0xD800 <= wbuf[0] && wbuf[0] <= 0xDBFF)
		wc = ((wbuf[0] & 0x3FF) << 10) + (wbuf[1] & 0x3FF) + 0x10000;
	return wc;
}

static void
ucs4_to_utf16(uint wc, ushort *wbuf, int *wbufsize)
{
	if (wc < 0x10000)
	{
		wbuf[0] = wc;
		*wbufsize = 1;
	}
	else
	{
		wc -= 0x10000;
		wbuf[0] = 0xD800 | ((wc >> 10) & 0x3FF);
		wbuf[1] = 0xDC00 | (wc & 0x3FF);
		*wbufsize = 2;
	}
}

/*
 * Check if codepage is one of those for which the dwFlags parameter
 * to MultiByteToWideChar() must be zero. Return zero or
 * MB_ERR_INVALID_CHARS.  The docs in Platform SDK for Windows
 * Server 2003 R2 claims that also codepage 65001 is one of these, but
 * that doesn't seem to be the case. The MSDN docs for MSVS2008 leave
 * out 65001 (UTF-8), and that indeed seems to be the case on XP, it
 * works fine to pass MB_ERR_INVALID_CHARS in dwFlags when converting
 * from UTF-8.
 */
static int
mbtowc_flags(int codepage)
{
	return (codepage == 50220 || codepage == 50221 ||
			codepage == 50222 || codepage == 50225 ||
			codepage == 50227 || codepage == 50229 ||
			codepage == 52936 || codepage == 54936 ||
			(codepage >= 57002 && codepage <= 57011) ||
			codepage == 65000 || codepage == 42) ? 0 : MB_ERR_INVALID_CHARS;
}

/*
 * Check if codepage is one those for which the lpUsedDefaultChar
 * parameter to WideCharToMultiByte() must be NULL.  The docs in
 * Platform SDK for Windows Server 2003 R2 claims that this is the
 * list below, while the MSDN docs for MSVS2008 claim that it is only
 * for 65000 (UTF-7) and 65001 (UTF-8). This time the earlier Platform
 * SDK seems to be correct, at least for XP.
 */
static int
must_use_null_useddefaultchar(int codepage)
{
	return (codepage == 65000 || codepage == 65001 ||
			codepage == 50220 || codepage == 50221 ||
			codepage == 50222 || codepage == 50225 ||
			codepage == 50227 || codepage == 50229 ||
			codepage == 52936 || codepage == 54936 ||
			(codepage >= 57002 && codepage <= 57011) ||
			codepage == 42);
}

static int
seterror(int err)
{
	errno = err;
	return -1;
}

static int
sbcs_mblen(csconv_t *cv UNUSED_PARAM, const uchar *buf UNUSED_PARAM,
			int bufsize UNUSED_PARAM)
{
	return 1;
}

static int
dbcs_mblen(csconv_t *cv, const uchar *buf, int bufsize)
{
	int len = IsDBCSLeadByteEx(cv->codepage, buf[0]) ? 2 : 1;
	if (bufsize < len)
		return seterror(EINVAL);
	return len;
}

static int
mbcs_mblen(csconv_t *cv, const uchar *buf, int bufsize)
{
	int len = 0;

	if (cv->codepage == 54936) {
		if (buf[0] <= 0x7F)
			len = 1;
		else if (buf[0] >= 0x81 && buf[0] <= 0xFE &&
				bufsize >= 2 &&
				((buf[1] >= 0x40 && buf[1] <= 0x7E) ||
				(buf[1] >= 0x80 && buf[1] <= 0xFE)))
			len = 2;
		else if (buf[0] >= 0x81 && buf[0] <= 0xFE &&
				bufsize >= 4 &&
				buf[1] >= 0x30 && buf[1] <= 0x39)
			len = 4;
		else
			return seterror(EINVAL);
		return len;
	}
	else
	return seterror(EINVAL);
}

static int
utf8_mblen(csconv_t *cv UNUSED_PARAM, const uchar *buf, int bufsize)
{
	int len = 0;

	if (buf[0] < 0x80) len = 1;
	else if ((buf[0] & 0xE0) == 0xC0) len = 2;
	else if ((buf[0] & 0xF0) == 0xE0) len = 3;
	else if ((buf[0] & 0xF8) == 0xF0) len = 4;
	else if ((buf[0] & 0xFC) == 0xF8) len = 5;
	else if ((buf[0] & 0xFE) == 0xFC) len = 6;

	if (len == 0)
		return seterror(EILSEQ);
	else if (bufsize < len)
		return seterror(EINVAL);
	return len;
}

static int
eucjp_mblen(csconv_t *cv UNUSED_PARAM, const uchar *buf, int bufsize)
{
	if (buf[0] < 0x80) /* ASCII */
		return 1;
	else if (buf[0] == 0x8E) /* JIS X 0201 */
	{
		if (bufsize < 2)
			return seterror(EINVAL);
		else if (!(0xA1 <= buf[1] && buf[1] <= 0xDF))
			return seterror(EILSEQ);
		return 2;
	}
	else if (buf[0] == 0x8F) /* JIS X 0212 */
	{
		if (bufsize < 3)
			return seterror(EINVAL);
		else if (!(0xA1 <= buf[1] && buf[1] <= 0xFE)
				|| !(0xA1 <= buf[2] && buf[2] <= 0xFE))
			return seterror(EILSEQ);
		return 3;
	}
	else /* JIS X 0208 */
	{
		if (bufsize < 2)
			return seterror(EINVAL);
		else if (!(0xA1 <= buf[0] && buf[0] <= 0xFE)
				|| !(0xA1 <= buf[1] && buf[1] <= 0xFE))
			return seterror(EILSEQ);
		return 2;
	}
}

static int
kernel_mbtowc(csconv_t *cv, const uchar *buf, int bufsize, ushort *wbuf, int *wbufsize)
{
	int len;

	len = cv->mblen(cv, buf, bufsize);
	if (len == -1)
		return -1;
	/* If converting from ASCII, reject 8bit
	 * chars. MultiByteToWideChar() doesn't. Note that for ASCII we
	 * know that the mblen function is sbcs_mblen() so len is 1.
	 */
	if (cv->codepage == 20127 && buf[0] >= 0x80)
		return seterror(EILSEQ);
	*wbufsize = MultiByteToWideChar(cv->codepage, mbtowc_flags (cv->codepage),
			(const char *)buf, len, (wchar_t *)wbuf, *wbufsize);
	if (*wbufsize == 0)
		return seterror(EILSEQ);
	return len;
}

static int
kernel_wctomb(csconv_t *cv, ushort *wbuf, int wbufsize, uchar *buf, int bufsize)
{
	BOOL usedDefaultChar = 0;
	BOOL *p = NULL;
	int flags = 0;
	int len;

	if (bufsize == 0)
		return seterror(E2BIG);
	if (!must_use_null_useddefaultchar(cv->codepage))
	{
		p = &usedDefaultChar;
#ifdef WC_NO_BEST_FIT_CHARS
		if (!(cv->flags & FLAG_TRANSLIT))
			flags |= WC_NO_BEST_FIT_CHARS;
#endif
	}
	len = WideCharToMultiByte(cv->codepage, flags,
			(const wchar_t *)wbuf, wbufsize, (char *)buf, bufsize, NULL, p);
	if (len == 0)
	{
		if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
			return seterror(E2BIG);
		return seterror(EILSEQ);
	}
	else if (usedDefaultChar && !(cv->flags & FLAG_TRANSLIT))
		return seterror(EILSEQ);
	else if (cv->mblen(cv, buf, len) != len) /* validate result */
		return seterror(EILSEQ);
	return len;
}

/*
 * It seems that the mode (cv->mode) is fixnum.
 * For example, when converting iso-2022-jp(cp50221) to unicode:
 *      in ascii sequence: mode=0xC42C0000
 *   in jisx0208 sequence: mode=0xC42C0001
 * "C42C" is same for each convert session.
 * It should be: ((codepage-1)<<16)|state
 */
static int
mlang_mbtowc(csconv_t *cv, const uchar *buf, int bufsize, ushort *wbuf, int *wbufsize)
{
	int len;
	int insize;
	HRESULT hr;

	len = cv->mblen(cv, buf, bufsize);
	if (len == -1)
		return -1;
	insize = len;
	hr = ConvertINetMultiByteToUnicode(&cv->mode, cv->codepage,
			(const char *)buf, &insize, (wchar_t *)wbuf, wbufsize);
	if (hr != S_OK || insize != len)
		return seterror(EILSEQ);
	return len;
}

static int
mlang_wctomb(csconv_t *cv, ushort *wbuf, int wbufsize, uchar *buf, int bufsize)
{
	char tmpbuf[MB_CHAR_MAX]; /* enough room for one character */
	int tmpsize = MB_CHAR_MAX;
	int insize = wbufsize;
	HRESULT hr;

	hr = ConvertINetUnicodeToMultiByte(&cv->mode, cv->codepage,
			(const wchar_t *)wbuf, &wbufsize, tmpbuf, &tmpsize);
	if (hr != S_OK || insize != wbufsize)
		return seterror(EILSEQ);
	else if (bufsize < tmpsize)
		return seterror(E2BIG);
	else if (cv->mblen(cv, (uchar *)tmpbuf, tmpsize) != tmpsize)
		return seterror(EILSEQ);
	memcpy(buf, tmpbuf, tmpsize);
	return tmpsize;
}

static int
utf16_mbtowc(csconv_t *cv, const uchar *buf, int bufsize, ushort *wbuf, int *wbufsize)
{
	int codepage = cv->codepage;

	/* swap endian: 1200 <-> 1201 */
	if (cv->mode & UNICODE_MODE_SWAPPED)
		codepage ^= 1;

	if (bufsize < 2)
		return seterror(EINVAL);
	if (codepage == 1200) /* little endian */
		wbuf[0] = (buf[1] << 8) | buf[0];
	else if (codepage == 1201) /* big endian */
		wbuf[0] = (buf[0] << 8) | buf[1];

	if ((cv->flags & FLAG_USE_BOM) && !(cv->mode & UNICODE_MODE_BOM_DONE))
	{
		cv->mode |= UNICODE_MODE_BOM_DONE;
		if (wbuf[0] == 0xFFFE)
		{
			cv->mode |= UNICODE_MODE_SWAPPED;
			*wbufsize = 0;
			return 2;
		}
		else if (wbuf[0] == 0xFEFF)
		{
			*wbufsize = 0;
			return 2;
		}
	}

	if (0xDC00 <= wbuf[0] && wbuf[0] <= 0xDFFF)
		return seterror(EILSEQ);
	if (0xD800 <= wbuf[0] && wbuf[0] <= 0xDBFF)
	{
		if (bufsize < 4)
			return seterror(EINVAL);
		if (codepage == 1200) /* little endian */
			wbuf[1] = (buf[3] << 8) | buf[2];
		else if (codepage == 1201) /* big endian */
			wbuf[1] = (buf[2] << 8) | buf[3];
		if (!(0xDC00 <= wbuf[1] && wbuf[1] <= 0xDFFF))
			return seterror(EILSEQ);
		*wbufsize = 2;
		return 4;
	}
	*wbufsize = 1;
	return 2;
}

static int
utf16_wctomb(csconv_t *cv, ushort *wbuf, int wbufsize, uchar *buf, int bufsize)
{
	if ((cv->flags & FLAG_USE_BOM) && !(cv->mode & UNICODE_MODE_BOM_DONE))
	{
		int r;

		cv->mode |= UNICODE_MODE_BOM_DONE;
		if (bufsize < 2)
			return seterror(E2BIG);
		if (cv->codepage == 1200) /* little endian */
			memcpy(buf, "\xFF\xFE", 2);
		else if (cv->codepage == 1201) /* big endian */
			memcpy(buf, "\xFE\xFF", 2);

		r = utf16_wctomb(cv, wbuf, wbufsize, buf + 2, bufsize - 2);
		if (r == -1)
			return -1;
		return r + 2;
	}

	if (bufsize < 2)
		return seterror(E2BIG);
	if (cv->codepage == 1200) /* little endian */
	{
		buf[0] = (wbuf[0] & 0x00FF);
		buf[1] = (wbuf[0] & 0xFF00) >> 8;
	}
	else if (cv->codepage == 1201) /* big endian */
	{
		buf[0] = (wbuf[0] & 0xFF00) >> 8;
		buf[1] = (wbuf[0] & 0x00FF);
	}
	if (0xD800 <= wbuf[0] && wbuf[0] <= 0xDBFF)
	{
		if (bufsize < 4)
			return seterror(E2BIG);
		if (cv->codepage == 1200) /* little endian */
		{
			buf[2] = (wbuf[1] & 0x00FF);
			buf[3] = (wbuf[1] & 0xFF00) >> 8;
		}
		else if (cv->codepage == 1201) /* big endian */
		{
			buf[2] = (wbuf[1] & 0xFF00) >> 8;
			buf[3] = (wbuf[1] & 0x00FF);
		}
		return 4;
	}
	return 2;
}

static int
utf32_mbtowc(csconv_t *cv, const uchar *buf, int bufsize, ushort *wbuf, int *wbufsize)
{
	int codepage = cv->codepage;
	uint wc = 0xD800;

	/* swap endian: 12000 <-> 12001 */
	if (cv->mode & UNICODE_MODE_SWAPPED)
		codepage ^= 1;

	if (bufsize < 4)
		return seterror(EINVAL);
	if (codepage == 12000) /* little endian */
		wc = (buf[3] << 24) | (buf[2] << 16) | (buf[1] << 8) | buf[0];
	else if (codepage == 12001) /* big endian */
		wc = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];

	if ((cv->flags & FLAG_USE_BOM) && !(cv->mode & UNICODE_MODE_BOM_DONE))
	{
		cv->mode |= UNICODE_MODE_BOM_DONE;
		if (wc == 0xFFFE0000)
		{
			cv->mode |= UNICODE_MODE_SWAPPED;
			*wbufsize = 0;
			return 4;
		}
		else if (wc == 0x0000FEFF)
		{
			*wbufsize = 0;
			return 4;
		}
	}

	if ((0xD800 <= wc && wc <= 0xDFFF) || 0x10FFFF < wc)
		return seterror(EILSEQ);
	ucs4_to_utf16(wc, wbuf, wbufsize);
	return 4;
}

static int
utf32_wctomb(csconv_t *cv, ushort *wbuf, int wbufsize, uchar *buf, int bufsize)
{
	uint wc;

	if ((cv->flags & FLAG_USE_BOM) && !(cv->mode & UNICODE_MODE_BOM_DONE))
	{
		int r;

		cv->mode |= UNICODE_MODE_BOM_DONE;
		if (bufsize < 4)
			return seterror(E2BIG);
		if (cv->codepage == 12000) /* little endian */
			memcpy(buf, "\xFF\xFE\x00\x00", 4);
		else if (cv->codepage == 12001) /* big endian */
			memcpy(buf, "\x00\x00\xFE\xFF", 4);

		r = utf32_wctomb(cv, wbuf, wbufsize, buf + 4, bufsize - 4);
		if (r == -1)
			return -1;
		return r + 4;
	}

	if (bufsize < 4)
		return seterror(E2BIG);
	wc = utf16_to_ucs4(wbuf);
	if (cv->codepage == 12000) /* little endian */
	{
		buf[0] = wc & 0x000000FF;
		buf[1] = (wc & 0x0000FF00) >> 8;
		buf[2] = (wc & 0x00FF0000) >> 16;
		buf[3] = (wc & 0xFF000000) >> 24;
	}
	else if (cv->codepage == 12001) /* big endian */
	{
		buf[0] = (wc & 0xFF000000) >> 24;
		buf[1] = (wc & 0x00FF0000) >> 16;
		buf[2] = (wc & 0x0000FF00) >> 8;
		buf[3] = wc & 0x000000FF;
	}
	return 4;
}

/*
 * 50220: ISO 2022 Japanese with no halfwidth Katakana; Japanese (JIS)
 * 50221: ISO 2022 Japanese with halfwidth Katakana; Japanese (JIS-Allow
 *        1 byte Kana)
 * 50222: ISO 2022 Japanese JIS X 0201-1989; Japanese (JIS-Allow 1 byte
 *        Kana - SO/SI)
 *
 * MultiByteToWideChar() and WideCharToMultiByte() behave differently
 * depending on Windows version.  On XP, WideCharToMultiByte() doesn't
 * terminate result sequence with ascii escape.  But Vista does.
 * Use MLang instead.
 */

#define ISO2022_MODE(cs, shift) (((cs) << 8) | (shift))
#define ISO2022_MODE_CS(mode) (((mode) >> 8) & 0xFF)
#define ISO2022_MODE_SHIFT(mode) ((mode) & 0xFF)

#define ISO2022_SI  0
#define ISO2022_SO  1

/* shift in */
static const char iso2022_SI_seq[] = "\x0F";
/* shift out */
static const char iso2022_SO_seq[] = "\x0E";

typedef struct iso2022_esc_t iso2022_esc_t;
struct iso2022_esc_t {
	const char *esc;
	int esc_len;
	int len;
	int cs;
};

#define ISO2022JP_CS_ASCII            0
#define ISO2022JP_CS_JISX0201_ROMAN   1
#define ISO2022JP_CS_JISX0201_KANA    2
#define ISO2022JP_CS_JISX0208_1978    3
#define ISO2022JP_CS_JISX0208_1983    4
#define ISO2022JP_CS_JISX0212         5

static iso2022_esc_t iso2022jp_esc[] = {
	{"\x1B\x28\x42", 3, 1, ISO2022JP_CS_ASCII},
	{"\x1B\x28\x4A", 3, 1, ISO2022JP_CS_JISX0201_ROMAN},
	{"\x1B\x28\x49", 3, 1, ISO2022JP_CS_JISX0201_KANA},
	{"\x1B\x24\x40", 3, 2, ISO2022JP_CS_JISX0208_1983}, /* unify 1978 with 1983 */
	{"\x1B\x24\x42", 3, 2, ISO2022JP_CS_JISX0208_1983},
	{"\x1B\x24\x28\x44", 4, 2, ISO2022JP_CS_JISX0212},
	{NULL, 0, 0, 0}
};

static int
iso2022jp_mbtowc(csconv_t *cv, const uchar *buf, int bufsize, ushort *wbuf, int *wbufsize)
{
	iso2022_esc_t *iesc = iso2022jp_esc;
	char tmp[MB_CHAR_MAX];
	int insize;
	HRESULT hr;
	DWORD dummy = 0;
	int len;
	int esc_len;
	int cs;
	int shift;
	int i;

	if (buf[0] == 0x1B)
	{
		for (i = 0; iesc[i].esc != NULL; ++i)
		{
			esc_len = iesc[i].esc_len;
			if (bufsize < esc_len)
			{
				if (strncmp((char *)buf, iesc[i].esc, bufsize) == 0)
					return seterror(EINVAL);
			}
			else
			{
				if (strncmp((char *)buf, iesc[i].esc, esc_len) == 0)
				{
					cv->mode = ISO2022_MODE(iesc[i].cs, ISO2022_SI);
					*wbufsize = 0;
					return esc_len;
				}
			}
		}
		/* not supported escape sequence */
		return seterror(EILSEQ);
	}
	else if (buf[0] == iso2022_SO_seq[0])
	{
		cv->mode = ISO2022_MODE(ISO2022_MODE_CS(cv->mode), ISO2022_SO);
		*wbufsize = 0;
		return 1;
	}
	else if (buf[0] == iso2022_SI_seq[0])
	{
		cv->mode = ISO2022_MODE(ISO2022_MODE_CS(cv->mode), ISO2022_SI);
		*wbufsize = 0;
		return 1;
	}

	cs = ISO2022_MODE_CS(cv->mode);
	shift = ISO2022_MODE_SHIFT(cv->mode);

	/* reset the mode for informal sequence */
	if (buf[0] < 0x20)
	{
		cs = ISO2022JP_CS_ASCII;
		shift = ISO2022_SI;
	}

	len = iesc[cs].len;
	if (bufsize < len)
		return seterror(EINVAL);
	for (i = 0; i < len; ++i)
		if (!(buf[i] < 0x80))
			return seterror(EILSEQ);
	esc_len = iesc[cs].esc_len;
	memcpy(tmp, iesc[cs].esc, esc_len);
	if (shift == ISO2022_SO)
	{
		memcpy(tmp + esc_len, iso2022_SO_seq, 1);
		esc_len += 1;
	}
	memcpy(tmp + esc_len, buf, len);

	if ((cv->codepage == 50220 || cv->codepage == 50221
				|| cv->codepage == 50222) && shift == ISO2022_SO)
	{
		/* XXX: shift-out cannot be used for mbtowc (both kernel and
		 * mlang) */
		esc_len = iesc[ISO2022JP_CS_JISX0201_KANA].esc_len;
		memcpy(tmp, iesc[ISO2022JP_CS_JISX0201_KANA].esc, esc_len);
		memcpy(tmp + esc_len, buf, len);
	}

	insize = len + esc_len;
	hr = ConvertINetMultiByteToUnicode(&dummy, cv->codepage,
			(const char *)tmp, &insize, (wchar_t *)wbuf, wbufsize);
	if (hr != S_OK || insize != len + esc_len)
		return seterror(EILSEQ);

	/* Check for conversion error.  Assuming defaultChar is 0x3F. */
	/* ascii should be converted from ascii */
	if (wbuf[0] == buf[0]
			&& cv->mode != ISO2022_MODE(ISO2022JP_CS_ASCII, ISO2022_SI))
		return seterror(EILSEQ);

	/* reset the mode for informal sequence */
	if (cv->mode != ISO2022_MODE(cs, shift))
		cv->mode = ISO2022_MODE(cs, shift);

	return len;
}

static int
iso2022jp_wctomb(csconv_t *cv, ushort *wbuf, int wbufsize, uchar *buf, int bufsize)
{
	iso2022_esc_t *iesc = iso2022jp_esc;
	char tmp[MB_CHAR_MAX];
	int tmpsize = MB_CHAR_MAX;
	int insize = wbufsize;
	HRESULT hr;
	DWORD dummy = 0;
	int len;
	int esc_len;
	int cs;
	int shift;
	int i;

	/*
	 * MultiByte = [escape sequence] + character + [escape sequence]
	 *
	 * Whether trailing escape sequence is added depends on which API is
	 * used (kernel or MLang, and its version).
	 */
	hr = ConvertINetUnicodeToMultiByte(&dummy, cv->codepage,
			(const wchar_t *)wbuf, &wbufsize, tmp, &tmpsize);
	if (hr != S_OK || insize != wbufsize)
		return seterror(EILSEQ);
	else if (bufsize < tmpsize)
		return seterror(E2BIG);

	if (tmpsize == 1)
	{
		cs = ISO2022JP_CS_ASCII;
		esc_len = 0;
	}
	else
	{
		for (i = 1; iesc[i].esc != NULL; ++i)
		{
			esc_len = iesc[i].esc_len;
			if (strncmp(tmp, iesc[i].esc, esc_len) == 0)
			{
				cs = iesc[i].cs;
				break;
			}
		}
		if (iesc[i].esc == NULL)
			/* not supported escape sequence */
			return seterror(EILSEQ);
	}

	shift = ISO2022_SI;
	if (tmp[esc_len] == iso2022_SO_seq[0])
	{
		shift = ISO2022_SO;
		esc_len += 1;
	}

	len = iesc[cs].len;

	/* Check for converting error.  Assuming defaultChar is 0x3F. */
	/* ascii should be converted from ascii */
	if (cs == ISO2022JP_CS_ASCII && !(wbuf[0] < 0x80))
		return seterror(EILSEQ);
	else if (tmpsize < esc_len + len)
		return seterror(EILSEQ);

	if (cv->mode == ISO2022_MODE(cs, shift))
	{
		/* remove escape sequence */
		if (esc_len != 0)
			memmove(tmp, tmp + esc_len, len);
		esc_len = 0;
	}
	else
	{
		if (cs == ISO2022JP_CS_ASCII)
		{
			esc_len = iesc[ISO2022JP_CS_ASCII].esc_len;
			memmove(tmp + esc_len, tmp, len);
			memcpy(tmp, iesc[ISO2022JP_CS_ASCII].esc, esc_len);
		}
		if (ISO2022_MODE_SHIFT(cv->mode) == ISO2022_SO)
		{
			/* shift-in before changing to other mode */
			memmove(tmp + 1, tmp, len + esc_len);
			memcpy(tmp, iso2022_SI_seq, 1);
			esc_len += 1;
		}
	}

	if (bufsize < len + esc_len)
		return seterror(E2BIG);
	memcpy(buf, tmp, len + esc_len);
	cv->mode = ISO2022_MODE(cs, shift);
	return len + esc_len;
}

static int
iso2022jp_flush(csconv_t *cv, uchar *buf, int bufsize)
{
	iso2022_esc_t *iesc = iso2022jp_esc;
	int esc_len;

	if (cv->mode != ISO2022_MODE(ISO2022JP_CS_ASCII, ISO2022_SI))
	{
		esc_len = 0;
		if (ISO2022_MODE_SHIFT(cv->mode) != ISO2022_SI)
			esc_len += 1;
		if (ISO2022_MODE_CS(cv->mode) != ISO2022JP_CS_ASCII)
			esc_len += iesc[ISO2022JP_CS_ASCII].esc_len;
		if (bufsize < esc_len)
			return seterror(E2BIG);

		esc_len = 0;
		if (ISO2022_MODE_SHIFT(cv->mode) != ISO2022_SI)
		{
			memcpy(buf, iso2022_SI_seq, 1);
			esc_len += 1;
		}
		if (ISO2022_MODE_CS(cv->mode) != ISO2022JP_CS_ASCII)
		{
			memcpy(buf + esc_len, iesc[ISO2022JP_CS_ASCII].esc,
					iesc[ISO2022JP_CS_ASCII].esc_len);
			esc_len += iesc[ISO2022JP_CS_ASCII].esc_len;
		}
		return esc_len;
	}
	return 0;
}

static void process_file(iconv_t cd, FILE *in, FILE *out)
{
	char inbuf[BUFSIZ];
	char outbuf[BUFSIZ];
	const char *pin;
	char *pout;
	size_t inbytesleft = 0;
	size_t outbytesleft;
	size_t rest = 0;
	size_t r;

	while ((!feof(in) &&
			(inbytesleft=fread(inbuf+rest, 1, sizeof(inbuf)-rest, in)) != 0)
				|| rest != 0) {
		inbytesleft += rest;
		pin = inbuf;
		pout = outbuf;
		outbytesleft = sizeof(outbuf);
		r = iconv(cd, &pin, &inbytesleft, &pout, &outbytesleft);
		if (r == (size_t)(-1) && errno != E2BIG &&
				(errno != EINVAL || feof(in)))
			bb_perror_msg_and_die("conversion error");
		fwrite(outbuf, 1, sizeof(outbuf) - outbytesleft, out);
		memmove(inbuf, pin, inbytesleft);
		rest = inbytesleft;
	}
	pout = outbuf;
	outbytesleft = sizeof(outbuf);
	r = iconv(cd, NULL, NULL, &pout, &outbytesleft);
	fwrite(outbuf, 1, sizeof(outbuf) - outbytesleft, out);
	if (r == (size_t)(-1))
		bb_perror_msg_and_die("conversion error");
}

enum {
	OPT_f = (1 << 0),
	OPT_t = (1 << 1),
	OPT_l = (1 << 2),
	OPT_c = (1 << 3),
	OPT_o = (1 << 4),
};

int iconv_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int iconv_main(int argc, char **argv)
{
	const char *fromcode = "", *tocode = "", *outfile;
	int i, opt;
	iconv_t cd;
	FILE *in;
	FILE *out = stdout;

	opt = getopt32(argv, "f:t:lco:", &fromcode, &tocode, &outfile);

	if (opt & OPT_l) {
		const char *alias = cp_alias;
		while (*alias) {
			printf("%s\n", alias);
			alias += strlen(alias) + 1;
		}
		return 0;
	}

	if (opt & OPT_o)
		out = xfopen(outfile, "wb");

	if (opt & OPT_c)
		tocode = xasprintf("%s//IGNORE", tocode);

	cd = iconv_open(tocode, fromcode);
	if (cd == (iconv_t)(-1))
		bb_perror_msg_and_die("iconv_open error");

	if (optind == argc)
		argv[argc++] = (char *)"-";

	for (i=optind; i<argc; ++i) {
		if (argv[i][0] == '-' && argv[i][1] == '\0')
			in = stdin;
		else
			in = xfopen(argv[optind], "rb");
		process_file(cd, in, out);
		fclose(in);
	}

	if (ENABLE_FEATURE_CLEAN_UP)
		iconv_close(cd);
	return 0;
}
