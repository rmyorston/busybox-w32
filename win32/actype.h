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

/* config options:
 *
 * ACTYPE_ENUM (0 or 1) decides whether actype_t is enum which can be used
 * to test match of specific classes, or a slightly more efficient uintptr_t
 * which is a direct function pointer to isalnum, etc, and without a way to
 * test match of specific classes (just like wctype/iswctype).
 *
 * Both modes return 0 for actype("UNKNOWN-NAME"), and isactype(c, 0),
 * and both should be compliant if POSIX had actype/isactype/actype_t.
 *
 * The implementation is O(1) function table access either way, where with
 * enum this happens at isactype, while without enum the function is chosen
 * at actype/actail, so isactype and especially isactype_not0 are very quick.
 */
#define ACTYPE_ENUM 0


typedef int (*_isactype_t)(int);  /* isalpha et-al prototype */

#if ACTYPE_ENUM
  typedef enum {
	AC_NONE = 0,
	AC_ALNUM, AC_ALPHA, AC_BLANK, AC_CNTRL, AC_DIGIT, AC_GRAPH,
	AC_LOWER, AC_PRINT, AC_PUNCT, AC_SPACE, AC_UPPER, AC_XDIGIT
  } actype_t;
  extern const _isactype_t _actype_fns[];
  #define _isactype_not0(c, t) (_actype_fns[t](c))
#else
  #include <stdint.h>  /* uintptr_t */
  typedef uintptr_t actype_t;
  #define _isactype_not0(c, t) (((_isactype_t)t)(c))
#endif

extern actype_t actype(const char *name);
extern int isactype(int c, actype_t t);

/* actype above is the official prototype, but in practice we invoke actail */
#define actype(name) (actail((name), (int*)0))


/* extensions */

/* same as isactype but faster, and will crash if t==0.
 * useful if isactype is needed multiple times with the same non-0 t.
 */
#define isactype_not0(c, t) _isactype_not0((c), (t))

/* if str starts with NAME:]... and actype("NAME") != 0:
 *   set *len=strlen("NAME:]") and return non-0 actype("NAME").
 * else return 0.
 * useful in typical char-class parsing scenarios.
 * if len is NULL: identical to actype.
 */
extern actype_t actail(const char *str, int *len);

#endif  /* ACTYPE_H */
