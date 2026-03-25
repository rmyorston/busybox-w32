#ifndef ACTYPE_H
#define ACTYPE_H

/* actype/isactype: char equivalent to C99/POSIX wctype/iswctype.
 * actype: char-class type from name.
 * isactype: test if char c is of type t.
 *
 * actype returns integral actype_t, which is 0 only for unknown name.
 * isactype returns 0 if t==0 or c is not type t, else non-0.
 * valid c is EOF or unsigned char. valid t is any value from actype/actail.
 */

#define CHAR_CLASSES \
			"alnum\0alpha\0blank\0cntrl\0digit\0graph\0" \
			"lower\0print\0punct\0space\0upper\0xdigit\0"

typedef enum {
	CCLASS_NONE = 0,
	CCLASS_ALNUM, CCLASS_ALPHA, CCLASS_BLANK, CCLASS_CNTRL,
	CCLASS_DIGIT, CCLASS_GRAPH, CCLASS_LOWER, CCLASS_PRINT,
	CCLASS_PUNCT, CCLASS_SPACE, CCLASS_UPPER, CCLASS_XDIGIT
} actype_t;

extern actype_t actype(const char *name);
extern int isactype(int c, actype_t t);


/* extensions */

/* same as isactype but faster, and may crash if t==0.
 * useful if isactype is needed multiple times with the same non-0 t.
 */
#define isactype_not0(c, t) (isactype((c), (t)))

/* if str starts with NAME:]... and actype("NAME") != 0:
 *   set *len=strlen("NAME:]") and return non-0 actype("NAME").
 * else return 0.
 * useful in typical char-class parsing scenarios.
 */
extern actype_t actail(const char *str, int *len);

#endif  /* ACTYPE_H */
