/* vi: set sw=4 ts=4: */
/*
 * make implementation for BusyBox
 *
 * Based on public domain POSIX make:  https://frippery.org/make
 */
//config:config MAKE
//config:	bool "make (18 kb)"
//config:	default n
//config:	help
//config:	The make command can be used to maintain files that depend on
//config:	other files. Normally it's used to build programs from source
//config:	code but it can be used in other situations too.
//config:
//config:config PDPMAKE
//config:	bool "pdpmake (18 kb)"
//config:	default n
//config:	help
//config:	Alias for "make"
//config:
//config:config FEATURE_MAKE_POSIX
//config:	bool "Runtime enforcement of POSIX"
//config:	default n
//config:	depends on MAKE || PDPMAKE
//config:	help
//config:	Allow strict enforcement of POSIX compliance at runtime by:
//config:	- .POSIX special target in makefile
//config:	- '--posix' command line option
//config:	- PDPMAKE_POSIXLY_CORRECT environment variable
//config:	Enable this if you want to check whether your makefiles are
//config:	POSIX compliant.  This adds about 1.7 kb.
//config:
//config:choice
//config:	prompt "Default POSIX level to enforce"
//config:	depends on FEATURE_MAKE_POSIX
//config:	default FEATURE_MAKE_POSIX_2017
//config:
//config:config FEATURE_MAKE_POSIX_2017
//config:	bool "2017"
//config:
//config:config FEATURE_MAKE_POSIX_2024
//config:	bool "2024"
//config:
//config:endchoice

//applet:IF_MAKE(APPLET(make, BB_DIR_USR_BIN, BB_SUID_DROP))
//applet:IF_PDPMAKE(APPLET_ODDNAME(pdpmake, make, BB_DIR_USR_BIN, BB_SUID_DROP, make))

//kbuild:lib-$(CONFIG_MAKE) += make.o
//kbuild:lib-$(CONFIG_PDPMAKE) += make.o

//usage:#define make_trivial_usage
//usage:	IF_FEATURE_MAKE_POSIX(
//usage:       "[--posix] [-C DIR] [-f FILE] [-j NUM] [-x PRAG] [-eiknpqrsSt] [MACRO[:[:[:]]]=VAL]... [TARGET]..."
//usage:	)
//usage:	IF_NOT_FEATURE_MAKE_POSIX(
//usage:       "[-C DIR] [-f FILE] [-j NUM] [-eiknpqrsSt] [MACRO[:[:[:]]]=VAL]... [TARGET]..."
//usage:	)
//usage:#define make_full_usage "\n\n"
//usage:       "Maintain files based on their dependencies\n"
//usage:	IF_FEATURE_MAKE_POSIX(
//usage:     "\n    --posix  Enforce POSIX mode"
//usage:	)
//usage:     "\n    -C DIR   Change to DIR"
//usage:     "\n    -f FILE  Makefile"
//usage:     "\n    -j NUM   Jobs to run in parallel (not implemented)"
//usage:	IF_FEATURE_MAKE_POSIX(
//usage:     "\n    -x PRAG  Make POSIX mode less strict"
//usage:	)
//usage:     "\n    -e       Environment variables override macros in makefiles"
//usage:     "\n    -i       Ignore exit status"
//usage:     "\n    -k       Continue on error"
//usage:     "\n    -n       Dry run"
//usage:     "\n    -p       Print macros and targets"
//usage:     "\n    -q       Query target; exit status 1 if not up to date"
//usage:     "\n    -r       Don't use built-in rules"
//usage:     "\n    -s       Make silently"
//usage:     "\n    -S       Stop on error"
//usage:     "\n    -t       Touch files instead of making them"
//usage:	IF_FEATURE_MAKE_POSIX(
//usage:     "\n\nThis build supports: non-POSIX extensions, POSIX 2024, POSIX 2017"
//usage:	)
//usage:	IF_FEATURE_MAKE_POSIX_2017(
//usage:     "\nIn strict POSIX mode the 2017 standard is enforced by default"
//usage:	)
//usage:	IF_FEATURE_MAKE_POSIX_2024(
//usage:     "\nIn strict POSIX mode the 2024 standard is enforced by default"
//usage:	)

#include "libbb.h"
#include "bb_archive.h"
#include "common_bufsiz.h"
#include <glob.h>

// Supported POSIX levels
#define STD_POSIX_2017 0
#define STD_POSIX_2024 1

#define POSIX_2017 (posix && posix_level == STD_POSIX_2017)

#if ENABLE_FEATURE_MAKE_POSIX_2017
# define DEFAULT_POSIX_LEVEL STD_POSIX_2017
#else
# define DEFAULT_POSIX_LEVEL STD_POSIX_2024
#endif

#define OPTSTR1 "eij:+knqrsSt"
#if ENABLE_FEATURE_MAKE_POSIX
#define OPTSTR2 "pf:*C:*x:*"
#else
#define OPTSTR2 "pf:*C:*"
#endif

enum {
	OPTBIT_e = 0,
	OPTBIT_i,
	OPTBIT_j,
	OPTBIT_k,
	OPTBIT_n,
	OPTBIT_q,
	OPTBIT_r,
	OPTBIT_s,
	OPTBIT_S,
	OPTBIT_t,
	OPTBIT_p,
	OPTBIT_f,
	OPTBIT_C,
	IF_FEATURE_MAKE_POSIX(OPTBIT_x,)
	OPTBIT_precious,
	OPTBIT_phony,
	OPTBIT_include,
	OPTBIT_make,

	OPT_e = (1 << OPTBIT_e),
	OPT_i = (1 << OPTBIT_i),
	OPT_j = (1 << OPTBIT_j),
	OPT_k = (1 << OPTBIT_k),
	OPT_n = (1 << OPTBIT_n),
	OPT_q = (1 << OPTBIT_q),
	OPT_r = (1 << OPTBIT_r),
	OPT_s = (1 << OPTBIT_s),
	OPT_S = (1 << OPTBIT_S),
	OPT_t = (1 << OPTBIT_t),
	// These options aren't allowed in MAKEFLAGS
	OPT_p = (1 << OPTBIT_p),
	OPT_f = (1 << OPTBIT_f),
	OPT_C = (1 << OPTBIT_C),
	OPT_x = IF_FEATURE_MAKE_POSIX((1 << OPTBIT_x)) + 0,
	// The following aren't command line options and must be last
	OPT_precious = (1 << OPTBIT_precious),
	OPT_phony = (1 << OPTBIT_phony),
	OPT_include = (1 << OPTBIT_include),
	OPT_make = (1 << OPTBIT_make),
};

// Options in OPTSTR1 that aren't included in MAKEFLAGS
#define OPT_MASK  (~OPT_S)

#define useenv    (opts & OPT_e)
#define ignore    (opts & OPT_i)
#define errcont   (opts & OPT_k)
#define dryrun    (opts & OPT_n)
#define print     (opts & OPT_p)
#define quest     (opts & OPT_q)
#define norules   (opts & OPT_r)
#define silent    (opts & OPT_s)
#define dotouch   (opts & OPT_t)
#define precious  (opts & OPT_precious)
#define doinclude (opts & OPT_include)
#define domake    (opts & OPT_make)

// A name.  This represents a file, either to be made, or pre-existing.
struct name {
	struct name *n_next;	// Next in the list of names
	char *n_name;			// Called
	struct rule *n_rule;	// Rules to build this (prerequisites/commands)
	struct timespec n_tim;	// Modification time of this name
	uint16_t n_flag;		// Info about the name
};

#define N_DOING		0x01	// Name in process of being built
#define N_DONE		0x02	// Name looked at
#define N_TARGET	0x04	// Name is a target
#define N_PRECIOUS	0x08	// Target is precious
#define N_DOUBLE	0x10	// Double-colon target
#define N_SILENT	0x20	// Build target silently
#define N_IGNORE	0x40	// Ignore build errors
#define N_SPECIAL	0x80	// Special target
#define N_MARK		0x100	// Mark for deduplication
#define N_PHONY		0x200	// Name is a phony target
#define N_INFERENCE	0x400	// Inference rule

// List of rules to build a target
struct rule {
	struct rule *r_next;	// Next rule
	struct depend *r_dep;	// Prerequisites for this rule
	struct cmd *r_cmd;		// Commands for this rule
};

// NOTE: the layout of the following two structures must be compatible.
// Also, their first two members must be compatible with llist_t.

// List of prerequisites for a rule
struct depend {
	struct depend *d_next;	// Next prerequisite
	struct name *d_name;	// Name of prerequisite
	int d_refcnt;			// Reference count
};

// List of commands for a rule
struct cmd {
	struct cmd *c_next;		// Next command line
	char *c_cmd;			// Text of command line
	int c_refcnt;			// Reference count
	const char *c_makefile;	// Makefile in which command was defined
	int c_dispno;			// Line number within makefile
};

// Macro storage
struct macro {
	struct macro *m_next;	// Next variable
	char *m_name;			// Its name
	char *m_val;			// Its value
	bool m_immediate;		// Immediate-expansion macro set using ::=
	bool m_flag;			// Infinite loop check
	uint8_t m_level;		// Level at which macro was created
};

// Flags passed to setmacro()
#define M_IMMEDIATE  0x08	// immediate-expansion macro is being defined
#define M_VALID      0x10	// assert macro name is valid
#define M_ENVIRON    0x20	// macro imported from environment

// Constants for PRAGMA.  Order must match strings in set_pragma().
enum {
	BIT_MACRO_NAME = 0,
	BIT_TARGET_NAME,
	BIT_COMMAND_COMMENT,
	BIT_EMPTY_SUFFIX,
#if ENABLE_PLATFORM_MINGW32
	BIT_WINDOWS,
#endif
	BIT_POSIX_2017,
	BIT_POSIX_2024,
	BIT_POSIX_202X,

	P_MACRO_NAME = (1 << BIT_MACRO_NAME),
	P_TARGET_NAME = (1 << BIT_TARGET_NAME),
	P_COMMAND_COMMENT = (1 << BIT_COMMAND_COMMENT),
	P_EMPTY_SUFFIX = (1 << BIT_EMPTY_SUFFIX),
#if ENABLE_PLATFORM_MINGW32
	P_WINDOWS = (1 << BIT_WINDOWS),
#endif
};

// Status of make()
#define MAKE_FAILURE		0x01
#define MAKE_DIDSOMETHING	0x02

// Return TRUE if c is allowed in a POSIX 2017 macro or target name
#define ispname(c) (isalpha(c) || isdigit(c) || c == '.' || c == '_')
// Return TRUE if c is in the POSIX 'portable filename character set'
#define isfname(c) (ispname(c) || c == '-')

#define HTABSIZE 39

struct globals {
	uint32_t opts;
	const char *makefile;
	llist_t *makefiles;
	llist_t *dirs;
	struct name *namehead[HTABSIZE];
	struct macro *macrohead[HTABSIZE];
	struct name *firstname;
	struct name *target;
	time_t ar_mtime;
	int lineno;	// Physical line number in file
	int dispno;	// Line number for display purposes
	struct cmd *curr_cmd;
	const char *rulepos;
	int rule_idx;
#define IF_MAX 10
	uint8_t clevel;
	uint8_t cstate[IF_MAX + 1];
	int numjobs;
#if ENABLE_FEATURE_MAKE_POSIX
	bool posix;
	bool seen_first;
	llist_t *pragmas;
	unsigned char pragma;
	unsigned char posix_level;
#endif
} FIX_ALIASING;

#define G (*(struct globals*)bb_common_bufsiz1)
#define INIT_G() do { \
	setup_common_bufsiz(); \
} while (0)

#define opts		(G.opts)
#define makefile	(G.makefile)
#define makefiles	(G.makefiles)
#define dirs		(G.dirs)
#define namehead	(G.namehead)
#define macrohead	(G.macrohead)
#define firstname	(G.firstname)
#define target		(G.target)
#define ar_mtime	(G.ar_mtime)
#define lineno		(G.lineno)
#define dispno		(G.dispno)
#define curr_cmd	(G.curr_cmd)
#define rulepos		(G.rulepos)
#define rule_idx	(G.rule_idx)
#define clevel		(G.clevel)
#define cstate		(G.cstate)
#define numjobs		(G.numjobs)
#if ENABLE_FEATURE_MAKE_POSIX
#define posix		(G.posix)
#define seen_first	(G.seen_first)
#define pragmas		(G.pragmas)
#define pragma		(G.pragma)
#define posix_level	(G.posix_level)
#else
#define posix		0
#define pragma		0
#define posix_level	DEFAULT_POSIX_LEVEL
#endif

static int make(struct name *np, int level);
static struct name *dyndep(struct name *np, struct rule *infrule,
								const char **ptsuff);

/*
 * Utility functions.
 */

