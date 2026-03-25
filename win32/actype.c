#include <ctype.h>
#include <string.h>

#include "actype.h"

/* don't include libbb.h which redefines isalpha etc - we use the platform's */
int index_in_strings(const char *strings, const char *key);

/* isblank is c99 and not always available. just provide our own. */
#define isblank ac_isblank
static int ac_isblank(int c)
{
	return c == ' ' || c == '\t';
}

int isactype(int c, actype_t t)
{
	switch (t) {
	case CCLASS_NONE:  /* fallthrough */
	default:           return 0;

	case CCLASS_ALNUM: return isalnum(c);
	case CCLASS_ALPHA: return isalpha(c);
	case CCLASS_BLANK: return isblank(c);
	case CCLASS_CNTRL: return iscntrl(c);
	case CCLASS_DIGIT: return isdigit(c);
	case CCLASS_GRAPH: return isgraph(c);
	case CCLASS_LOWER: return islower(c);
	case CCLASS_PRINT: return isprint(c);
	case CCLASS_PUNCT: return ispunct(c);
	case CCLASS_SPACE: return isspace(c);
	case CCLASS_UPPER: return isupper(c);
	case CCLASS_XDIGIT: return isxdigit(c);
	}
}

actype_t actype(const char *name)
{
	return 1 + index_in_strings(CHAR_CLASSES, name);
}

actype_t actail(const char *s, int *len)
{
	char buf[8] = {0};
	int clen = 5 + (*s == 'x');  /* all 5 except xdigit 6 */
	actype_t t;

	strncpy(buf, s, clen);
	t = actype(buf);
	if (!t || s[clen++] != ':' || s[clen++] != ']')
		return 0;

	*len = clen;
	return t;
}
