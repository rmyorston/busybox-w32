#define CHAR_CLASSES \
			"alnum\0alpha\0blank\0cntrl\0digit\0graph\0" \
			"lower\0print\0punct\0space\0upper\0xdigit\0"

enum {
	CCLASS_ALNUM, CCLASS_ALPHA, CCLASS_BLANK, CCLASS_CNTRL,
	CCLASS_DIGIT, CCLASS_GRAPH, CCLASS_LOWER, CCLASS_PRINT,
	CCLASS_PUNCT, CCLASS_SPACE, CCLASS_UPPER, CCLASS_XDIGIT
};

extern int match_class(const char *name);