/*
 * Print message, with makefile and line number if possible.
 */
static void
vwarning(FILE *stream, const char *msg, va_list list)
{
	const char *m = NULL;
	int d = 0;

	if (curr_cmd) {
		m = curr_cmd->c_makefile;
		d = curr_cmd->c_dispno;
	} else if (makefile) {
		m = makefile;
		d = dispno;
	}

	fprintf(stream, "%s: ", applet_name);
	if (m)
		fprintf(stream, "(%s:%d): ", m, d);
	vfprintf(stream, msg, list);
	fputc('\n', stream);
}

/*
 * Diagnostic handler.  Print message to standard error.
 */
static void
diagnostic(const char *msg, ...)
{
	va_list list;

	va_start(list, msg);
	vwarning(stderr, msg, list);
	va_end(list);
}

/*
 * Error handler.  Print message and exit.
 */
static void error(const char *msg, ...) NORETURN;
static void
error(const char *msg, ...)
{
	va_list list;

	va_start(list, msg);
	vwarning(stderr, msg, list);
	va_end(list);
	exit(2);
}

static void error_unexpected(const char *s) NORETURN;
static void
error_unexpected(const char *s)
{
	error("unexpected %s", s);
}

static void error_in_inference_rule(const char *s) NORETURN;
static void
error_in_inference_rule(const char *s)
{
	error("%s in inference rule", s);
}

static void
error_not_allowed(const char *s, const char *t)
{
	error("%s not allowed for %s", s, t);
}

static void
warning(const char *msg, ...)
{
	va_list list;

	va_start(list, msg);
	vwarning(stdout, msg, list);
	va_end(list);
}

static char *
auto_concat(const char *s1, const char *s2)
{
	return auto_string(xasprintf("%s%s", s1, s2));
}

#if !ENABLE_PLATFORM_MINGW32
/*
 * Append a word to a space-separated string of words.  The first
 * call should use a NULL pointer for str, subsequent calls should
 * pass an allocated string which will be freed.
 */
static char *
xappendword(const char *str, const char *word)
{
	char *newstr = str ? xasprintf("%s %s", str, word) : xstrdup(word);
	free((void *)str);
	return newstr;
}
#endif

static unsigned int
getbucket(const char *name)
{
	unsigned int hashval = 0;
	const unsigned char *p = (unsigned char *)name;

	while (*p)
		hashval ^= (hashval << 5) + (hashval >> 2) + *p++;
	return hashval % HTABSIZE;
}

/*
 * Add a prerequisite to the end of the supplied list.
 */
static void
newdep(struct depend **dphead, struct name *np)
{
	while (*dphead)
		dphead = &(*dphead)->d_next;
	*dphead = xzalloc(sizeof(struct depend));
	/*(*dphead)->d_next = NULL; - xzalloc did it */
	(*dphead)->d_name = np;
	/*(*dphead)->d_refcnt = 0; */
}

static void
freedeps(struct depend *dp)
{
	if (dp && --dp->d_refcnt <= 0)
		llist_free((llist_t *)dp, NULL);
}

/*
 * Add a command to the end of the supplied list of commands.
 */
static void
newcmd(struct cmd **cphead, char *str)
{
	while (isspace(*str))
		str++;

	while (*cphead)
		cphead = &(*cphead)->c_next;
	*cphead = xzalloc(sizeof(struct cmd));
	/*(*cphead)->c_next = NULL; - xzalloc did it */
	(*cphead)->c_cmd = xstrdup(str);
	/*(*cphead)->c_refcnt = 0; */
	(*cphead)->c_makefile = xstrdup(makefile);
	(*cphead)->c_dispno = dispno;
}

static void
freecmds(struct cmd *cp)
{
	struct cmd *nextcp;

	if (cp && --cp->c_refcnt <= 0) {
		for (; cp; cp = nextcp) {
			nextcp = cp->c_next;
			free(cp->c_cmd);
			free((void *)cp->c_makefile);
			free(cp);
		}
	}
}

static struct name *
findname(const char *name)
{
	struct name *np = namehead[getbucket(name)];
	return (struct name *)llist_find_str((llist_t *)np, name);
}

static int
check_name(const char *name)
{
	const char *s;

#if ENABLE_PLATFORM_MINGW32
	if (!posix || (pragma & P_WINDOWS)) {
		if (isalpha(name[0]) && name[1] == ':' && name[2] == '/') {
			name += 3;
		}
	}
#endif
	if (!posix) {
		for (s = name; *s; ++s) {
			if (*s == '=')
				return FALSE;
		}
		return TRUE;
	}

	for (s = name; *s; ++s) {
		if ((pragma & P_TARGET_NAME) || !POSIX_2017 ?
				!(isfname(*s) || *s == '/') : !ispname(*s))
			return FALSE;
	}
	return TRUE;
}

static char *splitlib(const char *name, char **member);

static int
is_valid_target(const char *name)
{
	char *archive, *member = NULL;
	int ret;

	/* Names of the form 'lib(member)' are referred to as 'expressions'
	 * in POSIX and are subjected to special treatment.  The 'lib'
	 * and 'member' elements must each be a valid target name. */
	archive = splitlib(name, &member);
	ret = check_name(archive) && (member == NULL || check_name(member));
	free(archive);

	return ret;
}

#if ENABLE_FEATURE_MAKE_POSIX
static int
potentially_valid_target(const char *name)
{
	int ret = FALSE;

	if (!(pragma & P_TARGET_NAME)) {
		pragma |= P_TARGET_NAME;
		ret = is_valid_target(name);
		pragma &= ~P_TARGET_NAME;
	}
	return ret;
}
#endif

/*
 * Intern a name.  Return a pointer to the name struct
 */
static struct name *
newname(const char *name)
{
	struct name *np = findname(name);

	if (np == NULL) {
		unsigned int bucket;

		if (!is_valid_target(name))
#if ENABLE_FEATURE_MAKE_POSIX
			error("invalid target name '%s'%s", name,
					potentially_valid_target(name) ?
						": allow with pragma target_name" : "");
#else
			error("invalid target name '%s'", name);
#endif

		bucket = getbucket(name);
		np = xzalloc(sizeof(struct name));
		np->n_next = namehead[bucket];
		namehead[bucket] = np;
		np->n_name = xstrdup(name);
		/*np->n_rule = NULL; - xzalloc did it */
		/*np->n_tim = (struct timespec){0, 0}; */
		/*np->n_flag = 0; */
	}
	return np;
}

/*
 * Return the commands on the first rule that has them or NULL.
 */
static struct cmd *
getcmd(struct name *np)
{
	struct rule *rp;

	if (np == NULL)
		return NULL;

	for (rp = np->n_rule; rp; rp = rp->r_next)
		if (rp->r_cmd)
			return rp->r_cmd;
	return NULL;
}

#if ENABLE_FEATURE_CLEAN_UP
static void
freenames(void)
{
	int i;
	struct name *np, *nextnp;

	for (i = 0; i < HTABSIZE; i++) {
		for (np = namehead[i]; np; np = nextnp) {
			nextnp = np->n_next;
			free(np->n_name);
			freerules(np->n_rule);
			free(np);
		}
	}
}
#endif

static void
freerules(struct rule *rp)
{
	struct rule *nextrp;

	for (; rp; rp = nextrp) {
		nextrp = rp->r_next;
		freedeps(rp->r_dep);
		freecmds(rp->r_cmd);
		free(rp);
	}
}

static void *
inc_ref(void *vp)
{
	if (vp) {
		struct depend *dp = vp;
		if (dp->d_refcnt == INT_MAX)
			bb_die_memory_exhausted();
		dp->d_refcnt++;
	}
	return vp;
}

#if ENABLE_FEATURE_MAKE_POSIX
// Order must match constants above.
// POSIX levels must be last and in increasing order
static const char *p_name =
	"macro_name\0"
	"target_name\0"
	"command_comment\0"
	"empty_suffix\0"
#if ENABLE_PLATFORM_MINGW32
	"windows\0"
#endif
	"posix_2017\0"
	"posix_2024\0"
	"posix_202x\0";

static void
set_pragma(const char *name)
{
	int idx = index_in_strings(p_name, name);

	if (idx != -1) {
		if (idx >= BIT_POSIX_2017) {
			// POSIX level is stored in a separate variable.
			// No bits in 'pragma' are used.
			if (posix_level == DEFAULT_POSIX_LEVEL) {
				posix_level = idx - BIT_POSIX_2017;
				if (posix_level > STD_POSIX_2024)
					posix_level = STD_POSIX_2024;
			} else if (posix_level != idx - BIT_POSIX_2017)
				warning("unable to change POSIX level");
		} else {
			pragma |= 1 << idx;
		}
		return;
	}
	warning("invalid pragma '%s'", name);
}

static void
pragmas_to_env(void)
{
	int i;
	char *val = NULL;

	for (i = 0; i < BIT_POSIX_2017; ++i) {
		if ((pragma & (1 << i)))
			val = xappendword(val, nth_string(p_name, i));
	}

	if (posix_level != DEFAULT_POSIX_LEVEL)
		val = xappendword(val,
					nth_string(p_name, BIT_POSIX_2017 + posix_level));

	if (val) {
		setenv("PDPMAKE_PRAGMAS", val, 1);
		free(val);
	}
}
#endif

/*
 * Add a new rule to a target.  This checks to see if commands already
 * exist for the target.  If flag is TRUE the target can have multiple
 * rules with commands (double-colon rules).
 *
 * i)  If the name is a special target and there are no prerequisites
 *     or commands to be added remove all prerequisites and commands.
 *     This is necessary when clearing a built-in inference rule.
 * ii) If name is a special target and has commands, replace them.
 *     This is for redefining commands for an inference rule.
 */
static void
addrule(struct name *np, struct depend *dp, struct cmd *cp, int flag)
{
	struct rule *rp;
	struct rule **rpp;
	struct cmd *old_cp;

	// Can't mix single-colon and double-colon rules
	if (!posix && (np->n_flag & N_TARGET)) {
		if (!(np->n_flag & N_DOUBLE) != !flag)		// like xor
			error("inconsistent rules for target %s", np->n_name);
	}

	// Clear out prerequisites and commands
	if ((np->n_flag & N_SPECIAL) && !dp && !cp) {
		if (strcmp(np->n_name, ".PHONY") == 0)
			return;
		freerules(np->n_rule);
		np->n_rule = NULL;
		return;
	}

	if (cp && !(np->n_flag & N_DOUBLE) && (old_cp = getcmd(np))) {
		// Handle the inference rule redefinition case
		// .DEFAULT rule can also be redefined (as an extension).
		if ((np->n_flag & N_INFERENCE)
				&& !(posix && (np->n_flag & N_SPECIAL))
		) {
			freerules(np->n_rule);
			np->n_rule = NULL;
		} else {
			// We're adding commands to a single colon rule which
			// already has some.  Clear the old ones first.
			warning("overriding rule for target %s", np->n_name);
			curr_cmd = old_cp;
			warning("previous rule for target %s", np->n_name);
			curr_cmd = NULL;

			for (rp = np->n_rule; rp; rp = rp->r_next) {
				freecmds(rp->r_cmd);
				rp->r_cmd = NULL;
			}
		}
	}

	rpp = &np->n_rule;
	while (*rpp)
		rpp = &(*rpp)->r_next;

	*rpp = rp = xzalloc(sizeof(struct rule));
	/*rp->r_next = NULL; - xzalloc did it */
	rp->r_dep = inc_ref(dp);
	rp->r_cmd = inc_ref(cp);

	np->n_flag |= N_TARGET;
	if (flag)
		np->n_flag |= N_DOUBLE;
#if ENABLE_FEATURE_MAKE_POSIX
	if (strcmp(np->n_name, ".PRAGMA") == 0) {
		for (; dp; dp = dp->d_next) {
			set_pragma(dp->d_name->n_name);
		}
		pragmas_to_env();
	}
#endif
}

/*
 * Macro control for make
 */
static struct macro *
getmp(const char *name)
{
	struct macro *mp = macrohead[getbucket(name)];
	return (struct macro *)llist_find_str((llist_t *)mp, name);
}

static int
is_valid_macro(const char *name)
{
	const char *s;
	for (s = name; *s; ++s) {
		// In POSIX mode only a limited set of characters are guaranteed
		// to be allowed in macro names.
		if (posix) {
			// Find the appropriate character set
			if ((pragma & P_MACRO_NAME) || !POSIX_2017 ?
				!isfname(*s) : !ispname(*s))
			return FALSE;
		}
		// As an extension allow anything that can get through the
		// input parser, apart from the following.
		if (*s == '=' || isblank(*s) || iscntrl(*s))
			return FALSE;
	}
	return TRUE;
}

#if ENABLE_FEATURE_MAKE_POSIX
static int
potentially_valid_macro(const char *name)
{
	int ret = FALSE;

	if (!(pragma & P_MACRO_NAME)) {
		pragma |= P_MACRO_NAME;
		ret = is_valid_macro(name);
		pragma &= ~P_MACRO_NAME;
	}
	return ret;
}
#endif

static void
setmacro(const char *name, const char *val, int level)
{
	struct macro *mp;
	bool valid = level & M_VALID;
	bool from_env = level & M_ENVIRON;
	bool immediate = level & M_IMMEDIATE;

	level &= ~(M_IMMEDIATE | M_VALID | M_ENVIRON);
	mp = getmp(name);
	if (mp) {
		// Don't replace existing macro from a lower level
		if (level > mp->m_level)
			return;

		// Replace existing macro
		free(mp->m_val);
	} else {
		// If not defined, allocate space for new
		unsigned int bucket;

		if (!valid && !is_valid_macro(name)) {
			// Silently drop invalid names from the environment
			if (from_env)
				return;
#if ENABLE_FEATURE_MAKE_POSIX
			error("invalid macro name '%s'%s", name,
					potentially_valid_macro(name) ?
					": allow with pragma macro_name" : "");
#else
			error("invalid macro name '%s'", name);
#endif
		}

		bucket = getbucket(name);
		mp = xzalloc(sizeof(struct macro));
		mp->m_next = macrohead[bucket];
		macrohead[bucket] = mp;
		/* mp->m_flag = FALSE; - xzalloc did it */
		mp->m_name = xstrdup(name);
	}
	mp->m_immediate = immediate;
	mp->m_level = level;
	mp->m_val = xstrdup(val ? val : "");
}

#if ENABLE_FEATURE_CLEAN_UP
static void
freemacros(void)
{
	int i;
	struct macro *mp, *nextmp;

	for (i = 0; i < HTABSIZE; i++) {
		for (mp = macrohead[i]; mp; mp = nextmp) {
			nextmp = mp->m_next;
			free(mp->m_name);
			free(mp->m_val);
			free(mp);
		}
	}
}
#endif

/*
 * Get modification time of file or archive member
 */
static void FAST_FUNC
record_mtime(const file_header_t *file_header)
{
	ar_mtime = file_header->mtime;
}

static time_t
artime(const char *archive, const char *member)
{
	archive_handle_t *archive_handle;

	ar_mtime = 0;
	archive_handle = init_handle();
	archive_handle->src_fd = open(archive, O_RDONLY);
	if (archive_handle->src_fd != -1) {
		archive_handle->action_header = record_mtime;
		archive_handle->filter = filter_accept_list;
		llist_add_to_end(&archive_handle->accept, (void *)member);
		unpack_ar_archive(archive_handle);
		close(archive_handle->src_fd);
	}

#if ENABLE_FEATURE_AR_LONG_FILENAMES
	free(archive_handle->ar__long_names);
#endif
	llist_free(archive_handle->accept, NULL);
	free(archive_handle->file_header);
	free(archive_handle);

	return ar_mtime;
}

/*
 * If the name is of the form 'libname(member.o)' split it into its
 * name and member parts and set the member pointer to point to the
 * latter.  Otherwise just take a copy of the name and don't alter
 * the member pointer.
 *
 * In either case the return value is an allocated string which must
 * be freed by the caller.
 */
static char *
splitlib(const char *name, char **member)
{
	char *s, *t;
	size_t len;

	t = xstrdup(name);
	s = strchr(t, '(');
	if (s) {
		// We have 'libname(member.o)'
		*s++ = '\0';
		len = strlen(s);
		if (len <= 1 || s[len - 1] != ')' || *t == '\0')
			error("invalid name '%s'", name);
		s[len - 1] = '\0';
		*member = s;
	}
	return t;
}

/*
 * Get the modification time of a file.  Set it to 0 if the file
 * doesn't exist.
 */
static void
modtime(struct name *np)
{
	char *name, *member = NULL;
	struct stat info;

	name = splitlib(np->n_name, &member);
	if (member) {
		// Looks like library(member)
		np->n_tim.tv_sec = artime(name, member);
		np->n_tim.tv_nsec = 0;
	} else if (stat(name, &info) < 0) {
		if (errno != ENOENT)
			bb_perror_msg("can't open %s", name);
		np->n_tim.tv_sec = 0;
		np->n_tim.tv_nsec = 0;
	} else {
		np->n_tim.tv_sec = info.st_mtim.tv_sec;
		np->n_tim.tv_nsec = info.st_mtim.tv_nsec;
	}
	free(name);
}

/*
 * Control of the implicit suffix rules
 */

/*
 * Return a pointer to the suffix of a name (which may be the
 * terminating NUL if there's no suffix).
 */
static char *
suffix(const char *name)
{
	char *p = strrchr(name, '.');
	return p ? p : (char *)name + strlen(name);
}

/*
 * Search for an inference rule to convert some suffix ('psuff')
 * to the target suffix 'tsuff'.  The basename of the prerequisite
 * is 'base'.
 */
static struct name *
dyndep0(char *base, const char *tsuff, struct rule *infrule)
{
	char *psuff;
	struct name *xp;		// Suffixes
	struct name *sp;		// Suffix rule
	struct rule *rp;
	struct depend *dp;
	bool chain = FALSE;

	xp = newname(".SUFFIXES");
 retry:
	for (rp = xp->n_rule; rp; rp = rp->r_next) {
		for (dp = rp->r_dep; dp; dp = dp->d_next) {
			// Generate new suffix rule to try
			psuff = dp->d_name->n_name;
			sp = findname(auto_concat(psuff, tsuff));
			if (sp && sp->n_rule) {
				struct name *ip;
				int got_ip;

				// Has rule already been used in this chain?
				if ((sp->n_flag & N_MARK))
					continue;
				// Generate a name for an implicit prerequisite
				ip = newname(auto_concat(base, psuff));
				if ((ip->n_flag & N_DOING))
					continue;

				if (!ip->n_tim.tv_sec)
					modtime(ip);

				if (!chain) {
					got_ip = ip->n_tim.tv_sec || (ip->n_flag & N_TARGET);
				} else {
					sp->n_flag |= N_MARK;
					got_ip = dyndep(ip, NULL, NULL) != NULL;
					sp->n_flag &= ~N_MARK;
				}

				if (got_ip) {
					// Prerequisite exists or we know how to make it
					if (infrule) {
						dp = NULL;
						newdep(&dp, ip);
						infrule->r_dep = dp;
						infrule->r_cmd = sp->n_rule->r_cmd;
					}
					return ip;
				}
			}
		}
	}
	// If we didn't find an existing file or an explicit rule try
	// again, this time looking for a chained inference rule.
	if (!posix && !chain) {
		chain = TRUE;
		goto retry;
	}
	return NULL;
}

/*
 * If 'name' ends with 'suffix' return an allocated string containing
 * the name with the suffix removed, else return NULL.
 */
static char *
has_suffix(const char *name, const char *suffix)
{
	ssize_t delta = strlen(name) - strlen(suffix);
	char *base = NULL;

	if (delta > 0 && strcmp(name + delta, suffix) == 0) {
		base = xstrdup(name);
		base[delta] = '\0';
	}

	return base;
}

/*
 * Dynamic dependency.  This routine applies the suffix rules
 * to try and find a source and a set of rules for a missing
 * target.  NULL is returned on failure.  On success the name of
 * the implicit prerequisite is returned and the rule used is
 * placed in the infrule structure provided by the caller.
 */
static struct name *
dyndep(struct name *np, struct rule *infrule, const char **ptsuff)
{
	const char *tsuff;
	char *base, *name, *member;
	struct name *pp = NULL;	// Implicit prerequisite

	member = NULL;
	name = splitlib(np->n_name, &member);

	// POSIX only allows inference rules with one or two periods.
	// As an extension this restriction is lifted, but not for
	// targets of the form lib.a(member.o).
	if (!posix && member == NULL) {
		struct name *xp = newname(".SUFFIXES");
		int found_suffix = FALSE;

		for (struct rule *rp = xp->n_rule; rp; rp = rp->r_next) {
			for (struct depend *dp = rp->r_dep; dp; dp = dp->d_next) {
				tsuff = dp->d_name->n_name;
				base = has_suffix(name, tsuff);
				if (base) {
					found_suffix = TRUE;
					pp = dyndep0(base, tsuff, infrule);
					free(base);
					if (pp) {
						goto done;
					}
				}
			}
		}

		if (!found_suffix) {
			// The name didn't have a known suffix. Try single-suffix rule.
			tsuff = "";
			pp = dyndep0(name, tsuff, infrule);
			if (pp) {
 done:
				if (ptsuff) {
					*ptsuff = tsuff;
				}
			}
		}
	} else {
		tsuff = xstrdup(suffix(name));
		base = member ? member : name;
		*suffix(base) = '\0';

		pp = dyndep0(base, tsuff, infrule);
		free((void *)tsuff);
	}
	free(name);

	return pp;
}

#define RULES \
	".c.o:\n" \
	"	$(CC) $(CFLAGS) -c $<\n" \
	".y.o:\n" \
	"	$(YACC) $(YFLAGS) $<\n" \
	"	$(CC) $(CFLAGS) -c y.tab.c\n" \
	"	rm -f y.tab.c\n" \
	"	mv y.tab.o $@\n" \
	".y.c:\n" \
	"	$(YACC) $(YFLAGS) $<\n" \
	"	mv y.tab.c $@\n" \
	".l.o:\n" \
	"	$(LEX) $(LFLAGS) $<\n" \
	"	$(CC) $(CFLAGS) -c lex.yy.c\n" \
	"	rm -f lex.yy.c\n" \
	"	mv lex.yy.o $@\n" \
	".l.c:\n" \
	"	$(LEX) $(LFLAGS) $<\n" \
	"	mv lex.yy.c $@\n" \
	".c.a:\n" \
	"	$(CC) -c $(CFLAGS) $<\n" \
	"	$(AR) $(ARFLAGS) $@ $*.o\n" \
	"	rm -f $*.o\n" \
	".c:\n" \
	"	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<\n" \
	".sh:\n" \
	"	cp $< $@\n" \
	"	chmod a+x $@\n"

#define RULES_2017 \
	".SUFFIXES:.o .c .y .l .a .sh .f\n" \
	".f.o:\n" \
	"	$(FC) $(FFLAGS) -c $<\n" \
	".f.a:\n" \
	"	$(FC) -c $(FFLAGS) $<\n" \
	"	$(AR) $(ARFLAGS) $@ $*.o\n" \
	"	rm -f $*.o\n" \
	".f:\n" \
	"	$(FC) $(FFLAGS) $(LDFLAGS) -o $@ $<\n"

#define RULES_2024 \
	".SUFFIXES:.o .c .y .l .a .sh\n"

#define MACROS \
	"CFLAGS=-O1\n" \
	"YACC=yacc\n" \
	"YFLAGS=\n" \
	"LEX=lex\n" \
	"LFLAGS=\n" \
	"AR=ar\n" \
	"ARFLAGS=-rv\n" \
	"LDFLAGS=\n"

#define MACROS_2017 \
	"CC=c99\n" \
	"FC=fort77\n" \
	"FFLAGS=-O1\n" \

#define MACROS_2024 \
	"CC=c17\n"

#define MACROS_EXT \
	"CC=cc\n"

/*
 * Read the built-in rules using a fake fgets-like interface.
 */
static char *
getrules(char *s, int size)
{
	char *r = s;

	if (rulepos == NULL || *rulepos == '\0') {
		if (rule_idx == 0) {
			rulepos = MACROS;
			rule_idx++;
		} else if (rule_idx == 1) {
			if (POSIX_2017)
				rulepos = MACROS_2017;
			else if (posix)
				rulepos = MACROS_2024;
			else
				rulepos = MACROS_EXT;
			rule_idx++;
		} else if (!norules) {
			if (rule_idx == 2) {
				rulepos = POSIX_2017 ? RULES_2017 : RULES_2024;
				rule_idx++;
			} else if (rule_idx == 3) {
				rulepos = RULES;
				rule_idx++;
			}
		}
	}

	if (*rulepos == '\0')
		return NULL;

	while (--size) {
		if ((*r++ = *rulepos++) == '\n')
			break;
	}
	*r = '\0';
	return s;
}

/*
 * Parse a makefile
 */

/*
 * Return a pointer to the next blank-delimited word or NULL if
 * there are none left.
 */
static char *
gettok(char **ptr)
{
	char *p;

	while (isblank(**ptr))	// Skip blanks
		(*ptr)++;

	if (**ptr == '\0')	// Nothing after blanks
		return NULL;

	p = *ptr;		// Word starts here

	while (**ptr != '\0' && !isblank(**ptr))
		(*ptr)++;	// Find end of word

	// Terminate token and move on unless already at end of string
	if (**ptr != '\0')
		*(*ptr)++ = '\0';

	return(p);
}

/*
 * Skip over (possibly adjacent or nested) macro expansions.
 */
static char *
skip_macro(const char *s)
{
	while (*s && s[0] == '$') {
		if (s[1] == '(' || s[1] == '{') {
			char end = *++s == '(' ? ')' : '}';
			while (*s && *s != end)
				s = skip_macro(s + 1);
			if (*s == end)
				++s;
		} else if (s[1] != '\0') {
			s += 2;
		} else {
			break;
		}
	}
	return (char *)s;
}

/*
 * Process each whitespace-separated word in the input string:
 *
 * - replace paths with their directory or filename part
 * - replace prefixes and suffixes
 *
 * Returns an allocated string or NULL if the input is unmodified.
 */
static char *
modify_words(const char *val, int modifier, size_t lenf, size_t lenr,
				const char *find_pref, const char *repl_pref,
				const char *find_suff, const char *repl_suff)
{
	char *s, *copy, *word, *sep, *buf = NULL;
	size_t find_pref_len = 0, find_suff_len = 0;

	if (!modifier && lenf == 0 && lenr == 0)
		return buf;

	if (find_pref) {
		// get length of find prefix, e.g: src/
		find_pref_len = strlen(find_pref);
		// get length of find suffix, e.g: .c
		find_suff_len = lenf - find_pref_len - 1;
	}

	s = copy = xstrdup(val);
	while ((word = gettok(&s)) != NULL) {
		if (modifier) {
			sep = strrchr(word, '/');
			if (modifier == 'D') {
				if (!sep) {
					word[0] = '.';	// no '/', return "."
					sep = word + 1;
				} else if (sep == word) {
					// '/' at start of word, return "/"
					sep = word + 1;
				}
				// else terminate at separator
				*sep = '\0';
			} else if (/* modifier == 'F' && */ sep) {
				word = sep + 1;
			}
		}
		if (find_pref != NULL || lenf != 0 || lenr != 0) {
			size_t lenw = strlen(word);
			// This code implements pattern macro expansions:
			//    https://austingroupbugs.net/view.php?id=519
			//
			// find: <prefix>%<suffix>
			// example: src/%.c
			//
			// For a pattern of the form:
			//    $(string1:[op]%[os]=[np][%][ns])
			// lenf is the length of [op]%[os].  So lenf >= 1.
			if (find_pref != NULL && lenw + 1 >= lenf) {
				// If prefix and suffix of word match find_pref and
				// find_suff, then do substitution.
				if (strncmp(word, find_pref, find_pref_len) == 0 &&
						strcmp(word + lenw - find_suff_len, find_suff) == 0) {
					// replace: <prefix>[%<suffix>]
					// example: build/%.o or build/all.o (notice no %)
					// If repl_suff is NULL, replace whole word with repl_pref.
					if (!repl_suff) {
						word = xstrdup(repl_pref);
					} else {
						word[lenw - find_suff_len] = '\0';
						word = xasprintf("%s%s%s", repl_pref,
									word + find_pref_len, repl_suff);
					}
					word = auto_string(word);
				}
			} else if (lenw >= lenf &&
						strcmp(word + lenw - lenf, find_suff) == 0) {
				word[lenw - lenf] = '\0';
				word = auto_concat(word, repl_suff);
			}
		}
		buf = xappendword(buf, word);
	}
	free(copy);
	return buf;
}

/*
 * Return a pointer to the next instance of a given character.  Macro
 * expansions are skipped so the ':' and '=' in $(VAR:.s1=.s2) aren't
 * detected as separators in macro definitions.  Some other situations
 * also require skipping the internals of a macro expansion.
 */
static char *
find_char(const char *str, int c)
{
	const char *s;

	for (s = skip_macro(str); *s; s = skip_macro(s + 1)) {
		if (*s == c)
			return (char *)s;
	}
	return NULL;
}

#if ENABLE_PLATFORM_MINGW32
/*
 * Check for a target rule by searching for a colon that isn't
 * part of a Windows path.  Return a pointer to the colon or NULL.
 */
static char *
find_colon(char *p)
{
	char *q;

	for (q = p; (q = strchr(q, ':')); ++q) {
		if (posix && !(pragma & P_WINDOWS))
			break;
		if (q == p || !isalpha(q[-1]) || q[1] != '/')
			break;
	}
	return q;
}
#else
# define find_colon(s) strchr(s, ':')
#endif

/*
 * Recursively expand any macros in str to an allocated string.
 */
static char *
expand_macros(const char *str, int except_dollar)
{
	char *exp, *newexp, *s, *t, *p, *q, *name;
	char *find, *replace, *modified;
	char *expval, *expfind, *find_suff, *repl_suff;
	char *find_pref = NULL, *repl_pref = NULL;
	size_t lenf, lenr;
	char modifier;
	struct macro *mp;

	exp = xstrdup(str);
	for (t = exp; *t; t++) {
		if (*t == '$') {
			if (t[1] == '\0') {
				break;
			}
			if (t[1] == '$' && except_dollar) {
				t++;
				continue;
			}
			// Need to expand a macro.  Find its extent (s to t inclusive)
			// and take a copy of its content.
			s = t;
			t++;
			if (*t == '{' || *t == '(') {
				t = find_char(t, *t == '{' ? '}' : ')');
				if (t == NULL)
					error("unterminated variable '%s'", s);
				name = xstrndup(s + 2, t - s - 2);
			} else {
				name = xzalloc(2);
				name[0] = *t;
				/*name[1] = '\0'; - xzalloc did it */
			}

			// Only do suffix replacement or pattern macro expansion
			// if both ':' and '=' are found, plus a '%' for the latter.
			// Suffix replacement is indicated by
			// find_pref == NULL && (lenf != 0 || lenr != 0);
			// pattern macro expansion by find_pref != NULL.
			expfind = NULL;
			find_suff = repl_suff = NULL;
			lenf = lenr = 0;
			if ((find = find_char(name, ':'))) {
				*find++ = '\0';
				expfind = expand_macros(find, FALSE);
				if ((replace = find_char(expfind, '='))) {
					*replace++ = '\0';
					lenf = strlen(expfind);
					if (!POSIX_2017 && (find_suff = strchr(expfind, '%'))) {
						find_pref = expfind;
						repl_pref = replace;
						*find_suff++ = '\0';
						if ((repl_suff = strchr(replace, '%')))
							*repl_suff++ = '\0';
					} else {
						if (posix && !(pragma & P_EMPTY_SUFFIX) && lenf == 0)
							error("empty suffix%s",
								!ENABLE_FEATURE_MAKE_POSIX ? "" :
									": allow with pragma empty_suffix");
						find_suff = expfind;
						repl_suff = replace;
						lenr = strlen(repl_suff);
					}
				}
			}

			p = q = name;
			// If not in POSIX mode expand macros in the name.
			if (!POSIX_2017) {
				char *expname = expand_macros(name, FALSE);
				free(name);
				name = expname;
			} else {
				// Skip over nested expansions in name
				do {
					*q++ = *p;
				} while ((p = skip_macro(p + 1)) && *p);
			}

			// The internal macros support 'D' and 'F' modifiers
			modifier = '\0';
			switch (name[0]) {
			case '^':
			case '+':
				if (POSIX_2017)
					break;
				// fall through
			case '@': case '%': case '?': case '<': case '*':
				if ((name[1] == 'D' || name[1] == 'F') && name[2] == '\0') {
					modifier = name[1];
					name[1] = '\0';
				}
				break;
			}

			modified = NULL;
			if ((mp = getmp(name)))  {
				// Recursive expansion
				if (mp->m_flag)
					error("recursive macro %s", name);
				// Note if we've expanded $(MAKE)
				if (strcmp(name, "MAKE") == 0)
					opts |= OPT_make;
				mp->m_flag = TRUE;
				expval = expand_macros(mp->m_val, FALSE);
				mp->m_flag = FALSE;
				modified = modify_words(expval, modifier, lenf, lenr,
								find_pref, repl_pref, find_suff, repl_suff);
				if (modified)
					free(expval);
				else
					modified = expval;
			}
			free(name);
			free(expfind);

			if (modified && *modified) {
				// The text to be replaced by the macro expansion is
				// from s to t inclusive.
				*s = '\0';
				newexp = xasprintf("%s%s%s", exp, modified, t + 1);
				t = newexp + (s - exp) + strlen(modified) - 1;
				free(exp);
				exp = newexp;
			} else {
				// Macro wasn't expanded or expanded to nothing.
				// Close the space occupied by the macro reference.
				q = t + 1;
				t = s - 1;
				while ((*s++ = *q++))
					continue;
			}
			free(modified);
		}
	}
	return exp;
}

/*
 * Process a non-command line
 */
static void
process_line(char *s)
{
	char *t;

	// Strip comment
	// don't treat '#' in macro expansion as a comment
	// nor '#' outside macro expansion preceded by backslash
	if (!posix) {
		char *u = s;
		while ((t = find_char(u, '#')) && t > u && t[-1] == '\\') {
			for (u = t; *u; ++u) {
				u[-1] = u[0];
			}
			*u = '\0';
			u = t;
		}
	} else
		t = strchr(s, '#');
	if (t)
		*t = '\0';

	// Replace escaped newline and any leading white space on the
	// following line with a single space.  Stop processing at a
	// non-escaped newline.
	for (t = s; *s && *s != '\n'; ) {
		if (s[0] == '\\' && s[1] == '\n') {
			s += 2;
			while (isspace(*s))
				++s;
			*t++ = ' ';
		} else {
			*t++ = *s++;
		}
	}
	*t = '\0';
}

enum {
	INITIAL = 0,
	SKIP_LINE = 1 << 0,
	EXPECT_ELSE = 1 << 1,
	GOT_MATCH = 1 << 2
};

#define IFDEF	0
#define IFNDEF	1
#define IFEQ	2
#define IFNEQ	3
#define ELSE	0
#define ENDIF	1

/*
 * Extract strings following ifeq/ifneq and compare them.
 * Return -1 on error.
 */
static int
compare_strings(char *arg1)
{
	char *arg2, *end, term, *t1, *t2;
	int ret;

	// Get first string terminator.
	if (arg1[0] == '(')
		term = ',';
	else if (arg1[0] == '"' || arg1[0] == '\'')
		term =  arg1[0];
	else
		return -1;

	arg2 = find_char(++arg1, term);
	if (arg2 == NULL)
		return -1;
	*arg2++ = '\0';

	// Get second string terminator.
	if (term == ',') {
		term = ')';
	} else {
		// Skip spaces between quoted strings.
		while (isspace(arg2[0]))
			arg2++;
		if (arg2[0] == '"' || arg2[0] == '\'')
			term = arg2[0];
		else
			return -1;
		++arg2;
	}

	end = find_char(arg2, term);
	if (end == NULL)
		return -1;
	*end++ = '\0';

	if (gettok(&end) != NULL) {
		warning("unexpected text");
	}

	t1 = expand_macros(arg1, FALSE);
	t2 = expand_macros(arg2, FALSE);

	ret = strcmp(t1, t2) == 0;
	free(t1);
	free(t2);
	return ret;
}

/*
 * Process conditional directives and return TRUE if the current line
 * should be skipped.
 */
static int
skip_line(const char *str1)
{
	char *copy, *q, *token;
	bool new_level = TRUE;
	// Default is to return skip flag for current level
	int ret = cstate[clevel] & SKIP_LINE;
	int key;

	q = copy = xstrdup(str1);
	process_line(copy);
	if ((token = gettok(&q)) != NULL) {
		switch (index_in_strings("else\0endif\0", token)) {
		case ENDIF:
			if (gettok(&q) != NULL)
				error_unexpected("text");
			if (clevel == 0)
				error_unexpected(token);
			--clevel;
			ret = TRUE;
			goto end;
		case ELSE:
			if (!(cstate[clevel] & EXPECT_ELSE))
				error_unexpected(token);

			// If an earlier condition matched we'll now skip lines.
			// If not we don't, though an 'else if' may override this.
			if ((cstate[clevel] & GOT_MATCH))
				cstate[clevel] |= SKIP_LINE;
			else
				cstate[clevel] &= ~SKIP_LINE;

			token = gettok(&q);
			if (token == NULL) {
				// Simple else with no conditional directive
				cstate[clevel] &= ~EXPECT_ELSE;
				ret = TRUE;
				goto end;
			} else {
				// A conditional directive is now required ('else if').
				new_level = FALSE;
			}
		}

		key = index_in_strings("ifdef\0ifndef\0ifeq\0ifneq\0", token);
		if (key != -1) {
			int match;

			if (key == IFDEF || key == IFNDEF) {
				// ifdef/ifndef: find out if macro is defined.
				char *name = gettok(&q);
				if (name != NULL && gettok(&q) == NULL) {
					char *t = expand_macros(name, FALSE);
					struct macro *mp = getmp(t);
					match = mp != NULL && mp->m_val[0] != '\0';
					free(t);
				} else {
					match = -1;
				}
			} else {
				// ifeq/ifneq: compare strings.
				match = compare_strings(q);
			}

			if (match >= 0) {
				if (new_level) {
					// Start a new level.
					if (clevel == IF_MAX)
						error("nesting too deep");
					++clevel;
					cstate[clevel] = EXPECT_ELSE | SKIP_LINE;
					// If we were skipping lines at the previous level
					// we need to continue doing that unconditionally
					// at the new level.
					if ((cstate[clevel - 1] & SKIP_LINE))
						cstate[clevel] |= GOT_MATCH;
				}

				if (!(cstate[clevel] & GOT_MATCH)) {
					if (key == IFNDEF || key == IFNEQ)
						match = !match;
					if (match) {
						cstate[clevel] &= ~SKIP_LINE;
						cstate[clevel] |= GOT_MATCH;
					}
				}
			} else {
				error("invalid condition");
			}
			ret = TRUE;
		} else if (!new_level) {
			error("missing conditional");
		}
	}
 end:
	free(copy);
	return ret;
}

/*
 * If fd is NULL read the built-in rules.  Otherwise read from the
 * specified file descriptor.
 */
static char *
make_fgets(char *s, int size, FILE *fd)
{
	return fd ? fgets(s, size, fd) : getrules(s, size);
}

/*
 * Read a newline-terminated line into an allocated string.
 * Backslash-escaped newlines don't terminate the line.
 * Ignore comment lines.  Return NULL on EOF.
 */
static char *
readline(FILE *fd, int want_command)
{
	char *p, *str = NULL;
	int pos = 0;
	int len = 0;

	for (;;) {
		// We need room for at least one character and a NUL terminator
		if (len - pos > 1 &&
				make_fgets(str + pos, len - pos, fd) == NULL) {
			if (pos)
				return str;
			free(str);
			return NULL;	// EOF
		}

		if (len - pos < 2 || (p = strchr(str + pos, '\n')) == NULL) {
			// Need more room
			if (len)
				pos = len - 1;
			len += 256;
			str = xrealloc(str, len);
			continue;
		}
		lineno++;

#if ENABLE_PLATFORM_MINGW32
		// Remove CR before LF
		if (p != str && p[-1] == '\r') {
			p[-1] = '\n';
			*p-- = '\0';
		}
#endif
		// Keep going if newline has been escaped
		if (p != str && p[-1] == '\\') {
			pos = p - str + 1;
			continue;
		}
		dispno = lineno;

		// Check for lines that are conditionally skipped.
		if (posix || !skip_line(str)) {
			if (want_command && *str == '\t')
				return str;

			// Check for comment lines
			p = str;
			while (isblank(*p))
				p++;

			if (*p != '\n' && (posix ? *str != '#' : *p != '#'))
				return str;
		}

		pos = 0;
	}
}

/*
 * Return a pointer to the suffix name if the argument is a known suffix
 * or NULL if it isn't.
 */
static const char *
is_suffix(const char *s)
{
	struct name *np;
	struct rule *rp;
	struct depend *dp;

	np = newname(".SUFFIXES");
	for (rp = np->n_rule; rp; rp = rp->r_next) {
		for (dp = rp->r_dep; dp; dp = dp->d_next) {
			if (strcmp(s, dp->d_name->n_name) == 0) {
				return dp->d_name->n_name;
			}
		}
	}
	return NULL;
}

/*
 * Return TRUE if the argument is formed by concatenating two
 * known suffixes.
 */
static int
is_inference_target(const char *s)
{
	struct name *np;
	struct rule *rp1, *rp2;
	struct depend *dp1, *dp2;

	np = newname(".SUFFIXES");
	for (rp1 = np->n_rule; rp1; rp1 = rp1->r_next) {
		for (dp1 = rp1->r_dep; dp1; dp1 = dp1->d_next) {
			const char *suff1 = dp1->d_name->n_name;
			size_t len = strlen(suff1);

			if (strncmp(s, suff1, len) == 0) {
				for (rp2 = np->n_rule; rp2; rp2 = rp2->r_next) {
					for (dp2 = rp2->r_dep; dp2; dp2 = dp2->d_next) {
						const char *suff2 = dp2->d_name->n_name;
						if (strcmp(s + len, suff2) == 0) {
							return TRUE;
						}
					}
				}
			}
		}
	}
	return FALSE;
}

enum {
	T_NORMAL    =  0,
	T_SPECIAL   = (1 << 0),
	T_INFERENCE = (1 << 1), // Inference rule
	T_NOPREREQ  = (1 << 2), // If set must not have prerequisites
	T_COMMAND   = (1 << 3), // If set must have commands, if unset must not
};

/*
 * Determine if the argument is a special target and return a set
 * of flags indicating its properties.
 */
static int
target_type(char *s)
{
	int ret;
	static const char *s_name =
		".DEFAULT\0"
		".POSIX\0"
		".IGNORE\0"
		".PRECIOUS\0"
		".SILENT\0"
		".SUFFIXES\0"
		".PHONY\0"
		".NOTPARALLEL\0"
		".WAIT\0"
#if ENABLE_FEATURE_MAKE_POSIX
		".PRAGMA\0"
#endif
	;

	static const uint8_t s_type[] = {
		T_SPECIAL | T_NOPREREQ | T_COMMAND,
		T_SPECIAL | T_NOPREREQ,
		T_SPECIAL,
		T_SPECIAL,
		T_SPECIAL,
		T_SPECIAL,
		T_SPECIAL,
		T_SPECIAL | T_NOPREREQ,
		T_SPECIAL | T_NOPREREQ,
		T_SPECIAL,
	};

	// Check for one of the known special targets
	ret = index_in_strings(s_name, s);
	if (ret >= 0)
		return s_type[ret];

	// Check for an inference rule
	ret = T_NORMAL;
	if (!posix) {
		if (is_suffix(s) || is_inference_target(s)) {
			ret = T_INFERENCE | T_NOPREREQ | T_COMMAND;
		}
	} else {
		// In POSIX inference rule targets must contain one or two dots
		char *sfx = suffix(s);
		if (*s == '.' && is_suffix(sfx)) {
			if (s == sfx) {	// Single suffix rule
				ret = T_INFERENCE | T_NOPREREQ | T_COMMAND;
			} else {
				// Suffix is valid, check that prefix is too
				*sfx = '\0';
				if (is_suffix(s))
					ret = T_INFERENCE | T_NOPREREQ | T_COMMAND;
				*sfx = '.';
			}
		}
	}
	return ret;
}

static int
ends_with_bracket(const char *s)
{
	return last_char_is(s, ')') != NULL;
}

/*
 * Process a command line
 */
static char *
process_command(char *s)
{
	char *t, *u;
	int len;
	char *outside;

	if (!(pragma & P_COMMAND_COMMENT) && posix) {
		// POSIX strips comments from command lines
		t = strchr(s, '#');
		if (t) {
			*t = '\0';
			warning("comment in command removed: keep with pragma command_comment");
		}
	}

	len = strlen(s) + 1;
	outside = xzalloc(len);
	for (t = skip_macro(s); *t; t = skip_macro(t + 1)) {
		outside[t - s] = 1;
	}

	// Process escaped newlines.  Stop at first non-escaped newline.
	for (t = u = s; *u && *u != '\n'; ) {
		if (u[0] == '\\' && u[1] == '\n') {
			if (POSIX_2017 || outside[u - s]) {
				// Outside macro: remove tab following escaped newline.
				*t++ = *u++;
				*t++ = *u++;
				u += (*u == '\t');
			} else {
				// Inside macro: replace escaped newline and any leading
				// whitespace on the following line with a single space.
				u += 2;
				while (isspace(*u))
					++u;
				*t++ = ' ';
			}
		} else {
			*t++ = *u++;
		}
	}
	*t = '\0';
	free(outside);
	return s;
}

static char *
run_command(const char *cmd)
{
	FILE *fd;
	char *s, *val = NULL;
	char buf[256];
	size_t len = 0, nread;

	if ((fd = popen(cmd, "r")) == NULL)
		return val;

	for (;;) {
		nread = fread(buf, 1, sizeof(buf), fd);
		if (nread == 0)
			break;

		val = xrealloc(val, len + nread + 1);
		memcpy(val + len, buf, nread);
		len += nread;
		val[len] = '\0';
	}
	pclose(fd);

	if (val == NULL)
		return NULL;

	// Strip leading whitespace in POSIX 2024 mode
	if (posix) {
		s = val;
		while (isspace(*s)) {
			++s;
			--len;
		}

		if (len == 0) {
			free(val);
			return NULL;
		}
		memmove(val, s, len + 1);
	}

#if ENABLE_PLATFORM_MINGW32
	len = remove_cr(val, len + 1) - 1;
	if (len == 0) {
		free(val);
		return NULL;
	}
#endif

	// Remove one newline from the end (BSD compatibility)
	if (val[len - 1] == '\n')
		val[len - 1] = '\0';
	// Other newlines are changed to spaces
	for (s = val; *s; ++s) {
		if (*s == '\n')
			*s = ' ';
	}
	return val;
}

/*
 * Check for an unescaped wildcard character
 */
static int wildchar(const char *p)
{
	while (*p) {
		switch (*p) {
		case '?':
		case '*':
		case '[':
			return 1;
		case '\\':
			if (p[1] != '\0')
				++p;
			break;
		}
		++p;
	}
	return 0;
}

/*
 * Expand any wildcards in a pattern.  Return TRUE if a match is
 * found, in which case the caller should call globfree() on the
 * glob_t structure.
 */
static int
wildcard(char *p, glob_t *gd)
{
	int ret;
	char *s;

	// Don't call glob() if there are no wildcards.
	if (!wildchar(p)) {
 nomatch:
		// Remove backslashes from the name.
		for (s = p; *p; ++p) {
			if (*p == '\\' && p[1] != '\0')
				continue;
			*s++ = *p;
		}
		*s = '\0';
		return 0;
	}

	memset(gd, 0, sizeof(*gd));
	ret = glob(p, GLOB_NOSORT, NULL, gd);
	if (ret == GLOB_NOMATCH) {
		globfree(gd);
		goto nomatch;
	} else if (ret != 0) {
		error("glob error for '%s'", p);
	}
	return 1;
}

#if ENABLE_FEATURE_MAKE_POSIX
static void
pragmas_from_env(void)
{
	char *p, *q, *var;
	const char *env = getenv("PDPMAKE_PRAGMAS");

	if (env == NULL)
		return;

	q = var = xstrdup(env);
	while ((p = gettok(&q)) != NULL)
		set_pragma(p);
	free(var);
}
#endif

/*
 * Determine if a line is a target rule with an inline command.
 * Return a pointer to the semicolon separator if it is, else NULL.
 */
static char *
inline_command(char *line)
{
	char *p = find_char(line, ':');

	if (p)
		p = strchr(p, ';');
	return p;
}

/*
 * Parse input from the makefile and construct a tree structure of it.
 */
static void
input(FILE *fd, int ilevel)
{
	char *p, *q, *s, *a, *str, *expanded, *copy;
	char *str1, *str2;
	struct name *np;
	struct depend *dp;
	struct cmd *cp;
	int startno, count;
	bool semicolon_cmd, seen_inference;
	uint8_t old_clevel = clevel;
	bool dbl;
	char *lib = NULL;
	glob_t gd;
	int nfile, i;
	char **files;
	bool minus;

	lineno = 0;
	str1 = readline(fd, FALSE);
	while (str1) {
		str2 = NULL;

		// Newlines and comments are handled differently in command lines
		// and other types of line.  Take a copy of the current line before
		// processing it as a non-command line in case it contains a
		// rule with a command line.  That is, a line of the form:
		//
		//   target: prereq; command
		//
		copy = xstrdup(str1);
		process_line(str1);
		str = str1;

		// Check for an include line
		if (!posix)
			while (isblank(*str))
				++str;
		minus = !POSIX_2017 && *str == '-';
		p = str + minus;
		if (strncmp(p, "include", 7) == 0 && isblank(p[7])) {
			const char *old_makefile = makefile;
			int old_lineno = lineno;

			if (ilevel > 16)
				error("too many includes");

			count = 0;
			q = expanded = expand_macros(p + 7, FALSE);
			while ((p = gettok(&q)) != NULL) {
				FILE *ifd;

				++count;
				if (!POSIX_2017) {
					// Try to create include file or bring it up-to-date
					opts |= OPT_include;
					make(newname(p), 1);
					opts &= ~OPT_include;
				}
				if ((ifd = fopen(p, "r")) == NULL) {
					if (!minus)
						error("can't open include file '%s'", p);
				} else {
					makefile = p;
					input(ifd, ilevel + 1);
					fclose(ifd);
					makefile = old_makefile;
					lineno = old_lineno;
				}
				if (POSIX_2017)
					break;
			}
			if (POSIX_2017) {
				// In POSIX 2017 zero or more than one include file is
				// unspecified behaviour.
				if (p == NULL || gettok(&q)) {
					error("one include file per line");
				}
			} else if (count == 0) {
				// In POSIX 2024 no include file is unspecified behaviour.
				if (posix)
					error("no include file");
			}
			goto end_loop;
		}

		// Check for a macro definition
		str = str1;
		// POSIX 2024 seems to allow a tab as the first character of
		// a macro definition, though most implementations don't.
		if (POSIX_2017 && *str == '\t')
			error("command not allowed here");
		if (find_char(str, '=') != NULL) {
			int level = (useenv || fd == NULL) ? 4 : 3;
			// Use a copy of the line:  we might need the original
			// if this turns out to be a target rule.
			char *copy2 = xstrdup(str);
			char *newq = NULL;
			char eq = '\0';
			q = find_char(copy2, '=');		// q can't be NULL

			if (q - 1 > copy2) {
				switch (q[-1]) {
				case ':':
					// '::=' and ':::=' are from POSIX 2024.
					if (!POSIX_2017 && q - 2 > copy2 && q[-2] == ':') {
						if (q - 3 > copy2 && q[-3] == ':') {
							eq = 'B';	// BSD-style ':='
							q[-3] = '\0';
						} else {
							eq = ':';	// GNU-style ':='
							q[-2] = '\0';
						}
						break;
					}
					// ':=' is a non-POSIX extension.
					if (posix)
						break;
					goto set_eq;
				case '+':
				case '?':
				case '!':
					// '+=', '?=' and '!=' are from POSIX 2024.
					if (POSIX_2017)
						break;
 set_eq:
					eq = q[-1];
					q[-1] = '\0';
					break;
				}
			}
			*q++ = '\0';	// Separate name and value
			while (isblank(*q))
				q++;
			if ((p = strrchr(q, '\n')) != NULL)
				*p = '\0';

			// Expand left-hand side of assignment
			p = expanded = expand_macros(copy2, FALSE);
			if ((a = gettok(&p)) == NULL)
				error("invalid macro assignment");

			// If the expanded LHS contains ':' and ';' it can't be a
			// macro assignment but it might be a target rule.
			if ((s = strchr(a, ':')) != NULL && strchr(s, ';') != NULL) {
				free(expanded);
				free(copy2);
				goto try_target;
			}

			if (gettok(&p))
				error("invalid macro assignment");

			if (eq == ':') {
				// GNU-style ':='.  Expand right-hand side of assignment.
				// Macro is of type immediate-expansion.
				q = newq = expand_macros(q, FALSE);
				level |= M_IMMEDIATE;
			}
			else if (eq == 'B') {
				// BSD-style ':='.  Expand right-hand side of assignment,
				// though not '$$'.  Macro is of type delayed-expansion.
				q = newq = expand_macros(q, TRUE);
			} else if (eq == '?' && getmp(a) != NULL) {
				// Skip assignment if macro is already set
				goto end_loop;
			} else if (eq == '+') {
				// Append to current value
				struct macro *mp = getmp(a);
				char *rhs;
				newq = mp && mp->m_val[0] ? xstrdup(mp->m_val) : NULL;
				if (mp && mp->m_immediate) {
					// Expand right-hand side of assignment (GNU make
					// compatibility)
					rhs = expand_macros(q, FALSE);
					level |= M_IMMEDIATE;
				} else {
					rhs = q;
				}
				newq = xappendword(newq, rhs);
				if (rhs != q)
					free(rhs);
				q = newq;
			} else if (eq == '!') {
				char *cmd = expand_macros(q, FALSE);
				q = newq = run_command(cmd);
				free(cmd);
			}
			setmacro(a, q, level);
			free(newq);
			free(copy2);
			goto end_loop;
		}

		// If we get here it must be a target rule
 try_target:
		if (*str == '\t')	// Command without target
			error("command not allowed here");
		p = expanded = expand_macros(str, FALSE);

		// Look for colon separator
		q = find_colon(p);
		if (q == NULL)
			error("expected separator");

		*q++ = '\0';	// Separate targets and prerequisites

		// Double colon
		dbl = !posix && *q == ':';
		if (dbl)
			q++;

		// Look for semicolon separator
		cp = NULL;
		s = strchr(q, ';');
		if (s) {
			// Retrieve command from original or expanded copy of line
			char *copy3 = expand_macros(copy, FALSE);
			if ((p = inline_command(copy)) || (p = inline_command(copy3)))
				newcmd(&cp, process_command(p + 1));
			free(copy3);
			*s = '\0';
		}
		semicolon_cmd = cp != NULL && cp->c_cmd[0] != '\0';

		// Create list of prerequisites
		dp = NULL;
		while (((p = gettok(&q)) != NULL)) {
			char *newp = NULL;

			if (!posix) {
				// Allow prerequisites of form library(member1 member2).
				// Leading and trailing spaces in the brackets are skipped.
				if (!lib) {
					s = strchr(p, '(');
					if (s && !ends_with_bracket(s) && strchr(q, ')')) {
						// Looks like an unterminated archive member
						// with a terminator later on the line.
						lib = p;
						if (s[1] != '\0') {
							p = newp = auto_concat(lib, ")");
							s[1] = '\0';
						} else {
							continue;
						}
					}
				} else if (ends_with_bracket(p)) {
					if (*p != ')')
						p = newp = auto_concat(lib, p);
					lib = NULL;
					if (newp == NULL)
						continue;
				} else {
					p = newp = auto_string(xasprintf("%s%s)", lib, p));
				}
			}

			// If not in POSIX mode expand wildcards in the name.
			nfile = 1;
			files = &p;
			if (!posix && wildcard(p, &gd)) {
				nfile = gd.gl_pathc;
				files = gd.gl_pathv;
			}
			for (i = 0; i < nfile; ++i) {
				if (!POSIX_2017 && strcmp(files[i], ".WAIT") == 0)
					continue;
				np = newname(files[i]);
				newdep(&dp, np);
			}
			if (files != &p)
				globfree(&gd);
			free(newp);
		}
		lib = NULL;

		// Create list of commands
		startno = dispno;
		while ((str2 = readline(fd, TRUE)) && *str2 == '\t') {
			newcmd(&cp, process_command(str2));
			free(str2);
		}
		dispno = startno;

		// Create target names and attach rule to them
		q = expanded;
		count = 0;
		seen_inference = FALSE;
		while ((p = gettok(&q)) != NULL) {
			// If not in POSIX mode expand wildcards in the name.
			nfile = 1;
			files = &p;
			if (!posix && wildcard(p, &gd)) {
				nfile = gd.gl_pathc;
				files = gd.gl_pathv;
			}
			for (i = 0; i < nfile; ++i) {
				int ttype = target_type(files[i]);

				np = newname(files[i]);
				if (ttype != T_NORMAL) {
					// Enforce prerequisites/commands in POSIX mode
					if (posix) {
						if ((ttype & T_NOPREREQ) && dp)
							error_not_allowed("prerequisites", p);
						if ((ttype & T_INFERENCE)) {
							if (semicolon_cmd)
								error_in_inference_rule("'; command'");
							seen_inference = TRUE;
						}
						if ((ttype & T_COMMAND) && !cp &&
								!((ttype & T_INFERENCE) && !semicolon_cmd))
							error("commands required for %s", p);
						if (!(ttype & T_COMMAND) && cp)
							error_not_allowed("commands", p);
					}

					if ((ttype & T_INFERENCE)) {
						np->n_flag |= N_INFERENCE;
					} else if (strcmp(p, ".DEFAULT") == 0) {
						// .DEFAULT rule is a special case
						np->n_flag |= N_SPECIAL | N_INFERENCE;
					} else {
						np->n_flag |= N_SPECIAL;
					}
				} else if (!firstname) {
					firstname = np;
				}
				addrule(np, dp, cp, dbl);
				count++;
			}
			if (files != &p)
				globfree(&gd);
		}
		if (posix && seen_inference && count != 1)
			error_in_inference_rule("multiple targets");

		// Prerequisites and commands will be unused if there were
		// no targets.  Avoid leaking memory.
		if (count == 0) {
			freedeps(dp);
			freecmds(cp);
		}

 end_loop:
		free(str1);
		dispno = lineno;
		str1 = str2 ? str2 : readline(fd, FALSE);
		free(copy);
		free(expanded);
#if ENABLE_FEATURE_MAKE_POSIX
		if (!seen_first && fd) {
			if (findname(".POSIX")) {
				// The first non-comment line from a real makefile
				// defined the .POSIX special target.
				setenv("PDPMAKE_POSIXLY_CORRECT", "", 1);
				posix = TRUE;
			}
			seen_first = TRUE;
		}
#endif
	}
	// Conditionals aren't allowed to span files
	if (clevel != old_clevel)
		error("invalid conditional");
}

static void
remove_target(void)
{
	if (!dryrun && !print && !precious &&
			target && !(target->n_flag & (N_PRECIOUS | N_PHONY)) &&
			unlink(target->n_name) == 0) {
		diagnostic("'%s' removed", target->n_name);
	}
}

/*
 * Update the modification time of a file to now.
 */
static void
touch(struct name *np)
{
	if (dryrun || !silent)
		printf("touch %s\n", np->n_name);

	if (!dryrun) {
		const struct timespec timebuf[2] = {{0, UTIME_NOW}, {0, UTIME_NOW}};

		if (utimensat(AT_FDCWD, np->n_name, timebuf, 0) < 0) {
			if (errno == ENOENT) {
				int fd = open(np->n_name, O_RDWR | O_CREAT, 0666);
				if (fd >= 0) {
					close(fd);
					return;
				}
			}
			warning("touch %s failed", np->n_name);
		}
	}
}

/*
 * Do commands to make a target
 */
static int
docmds(struct name *np, struct cmd *cp)
{
	int estat = 0;
	char *q, *command;

	for (; cp; cp = cp->c_next) {
		uint32_t ssilent, signore, sdomake;

		// Location of command in makefile (for use in error messages)
		curr_cmd = cp;
		opts &= ~OPT_make;	// We want to know if $(MAKE) is expanded
		q = command = expand_macros(cp->c_cmd, FALSE);
		ssilent = silent || (np->n_flag & N_SILENT) || dotouch;
		signore = ignore || (np->n_flag & N_IGNORE);
		sdomake = (!dryrun || doinclude || domake) && !dotouch;
		for (;;) {
			if (*q == '@')	// Specific silent
				ssilent = TRUE + 1;
			else if (*q == '-')	// Specific ignore
				signore = TRUE;
			else if (*q == '+')	// Specific domake
				sdomake = TRUE + 1;
			else
				break;
			do {
				q++;
			} while (isblank(*q));
		}

		if (sdomake > TRUE) {
			// '+' must not override '@' or .SILENT
			if (ssilent != TRUE + 1 && !(np->n_flag & N_SILENT))
				ssilent = FALSE;
		} else if (!sdomake)
			ssilent = dotouch;

		if (!ssilent && *q != '\0') {	// Ignore empty commands
			puts(q);
			fflush_all();
		}

		if (quest && sdomake != TRUE + 1) {
			// MAKE_FAILURE means rebuild is needed
			estat |= MAKE_FAILURE | MAKE_DIDSOMETHING;
			continue;
		}

		if (sdomake && *q != '\0') {	// Ignore empty commands
			// Get the shell to execute it
			int status;
			char *cmd = !signore && posix ? auto_concat("set -e;", q) : q;

			target = np;
			status = system(cmd);
			// If this command was being run to create an include file
			// or bring it up-to-date errors should be ignored and a
			// failure status returned.
			if (status == -1 && !doinclude) {
				error("couldn't execute '%s'", q);
			} else if (status != 0 && !signore) {
				if (!posix && WIFSIGNALED(status))
					remove_target();
				if (doinclude) {
					warning("failed to build '%s'", np->n_name);
				} else {
					const char *err_type = NULL;
					int err_value = 1;

					if (WIFEXITED(status)) {
						err_type = "exit";
						err_value = WEXITSTATUS(status);
					} else if (WIFSIGNALED(status)) {
						err_type = "signal";
						err_value = WTERMSIG(status);
					}

					if (!quest || err_value == 127) {
						if (err_type)
							diagnostic("failed to build '%s' %s %d",
									np->n_name, err_type, err_value);
						else
							diagnostic("failed to build '%s'", np->n_name);
					}

					if (errcont) {
						estat |= MAKE_FAILURE;
						free(command);
						break;
					}
					exit(2);
				}
			}
		}
		if (sdomake || dryrun)
			estat = MAKE_DIDSOMETHING;
		free(command);
	}

	if (dotouch && !(np->n_flag & N_PHONY) && !(estat & MAKE_DIDSOMETHING)) {
		touch(np);
		estat = MAKE_DIDSOMETHING;
	}

	curr_cmd = NULL;
	return estat;
}

/*
 * Remove the suffix from a name, either the one provided in 'tsuff'
 * or, if 'tsuff' is NULL, one of the known suffixes.
 */
static char *
remove_suffix(const char *name, const char *tsuff)
{
	char *base = NULL;

	if (tsuff != NULL) {
		base = has_suffix(name, tsuff);
	} else {
		struct name *xp = newname(".SUFFIXES");
		for (struct rule *rp = xp->n_rule; rp; rp = rp->r_next) {
			for (struct depend *dp = rp->r_dep; dp; dp = dp->d_next) {
				base = has_suffix(name, dp->d_name->n_name);
				if (base) {
					return base;
				}
			}
		}
	}
	return base;
}

static int
make1(struct name *np, struct cmd *cp, char *oodate, char *allsrc,
		char *dedup, struct name *implicit, const char *tsuff)
{
	char *name, *member = NULL, *base = NULL, *prereq = NULL;

	name = splitlib(np->n_name, &member);
	setmacro("?", oodate, 0 | M_VALID);
	if (!POSIX_2017) {
		setmacro("+", allsrc, 0 | M_VALID);
		setmacro("^", dedup, 0 | M_VALID);
	}
	setmacro("%", member, 0 | M_VALID);
	setmacro("@", name, 0 | M_VALID);
	if (implicit || !posix) {
		char *s;

		// As an extension, if we're not dealing with an implicit
		// prerequisite set $< to the first out-of-date prerequisite.
		if (implicit == NULL) {
			if (oodate) {
				s = strchr(oodate, ' ');
				if (s)
					*s = '\0';
				prereq = oodate;
			}
		} else
			prereq = implicit->n_name;

		if (!posix && member == NULL) {
			// As an extension remove a suffix that doesn't necessarily
			// start with a period from a target, but not for targets
			// of the form lib.a(member.o).
			base = remove_suffix(name, tsuff);
			if (base) {
				free(name);
				name = base;
			}
		} else {
			base = member ? member : name;
			s = suffix(base);
			// As an extension, if we're not dealing with an implicit
			// prerequisite and the target ends with a known suffix,
			// remove it and set $* to the stem, else to an empty string.
			if (implicit == NULL && !is_suffix(s))
				base = NULL;
			else
				*s = '\0';
		}
	}
	setmacro("<", prereq, 0 | M_VALID);
	setmacro("*", base, 0 | M_VALID);
	free(name);

	return docmds(np, cp);
}

/*
 * Determine if the modification time of a target, t, is less than
 * that of a prerequisite, p.  If the tv_nsec member of either is
 * exactly 0 we assume (possibly incorrectly) that the time resolution
 * is 1 second and only compare tv_sec values.
 */
static int
timespec_le(const struct timespec *t, const struct timespec *p)
{
	if (t->tv_nsec == 0 || p->tv_nsec == 0)
		return t->tv_sec <= p->tv_sec;
	else if (t->tv_sec < p->tv_sec)
		return TRUE;
	else if (t->tv_sec == p->tv_sec)
		return t->tv_nsec <= p->tv_nsec;
	return FALSE;
}

/*
 * Return the greater of two struct timespecs
 */
static const struct timespec *
timespec_max(const struct timespec *t, const struct timespec *p)
{
	return timespec_le(t, p) ? p : t;
}

/*
 * Recursive routine to make a target.
 */
static int
make(struct name *np, int level)
{
	struct depend *dp;
	struct rule *rp;
	struct name *impdep = NULL;	// implicit prerequisite
	struct rule infrule;
	struct cmd *sc_cmd = NULL;	// commands for single-colon rule
	char *oodate = NULL;
	char *allsrc = NULL;
	char *dedup = NULL;
	const char *tsuff = NULL;
	struct timespec dtim = {1, 0};
	int estat = 0;

	if (np->n_flag & N_DONE)
		return 0;
	if (np->n_flag & N_DOING)
		error("circular dependency for %s", np->n_name);
	np->n_flag |= N_DOING;

	if (!np->n_tim.tv_sec)
		modtime(np);		// Get modtime of this file

	if (!(np->n_flag & N_DOUBLE)) {
		// Find the commands needed for a single-colon rule, using
		// an inference rule or .DEFAULT rule if necessary (but,
		// as an extension, not for phony targets)
		sc_cmd = getcmd(np);
		if (!sc_cmd && (posix || !(np->n_flag & N_PHONY))) {
			impdep = dyndep(np, &infrule, &tsuff);
			if (impdep) {
				sc_cmd = infrule.r_cmd;
				addrule(np, infrule.r_dep, NULL, FALSE);
			}
		}

		// As a last resort check for a default rule
		if (!(np->n_flag & N_TARGET) && np->n_tim.tv_sec == 0) {
			if (posix || !(np->n_flag & N_PHONY))
				sc_cmd = getcmd(findname(".DEFAULT"));
			if (!sc_cmd) {
				if (doinclude)
					return 1;
				error("don't know how to make %s", np->n_name);
			}
			impdep = np;
		}
	} else {
		// If any double-colon rule has no commands we need
		// an inference rule.
		for (rp = np->n_rule; rp; rp = rp->r_next) {
			if (!rp->r_cmd) {
				// Phony targets don't need an inference rule.
				if (!posix && (np->n_flag & N_PHONY))
					continue;
				impdep = dyndep(np, &infrule, &tsuff);
				if (!impdep) {
					if (doinclude)
						return 1;
					error("don't know how to make %s", np->n_name);
				}
				break;
			}
		}
	}

	// Reset flag to detect duplicate prerequisites
	if (!(np->n_flag & N_DOUBLE)) {
		for (rp = np->n_rule; rp; rp = rp->r_next) {
			for (dp = rp->r_dep; dp; dp = dp->d_next) {
				dp->d_name->n_flag &= ~N_MARK;
			}
		}
	}

	for (rp = np->n_rule; rp; rp = rp->r_next) {
		struct name *locdep = NULL;

		// Each double-colon rule is handled separately.
		if ((np->n_flag & N_DOUBLE)) {
			// If the rule has no commands use the inference rule.
			// Unless there isn't one, as allowed for phony targets.
			if (!rp->r_cmd) {
				if (impdep) {
					locdep = impdep;
					infrule.r_dep->d_next = rp->r_dep;
					rp->r_dep = infrule.r_dep;
					rp->r_cmd = infrule.r_cmd;
				}
			}
			// A rule with no prerequisities is executed unconditionally.
			if (!rp->r_dep)
				dtim = np->n_tim;
			// Reset flag to detect duplicate prerequisites
			for (dp = rp->r_dep; dp; dp = dp->d_next) {
				dp->d_name->n_flag &= ~N_MARK;
			}
		}
		for (dp = rp->r_dep; dp; dp = dp->d_next) {
			// Make prerequisite
			estat |= make(dp->d_name, level + 1);

			// Make strings of out-of-date prerequisites (for $?),
			// all prerequisites (for $+) and deduplicated prerequisites
			// (for $^).
			if (timespec_le(&np->n_tim, &dp->d_name->n_tim)) {
				if (posix || !(dp->d_name->n_flag & N_MARK))
					oodate = xappendword(oodate, dp->d_name->n_name);
			}
			allsrc = xappendword(allsrc, dp->d_name->n_name);
			if (!(dp->d_name->n_flag & N_MARK))
				dedup = xappendword(dedup, dp->d_name->n_name);
			dp->d_name->n_flag |= N_MARK;
			dtim = *timespec_max(&dtim, &dp->d_name->n_tim);
		}
		if ((np->n_flag & N_DOUBLE)) {
			if (((np->n_flag & N_PHONY) || timespec_le(&np->n_tim, &dtim))) {
				if (!(estat & MAKE_FAILURE)) {
					estat |= make1(np, rp->r_cmd, oodate, allsrc,
										dedup, locdep, tsuff);
					dtim = (struct timespec){1, 0};
				}
				free(oodate);
				oodate = NULL;
			}
			free(allsrc);
			free(dedup);
			allsrc = dedup = NULL;
			if (locdep) {
				rp->r_dep = rp->r_dep->d_next;
				rp->r_cmd = NULL;
			}
		}
	}
	if ((np->n_flag & N_DOUBLE) && impdep)
		free(infrule.r_dep);

	np->n_flag |= N_DONE;
	np->n_flag &= ~N_DOING;

	if (!(np->n_flag & N_DOUBLE) &&
				((np->n_flag & N_PHONY) || (timespec_le(&np->n_tim, &dtim)))) {
		if (!(estat & MAKE_FAILURE)) {
			if (sc_cmd)
				estat |= make1(np, sc_cmd, oodate, allsrc, dedup,
								impdep, tsuff);
			else if (!doinclude && level == 0 && !(estat & MAKE_DIDSOMETHING))
				warning("nothing to be done for %s", np->n_name);
		} else if (!doinclude && !quest) {
			diagnostic("'%s' not built due to errors", np->n_name);
		}
		free(oodate);
	}

	if (estat & MAKE_DIDSOMETHING) {
		modtime(np);
		if (!np->n_tim.tv_sec)
			clock_gettime(CLOCK_REALTIME, &np->n_tim);
	} else if (!quest && level == 0 && !timespec_le(&np->n_tim, &dtim))
		printf("%s: '%s' is up to date\n", applet_name, np->n_name);

	free(allsrc);
	free(dedup);
	return estat;
}

/*
 * Check structures for make.
 */

static void
print_name(struct name *np)
{
	if (np == firstname)
		printf("# default target\n");
	printf("%s:", np->n_name);
	if ((np->n_flag & N_DOUBLE))
		putchar(':');
}

static void
print_prerequisites(struct rule *rp)
{
	struct depend *dp;

	for (dp = rp->r_dep; dp; dp = dp->d_next)
		printf(" %s", dp->d_name->n_name);
}

static void
print_commands(struct rule *rp)
{
	struct cmd *cp;

	for (cp = rp->r_cmd; cp; cp = cp->c_next)
		printf("\t%s\n", cp->c_cmd);
}

static void
print_details(void)
{
	int i;
	struct macro *mp;
	struct name *np;
	struct rule *rp;

	for (i = 0; i < HTABSIZE; i++)
		for (mp = macrohead[i]; mp; mp = mp->m_next)
			printf("%s = %s\n", mp->m_name, mp->m_val);
	putchar('\n');

	for (i = 0; i < HTABSIZE; i++) {
		for (np = namehead[i]; np; np = np->n_next) {
			if (!(np->n_flag & N_DOUBLE)) {
				print_name(np);
				for (rp = np->n_rule; rp; rp = rp->r_next) {
					print_prerequisites(rp);
				}
				putchar('\n');

				for (rp = np->n_rule; rp; rp = rp->r_next) {
					print_commands(rp);
				}
				putchar('\n');
			} else {
				for (rp = np->n_rule; rp; rp = rp->r_next) {
					print_name(np);
					print_prerequisites(rp);
					putchar('\n');

					print_commands(rp);
					putchar('\n');
				}
			}
		}
	}
}

/*
 * Process options from an argv array.  If from_env is non-zero we're
 * handling options from MAKEFLAGS so skip '-C', '-f', '-p' and '-x'.
 */
static uint32_t
process_options(char **argv, int from_env)
{
	uint32_t flags;

	flags = getopt32(argv, "^" OPTSTR1 OPTSTR2 "\0k-S:S-k",
						&numjobs, &makefiles, &dirs
						IF_FEATURE_MAKE_POSIX(, &pragmas));
	if (from_env && (flags & (OPT_C | OPT_f | OPT_p | OPT_x)))
		error("invalid MAKEFLAGS");
	if (posix && (flags & OPT_C))
		error("-C not allowed");

	return flags;
}

/*
 * Split the contents of MAKEFLAGS into an argv array.  If the return
 * value (call it fargv) isn't NULL the caller should free fargv[1] and
 * fargv.
 */
static char **
expand_makeflags(void)
{
	const char *m, *makeflags = getenv("MAKEFLAGS");
	char *p, *argstr;
	int argc;
	char **argv;

	if (makeflags == NULL)
		return NULL;

	while (isblank(*makeflags))
		makeflags++;

	if (*makeflags == '\0')
		return NULL;

	p = argstr = xzalloc(strlen(makeflags) + 2);

	// If MAKEFLAGS doesn't start with a hyphen, doesn't look like
	// a macro definition and only contains valid option characters,
	// add a hyphen.
	argc = 3;
	if (makeflags[0] != '-' && strchr(makeflags, '=') == NULL) {
		if (strspn(makeflags, OPTSTR1) != strlen(makeflags))
			error("invalid MAKEFLAGS");
		*p++ = '-';
	} else {
		// MAKEFLAGS may need to be split, estimate size of argv array.
		for (m = makeflags; *m; ++m) {
			if (isblank(*m))
				argc++;
		}
	}

	argv = xzalloc(argc * sizeof(char *));
	argc = 0;
	argv[argc++] = (char *)applet_name;
	argv[argc++] = argstr;

	// Copy MAKEFLAGS into argstr, splitting at non-escaped blanks.
	m = makeflags;
	do {
		if (*m == '\\' && m[1] != '\0')
			m++;	// Skip backslash, copy next character unconditionally.
		else if (isblank(*m)) {
			// Terminate current argument and start a new one.
			/* *p = '\0'; - xzalloc did it */
			argv[argc++] = ++p;
			do {
				m++;
			} while (isblank(*m));
			continue;
		}
		*p++ = *m++;
	} while (*m != '\0');
	/* *p = '\0'; - xzalloc did it */
	/* argv[argc] = NULL; - and this */

	return argv;
}

// These macros require special treatment
#define SPECIAL_MACROS "MAKEFLAGS\0SHELL\0CURDIR\0"
#define MAKEFLAGS 0
#define SHELL 1
#define CURDIR 2

/*
 * Instantiate all macros in an argv-style array of pointers.  Stop
 * processing at the first string that doesn't contain an equal sign.
 * As an extension, target arguments on the command line (level 1)
 * are skipped and will be processed later.
 */
static char **
process_macros(char **argv, int level)
{
	char *equal;

	for (; *argv; argv++) {
		char *colon = NULL;
		int idx, immediate = 0;
		int except_dollar = FALSE;

		if (!(equal = strchr(*argv, '='))) {
			// Skip targets on the command line
			if (!posix && level == 1)
				continue;
			else
				// Stop at first target
				break;
		}

		if (equal - 1 > *argv && equal[-1] == ':') {
			if (equal - 2 > *argv && equal[-2] == ':') {
				if (POSIX_2017)
					error("invalid macro assignment");
				if (equal - 3 > *argv  && equal[-3] == ':') {
					// BSD-style ':='.  Expand RHS, but not '$$',
					// resulting macro is delayed expansion.
					colon = equal - 3;
					except_dollar = TRUE;
				} else {
					// GNU-style ':='. Expand RHS, including '$$',
					// resulting macro is immediate expansion.
					colon = equal - 2;
					immediate = M_IMMEDIATE;
				}
				*colon = '\0';
			} else {
				if (posix)
					error("invalid macro assignment");
				colon = equal - 1;
				immediate = M_IMMEDIATE;
				*colon = '\0';
			}
		} else
			*equal = '\0';

		/* We want to process _most_ macro assignments.
		 * There are exceptions for particular values from the
		 * environment. */
		idx = index_in_strings(SPECIAL_MACROS, *argv);
		if (!((level & M_ENVIRON) &&
				(idx == MAKEFLAGS || idx == SHELL ||
					(idx == CURDIR && !useenv && !POSIX_2017)))) {
			if (colon) {
				char *exp = expand_macros(equal + 1, except_dollar);
				setmacro(*argv, exp, level | immediate);
				free(exp);
			} else
				setmacro(*argv, equal + 1, level);
		}

		if (colon)
			*colon = ':';
		else
			*equal = '=';
	}
	return argv;
}

/*
 * Update the MAKEFLAGS macro and environment variable to include any
 * command line options that don't have their default value (apart from
 * -f, -p and -S).  Also add any macros defined on the command line or
 * by the MAKEFLAGS environment variable (apart from MAKEFLAGS itself).
 * Add macros that were defined on the command line to the environment.
 */
static void
update_makeflags(void)
{
	int i;
	char optbuf[] = "-?";
	char *makeflags = NULL;
	char *macro, *s;
	const char *t;
	struct macro *mp;

	t = OPTSTR1;
	for (i = 0; *t; t++) {
		if (*t == ':' || *t == '+')
			continue;
		if ((opts & OPT_MASK & (1 << i))) {
			optbuf[1] = *t;
			makeflags = xappendword(makeflags, optbuf);
			if (*t == 'j') {
				s = auto_string(xasprintf("%d", numjobs));
				makeflags = xappendword(makeflags, s);
			}
		}
		i++;
	}

	for (i = 0; i < HTABSIZE; ++i) {
		for (mp = macrohead[i]; mp; mp = mp->m_next) {
			if (mp->m_level == 1 || mp->m_level == 2) {
				int idx = index_in_strings(SPECIAL_MACROS, mp->m_name);
				if (idx == MAKEFLAGS)
					continue;
				macro = xzalloc(strlen(mp->m_name) + 2 * strlen(mp->m_val) + 1);
				s = stpcpy(macro, mp->m_name);
				*s++ = '=';
				for (t = mp->m_val; *t; t++) {
					if (*t == '\\' || isblank(*t))
						*s++ = '\\';
					*s++ = *t;
				}
				/* *s = '\0'; - xzalloc did it */

				makeflags = xappendword(makeflags, macro);
				free(macro);

				// Add command line macro definitions to the environment
				if (mp->m_level == 1 && idx != SHELL)
					setenv(mp->m_name, mp->m_val, 1);
			}
		}
	}

	if (makeflags) {
		setmacro("MAKEFLAGS", makeflags, 0);
		setenv("MAKEFLAGS", makeflags, 1);
		free(makeflags);
	}
}

#if !ENABLE_PLATFORM_MINGW32
static void
make_handler(int sig)
{
	signal(sig, SIG_DFL);
	remove_target();
	kill(getpid(), sig);
}

static void
init_signal(int sig)
{
	struct sigaction sa, new_action;

	sigemptyset(&new_action.sa_mask);
	new_action.sa_flags = 0;
	new_action.sa_handler = make_handler;

	sigaction(sig, NULL, &sa);
	if (sa.sa_handler != SIG_IGN)
		sigaction_set(sig, &new_action);
}
#endif

/*
 * If the global option flag associated with a special target hasn't
 * been set mark all prerequisites of the target with a flag.  If the
 * target had no prerequisites set the global option flag.
 */
static void
mark_special(const char *special, uint32_t oflag, uint16_t nflag)
{
	struct name *np;
	struct rule *rp;
	struct depend *dp;
	int marked = FALSE;

	if (!(opts & oflag) && (np = findname(special))) {
		for (rp = np->n_rule; rp; rp = rp->r_next) {
			for (dp = rp->r_dep; dp; dp = dp->d_next) {
				dp->d_name->n_flag |= nflag;
				marked = TRUE;
			}
		}

		if (!marked)
			opts |= oflag;
	}
}

int make_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int make_main(int argc UNUSED_PARAM, char **argv)
{
	const char *path, *newpath = NULL;
	char **fargv, **fargv0;
	const char *dir, *file;
#if ENABLE_FEATURE_MAKE_POSIX
	const char *prag;
#endif
	int estat;
	bool found_target;
	FILE *ifd;

	INIT_G();

#if ENABLE_FEATURE_MAKE_POSIX
	if (argv[1] && strcmp(argv[1], "--posix") == 0) {
		argv[1] = argv[0];
		++argv;
		--argc;
		setenv("PDPMAKE_POSIXLY_CORRECT", "", 1);
		posix = TRUE;
	} else {
		posix = getenv("PDPMAKE_POSIXLY_CORRECT") != NULL;
	}
	posix_level = DEFAULT_POSIX_LEVEL;
	pragmas_from_env();
#endif

	if (!POSIX_2017) {
		path = argv[0];
#if ENABLE_PLATFORM_MINGW32
		if (has_path(argv[0])) {
			// Add extension if necessary, else realpath() will fail
			char *p = alloc_ext_space(argv[0]);
			add_win32_extension(p);
			path = newpath = xmalloc_realpath(p);
			free(p);
			if (!path) {
				if (unix_path(argv[0]))
					path = argv[0];
				else
					bb_perror_msg("can't resolve path for %s", argv[0]);
			}
		}
#else
		if (argv[0][0] != '/' && strchr(argv[0], '/')) {
			// Make relative path absolute
			path = newpath = realpath(argv[0], NULL);
			if (!path) {
				bb_perror_msg("can't resolve path for %s", argv[0]);
			}
		}
#endif
	} else {
		path = "make";
	}

	// Process options from MAKEFLAGS
	fargv = fargv0 = expand_makeflags();
	if (fargv0) {
		opts = process_options(fargv, TRUE);
		fargv = fargv0 + optind;
		// Reset getopt(3) so we can call it again
		GETOPT_RESET();
	}

	// Process options from the command line
	opts |= process_options(argv, FALSE);
	argv += optind;

	while ((dir = llist_pop(&dirs))) {
		if (chdir(dir) == -1) {
			bb_perror_msg("can't chdir to %s", dir);
		}
	}

#if ENABLE_FEATURE_MAKE_POSIX
	while ((prag = llist_pop(&pragmas)))
		set_pragma(prag);
	pragmas_to_env();
#endif

#if !ENABLE_PLATFORM_MINGW32
	init_signal(SIGHUP);
	init_signal(SIGTERM);
#endif

	setmacro("$", "$", 0 | M_VALID);

	// Process macro definitions from the command line
	if (!posix)
		process_macros(argv, 1);
	else
		// In POSIX mode macros must appear before targets.
		// argv should now point to targets only.
		argv = process_macros(argv, 1);

	// Process macro definitions from MAKEFLAGS
	if (fargv) {
		process_macros(fargv, 2);
		free(fargv0[1]);
		free(fargv0);
	}

	// Process macro definitions from the environment
	process_macros(environ, 3 | M_ENVIRON);

	// Update MAKEFLAGS and environment
	update_makeflags();

	// Read built-in rules
	input(NULL, 0);

	setmacro("SHELL", DEFAULT_SHELL, 4);
	setmacro("MAKE", path, 4);
	if (!POSIX_2017) {
		char *cwd = xrealloc_getcwd_or_warn(NULL);

		if (cwd) {
			if (!useenv) {
				// Export cwd to environment, if necessary
				char *envcwd = getenv("CURDIR");
				if (envcwd && strcmp(cwd, envcwd) != 0)
					setenv("CURDIR", cwd, 1);
			}
			setmacro("CURDIR", cwd, 4);
		}
		free(cwd);
	}
	free((void *)newpath);

	if (!makefiles) {	// Look for a default Makefile
		if (!posix && (ifd = fopen("PDPmakefile", "r")) != NULL)
			makefile = "PDPmakefile";
		else if ((ifd = fopen("PDPmakefile" + 3, "r")) != NULL)
			makefile = "PDPmakefile" + 3;
#if !ENABLE_PLATFORM_MINGW32
		else if ((ifd = fopen("Makefile", "r")) != NULL)
			makefile = "Makefile";
#endif
		else
			error("no makefile found");
       goto read_makefile;
	}

	while ((file = llist_pop(&makefiles))) {
		if (strcmp(file, "-") == 0) {	// Can use stdin as makefile
			ifd = stdin;
			makefile = "stdin";
		} else {
			ifd = xfopen_for_read(file);
			makefile = file;
		}
 read_makefile:
		input(ifd, 0);
		fclose(ifd);
		makefile = NULL;
	}

	if (print)
		print_details();

	mark_special(".SILENT", OPT_s, N_SILENT);
	mark_special(".IGNORE", OPT_i, N_IGNORE);
	mark_special(".PRECIOUS", OPT_precious, N_PRECIOUS);
	if (!POSIX_2017)
		mark_special(".PHONY", OPT_phony, N_PHONY);

	if (posix) {
		// In POSIX mode only targets should now be in argv.
		found_target = FALSE;
		for (char **a = argv; *a; a++) {
			if (!strchr(*a, '='))
				found_target = TRUE;
			else if (found_target)
				error("macro assignments must precede targets");
		}
	}

	estat = 0;
	found_target = FALSE;
	for (; *argv; argv++) {
		// Skip macro assignments.
		if (strchr(*argv, '='))
			continue;
		found_target = TRUE;
		estat |= make(newname(*argv), 0);
	}
	if (!found_target) {
		if (!firstname)
			error("no targets defined");
		estat = make(firstname, 0);
	}

#if ENABLE_FEATURE_CLEAN_UP
	freenames();
	freemacros();
	llist_free(makefiles, NULL);
	llist_free(dirs, NULL);
#endif

	return estat & MAKE_FAILURE;
}
