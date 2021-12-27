/* vi: set sw=4 ts=4: */
/*
 * ash shell port for busybox
 *
 * This code is derived from software contributed to Berkeley by
 * Kenneth Almquist.
 *
 * Original BSD copyright notice is retained at the end of this file.
 *
 * Copyright (c) 1989, 1991, 1993, 1994
 *      The Regents of the University of California.  All rights reserved.
 *
 * Copyright (c) 1997-2005 Herbert Xu <herbert@gondor.apana.org.au>
 * was re-ported from NetBSD and debianized.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

/*
 * MinGW notes
 *
 * - Environment variables from Windows will all be turned to uppercase.
 * - PATH accepts both ; and : as separator, but can't be mixed
 * - command without ".exe" extension is still understood as executable
 * - shell scripts on the path are detected by the presence of '#!'
 * - both / and \ are supported in PATH. Usually you must use /
 * - job control doesn't work, though the jobs builtin is available
 * - trap doesn't work for signals, only EXIT
 * - /dev/null is supported for redirection
 * - fake $PPID
 */

//config:config SHELL_ASH
//config:	bool #hidden option
//config:	depends on !NOMMU
//config:
//config:config ASH
//config:	bool "ash (78 kb)"
//config:	default y
//config:	depends on !NOMMU
//config:	select SHELL_ASH
//config:	help
//config:	The most complete and most pedantically correct shell included with
//config:	busybox. This shell is actually a derivative of the Debian 'dash'
//config:	shell (by Herbert Xu), which was created by porting the 'ash' shell
//config:	(written by Kenneth Almquist) from NetBSD.
//config:
//config:# ash options
//config:# note: Don't remove !NOMMU part in the next line; it would break
//config:# menuconfig's indenting.
//config:if !NOMMU && (SHELL_ASH || ASH || SH_IS_ASH || BASH_IS_ASH)
//config:
//config:config ASH_OPTIMIZE_FOR_SIZE
//config:	bool "Optimize for size instead of speed"
//config:	default y
//config:	depends on SHELL_ASH
//config:
//config:config ASH_INTERNAL_GLOB
//config:	bool "Use internal glob() implementation"
//config:	default y	# Y is bigger, but because of uclibc glob() bug, let Y be default for now
//config:	depends on SHELL_ASH
//config:	help
//config:	Do not use glob() function from libc, use internal implementation.
//config:	Use this if you are getting "glob.h: No such file or directory"
//config:	or similar build errors.
//config:	Note that as of now (2017-01), uclibc and musl glob() both have bugs
//config:	which would break ash if you select N here.
//config:
//config:config ASH_BASH_COMPAT
//config:	bool "bash-compatible extensions"
//config:	default y
//config:	depends on SHELL_ASH
//config:
//config:config ASH_BASH_SOURCE_CURDIR
//config:	bool "'source' and '.' builtins search current directory after $PATH"
//config:	default n   # do not encourage non-standard behavior
//config:	depends on ASH_BASH_COMPAT
//config:	help
//config:	This is not compliant with standards. Avoid if possible.
//config:
//config:config ASH_BASH_NOT_FOUND_HOOK
//config:	bool "command_not_found_handle hook support"
//config:	default y
//config:	depends on ASH_BASH_COMPAT
//config:	help
//config:	Enable support for the 'command_not_found_handle' hook function,
//config:	from GNU bash, which allows for alternative command not found
//config:	handling.
//config:
//config:config ASH_JOB_CONTROL
//config:	bool "Job control"
//config:	default y
//config:	depends on SHELL_ASH
//config:
//config:config ASH_ALIAS
//config:	bool "Alias support"
//config:	default y
//config:	depends on SHELL_ASH
//config:
//config:config ASH_RANDOM_SUPPORT
//config:	bool "Pseudorandom generator and $RANDOM variable"
//config:	default y
//config:	depends on SHELL_ASH
//config:	help
//config:	Enable pseudorandom generator and dynamic variable "$RANDOM".
//config:	Each read of "$RANDOM" will generate a new pseudorandom value.
//config:	You can reset the generator by using a specified start value.
//config:	After "unset RANDOM" the generator will switch off and this
//config:	variable will no longer have special treatment.
//config:
//config:config ASH_EXPAND_PRMT
//config:	bool "Expand prompt string"
//config:	default y
//config:	depends on SHELL_ASH
//config:	help
//config:	$PS# may contain volatile content, such as backquote commands.
//config:	This option recreates the prompt string from the environment
//config:	variable each time it is displayed.
//config:
//config:config ASH_IDLE_TIMEOUT
//config:	bool "Idle timeout variable $TMOUT"
//config:	default y
//config:	depends on SHELL_ASH
//config:	help
//config:	Enable bash-like auto-logout after $TMOUT seconds of idle time.
//config:
//config:config ASH_MAIL
//config:	bool "Check for new mail in interactive shell"
//config:	default y
//config:	depends on SHELL_ASH
//config:	help
//config:	Enable "check for new mail" function:
//config:	if set, $MAIL file and $MAILPATH list of files
//config:	are checked for mtime changes, and "you have mail"
//config:	message is printed if change is detected.
//config:
//config:config ASH_ECHO
//config:	bool "echo builtin"
//config:	default y
//config:	depends on SHELL_ASH
//config:
//config:config ASH_PRINTF
//config:	bool "printf builtin"
//config:	default y
//config:	depends on SHELL_ASH
//config:
//config:config ASH_TEST
//config:	bool "test builtin"
//config:	default y
//config:	depends on SHELL_ASH
//config:
//config:config ASH_HELP
//config:	bool "help builtin"
//config:	default y
//config:	depends on SHELL_ASH
//config:
//config:config ASH_GETOPTS
//config:	bool "getopts builtin"
//config:	default y
//config:	depends on SHELL_ASH
//config:
//config:config ASH_CMDCMD
//config:	bool "command builtin"
//config:	default y
//config:	depends on SHELL_ASH
//config:	help
//config:	Enable support for the 'command' builtin, which allows
//config:	you to run the specified command or builtin,
//config:	even when there is a function with the same name.
//config:
//config:
//config:config ASH_NOCONSOLE
//config:	bool "'noconsole' option"
//config:	default y
//config:	depends on (ASH || SH_IS_ASH || BASH_IS_ASH) && PLATFORM_MINGW32
//config:	help
//config:	Enable support for the 'noconsole' option, which attempts to
//config:	hide the console normally associated with a command line
//config:	application.  This may be useful when running a shell script
//config:	from a GUI application.
//config:
//config:config ASH_NOCASEGLOB
//config:	bool "'nocaseglob' option"
//config:	default y
//config:	depends on (ASH || SH_IS_ASH || BASH_IS_ASH) && PLATFORM_MINGW32
//config:	help
//config:	Enable support for the 'nocaseglob' option, which allows
//config:	case-insensitive filename globbing.
//config:
//config:endif # ash options

//applet:IF_ASH(APPLET(ash, BB_DIR_BIN, BB_SUID_DROP))
//                      APPLET_ODDNAME:name  main location    suid_type     help
//applet:IF_SH_IS_ASH(  APPLET_ODDNAME(sh,   ash, BB_DIR_BIN, BB_SUID_DROP, ash))
//applet:IF_BASH_IS_ASH(APPLET_ODDNAME(bash, ash, BB_DIR_BIN, BB_SUID_DROP, ash))

//kbuild:lib-$(CONFIG_SHELL_ASH) += ash.o ash_ptr_hack.o shell_common.o
//kbuild:lib-$(CONFIG_ASH_RANDOM_SUPPORT) += random.o

/*
 * DEBUG=1 to compile in debugging ('set -o debug' turns on)
 * DEBUG=2 to compile in and turn on debugging.
 * When debugging is on ("set -o debug" was executed, or DEBUG=2),
 * debugging info is written to ./trace, quit signal generates core dump.
 */
#define DEBUG 0
/* Tweak debug output verbosity here */
#define DEBUG_TIME 0
#define DEBUG_PID 1
#define DEBUG_SIG 1
#define DEBUG_INTONOFF 0

#define PROFILE 0

/*
 * Only one of JOBS or JOBS_WIN32 is enabled at a time (or neither).
 * JOBS_WIN32 doesn't enable job control, just some job-related features.
 */
#if ENABLE_PLATFORM_MINGW32
#define JOBS_WIN32 ENABLE_ASH_JOB_CONTROL
#define JOBS 0
#else
#define JOBS_WIN32 0
#define JOBS ENABLE_ASH_JOB_CONTROL
#endif

#include <fnmatch.h>
#include <sys/times.h>
#include <sys/utsname.h> /* for setting $HOSTNAME */
#include "busybox.h" /* for applet_names */
#if ENABLE_FEATURE_SH_EMBEDDED_SCRIPTS
# include "embedded_scripts.h"
#else
# define NUM_SCRIPTS 0
#endif

/* So far, all bash compat is controlled by one config option */
/* Separate defines document which part of code implements what */
/* function keyword */
#define    BASH_FUNCTION        ENABLE_ASH_BASH_COMPAT
#define IF_BASH_FUNCTION            IF_ASH_BASH_COMPAT
/* &>file */
#define    BASH_REDIR_OUTPUT    ENABLE_ASH_BASH_COMPAT
#define IF_BASH_REDIR_OUTPUT        IF_ASH_BASH_COMPAT
/* $'...' */
#define    BASH_DOLLAR_SQUOTE   ENABLE_ASH_BASH_COMPAT
#define IF_BASH_DOLLAR_SQUOTE       IF_ASH_BASH_COMPAT
#define    BASH_PATTERN_SUBST   ENABLE_ASH_BASH_COMPAT
#define IF_BASH_PATTERN_SUBST       IF_ASH_BASH_COMPAT
#define    BASH_SUBSTR          ENABLE_ASH_BASH_COMPAT
#define IF_BASH_SUBSTR              IF_ASH_BASH_COMPAT
/* BASH_TEST2: [[ EXPR ]]
 * Status of [[ support:
 *   && and || work as they should
 *   = is glob match operator, not equality operator: STR = GLOB
 *   == same as =
 *   =~ is regex match operator: STR =~ REGEX
 * TODO:
 * singleword+noglob expansion:
 *   v='a b'; [[ $v = 'a b' ]]; echo 0:$?
 *   [[ /bin/n* ]]; echo 0:$?
 * quoting needs to be considered (-f is an operator, "-f" and ""-f are not; etc)
 * ( ) < > should not have special meaning (IOW: should not require quoting)
 * in word = GLOB, quoting should be significant on char-by-char basis: a*cd"*"
 */
#define    BASH_TEST2           (ENABLE_ASH_BASH_COMPAT * ENABLE_ASH_TEST)
#define    BASH_SOURCE          ENABLE_ASH_BASH_COMPAT
#define    BASH_PIPEFAIL        ENABLE_ASH_BASH_COMPAT
#define    BASH_HOSTNAME_VAR    ENABLE_ASH_BASH_COMPAT
#define    BASH_EPOCH_VARS      ENABLE_ASH_BASH_COMPAT
#define    BASH_SHLVL_VAR       ENABLE_ASH_BASH_COMPAT
#define    BASH_XTRACEFD        ENABLE_ASH_BASH_COMPAT
#define    BASH_READ_D          ENABLE_ASH_BASH_COMPAT
#define IF_BASH_READ_D              IF_ASH_BASH_COMPAT
#define    BASH_WAIT_N          ENABLE_ASH_BASH_COMPAT
/* <(...) and >(...) */
#if HAVE_DEV_FD
# define    BASH_PROCESS_SUBST   ENABLE_ASH_BASH_COMPAT
# define IF_BASH_PROCESS_SUBST       IF_ASH_BASH_COMPAT
#else
# define    BASH_PROCESS_SUBST 0
# define IF_BASH_PROCESS_SUBST(...)
#endif

#if defined(__ANDROID_API__) && __ANDROID_API__ <= 24
/* Bionic at least up to version 24 has no glob() */
# undef  ENABLE_ASH_INTERNAL_GLOB
# define ENABLE_ASH_INTERNAL_GLOB 1
#endif

#if !ENABLE_ASH_INTERNAL_GLOB && defined(__UCLIBC__)
# error uClibc glob() is buggy, use ASH_INTERNAL_GLOB.
# error The bug is: for "$PWD"/<pattern> ash will escape e.g. dashes in "$PWD"
# error with backslash, even ones which do not need to be: "/a-b" -> "/a\-b"
# error glob() should unbackslash them and match. uClibc does not unbackslash,
# error fails to match dirname, subsequently not expanding <pattern> in it.
// Testcase:
// if (glob("/etc/polkit\\-1", 0, NULL, &pglob)) - this returns 0 on uclibc, no bug
// if (glob("/etc/polkit\\-1/*", 0, NULL, &pglob)) printf("uclibc bug!\n");
#endif

#if !ENABLE_ASH_INTERNAL_GLOB
# include <glob.h>
#endif

#include "unicode.h"
#include "shell_common.h"
#if ENABLE_FEATURE_SH_MATH
# include "math.h"
#else
typedef long arith_t;
# define ARITH_FMT "%ld"
#endif
#if ENABLE_ASH_RANDOM_SUPPORT
# include "random.h"
#else
# define CLEAR_RANDOM_T(rnd) ((void)0)
#endif

#include "NUM_APPLETS.h"
#if NUM_APPLETS == 1
/* STANDALONE does not make sense, and won't compile */
# undef CONFIG_FEATURE_SH_STANDALONE
# undef ENABLE_FEATURE_SH_STANDALONE
# undef IF_FEATURE_SH_STANDALONE
# undef IF_NOT_FEATURE_SH_STANDALONE
# define ENABLE_FEATURE_SH_STANDALONE 0
# define IF_FEATURE_SH_STANDALONE(...)
# define IF_NOT_FEATURE_SH_STANDALONE(...) __VA_ARGS__
#endif

#ifndef F_DUPFD_CLOEXEC
# define F_DUPFD_CLOEXEC F_DUPFD
#endif
#ifndef O_CLOEXEC
# define O_CLOEXEC 0
#endif
#ifndef PIPE_BUF
# define PIPE_BUF 4096           /* amount of buffering in a pipe */
#endif

#ifndef unlikely
# define unlikely(cond) (cond)
#endif

#if !ENABLE_PLATFORM_MINGW32
# define is_relative_path(path) ((path)[0] != '/')
#endif

#if !BB_MMU
# error "Do not even bother, ash will not run on NOMMU machine"
#endif

#if ENABLE_PLATFORM_MINGW32
# define FORKSHELL_DEBUG 0

union node;
struct strlist;
struct job;

struct forkshell {
	/* filled by forkshell_copy() */
	struct globals_var *gvp;
	struct globals_misc *gmp;
	struct tblentry **cmdtable;
#if ENABLE_ASH_ALIAS
	struct alias **atab;
#endif
#if MAX_HISTORY
	char **history;
	int cnt_history;
#endif
	/* struct parsefile *g_parsefile; */
	HANDLE hMapFile;
	char *old_base;
	int size;
# if FORKSHELL_DEBUG
	int funcblocksize;
	int funcstringsize;
# endif
	int relocatesize;

	/* type of forkshell */
	int fpid;

	/* generic data, used by forkshell_child */
	int mode;
	int nprocs;

	/* optional data, used by forkshell_child */
	int flags;
	int fd[3];
	union node *n;
	char **argv;
	char *path;
};

enum {
	FS_OPENHERE,
	FS_EVALBACKCMD,
	FS_EVALSUBSHELL,
	FS_EVALPIPE,
	FS_SHELLEXEC
};

static struct forkshell* forkshell_prepare(struct forkshell *fs);
static void forkshell_init(const char *idstr);
static void *sticky_mem_start, *sticky_mem_end;
static void sticky_free(void *p);
# define free(p) sticky_free(p)
#if !JOBS && !JOBS_WIN32
#define spawn_forkshell(fs, jp, n, mode) spawn_forkshell(fs, jp, mode)
#endif
static void spawn_forkshell(struct forkshell *fs, struct job *jp,
							union node *n, int mode);
# if FORKSHELL_DEBUG
static void forkshell_print(FILE *fp0, struct forkshell *fs, const char **notes);
# endif
#endif

/* ============ Hash table sizes. Configurable. */

#define VTABSIZE 39
#define ATABSIZE 39
#define CMDTABLESIZE 31         /* should be prime */


/* ============ Shell options */

/* If you add/change options hare, update --help text too */
static const char *const optletters_optnames[] = {
	"e"   "errexit",
	"f"   "noglob",
/* bash has '-o ignoreeof', but no short synonym -I for it */
/* (in bash, set -I disables invisible variables (what's that?)) */
	"I"   "ignoreeof",
/* The below allowed this invocation:
 * ash -c 'set -i; echo $-; sleep 5; echo $-'
 * to be ^C-ed and get to interactive ash prompt.
 * bash does not support such "set -i".
 * In our code, this is denoted by empty long name:
 */
	"i"   "",
/* (removing "i" altogether would remove it from "$-", not good) */
	"m"   "monitor",
	"n"   "noexec",
/* Ditto: bash has no "set -s", "set -c" */
#if !ENABLE_PLATFORM_MINGW32
	"s"   "",
#else
	"s"   "stdin",
#endif
	"c"   "",
	"x"   "xtrace",
	"v"   "verbose",
	"C"   "noclobber",
	"a"   "allexport",
	"b"   "notify",
	"u"   "nounset",
	"E"   "errtrace",
	"\0"  "vi"
#if BASH_PIPEFAIL
	,"\0"  "pipefail"
#endif
#if DEBUG
	,"\0"  "nolog"
	,"\0"  "debug"
#endif
#if ENABLE_PLATFORM_MINGW32
	,"X"   "winxp"
#endif
#if ENABLE_ASH_NOCONSOLE
	,"\0"  "noconsole"
#endif
#if ENABLE_ASH_NOCASEGLOB
	,"\0"  "nocaseglob"
#endif
};
//bash 4.4.23 also has these opts (with these defaults):
//braceexpand           on
//emacs                 on
//errtrace              off
//functrace             off
//hashall               on
//histexpand            off
//history               on
//interactive-comments  on
//keyword               off
//onecmd                off
//physical              off
//posix                 off
//privileged            off

#define optletters(n)  optletters_optnames[n][0]
#define optnames(n)   (optletters_optnames[n] + 1)

enum { NOPTS = ARRAY_SIZE(optletters_optnames) };


/* ============ Misc data */

#define msg_illnum "Illegal number: %s"

/*
 * We enclose jmp_buf in a structure so that we can declare pointers to
 * jump locations.  The global variable handler contains the location to
 * jump to when an exception occurs, and the global variable exception_type
 * contains a code identifying the exception.  To implement nested
 * exception handlers, the user should save the value of handler on entry
 * to an inner scope, set handler to point to a jmploc structure for the
 * inner scope, and restore handler on exit from the scope.
 */
struct jmploc {
	jmp_buf loc;
};

struct globals_misc {
	uint8_t exitstatus;     /* exit status of last command */
	uint8_t back_exitstatus;/* exit status of backquoted command */
#if !ENABLE_PLATFORM_MINGW32
	smallint job_warning;   /* user was warned about stopped jobs (can be 2, 1 or 0). */
#endif
	smallint inps4;		/* Prevent PS4 nesting. */
	int savestatus;         /* exit status of last command outside traps */
	int rootpid;            /* pid of main shell */
	/* shell level: 0 for the main shell, 1 for its children, and so on */
	int shlvl;
#if ENABLE_PLATFORM_MINGW32
	int loopnest;           /* current loop nesting level */
#endif
#define rootshell (!shlvl)
	int errlinno;

	char *minusc;  /* argument to -c option */
#if ENABLE_PLATFORM_MINGW32
	char *dirarg;  /* argument to -d option */
	char *title;   /* argument to -t option */
#endif

	char *curdir; // = nullstr;     /* current working directory */
	char *physdir; // = nullstr;    /* physical working directory */

	char *arg0; /* value of $0 */
#if ENABLE_PLATFORM_MINGW32
	char *commandname;
#endif

	struct jmploc *exception_handler;

	volatile int suppress_int; /* counter */
	volatile /*sig_atomic_t*/ smallint pending_int; /* 1 = got SIGINT */
#if !ENABLE_PLATFORM_MINGW32
	volatile /*sig_atomic_t*/ smallint got_sigchld; /* 1 = got SIGCHLD */
	volatile /*sig_atomic_t*/ smallint pending_sig;	/* last pending signal */
#endif
	smallint exception_type; /* kind of exception: */
#define EXINT 0         /* SIGINT received */
#define EXERROR 1       /* a generic error */
#define EXEND 3         /* exit the shell */
#define EXEXIT 4        /* exit the shell via exitcmd */

	char nullstr[1];        /* zero length string */

	char optlist[NOPTS];
#define eflag optlist[0]
#define fflag optlist[1]
#define Iflag optlist[2]
#define iflag optlist[3]
#define mflag optlist[4]
#define nflag optlist[5]
#define sflag optlist[6]
#define cflag optlist[7]
#define xflag optlist[8]
#define vflag optlist[9]
#define Cflag optlist[10]
#define aflag optlist[11]
#define bflag optlist[12]
#define uflag optlist[13]
#define Eflag optlist[14]
#define viflag optlist[15]
#if BASH_PIPEFAIL
# define pipefail optlist[16]
#else
# define pipefail 0
#endif
#if DEBUG
# define nolog optlist[16 + BASH_PIPEFAIL]
# define debug optlist[17 + BASH_PIPEFAIL]
#endif
#if ENABLE_PLATFORM_MINGW32
# define winxp optlist[16 + BASH_PIPEFAIL + 2*(DEBUG != 0)]
# if ENABLE_ASH_NOCONSOLE
#  define noconsole optlist[17 + BASH_PIPEFAIL + 2*(DEBUG != 0)]
# endif
# if ENABLE_ASH_NOCASEGLOB
#  define nocaseglob optlist[17 + BASH_PIPEFAIL + 2*(DEBUG != 0) + ENABLE_ASH_NOCONSOLE]
# endif
#endif

	/* trap handler commands */
#if !ENABLE_PLATFORM_MINGW32
	/*
	 * Sigmode records the current value of the signal handlers for the various
	 * modes.  A value of zero means that the current handler is not known.
	 * S_HARD_IGN indicates that the signal was ignored on entry to the shell.
	 */
	char sigmode[NSIG - 1];
#define S_DFL      1            /* default signal handling (SIG_DFL) */
#define S_CATCH    2            /* signal is caught */
#define S_IGN      3            /* signal is ignored (SIG_IGN) */
#define S_HARD_IGN 4            /* signal is ignored permanently (it was SIG_IGN on entry to shell) */

	/* indicates specified signal received */
	uint8_t gotsig[NSIG - 1]; /* offset by 1: "signal" 0 is meaningless */
	uint8_t may_have_traps; /* 0: definitely no traps are set, 1: some traps may be set */
#endif
	char *trap[NSIG + 1];
/* trap[0] is EXIT trap, trap[NTRAP_ERR] is ERR trap, other trap[i] are signal traps */
#define NTRAP_ERR  NSIG
#define NTRAP_LAST NSIG

#if !ENABLE_PLATFORM_MINGW32
	char **trap_ptr;        /* used only by "trap hack" */
#endif

	/* Rarely referenced stuff */
#if ENABLE_ASH_RANDOM_SUPPORT
	random_t random_gen;
#endif
	pid_t backgndpid;        /* pid of last background process */
};
extern struct globals_misc *BB_GLOBAL_CONST ash_ptr_to_globals_misc;
#define G_misc (*ash_ptr_to_globals_misc)
#define exitstatus        (G_misc.exitstatus )
#define back_exitstatus   (G_misc.back_exitstatus )
#define job_warning       (G_misc.job_warning)
#define inps4       (G_misc.inps4      )
#define savestatus  (G_misc.savestatus )
#define rootpid     (G_misc.rootpid    )
#define shlvl       (G_misc.shlvl      )
#define errlinno    (G_misc.errlinno   )
#if ENABLE_PLATFORM_MINGW32
#define loopnest    (G_misc.loopnest   )
#endif
#define minusc      (G_misc.minusc     )
#if ENABLE_PLATFORM_MINGW32
#define dirarg      (G_misc.dirarg     )
#define title       (G_misc.title      )
#endif
#define curdir      (G_misc.curdir     )
#define physdir     (G_misc.physdir    )
#define arg0        (G_misc.arg0       )
#if ENABLE_PLATFORM_MINGW32
#define commandname (G_misc.commandname)
#endif
#define exception_handler (G_misc.exception_handler)
#define exception_type    (G_misc.exception_type   )
#define suppress_int      (G_misc.suppress_int     )
#define pending_int       (G_misc.pending_int      )
#define got_sigchld       (G_misc.got_sigchld      )
#define pending_sig       (G_misc.pending_sig      )
#define nullstr     (G_misc.nullstr    )
#define optlist     (G_misc.optlist    )
#define sigmode     (G_misc.sigmode    )
#define gotsig      (G_misc.gotsig     )
#define may_have_traps    (G_misc.may_have_traps   )
#define trap        (G_misc.trap       )
#define trap_ptr    (G_misc.trap_ptr   )
#define random_gen  (G_misc.random_gen )
#define backgndpid  (G_misc.backgndpid )

#if ENABLE_PLATFORM_MINGW32
#undef got_sigchld
#undef pending_sig
#undef may_have_traps
#undef trap_ptr
#define pending_sig       (0)
#define may_have_traps    (0)
#define trap_ptr          trap
#endif

#define INIT_G_misc() do { \
	XZALLOC_CONST_PTR(&ash_ptr_to_globals_misc, sizeof(G_misc)); \
	savestatus = -1; \
	curdir = nullstr; \
	physdir = nullstr; \
	IF_NOT_PLATFORM_MINGW32(trap_ptr = trap;) \
} while (0)


/* ============ DEBUG */
#if DEBUG
static void trace_printf(const char *fmt, ...);
static void trace_vprintf(const char *fmt, va_list va);
# define TRACE(param)    trace_printf param
# define TRACEV(param)   trace_vprintf param
# if ENABLE_PLATFORM_MINGW32 && defined(close)
#  undef close
# endif
# define close(fd) do { \
	int dfd = (fd); \
	if (close(dfd) < 0) \
		bb_error_msg("bug on %d: closing %d(0x%x)", \
			__LINE__, dfd, dfd); \
} while (0)
#else
# define TRACE(param)
# define TRACEV(param)
#endif


/* ============ Utility functions */
#define is_name(c)      ((c) == '_' || isalpha((unsigned char)(c)))
#define is_in_name(c)   ((c) == '_' || isalnum((unsigned char)(c)))

static int
isdigit_str9(const char *str)
{
	int maxlen = 9 + 1; /* max 9 digits: 999999999 */
	while (--maxlen && isdigit(*str))
		str++;
	return (*str == '\0');
}

static const char *
var_end(const char *var)
{
	while (*var)
		if (*var++ == '=')
			break;
	return var;
}


/* ============ Parser data */

/*
 * ash_vmsg() needs parsefile->fd, hence parsefile definition is moved up.
 */
struct strlist {
	struct strlist *next;
	char *text;
};

struct alias;

struct strpush {
	struct strpush *prev;   /* preceding string on stack */
	char *prev_string;
	int prev_left_in_line;
#if ENABLE_ASH_ALIAS
	struct alias *ap;       /* if push was associated with an alias */
#endif
	char *string;           /* remember the string since it may change */

	/* Delay freeing so we can stop nested aliases. */
	struct strpush *spfree;

	/* Remember last two characters for pungetc. */
	int lastc[2];

	/* Number of outstanding calls to pungetc. */
	int unget;
};

/*
 * The parsefile structure pointed to by the global variable parsefile
 * contains information about the current file being read.
 */
struct parsefile {
	struct parsefile *prev; /* preceding file on stack */
	int linno;              /* current line */
	int pf_fd;              /* file descriptor (or -1 if string) */
	int left_in_line;       /* number of chars left in this line */
	int left_in_buffer;     /* number of chars left in this buffer past the line */
	char *next_to_pgetc;    /* next char in buffer */
	char *buf;              /* input buffer */
	struct strpush *strpush; /* for pushing strings at this level */
	struct strpush basestrpush; /* so pushing one is fast */

	/* Delay freeing so we can stop nested aliases. */
	struct strpush *spfree;

	/* Remember last two characters for pungetc. */
	int lastc[2];

	/* Number of outstanding calls to pungetc. */
	int unget;
};

static struct parsefile basepf;        /* top level input file */
static struct parsefile *g_parsefile = &basepf;  /* current input file */
#if ENABLE_PLATFORM_POSIX
static char *commandname;              /* currently executing command */
#endif


/* ============ Interrupts / exceptions */

static void exitshell(void) NORETURN;

/*
 * These macros allow the user to suspend the handling of interrupt signals
 * over a period of time.  This is similar to SIGHOLD or to sigblock, but
 * much more efficient and portable.  (But hacking the kernel is so much
 * more fun than worrying about efficiency and portability. :-))
 */
#if DEBUG_INTONOFF
# define INT_OFF do { \
	TRACE(("%s:%d INT_OFF(%d)\n", __func__, __LINE__, suppress_int)); \
	suppress_int++; \
	barrier(); \
} while (0)
#else
# define INT_OFF do { \
	suppress_int++; \
	barrier(); \
} while (0)
#endif

/*
 * Called to raise an exception.  Since C doesn't include exceptions, we
 * just do a longjmp to the exception handler.  The type of exception is
 * stored in the global variable "exception_type".
 */
static void raise_exception(int) NORETURN;
static void
raise_exception(int e)
{
#if DEBUG
	if (exception_handler == NULL)
		abort();
#endif
	INT_OFF;
	exception_type = e;
	longjmp(exception_handler->loc, 1);
}
#if DEBUG
#define raise_exception(e) do { \
	TRACE(("raising exception %d on line %d\n", (e), __LINE__)); \
	raise_exception(e); \
} while (0)
#endif

/*
 * Called when a SIGINT is received.  (If the user specifies
 * that SIGINT is to be trapped or ignored using the trap builtin, then
 * this routine is not called.)  Suppressint is nonzero when interrupts
 * are held using the INT_OFF macro.  (The test for iflag is just
 * defensive programming.)
 */
static void raise_interrupt(void) NORETURN;
static void
raise_interrupt(void)
{
	pending_int = 0;
#if !ENABLE_PLATFORM_MINGW32
	/* Signal is not automatically unmasked after it is raised,
	 * do it ourself - unmask all signals */
	sigprocmask_allsigs(SIG_UNBLOCK);
#endif
	/* pending_sig = 0; - now done in signal_handler() */

	if (!(rootshell && iflag)) {
		/* Kill ourself with SIGINT */
		signal(SIGINT, SIG_DFL);
		raise(SIGINT);
	}
#if ENABLE_PLATFORM_MINGW32
	if (iflag)
		write(STDOUT_FILENO, "^C", 2);
#endif
	/* bash: ^C even on empty command line sets $? */
	exitstatus = SIGINT + 128;
	raise_exception(EXINT);
	/* NOTREACHED */
}
#if DEBUG
#define raise_interrupt() do { \
	TRACE(("raising interrupt on line %d\n", __LINE__)); \
	raise_interrupt(); \
} while (0)
#endif

static IF_ASH_OPTIMIZE_FOR_SIZE(inline) void
int_on(void)
{
	barrier();
	if (--suppress_int == 0 && pending_int) {
		raise_interrupt();
	}
}
#if DEBUG_INTONOFF
# define INT_ON do { \
	TRACE(("%s:%d INT_ON(%d)\n", __func__, __LINE__, suppress_int-1)); \
	int_on(); \
} while (0)
#else
# define INT_ON int_on()
#endif
static IF_ASH_OPTIMIZE_FOR_SIZE(inline) void
force_int_on(void)
{
	barrier();
	suppress_int = 0;
	if (pending_int)
		raise_interrupt();
}
#define FORCE_INT_ON force_int_on()

#define SAVE_INT(v) ((v) = suppress_int)

#define RESTORE_INT(v) do { \
	barrier(); \
	suppress_int = (v); \
	if (suppress_int == 0 && pending_int) \
		raise_interrupt(); \
} while (0)


/* ============ Stdout/stderr output */

static void
outstr(const char *p, FILE *file)
{
	INT_OFF;
	fputs(p, file);
	INT_ON;
}

static void
flush_stdout_stderr(void)
{
	INT_OFF;
	fflush_all();
	INT_ON;
}

/* Was called outcslow(c,FILE*), but c was always '\n' */
static void
newline_and_flush(FILE *dest)
{
	INT_OFF;
	putc('\n', dest);
	fflush(dest);
	INT_ON;
}

static int out1fmt(const char *, ...) __attribute__((__format__(__printf__,1,2)));
static int
out1fmt(const char *fmt, ...)
{
	va_list ap;
	int r;

	INT_OFF;
	va_start(ap, fmt);
	r = vprintf(fmt, ap);
	va_end(ap);
	INT_ON;
	return r;
}

static int fmtstr(char *, size_t, const char *, ...) __attribute__((__format__(__printf__,3,4)));
static int
fmtstr(char *outbuf, size_t length, const char *fmt, ...)
{
	va_list ap;
	int ret;

	INT_OFF;
	va_start(ap, fmt);
	ret = vsnprintf(outbuf, length, fmt, ap);
	va_end(ap);
	INT_ON;
	return ret > (int)length ? length : ret;
}

static void
out1str(const char *p)
{
	outstr(p, stdout);
}

static void
out2str(const char *p)
{
	outstr(p, stderr);
	flush_stdout_stderr();
}


/* ============ Parser structures */

/* control characters in argument strings */
#define CTL_FIRST CTLESC
#define CTLESC       ((unsigned char)'\201')    /* escape next character */
#define CTLVAR       ((unsigned char)'\202')    /* variable defn */
#define CTLENDVAR    ((unsigned char)'\203')
#define CTLBACKQ     ((unsigned char)'\204')
#define CTLARI       ((unsigned char)'\206')    /* arithmetic expression */
#define CTLENDARI    ((unsigned char)'\207')
#define CTLQUOTEMARK ((unsigned char)'\210')
#define CTL_LAST CTLQUOTEMARK
#if BASH_PROCESS_SUBST
# define CTLTOPROC    ((unsigned char)'\211')
# define CTLFROMPROC  ((unsigned char)'\212')
# undef CTL_LAST
# define CTL_LAST CTLFROMPROC
#endif

/* variable substitution byte (follows CTLVAR) */
#define VSTYPE  0x0f            /* type of variable substitution */
#define VSNUL   0x10            /* colon--treat the empty string as unset */

/* values of VSTYPE field */
#define VSNORMAL        0x1     /* normal variable:  $var or ${var} */
#define VSMINUS         0x2     /* ${var-text} */
#define VSPLUS          0x3     /* ${var+text} */
#define VSQUESTION      0x4     /* ${var?message} */
#define VSASSIGN        0x5     /* ${var=text} */
#define VSTRIMRIGHT     0x6     /* ${var%pattern} */
#define VSTRIMRIGHTMAX  0x7     /* ${var%%pattern} */
#define VSTRIMLEFT      0x8     /* ${var#pattern} */
#define VSTRIMLEFTMAX   0x9     /* ${var##pattern} */
#define VSLENGTH        0xa     /* ${#var} */
#if BASH_SUBSTR
#define VSSUBSTR        0xc     /* ${var:position:length} */
#endif
#if BASH_PATTERN_SUBST
#define VSREPLACE       0xd     /* ${var/pattern/replacement} */
#define VSREPLACEALL    0xe     /* ${var//pattern/replacement} */
#endif

static const char dolatstr[] ALIGN1 = {
	CTLQUOTEMARK, CTLVAR, VSNORMAL, '@', '=', CTLQUOTEMARK, '\0'
};
#define DOLATSTRLEN 6

#define NCMD      0
#define NPIPE     1
#define NREDIR    2
#define NBACKGND  3
#define NSUBSHELL 4
#define NAND      5
#define NOR       6
#define NSEMI     7
#define NIF       8
#define NWHILE    9
#define NUNTIL   10
#define NFOR     11
#define NCASE    12
#define NCLIST   13
#define NDEFUN   14
#define NARG     15
#define NTO      16
#if BASH_REDIR_OUTPUT
#define NTO2     17
#endif
#define NCLOBBER 18
#define NFROM    19
#define NFROMTO  20
#define NAPPEND  21
#define NTOFD    22
#define NFROMFD  23
#define NHERE    24
#define NXHERE   25
#define NNOT     26
#define N_NUMBER 27

union node;

struct ncmd {
	smallint type; /* Nxxxx */
	int linno;
	union node *assign;
	union node *args;
	union node *redirect;
};

struct npipe {
	smallint type;
	smallint pipe_backgnd;
	struct nodelist *cmdlist;
};

struct nredir {
	smallint type;
	int linno;
	union node *n;
	union node *redirect;
};

struct nbinary {
	smallint type;
	union node *ch1;
	union node *ch2;
};

struct nif {
	smallint type;
	union node *test;
	union node *ifpart;
	union node *elsepart;
};

struct nfor {
	smallint type;
	int linno;
	union node *args;
	union node *body;
	char *var;
};

struct ncase {
	smallint type;
	int linno;
	union node *expr;
	union node *cases;
};

struct nclist {
	smallint type;
	union node *next;
	union node *pattern;
	union node *body;
};

struct ndefun {
	smallint type;
	int linno;
	char *text;
	union node *body;
};

struct narg {
	smallint type;
	union node *next;
	char *text;
	struct nodelist *backquote;
};

/* nfile and ndup layout must match!
 * NTOFD (>&fdnum) uses ndup structure, but we may discover mid-flight
 * that it is actually NTO2 (>&file), and change its type.
 */
struct nfile {
	smallint type;
	union node *next;
	int fd;
	int _unused_dupfd;
	union node *fname;
	char *expfname;
};

struct ndup {
	smallint type;
	union node *next;
	int fd;
	int dupfd;
	union node *vname;
	char *_unused_expfname;
};

struct nhere {
	smallint type;
	union node *next;
	int fd;
	union node *doc;
};

struct nnot {
	smallint type;
	union node *com;
};

union node {
	smallint type;
	struct ncmd ncmd;
	struct npipe npipe;
	struct nredir nredir;
	struct nbinary nbinary;
	struct nif nif;
	struct nfor nfor;
	struct ncase ncase;
	struct nclist nclist;
	struct ndefun ndefun;
	struct narg narg;
	struct nfile nfile;
	struct ndup ndup;
	struct nhere nhere;
	struct nnot nnot;
};

/*
 * NODE_EOF is returned by parsecmd when it encounters an end of file.
 * It must be distinct from NULL.
 */
#define NODE_EOF ((union node *) -1L)

struct nodelist {
	struct nodelist *next;
	union node *n;
};

struct funcnode {
	int count;
	union node n;
};

/*
 * Free a parse tree.
 */
static void
freefunc(struct funcnode *f)
{
	if (f && --f->count < 0)
		free(f);
}


/* ============ Debugging output */

#if DEBUG

static FILE *tracefile;

static void
trace_printf(const char *fmt, ...)
{
	va_list va;

	if (debug != 1)
		return;
	if (DEBUG_TIME)
		fprintf(tracefile, "%u ", (int) time(NULL));
	if (DEBUG_PID)
		fprintf(tracefile, "[%u] ", (int) getpid());
	if (DEBUG_SIG)
		fprintf(tracefile, "pending s:%d i:%d(supp:%d) ", pending_sig, pending_int, suppress_int);
	va_start(va, fmt);
	vfprintf(tracefile, fmt, va);
	va_end(va);
}

static void
trace_vprintf(const char *fmt, va_list va)
{
	if (debug != 1)
		return;
	vfprintf(tracefile, fmt, va);
	fprintf(tracefile, "\n");
}

static void
trace_puts(const char *s)
{
	if (debug != 1)
		return;
	fputs(s, tracefile);
}

static void
trace_puts_quoted(char *s)
{
	char *p;
	char c;

	if (debug != 1)
		return;
	putc('"', tracefile);
	for (p = s; *p; p++) {
		switch ((unsigned char)*p) {
		case '\n': c = 'n'; goto backslash;
		case '\t': c = 't'; goto backslash;
		case '\r': c = 'r'; goto backslash;
		case '\"': c = '\"'; goto backslash;
		case '\\': c = '\\'; goto backslash;
		case CTLESC: c = 'e'; goto backslash;
		case CTLVAR: c = 'v'; goto backslash;
		case CTLBACKQ: c = 'q'; goto backslash;
#if BASH_PROCESS_SUBST
		case CTLTOPROC: c = 'p'; goto backslash;
		case CTLFROMPROC: c = 'P'; goto backslash;
#endif
 backslash:
			putc('\\', tracefile);
			putc(c, tracefile);
			break;
		default:
			if (*p >= ' ' && *p <= '~')
				putc(*p, tracefile);
			else {
				putc('\\', tracefile);
				putc((*p >> 6) & 03, tracefile);
				putc((*p >> 3) & 07, tracefile);
				putc(*p & 07, tracefile);
			}
			break;
		}
	}
	putc('"', tracefile);
}

static void
trace_puts_args(char **ap)
{
	if (debug != 1)
		return;
	if (!*ap)
		return;
	while (1) {
		trace_puts_quoted(*ap);
		if (!*++ap) {
			putc('\n', tracefile);
			break;
		}
		putc(' ', tracefile);
	}
}

static void
opentrace(void)
{
	char s[100];
#ifdef O_APPEND
	int flags;
#endif

	if (debug != 1) {
		if (tracefile)
			fflush(tracefile);
		/* leave open because libedit might be using it */
		return;
	}
	strcpy(s, "./trace");
	if (tracefile) {
		if (!freopen(s, "a", tracefile)) {
			fprintf(stderr, "Can't re-open %s\n", s);
			debug = 0;
			return;
		}
	} else {
		tracefile = fopen(s, "a");
		if (tracefile == NULL) {
			fprintf(stderr, "Can't open %s\n", s);
			debug = 0;
			return;
		}
	}
#ifdef O_APPEND
	flags = fcntl(fileno(tracefile), F_GETFL);
	if (flags >= 0)
		fcntl(fileno(tracefile), F_SETFL, flags | O_APPEND);
#endif
	setlinebuf(tracefile);
	fputs("\nTracing started.\n", tracefile);
}

static void
indent(int amount, char *pfx, FILE *fp)
{
	int i;

	for (i = 0; i < amount; i++) {
		if (pfx && i == amount - 1)
			fputs(pfx, fp);
		putc('\t', fp);
	}
}

/* little circular references here... */
static void shtree(union node *n, int ind, char *pfx, FILE *fp);

static void
sharg(union node *arg, FILE *fp)
{
	char *p;
	struct nodelist *bqlist;
	unsigned char subtype;

	if (arg->type != NARG) {
		out1fmt("<node type %d>\n", arg->type);
		abort();
	}
	bqlist = arg->narg.backquote;
	for (p = arg->narg.text; *p; p++) {
		switch ((unsigned char)*p) {
		case CTLESC:
			p++;
			putc(*p, fp);
			break;
		case CTLVAR:
			putc('$', fp);
			putc('{', fp);
			subtype = *++p;
			if (subtype == VSLENGTH)
				putc('#', fp);

			while (*p != '=') {
				putc(*p, fp);
				p++;
			}

			if (subtype & VSNUL)
				putc(':', fp);

			switch (subtype & VSTYPE) {
			case VSNORMAL:
				putc('}', fp);
				break;
			case VSMINUS:
				putc('-', fp);
				break;
			case VSPLUS:
				putc('+', fp);
				break;
			case VSQUESTION:
				putc('?', fp);
				break;
			case VSASSIGN:
				putc('=', fp);
				break;
			case VSTRIMLEFT:
				putc('#', fp);
				break;
			case VSTRIMLEFTMAX:
				putc('#', fp);
				putc('#', fp);
				break;
			case VSTRIMRIGHT:
				putc('%', fp);
				break;
			case VSTRIMRIGHTMAX:
				putc('%', fp);
				putc('%', fp);
				break;
			case VSLENGTH:
				break;
			default:
				out1fmt("<subtype %d>", subtype);
			}
			break;
		case CTLENDVAR:
			putc('}', fp);
			break;
#if BASH_PROCESS_SUBST
		case CTLTOPROC:
			putc('>', fp);
			goto backq;
		case CTLFROMPROC:
			putc('<', fp);
			goto backq;
#endif
		case CTLBACKQ:
			putc('$', fp);
 IF_BASH_PROCESS_SUBST(backq:)
			putc('(', fp);
			shtree(bqlist->n, -1, NULL, fp);
			putc(')', fp);
			break;
		default:
			putc(*p, fp);
			break;
		}
	}
}

static void
shcmd(union node *cmd, FILE *fp)
{
	union node *np;
	int first;
	const char *s;
	int dftfd;

	first = 1;
	for (np = cmd->ncmd.args; np; np = np->narg.next) {
		if (!first)
			putc(' ', fp);
		sharg(np, fp);
		first = 0;
	}
	for (np = cmd->ncmd.redirect; np; np = np->nfile.next) {
		if (!first)
			putc(' ', fp);
		dftfd = 0;
		switch (np->nfile.type) {
		case NTO:      s = ">>"+1; dftfd = 1; break;
		case NCLOBBER: s = ">|"; dftfd = 1; break;
		case NAPPEND:  s = ">>"; dftfd = 1; break;
#if BASH_REDIR_OUTPUT
		case NTO2:
#endif
		case NTOFD:    s = ">&"; dftfd = 1; break;
		case NFROM:    s = "<"; break;
		case NFROMFD:  s = "<&"; break;
		case NFROMTO:  s = "<>"; break;
		default:       s = "*error*"; break;
		}
		if (np->nfile.fd != dftfd)
			fprintf(fp, "%d", np->nfile.fd);
		fputs(s, fp);
		if (np->nfile.type == NTOFD || np->nfile.type == NFROMFD) {
			fprintf(fp, "%d", np->ndup.dupfd);
		} else {
			sharg(np->nfile.fname, fp);
		}
		first = 0;
	}
}

static void
shtree(union node *n, int ind, char *pfx, FILE *fp)
{
	struct nodelist *lp;
	const char *s;

	if (n == NULL)
		return;

	indent(ind, pfx, fp);

	if (n == NODE_EOF) {
		fputs("<EOF>", fp);
		return;
	}

	switch (n->type) {
	case NSEMI:
		s = "; ";
		goto binop;
	case NAND:
		s = " && ";
		goto binop;
	case NOR:
		s = " || ";
 binop:
		shtree(n->nbinary.ch1, ind, NULL, fp);
		/* if (ind < 0) */
			fputs(s, fp);
		shtree(n->nbinary.ch2, ind, NULL, fp);
		break;
	case NCMD:
		shcmd(n, fp);
		if (ind >= 0)
			putc('\n', fp);
		break;
	case NPIPE:
		for (lp = n->npipe.cmdlist; lp; lp = lp->next) {
			shtree(lp->n, 0, NULL, fp);
			if (lp->next)
				fputs(" | ", fp);
		}
		if (n->npipe.pipe_backgnd)
			fputs(" &", fp);
		if (ind >= 0)
			putc('\n', fp);
		break;
	default:
		fprintf(fp, "<node type %d>", n->type);
		if (ind >= 0)
			putc('\n', fp);
		break;
	}
}

static void
showtree(union node *n)
{
	trace_puts("showtree called\n");
	shtree(n, 1, NULL, stderr);
}

#endif /* DEBUG */


/* ============ Message printing */

static void
ash_vmsg(const char *msg, va_list ap)
{
	fprintf(stderr, "%s: ", arg0);
	if (commandname) {
		if (strcmp(arg0, commandname))
			fprintf(stderr, "%s: ", commandname);
		if (!iflag || g_parsefile->pf_fd > 0)
			fprintf(stderr, "line %d: ", errlinno);
	}
	vfprintf(stderr, msg, ap);
	newline_and_flush(stderr);
}

/*
 * Exverror is called to raise the error exception.  If the second argument
 * is not NULL then error prints an error message using printf style
 * formatting.  It then raises the error exception.
 */
static void ash_vmsg_and_raise(int, const char *, va_list) NORETURN;
static void
ash_vmsg_and_raise(int cond, const char *msg, va_list ap)
{
#if DEBUG
	if (msg) {
		TRACE(("ash_vmsg_and_raise(%d):", cond));
		TRACEV((msg, ap));
	} else
		TRACE(("ash_vmsg_and_raise(%d):NULL\n", cond));
	if (msg)
#endif
		ash_vmsg(msg, ap);

	flush_stdout_stderr();
	raise_exception(cond);
	/* NOTREACHED */
}

static void ash_msg_and_raise_error(const char *, ...) NORETURN;
static void
ash_msg_and_raise_error(const char *msg, ...)
{
	va_list ap;

	exitstatus = 2;

	va_start(ap, msg);
	ash_vmsg_and_raise(EXERROR, msg, ap);
	/* NOTREACHED */
	va_end(ap);
}

/*
 * 'fmt' must be a string literal.
 */
#define ash_msg_and_raise_perror(fmt, ...) ash_msg_and_raise_error(fmt ": "STRERROR_FMT, ##__VA_ARGS__ STRERROR_ERRNO)

static void raise_error_syntax(const char *) NORETURN;
static void
raise_error_syntax(const char *msg)
{
	errlinno = g_parsefile->linno;
	ash_msg_and_raise_error("syntax error: %s", msg);
	/* NOTREACHED */
}

static void ash_msg_and_raise(int, const char *, ...) NORETURN;
static void
ash_msg_and_raise(int cond, const char *msg, ...)
{
	va_list ap;

	va_start(ap, msg);
	ash_vmsg_and_raise(cond, msg, ap);
	/* NOTREACHED */
	va_end(ap);
}

/*
 * error/warning routines for external builtins
 */
static void
ash_msg(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	ash_vmsg(fmt, ap);
	va_end(ap);
}

/*
 * Return a string describing an error.  The returned string may be a
 * pointer to a static buffer that will be overwritten on the next call.
 * Action describes the operation that got the error.
 */
static const char *
errmsg(int e, const char *em)
{
	if (e == ENOENT || e == ENOTDIR) {
		return em;
	}
	return strerror(e);
}


/* ============ Memory allocation */

#if 0
/* I consider these wrappers nearly useless:
 * ok, they return you to nearest exception handler, but
 * how much memory do you leak in the process, making
 * memory starvation worse?
 */
static void *
ckrealloc(void * p, size_t nbytes)
{
	p = realloc(p, nbytes);
	if (!p)
		ash_msg_and_raise_error(bb_msg_memory_exhausted);
	return p;
}

static void *
ckmalloc(size_t nbytes)
{
	return ckrealloc(NULL, nbytes);
}

static void *
ckzalloc(size_t nbytes)
{
	return memset(ckmalloc(nbytes), 0, nbytes);
}

static char *
ckstrdup(const char *s)
{
	char *p = strdup(s);
	if (!p)
		ash_msg_and_raise_error(bb_msg_memory_exhausted);
	return p;
}
#else
/* Using bbox equivalents. They exit if out of memory */
# define ckrealloc xrealloc
# define ckmalloc  xmalloc
# define ckzalloc  xzalloc
# define ckstrdup  xstrdup
#endif

/*
 * It appears that grabstackstr() will barf with such alignments
 * because stalloc() will return a string allocated in a new stackblock.
 */
#define SHELL_ALIGN(nbytes) (((nbytes) + SHELL_SIZE) & ~SHELL_SIZE)
enum {
	/* Most machines require the value returned from malloc to be aligned
	 * in some way.  The following macro will get this right
	 * on many machines.  */
	SHELL_SIZE = sizeof(union { int i; char *cp; double d; }) - 1,
	/* Minimum size of a block */
	MINSIZE = SHELL_ALIGN(504),
};

struct stack_block {
	struct stack_block *prev;
	char space[MINSIZE];
};

struct stackmark {
	struct stack_block *stackp;
	char *stacknxt;
	size_t stacknleft;
};


struct globals_memstack {
	struct stack_block *g_stackp; // = &stackbase;
	char *g_stacknxt; // = stackbase.space;
	char *sstrend; // = stackbase.space + MINSIZE;
	size_t g_stacknleft; // = MINSIZE;
	struct stack_block stackbase;
};
extern struct globals_memstack *BB_GLOBAL_CONST ash_ptr_to_globals_memstack;
#define G_memstack (*ash_ptr_to_globals_memstack)
#define g_stackp     (G_memstack.g_stackp    )
#define g_stacknxt   (G_memstack.g_stacknxt  )
#define sstrend      (G_memstack.sstrend     )
#define g_stacknleft (G_memstack.g_stacknleft)
#define stackbase    (G_memstack.stackbase   )
#define INIT_G_memstack() do { \
	XZALLOC_CONST_PTR(&ash_ptr_to_globals_memstack, sizeof(G_memstack)); \
	g_stackp = &stackbase; \
	g_stacknxt = stackbase.space; \
	g_stacknleft = MINSIZE; \
	sstrend = stackbase.space + MINSIZE; \
} while (0)


#define stackblock()     ((void *)g_stacknxt)
#define stackblocksize() g_stacknleft

/*
 * Parse trees for commands are allocated in lifo order, so we use a stack
 * to make this more efficient, and also to avoid all sorts of exception
 * handling code to handle interrupts in the middle of a parse.
 *
 * The size 504 was chosen because the Ultrix malloc handles that size
 * well.
 */
static void *
stalloc(size_t nbytes)
{
	char *p;
	size_t aligned;

	aligned = SHELL_ALIGN(nbytes);
	if (aligned > g_stacknleft) {
		size_t len;
		size_t blocksize;
		struct stack_block *sp;

		blocksize = aligned;
		if (blocksize < MINSIZE)
			blocksize = MINSIZE;
		len = sizeof(struct stack_block) - MINSIZE + blocksize;
		if (len < blocksize)
			ash_msg_and_raise_error(bb_msg_memory_exhausted);
		INT_OFF;
		sp = ckmalloc(len);
		sp->prev = g_stackp;
		g_stacknxt = sp->space;
		g_stacknleft = blocksize;
		sstrend = g_stacknxt + blocksize;
		g_stackp = sp;
		INT_ON;
	}
	p = g_stacknxt;
	g_stacknxt += aligned;
	g_stacknleft -= aligned;
	return p;
}

static void *
stzalloc(size_t nbytes)
{
	return memset(stalloc(nbytes), 0, nbytes);
}

static void
stunalloc(void *p)
{
#if DEBUG
	if (!p || (g_stacknxt < (char *)p) || ((char *)p < g_stackp->space)) {
		write(STDERR_FILENO, "stunalloc\n", 10);
		abort();
	}
#endif
	g_stacknleft += g_stacknxt - (char *)p;
	g_stacknxt = p;
}

/*
 * Like strdup but works with the ash stack.
 */
static char *
sstrdup(const char *p)
{
	size_t len = strlen(p) + 1;
	return memcpy(stalloc(len), p, len);
}

static ALWAYS_INLINE void
grabstackblock(size_t len)
{
	stalloc(len);
}

static void
pushstackmark(struct stackmark *mark, size_t len)
{
	mark->stackp = g_stackp;
	mark->stacknxt = g_stacknxt;
	mark->stacknleft = g_stacknleft;
	grabstackblock(len);
}

static void
setstackmark(struct stackmark *mark)
{
	pushstackmark(mark, g_stacknxt == g_stackp->space && g_stackp != &stackbase);
}

static void
popstackmark(struct stackmark *mark)
{
	struct stack_block *sp;

	if (!mark->stackp)
		return;

	INT_OFF;
	while (g_stackp != mark->stackp) {
		sp = g_stackp;
		g_stackp = sp->prev;
		free(sp);
	}
	g_stacknxt = mark->stacknxt;
	g_stacknleft = mark->stacknleft;
	sstrend = mark->stacknxt + mark->stacknleft;
	INT_ON;
}

/*
 * When the parser reads in a string, it wants to stick the string on the
 * stack and only adjust the stack pointer when it knows how big the
 * string is.  Stackblock (defined in stack.h) returns a pointer to a block
 * of space on top of the stack and stackblocklen returns the length of
 * this block.  Growstackblock will grow this space by at least one byte,
 * possibly moving it (like realloc).  Grabstackblock actually allocates the
 * part of the block that has been used.
 */
static void
growstackblock(size_t min)
{
	size_t newlen;

	newlen = g_stacknleft * 2;
	if (newlen < g_stacknleft)
		ash_msg_and_raise_error(bb_msg_memory_exhausted);
	min = SHELL_ALIGN(min | 128);
	if (newlen < min)
		newlen += min;

	if (g_stacknxt == g_stackp->space && g_stackp != &stackbase) {
		struct stack_block *sp;
		struct stack_block *prevstackp;
		size_t grosslen;

		INT_OFF;
		sp = g_stackp;
		prevstackp = sp->prev;
		grosslen = newlen + sizeof(struct stack_block) - MINSIZE;
		sp = ckrealloc(sp, grosslen);
		sp->prev = prevstackp;
		g_stackp = sp;
		g_stacknxt = sp->space;
		g_stacknleft = newlen;
		sstrend = sp->space + newlen;
		INT_ON;
	} else {
		char *oldspace = g_stacknxt;
		size_t oldlen = g_stacknleft;
		char *p = stalloc(newlen);

		/* free the space we just allocated */
		g_stacknxt = memcpy(p, oldspace, oldlen);
		g_stacknleft += newlen;
	}
}

/*
 * The following routines are somewhat easier to use than the above.
 * The user declares a variable of type STACKSTR, which may be declared
 * to be a register.  The macro STARTSTACKSTR initializes things.  Then
 * the user uses the macro STPUTC to add characters to the string.  In
 * effect, STPUTC(c, p) is the same as *p++ = c except that the stack is
 * grown as necessary.  When the user is done, she can just leave the
 * string there and refer to it using stackblock().  Or she can allocate
 * the space for it using grabstackstr().  If it is necessary to allow
 * someone else to use the stack temporarily and then continue to grow
 * the string, the user should use grabstack to allocate the space, and
 * then call ungrabstr(p) to return to the previous mode of operation.
 *
 * USTPUTC is like STPUTC except that it doesn't check for overflow.
 * CHECKSTACKSPACE can be called before USTPUTC to ensure that there
 * is space for at least one character.
 */
static void *
growstackstr(void)
{
	size_t len = stackblocksize();
	growstackblock(0);
	return (char *)stackblock() + len;
}

static char *
growstackto(size_t len)
{
	if (stackblocksize() < len)
		growstackblock(len);
	return stackblock();
}

/*
 * Called from CHECKSTRSPACE.
 */
static char *
makestrspace(size_t newlen, char *p)
{
	size_t len = p - g_stacknxt;

	return growstackto(len + newlen) + len;
}

static char *
stnputs(const char *s, size_t n, char *p)
{
	p = makestrspace(n, p);
	p = (char *)mempcpy(p, s, n);
	return p;
}

static char *
stack_putstr(const char *s, char *p)
{
	return stnputs(s, strlen(s), p);
}

static char *
_STPUTC(int c, char *p)
{
	if (p == sstrend)
		p = growstackstr();
	*p++ = c;
	return p;
}

#define STARTSTACKSTR(p)        ((p) = stackblock())
#define STPUTC(c, p)            ((p) = _STPUTC((c), (p)))
#define CHECKSTRSPACE(n, p) do { \
	char *q = (p); \
	size_t l = (n); \
	size_t m = sstrend - q; \
	if (l > m) \
		(p) = makestrspace(l, q); \
} while (0)
#define USTPUTC(c, p)           (*(p)++ = (c))
#define STACKSTRNUL(p) do { \
	if ((p) == sstrend) \
		(p) = growstackstr(); \
	*(p) = '\0'; \
} while (0)
#define STUNPUTC(p)             (--(p))
#define STTOPC(p)               ((p)[-1])
#define STADJUST(amount, p)     ((p) += (amount))

#define grabstackstr(p)         stalloc((char *)(p) - (char *)stackblock())
#define ungrabstackstr(s, p)    stunalloc(s)
#define stackstrend()           ((void *)sstrend)


/* ============ String helpers */

/*
 * prefix -- see if pfx is a prefix of string.
 */
static char *
prefix(const char *string, const char *pfx)
{
	while (*pfx) {
		if (*pfx++ != *string++)
			return NULL;
	}
	return (char *) string;
}

/*
 * Check for a valid number.  This should be elsewhere.
 */
static int
is_number(const char *p)
{
	do {
		if (!isdigit(*p))
			return 0;
	} while (*++p != '\0');
	return 1;
}

/*
 * Convert a string of digits to an integer, printing an error message on
 * failure.
 */
static int
number(const char *s)
{
	if (!is_number(s))
		ash_msg_and_raise_error(msg_illnum, s);
	return atoi(s);
}

/*
 * Produce a single quoted string suitable as input to the shell.
 * The return string is allocated on the stack.
 */
static char *
single_quote(const char *s)
{
	char *p;

	STARTSTACKSTR(p);

	do {
		char *q;
		size_t len;

		len = strchrnul(s, '\'') - s;

		q = p = makestrspace(len + 3, p);

		*q++ = '\'';
		q = (char *)mempcpy(q, s, len);
		*q++ = '\'';
		s += len;

		STADJUST(q - p, p);

		if (*s != '\'')
			break;
		len = 0;
		do len++; while (*++s == '\'');

		q = p = makestrspace(len + 3, p);

		*q++ = '"';
		q = (char *)mempcpy(q, s - len, len);
		*q++ = '"';

		STADJUST(q - p, p);
	} while (*s);

	USTPUTC('\0', p);

	return stackblock();
}

/*
 * Produce a possibly single quoted string suitable as input to the shell.
 * If quoting was done, the return string is allocated on the stack,
 * otherwise a pointer to the original string is returned.
 */
static const char *
maybe_single_quote(const char *s)
{
	const char *p = s;

	while (*p) {
		/* Assuming ACSII */
		/* quote ctrl_chars space !"#$%&'()* */
		if (*p < '+')
			goto need_quoting;
		/* quote ;<=>? */
		if (*p >= ';' && *p <= '?')
			goto need_quoting;
		/* quote `[\ */
		if (*p == '`')
			goto need_quoting;
		if (*p == '[')
			goto need_quoting;
		if (*p == '\\')
			goto need_quoting;
		/* quote {|}~ DEL and high bytes */
		if (*p > 'z')
			goto need_quoting;
		/* Not quoting these: +,-./ 0-9 :@ A-Z ]^_ a-z */
		/* TODO: maybe avoid quoting % */
		p++;
	}
	return s;

 need_quoting:
	return single_quote(s);
}

#if ENABLE_PLATFORM_MINGW32
/*
 * Place 'path' in a string on the stack, adding the system drive prefix
 * if necessary and leaving room for an optional extension.
 */
static char *
stack_add_system_drive(const char *path)
{
	const char *sd = need_system_drive(path);
	char *p = growstackto(strlen(path) + 5 + (sd ? strlen(sd) : 0));

	strcpy(stpcpy(p, sd ?: ""), path);
	return p;
}
#endif


/* ============ nextopt */

static char **argptr;                  /* argument list for builtin commands */
static char *optionarg;                /* set by nextopt (like getopt) */
static char *optptr;                   /* used by nextopt */

/*
 * XXX - should get rid of. Have all builtins use getopt(3).
 * The library getopt must have the BSD extension static variable
 * "optreset", otherwise it can't be used within the shell safely.
 *
 * Standard option processing (a la getopt) for builtin routines.
 * The only argument that is passed to nextopt is the option string;
 * the other arguments are unnecessary. It returns the character,
 * or '\0' on end of input.
 */
static int
nextopt(const char *optstring)
{
	char *p;
	const char *q;
	char c;

	p = optptr;
	if (p == NULL || *p == '\0') {
		/* We ate entire "-param", take next one */
		p = *argptr;
		if (p == NULL)
			return '\0';
		if (*p != '-')
			return '\0';
		if (*++p == '\0') /* just "-" ? */
			return '\0';
		argptr++;
		if (LONE_DASH(p)) /* "--" ? */
			return '\0';
		/* p => next "-param" */
	}
	/* p => some option char in the middle of a "-param" */
	c = *p++;
	for (q = optstring; *q != c;) {
		if (*q == '\0')
			ash_msg_and_raise_error("illegal option -%c", c);
		if (*++q == ':')
			q++;
	}
	if (*++q == ':') {
		if (*p == '\0') {
			p = *argptr++;
			if (p == NULL)
				ash_msg_and_raise_error("no arg for -%c option", c);
		}
		optionarg = p;
		p = NULL;
	}
	optptr = p;
	return c;
}


/* ============ Shell variables */

struct shparam {
	int nparam;             /* # of positional parameters (without $0) */
#if ENABLE_ASH_GETOPTS
	int optind;             /* next parameter to be processed by getopts */
	int optoff;             /* used by getopts */
#endif
	unsigned char malloced; /* if parameter list dynamically allocated */
	char **p;               /* parameter list */
};

/*
 * Free the list of positional parameters.
 */
static void
freeparam(volatile struct shparam *param)
{
	if (param->malloced) {
		char **ap, **ap1;
		ap = ap1 = param->p;
		while (*ap)
			free(*ap++);
		free(ap1);
	}
}

#if ENABLE_ASH_GETOPTS
static void FAST_FUNC getoptsreset(const char *value);
#endif

struct var {
	struct var *next;               /* next entry in hash list */
	int flags;                      /* flags are defined above */
	const char *var_text;           /* name=value */
	void (*var_func)(const char *) FAST_FUNC; /* function to be called when  */
					/* the variable gets set/unset */
};

struct localvar {
	struct localvar *next;          /* next local variable in list */
	struct var *vp;                 /* the variable that was made local */
	int flags;                      /* saved flags */
	const char *text;               /* saved text */
};

/* flags */
#define VEXPORT         0x01    /* variable is exported */
#define VREADONLY       0x02    /* variable cannot be modified */
#define VSTRFIXED       0x04    /* variable struct is statically allocated */
#define VTEXTFIXED      0x08    /* text is statically allocated */
#define VSTACK          0x10    /* text is allocated on the stack */
#define VUNSET          0x20    /* the variable is not set */
#define VNOFUNC         0x40    /* don't call the callback function */
#define VNOSET          0x80    /* do not set variable - just readonly test */
#define VNOSAVE         0x100   /* when text is on the heap before setvareq */
#if ENABLE_ASH_RANDOM_SUPPORT
# define VDYNAMIC       0x200   /* dynamic variable */
#else
# define VDYNAMIC       0
#endif


/* Need to be before varinit_data[] */
#if ENABLE_LOCALE_SUPPORT
static void FAST_FUNC
change_lc_all(const char *value)
{
	if (value && *value != '\0')
		setlocale(LC_ALL, value);
}
static void FAST_FUNC
change_lc_ctype(const char *value)
{
	if (value && *value != '\0')
		setlocale(LC_CTYPE, value);
}
#endif
#if ENABLE_ASH_MAIL
static void chkmail(void);
static void changemail(const char *var_value) FAST_FUNC;
#else
# define chkmail()  ((void)0)
#endif
static void changepath(const char *) FAST_FUNC;
#if ENABLE_ASH_RANDOM_SUPPORT
static void change_random(const char *) FAST_FUNC;
#endif
#if BASH_EPOCH_VARS
static void change_seconds(const char *) FAST_FUNC;
static void change_realtime(const char *) FAST_FUNC;
#endif

#if ENABLE_PLATFORM_MINGW32
static void FAST_FUNC
change_skip_ansi(const char *newval UNUSED_PARAM)
{
	skip_ansi_emulation(TRUE);
}

# define LINENO_INDEX (5 + 2 * ENABLE_ASH_MAIL + ENABLE_ASH_GETOPTS)
# define FUNCNAME_INDEX (LINENO_INDEX + 1)
#endif

static const struct {
	int flags;
	const char *var_text;
	void (*var_func)(const char *) FAST_FUNC;
} varinit_data[] ALIGN_PTR = {
	/*
	 * Note: VEXPORT would not work correctly here for NOFORK applets:
	 * some environment strings may be constant.
	 */
	{ VSTRFIXED|VTEXTFIXED       , defifsvar   , NULL            },
#if ENABLE_ASH_MAIL
	{ VSTRFIXED|VTEXTFIXED|VUNSET, "MAIL"      , changemail      },
	{ VSTRFIXED|VTEXTFIXED|VUNSET, "MAILPATH"  , changemail      },
#endif
	{ VSTRFIXED|VTEXTFIXED       , bb_PATH_root_path, changepath },
	{ VSTRFIXED|VTEXTFIXED       , "PS1=$ "    , NULL            },
	{ VSTRFIXED|VTEXTFIXED       , "PS2=> "    , NULL            },
	{ VSTRFIXED|VTEXTFIXED       , "PS4=+ "    , NULL            },
#if ENABLE_ASH_GETOPTS
	{ VSTRFIXED|VTEXTFIXED       , defoptindvar, getoptsreset    },
#endif
	{ VSTRFIXED|VTEXTFIXED       , NULL /* inited to linenovar */, NULL },
	{ VSTRFIXED|VTEXTFIXED       , NULL /* inited to funcnamevar */, NULL },
#if ENABLE_ASH_RANDOM_SUPPORT
	{ VSTRFIXED|VTEXTFIXED|VUNSET|VDYNAMIC, "RANDOM", change_random },
#endif
#if BASH_EPOCH_VARS
	{ VSTRFIXED|VTEXTFIXED|VUNSET|VDYNAMIC, "EPOCHSECONDS", change_seconds },
	{ VSTRFIXED|VTEXTFIXED|VUNSET|VDYNAMIC, "EPOCHREALTIME", change_realtime },
#endif
#if ENABLE_LOCALE_SUPPORT
	{ VSTRFIXED|VTEXTFIXED|VUNSET, "LC_ALL"    , change_lc_all   },
	{ VSTRFIXED|VTEXTFIXED|VUNSET, "LC_CTYPE"  , change_lc_ctype },
#endif
#if ENABLE_FEATURE_EDITING_SAVEHISTORY
	{ VSTRFIXED|VTEXTFIXED|VUNSET, "HISTFILE"  , NULL            },
#endif
#if ENABLE_PLATFORM_MINGW32
	{ VSTRFIXED|VTEXTFIXED|VUNSET, bb_skip_ansi_emulation, change_skip_ansi },
#endif
};

struct redirtab;

struct globals_var {
	struct shparam shellparam;      /* $@ current positional parameters */
	struct redirtab *redirlist;
	int preverrout_fd;   /* stderr fd: usually 2, unless redirect moved it */
	struct var *vartab[VTABSIZE];
	struct var varinit[ARRAY_SIZE(varinit_data)];
	int lineno;
	char linenovar[sizeof("LINENO=") + sizeof(int)*3];
	char funcnamevar[sizeof("FUNCNAME=") + 64];
	char *funcname;
	unsigned trap_depth;
	bool in_trap_ERR; /* ERR cannot recurse, no need to be a counter */
};
extern struct globals_var *BB_GLOBAL_CONST ash_ptr_to_globals_var;
#define G_var (*ash_ptr_to_globals_var)
#define shellparam    (G_var.shellparam   )
//#define redirlist     (G_var.redirlist    )
#define preverrout_fd (G_var.preverrout_fd)
#define vartab        (G_var.vartab       )
#define varinit       (G_var.varinit      )
#define lineno        (G_var.lineno       )
#define linenovar     (G_var.linenovar    )
#define funcnamevar   (G_var.funcnamevar  )
#define funcname      (G_var.funcname     )
#define trap_depth    (G_var.trap_depth   )
#define in_trap_ERR   (G_var.in_trap_ERR  )
#define vifs      varinit[0]
#if ENABLE_ASH_MAIL
# define vmail    varinit[1]
# define vmpath   varinit[2]
#endif
#define VAR_OFFSET1 (ENABLE_ASH_MAIL*2)
#define vpath     varinit[VAR_OFFSET1 + 1]
#define vps1      varinit[VAR_OFFSET1 + 2]
#define vps2      varinit[VAR_OFFSET1 + 3]
#define vps4      varinit[VAR_OFFSET1 + 4]
#if ENABLE_ASH_GETOPTS
# define voptind  varinit[VAR_OFFSET1 + 5]
#endif
#define VAR_OFFSET2 (VAR_OFFSET1 + ENABLE_ASH_GETOPTS)
#define vlineno   varinit[VAR_OFFSET2 + 5]
#define vfuncname varinit[VAR_OFFSET2 + 6]
#if ENABLE_ASH_RANDOM_SUPPORT
# define vrandom  varinit[VAR_OFFSET2 + 7]
#endif
#define VAR_OFFSET3 (VAR_OFFSET2 + ENABLE_ASH_RANDOM_SUPPORT)
#if BASH_EPOCH_VARS
# define vepochs  varinit[VAR_OFFSET3 + 7]
# define vepochr  varinit[VAR_OFFSET3 + 8]
#endif
#define INIT_G_var() do { \
	unsigned i; \
	XZALLOC_CONST_PTR(&ash_ptr_to_globals_var, sizeof(G_var)); \
	for (i = 0; i < ARRAY_SIZE(varinit_data); i++) { \
		varinit[i].flags    = varinit_data[i].flags; \
		varinit[i].var_text = varinit_data[i].var_text; \
		varinit[i].var_func = varinit_data[i].var_func; \
	} \
	strcpy(linenovar, "LINENO="); \
	vlineno.var_text = linenovar; \
	strcpy(funcnamevar, "FUNCNAME="); \
	vfuncname.var_text = funcnamevar; \
} while (0)

/*
 * The following macros access the values of the above variables.
 * They have to skip over the name.  They return the null string
 * for unset variables.
 */
#define ifsval()        (vifs.var_text + 4)
#define ifsset()        ((vifs.flags & VUNSET) == 0)
#if ENABLE_ASH_MAIL
# define mailval()      (vmail.var_text + 5)
# define mpathval()     (vmpath.var_text + 9)
# define mpathset()     ((vmpath.flags & VUNSET) == 0)
#endif
#define pathval()       (vpath.var_text + 5)
#define ps1val()        (vps1.var_text + 4)
#define ps2val()        (vps2.var_text + 4)
#define ps4val()        (vps4.var_text + 4)
#if ENABLE_ASH_GETOPTS
# define optindval()    (voptind.var_text + 7)
#endif

#if ENABLE_ASH_GETOPTS
static void FAST_FUNC
getoptsreset(const char *value)
{
	shellparam.optind = 1;
	if (is_number(value))
		shellparam.optind = number(value) ?: 1;
	shellparam.optoff = -1;
}
#endif

/*
 * Compares two strings up to the first = or '\0'.  The first
 * string must be terminated by '='; the second may be terminated by
 * either '=' or '\0'.
 */
static int
varcmp(const char *p, const char *q)
{
	int c, d;

	while ((c = *p) == (d = *q)) {
		if (c == '\0' || c == '=')
			goto out;
		p++;
		q++;
	}
	if (c == '=')
		c = '\0';
	if (d == '=')
		d = '\0';
 out:
	return c - d;
}

/*
 * Find the appropriate entry in the hash table from the name.
 */
static struct var **
hashvar(const char *p)
{
	unsigned hashval;

	hashval = ((unsigned char) *p) << 4;
	while (*p && *p != '=')
		hashval += (unsigned char) *p++;
	return &vartab[hashval % VTABSIZE];
}

static int
vpcmp(const void *a, const void *b)
{
	return varcmp(*(const char **)a, *(const char **)b);
}

/*
 * This routine initializes the builtin variables.
 */
static void
initvar(void)
{
	struct var *vp;
	struct var *end;
	struct var **vpp;

	/*
	 * PS1 depends on uid
	 */
#if ENABLE_FEATURE_EDITING && ENABLE_FEATURE_EDITING_FANCY_PROMPT
	vps1.var_text = "PS1=\\w \\$ ";
#else
	if (!geteuid())
		vps1.var_text = "PS1=# ";
#endif
	vp = varinit;
	end = vp + ARRAY_SIZE(varinit);
	do {
		vpp = hashvar(vp->var_text);
		vp->next = *vpp;
		*vpp = vp;
	} while (++vp < end);
}

static struct var **
findvar(struct var **vpp, const char *name)
{
	for (; *vpp; vpp = &(*vpp)->next) {
		if (varcmp((*vpp)->var_text, name) == 0) {
			break;
		}
	}
	return vpp;
}

/*
 * Find the value of a variable.  Returns NULL if not set.
 */
static const char* FAST_FUNC
lookupvar(const char *name)
{
	struct var *v;

	v = *findvar(hashvar(name), name);
	if (v) {
#if ENABLE_ASH_RANDOM_SUPPORT || BASH_EPOCH_VARS
	/*
	 * Dynamic variables are implemented roughly the same way they are
	 * in bash. Namely, they're "special" so long as they aren't unset.
	 * As soon as they're unset, they're no longer dynamic, and dynamic
	 * lookup will no longer happen at that point. -- PFM.
	 */
		if (v->flags & VDYNAMIC)
			v->var_func(NULL);
#endif
		if (!(v->flags & VUNSET)) {
			if (v->var_text == linenovar) {
				fmtstr(linenovar+7, sizeof(linenovar)-7, "%d", lineno);
			} else
			if (v->var_text == funcnamevar) {
				safe_strncpy(funcnamevar+9, funcname ? funcname : "", sizeof(funcnamevar)-9);
			}
			return var_end(v->var_text);
		}
	}
	return NULL;
}

#if ENABLE_UNICODE_SUPPORT
static void
reinit_unicode_for_ash(void)
{
	/* Unicode support should be activated even if LANG is set
	 * _during_ shell execution, not only if it was set when
	 * shell was started. Therefore, re-check LANG every time:
	 */
	if (ENABLE_FEATURE_CHECK_UNICODE_IN_ENV
	 || ENABLE_UNICODE_USING_LOCALE
	) {
		const char *s = lookupvar("LC_ALL");
		if (!s) s = lookupvar("LC_CTYPE");
		if (!s) s = lookupvar("LANG");
		reinit_unicode(s);
	}
}
#else
# define reinit_unicode_for_ash() ((void)0)
#endif

/*
 * Search the environment of a builtin command.
 */
static ALWAYS_INLINE const char *
bltinlookup(const char *name)
{
	return lookupvar(name);
}

#if ENABLE_PLATFORM_MINGW32
static char *
fix_pathvar(const char *path, int len)
{
	char *newpath = xstrdup(path);
	char *p;
	int modified = FALSE;

	p = newpath + len;
	while (*p) {
		if (*p != ':' && *p != ';') {
			/* skip drive */
			if (isalpha(*p) && p[1] == ':')
				p += 2;
			/* skip through path component */
			for (; *p != '\0' && *p != ':' && *p != ';'; ++p)
				continue;
		}
		/* *p is ':', ';' or '\0' here */
		if (*p == ':') {
			*p++ = ';';
			modified = TRUE;
		}
		else if (*p == ';') {
			++p;
		}
	}

	if (!modified) {
		free(newpath);
		newpath = NULL;
	}
	return newpath;
}
#endif

/*
 * Same as setvar except that the variable and value are passed in
 * the first argument as name=value.  Since the first argument will
 * be actually stored in the table, it should not be a string that
 * will go away.
 * Called with interrupts off.
 */
static struct var *
setvareq(char *s, int flags)
{
	struct var *vp, **vpp;

#if ENABLE_PLATFORM_MINGW32
	const char *paths = "PATH=\0""CDPATH=\0""MANPATH=\0";
	const char *p;
	int len;

	for (p = paths; *p; p += len + 1) {
		len = strlen(p);
		if (strncmp(s, p, len) == 0) {
			char *newpath = fix_pathvar(s, len);
			if (newpath) {
				if ((flags & (VTEXTFIXED|VSTACK|VNOSAVE)) == VNOSAVE)
					free(s);
				flags |= VNOSAVE;
				flags &= ~(VTEXTFIXED|VSTACK);
				s = newpath;
			}
			break;
		}
	}
#endif

	vpp = hashvar(s);
	flags |= (VEXPORT & (((unsigned) (1 - aflag)) - 1));
	vpp = findvar(vpp, s);
	vp = *vpp;
	if (vp) {
		if ((vp->flags & (VREADONLY|VDYNAMIC)) == VREADONLY) {
			const char *n;

			if (flags & VNOSAVE)
				free(s);
			n = vp->var_text;
			exitstatus = 1;
			ash_msg_and_raise_error("%.*s: is read only", strchrnul(n, '=') - n, n);
		}

		if (flags & VNOSET)
			goto out;

		if (vp->var_func && !(flags & VNOFUNC))
			vp->var_func(var_end(s));

		if (!(vp->flags & (VTEXTFIXED|VSTACK)))
			free((char*)vp->var_text);

		if (((flags & (VEXPORT|VREADONLY|VSTRFIXED|VUNSET)) | (vp->flags & VSTRFIXED)) == VUNSET) {
			*vpp = vp->next;
			free(vp);
 out_free:
			if ((flags & (VTEXTFIXED|VSTACK|VNOSAVE)) == VNOSAVE)
				free(s);
			goto out;
		}

		flags |= vp->flags & ~(VTEXTFIXED|VSTACK|VNOSAVE|VUNSET);
#if ENABLE_ASH_RANDOM_SUPPORT || BASH_EPOCH_VARS
		if (flags & VUNSET)
			flags &= ~VDYNAMIC;
#endif
	} else {
		/* variable s is not found */
		if (flags & VNOSET)
			goto out;
		if ((flags & (VEXPORT|VREADONLY|VSTRFIXED|VUNSET)) == VUNSET)
			goto out_free;
		vp = ckzalloc(sizeof(*vp));
		vp->next = *vpp;
		/*vp->func = NULL; - ckzalloc did it */
		*vpp = vp;
	}
	if (!(flags & (VTEXTFIXED|VSTACK|VNOSAVE)))
		s = ckstrdup(s);
	vp->var_text = s;
	vp->flags = flags;

 out:
	return vp;
}

/*
 * Set the value of a variable.  The flags argument is ored with the
 * flags of the variable.  If val is NULL, the variable is unset.
 */
static struct var *
setvar(const char *name, const char *val, int flags)
{
	const char *q;
	char *p;
	char *nameeq;
	size_t namelen;
	size_t vallen;
	struct var *vp;

	q = endofname(name);
	p = strchrnul(q, '=');
	namelen = p - name;
	if (!namelen || p != q)
		ash_msg_and_raise_error("%.*s: bad variable name", namelen, name);
	vallen = 0;
	if (val == NULL) {
		flags |= VUNSET;
	} else {
		vallen = strlen(val);
	}

	INT_OFF;
	nameeq = ckzalloc(namelen + vallen + 2);
	p = mempcpy(nameeq, name, namelen);
	if (val) {
		*p++ = '=';
		memcpy(p, val, vallen);
	}
	vp = setvareq(nameeq, flags | VNOSAVE);
	INT_ON;

	return vp;
}

static void FAST_FUNC
setvar0(const char *name, const char *val)
{
	setvar(name, val, 0);
}

/*
 * Unset the specified variable.
 */
static void
unsetvar(const char *s)
{
	setvar(s, NULL, 0);
}

/*
 * Generate a list of variables satisfying the given conditions.
 */
#if !ENABLE_FEATURE_SH_NOFORK
# define listvars(on, off, lp, end) listvars(on, off, end)
#endif
static char **
listvars(int on, int off, struct strlist *lp, char ***end)
{
	struct var **vpp;
	struct var *vp;
	char **ep;
	int mask;

	STARTSTACKSTR(ep);
	vpp = vartab;
	mask = on | off;
	do {
		for (vp = *vpp; vp; vp = vp->next) {
			if ((vp->flags & mask) == on) {
#if ENABLE_FEATURE_SH_NOFORK
				/* If variable with the same name is both
				 * exported and temporarily set for a command:
				 *  export ZVAR=5
				 *  ZVAR=6 printenv
				 * then "ZVAR=6" will be both in vartab and
				 * lp lists. Do not pass it twice to printenv.
				 */
				struct strlist *lp1 = lp;
				while (lp1) {
					if (strcmp(lp1->text, vp->var_text) == 0)
						goto skip;
					lp1 = lp1->next;
				}
#endif
				if (ep == stackstrend())
					ep = growstackstr();
				*ep++ = (char*)vp->var_text;
#if ENABLE_FEATURE_SH_NOFORK
 skip: ;
#endif
			}
		}
	} while (++vpp < vartab + VTABSIZE);

#if ENABLE_FEATURE_SH_NOFORK
	while (lp) {
		if (ep == stackstrend())
			ep = growstackstr();
		*ep++ = lp->text;
		lp = lp->next;
	}
#endif

	if (ep == stackstrend())
		ep = growstackstr();
	if (end)
		*end = ep;
	*ep++ = NULL;
	return grabstackstr(ep);
}


/* ============ Path search helper */
static const char *
legal_pathopt(const char *opt, const char *term, int magic)
{
	switch (magic) {
	case 0:
		opt = NULL;
		break;

	case 1:
		opt = prefix(opt, "builtin") ?: prefix(opt, "func");
		break;

	default:
		opt += strcspn(opt, term);
		break;
	}

	if (opt && *opt == '%')
		opt++;

	return opt;
}

/*
 * The variable path (passed by reference) should be set to the start
 * of the path before the first call; padvance will update
 * this value as it proceeds.  Successive calls to padvance will return
 * the possible path expansions in sequence.  If an option (indicated by
 * a percent sign) appears in the path entry then the global variable
 * pathopt will be set to point to it; otherwise pathopt will be set to
 * NULL.
 *
 * If magic is 0 then pathopt recognition will be disabled.  If magic is
 * 1 we shall recognise %builtin/%func.  Otherwise we shall accept any
 * pathopt.
 */
static const char *pathopt;     /* set by padvance */

static int
padvance_magic(const char **path, const char *name, int magic)
{
	const char *term = "%"PATH_SEP_STR;
	const char *lpathopt;
	const char *p;
	char *q;
	const char *start;
	size_t qlen;
	size_t len;
#if ENABLE_PLATFORM_MINGW32
	size_t sdlen = 0;
	const char *sd;
#endif

	if (*path == NULL)
		return -1;

	lpathopt = NULL;
	start = *path;

	if (*start == '%' && (p = legal_pathopt(start + 1, term, magic))) {
		lpathopt = start + 1;
		start = p;
		term = PATH_SEP_STR;
	}

	len = strcspn(start, term);
	p = start + len;

	if (*p == '%') {
		size_t extra = strchrnul(p, PATH_SEP) - p;

		if (legal_pathopt(p + 1, term, magic))
			lpathopt = p + 1;
		else
			len += extra;

		p += extra;
	}

	pathopt = lpathopt;
	*path = *p == PATH_SEP ? p + 1 : NULL;

	/* "2" is for '/' and '\0' */
	qlen = len + strlen(name) + 2;
#if ENABLE_PLATFORM_MINGW32
	/* reserve space for system drive prefix and extension */
	sd = need_system_drive(start);
	if (sd != NULL)
		sdlen = strlen(sd);
	qlen += 4 + sdlen;
#endif
	q = growstackto(qlen);

	if (len) {
#if ENABLE_PLATFORM_MINGW32
		q = mempcpy(q, sd, sdlen);
#endif
		q = mempcpy(q, start, len);
#if ENABLE_PLATFORM_MINGW32
		if (q[-1] != '/' && q[-1] != '\\')
#endif
		*q++ = '/';
	}
	strcpy(q, name);

	return qlen;
}

static int
padvance(const char **path, const char *name)
{
	return padvance_magic(path, name, 1);
}


/* ============ Prompt */

static smallint doprompt;                   /* if set, prompt the user */
static smallint needprompt;                 /* true if interactive and at start of line */

#if ENABLE_FEATURE_EDITING
static line_input_t *line_input_state;
static const char *cmdedit_prompt;
static void
putprompt(const char *s)
{
	if (ENABLE_ASH_EXPAND_PRMT) {
		free((char*)cmdedit_prompt);
		cmdedit_prompt = ckstrdup(s);
		return;
	}
	cmdedit_prompt = s;
}
#else
static void
putprompt(const char *s)
{
	out2str(s);
}
#endif

/* expandstr() needs parsing machinery, so it is far away ahead... */
static const char *expandstr(const char *ps, int syntax_type);
/* Values for syntax param */
#define BASESYNTAX 0    /* not in quotes */
#define DQSYNTAX   1    /* in double quotes */
#define SQSYNTAX   2    /* in single quotes */
#define ARISYNTAX  3    /* in arithmetic */
#if ENABLE_ASH_EXPAND_PRMT
# define PSSYNTAX  4    /* prompt. never passed to SIT() */
#endif
/* PSSYNTAX expansion is identical to DQSYNTAX, except keeping '\$' as '\$' */

/*
 * called by editline -- any expansions to the prompt should be added here.
 */
static void
setprompt_if(smallint do_set, int whichprompt)
{
	const char *prompt;
	IF_ASH_EXPAND_PRMT(struct stackmark smark;)

	if (!do_set)
		return;

	needprompt = 0;

	switch (whichprompt) {
	case 1:
		prompt = ps1val();
		break;
	case 2:
		prompt = ps2val();
		break;
	default:                        /* 0 */
		prompt = nullstr;
	}
#if ENABLE_PLATFORM_MINGW32
	skip_ansi_emulation(TRUE);
#endif
#if ENABLE_ASH_EXPAND_PRMT
	pushstackmark(&smark, stackblocksize());
	putprompt(expandstr(prompt, PSSYNTAX));
	popstackmark(&smark);
#else
	putprompt(prompt);
#endif
}


/* ============ The cd and pwd commands */

#define CD_PHYSICAL 1
#define CD_PRINT 2
#define CD_PRINT_ALL 4

static int
cdopt(void)
{
	int flags = 0;
	int i, j;

	j = 'L';
#if ENABLE_PLATFORM_MINGW32
	while ((i = nextopt("LPa")) != '\0') {
		if (i == 'a')
			flags |= CD_PRINT_ALL;
		else
#else
	while ((i = nextopt("LP")) != '\0') {
#endif
		if (i != j) {
			flags ^= CD_PHYSICAL;
			j = i;
		}
	}

	return flags;
}

/*
 * Update curdir (the name of the current directory) in response to a
 * cd command.
 */
static const char *
updatepwd(const char *dir)
{
#if ENABLE_PLATFORM_MINGW32
	/*
	 * Due to Windows drive notion, getting pwd is a completely
	 * different thing. Handle it in a separate routine
	 */

	char *new;
	char *p;
	char *cdcomppath;
	const char *lim;
	int len;
	char buffer[PATH_MAX];
	/*
	 * There are five cases that make some kind of sense
	 *
	 * Absolute paths:
	 *    c:/path
	 *    //host/share
	 *
	 * Relative to current working directory of other drive:
	 *    c:path
	 *
	 * Relative to current root (drive/share):
	 *    /path
	 *
	 * Relative to current working directory of current root (drive/share):
	 *    path
	 */
	enum {ABS_DRIVE, ABS_SHARE, REL_OTHER, REL_ROOT, REL_CWD} target;

	/* skip multiple leading separators unless dir is a UNC path */
	if (is_dir_sep(*dir) && unc_root_len(dir) == 0) {
		while (is_dir_sep(dir[1]))
			++dir;
	}

	len = strlen(dir);
	if (len >= 2 && has_dos_drive_prefix(dir))
		target = len >= 3 && is_dir_sep(dir[2]) ? ABS_DRIVE : REL_OTHER;
	else if (unc_root_len(dir) != 0)
		target = ABS_SHARE;
	else if (is_dir_sep(*dir))
		target = REL_ROOT;
	else
		target = REL_CWD;

	cdcomppath = sstrdup(dir);
	STARTSTACKSTR(new);

	switch (target) {
	case REL_OTHER:
		/* c:path */
		if (get_drive_cwd(dir, buffer, PATH_MAX) == NULL)
			return 0;
		new = stack_putstr(buffer, new);
		len = 2;
		cdcomppath += len;
		dir += len;
		break;
	case REL_CWD:
	case REL_ROOT:
		/* path or /path */
		len = root_len(curdir);
		if (len == 0)
			return 0;
		new = target == REL_CWD ? stack_putstr(curdir, new) :
									stnputs(curdir, len, new);
		break;
	default:
		/* //host/share or c:/path */
		len = root_len(dir);
		if (len == 0)
			return 0;
		new = stnputs(dir, len, new);
		cdcomppath += len;
		dir += len;
		break;
	}

	new = makestrspace(strlen(dir) + 2, new);
	lim = (char *)stackblock() + len + 1;

	if (!is_dir_sep(*dir)) {
		if (!is_dir_sep(new[-1]))
			USTPUTC('/', new);
		if (new > lim && is_dir_sep(*lim))
			lim++;
	} else {
		USTPUTC('/', new);
		cdcomppath++;
		if (is_dir_sep(dir[1]) && !is_dir_sep(dir[2])) {
			USTPUTC('/', new);
			cdcomppath++;
			lim++;
		}
	}
	p = strtok(cdcomppath, "/\\");
	while (p) {
		switch (*p) {
		case '.':
			if (p[1] == '.' && p[2] == '\0') {
				while (new > lim) {
					STUNPUTC(new);
					if (is_dir_sep(new[-1]))
						break;
				}
				break;
			}
			if (p[1] == '\0')
				break;
			/* fall through */
		default:
			new = stack_putstr(p, new);
			USTPUTC('/', new);
		}
		p = strtok(NULL, "/\\");
	}
	if (new > lim)
		STUNPUTC(new);
	*new = 0;
	fix_path_case((char *)stackblock());
	return bs_to_slash((char *)stackblock());
#else
	char *new;
	char *p;
	char *cdcomppath;
	const char *lim;

	cdcomppath = sstrdup(dir);
	STARTSTACKSTR(new);
	if (*dir != '/') {
		if (curdir == nullstr)
			return 0;
		new = stack_putstr(curdir, new);
	}
	new = makestrspace(strlen(dir) + 2, new);
	lim = (char *)stackblock() + 1;
	if (*dir != '/') {
		if (new[-1] != '/')
			USTPUTC('/', new);
		if (new > lim && *lim == '/')
			lim++;
	} else {
		USTPUTC('/', new);
		cdcomppath++;
		if (dir[1] == '/' && dir[2] != '/') {
			USTPUTC('/', new);
			cdcomppath++;
			lim++;
		}
	}
	p = strtok_r(cdcomppath, "/", &cdcomppath);
	while (p) {
		switch (*p) {
		case '.':
			if (p[1] == '.' && p[2] == '\0') {
				while (new > lim) {
					STUNPUTC(new);
					if (new[-1] == '/')
						break;
				}
				break;
			}
			if (p[1] == '\0')
				break;
			/* fall through */
		default:
			new = stack_putstr(p, new);
			USTPUTC('/', new);
		}
		p = strtok_r(NULL, "/", &cdcomppath);
	}
	if (new > lim)
		STUNPUTC(new);
	*new = 0;
	return stackblock();
#endif
}

/*
 * Find out what the current directory is. If we already know the current
 * directory, this routine returns immediately.
 */
static char *
getpwd(void)
{
	char *dir = getcwd(NULL, 0); /* huh, using glibc extension? */
	return dir ? dir : nullstr;
}

static void
setpwd(const char *val, int setold)
{
	char *oldcur, *dir;

	oldcur = dir = curdir;

	if (setold) {
		setvar("OLDPWD", oldcur, VEXPORT);
	}
	INT_OFF;
	if (physdir != nullstr) {
		if (physdir != oldcur)
			free(physdir);
		physdir = nullstr;
	}
	if (oldcur == val || !val) {
		char *s = getpwd();
		physdir = s;
		if (!val)
			dir = s;
	} else
		dir = ckstrdup(val);
	if (oldcur != dir && oldcur != nullstr) {
		free(oldcur);
	}
	curdir = dir;
	INT_ON;
	setvar("PWD", dir, VEXPORT);
}

static void hashcd(void);

/*
 * Actually do the chdir.  We also call hashcd to let other routines
 * know that the current directory has changed.
 */
static int
docd(const char *dest, int flags)
{
	const char *dir = NULL;
	int err;

	TRACE(("docd(\"%s\", %d) called\n", dest, flags));

	INT_OFF;
	if (!(flags & CD_PHYSICAL)) {
		dir = updatepwd(dest);
		if (dir)
			dest = dir;
	}
	err = chdir(dest);
	if (err)
		goto out;
	setpwd(dir, 1);
	hashcd();
 out:
	INT_ON;
	return err;
}

static int FAST_FUNC
cdcmd(int argc UNUSED_PARAM, char **argv UNUSED_PARAM)
{
	const char *dest;
	const char *path;
	const char *p;
	char c;
	struct stat statb;
	int flags;
	int len;

	flags = cdopt();
	dest = *argptr;
	if (!dest)
		dest = bltinlookup("HOME");
	else if (LONE_DASH(dest)) {
		dest = bltinlookup("OLDPWD");
		flags |= CD_PRINT;
	}
	if (!dest)
		dest = nullstr;
	if (!is_relative_path(dest))
		goto step6;
	if (*dest == '.') {
		c = dest[1];
 dotdot:
		switch (c) {
		case '\0':
		case '/':
			goto step6;
		case '.':
			c = dest[2];
			if (c != '.')
				goto dotdot;
		}
	}
	if (!*dest)
		dest = ".";
	path = bltinlookup("CDPATH");
	while (p = path, (len = padvance(&path, dest)) >= 0) {
		c = *p;
		p = stalloc(len);

		if (stat(p, &statb) >= 0 && S_ISDIR(statb.st_mode)) {
			if (c && c != PATH_SEP)
				flags |= CD_PRINT;
 docd:
			if (!docd(p, flags))
				goto out;
			goto err;
		}
	}

 step6:
	p = dest;
	goto docd;

 err:
	ash_msg_and_raise_perror("can't cd to %s", dest);
	/* NOTREACHED */
 out:
	if (flags & CD_PRINT)
		out1fmt("%s\n", curdir);
	return 0;
}

#if ENABLE_PLATFORM_MINGW32
static void
print_all_cwd(void)
{
	FILE *mnt;
	struct mntent *entry;
	char buffer[PATH_MAX];

	mnt = setmntent(bb_path_mtab_file, "r");
	if (mnt) {
		while ((entry=getmntent(mnt)) != NULL) {
			entry->mnt_dir[2] = '\0';
			if (get_drive_cwd(entry->mnt_dir, buffer, PATH_MAX) != NULL)
				out1fmt("%s\n", buffer);
		}
		endmntent(mnt);
	}
}
#endif

static int FAST_FUNC
pwdcmd(int argc UNUSED_PARAM, char **argv UNUSED_PARAM)
{
	int flags;
	const char *dir = curdir;

	flags = cdopt();
#if ENABLE_PLATFORM_MINGW32
	if (flags & CD_PRINT_ALL) {
		print_all_cwd();
		return 0;
	}
#endif
	if (flags) {
		if (physdir == nullstr)
			setpwd(dir, 0);
		dir = physdir;
	}
	out1fmt("%s\n", dir);
	return 0;
}


/* ============ ... */


#define IBUFSIZ (ENABLE_FEATURE_EDITING ? CONFIG_FEATURE_EDITING_MAX_LEN : 1024)

/* Syntax classes */
#define CWORD     0             /* character is nothing special */
#define CNL       1             /* newline character */
#define CBACK     2             /* a backslash character */
#define CSQUOTE   3             /* single quote */
#define CDQUOTE   4             /* double quote */
#define CENDQUOTE 5             /* a terminating quote */
#define CBQUOTE   6             /* backwards single quote */
#define CVAR      7             /* a dollar sign */
#define CENDVAR   8             /* a '}' character */
#define CLP       9             /* a left paren in arithmetic */
#define CRP      10             /* a right paren in arithmetic */
#define CENDFILE 11             /* end of file */
#define CCTL     12             /* like CWORD, except it must be escaped */
#define CSPCL    13             /* these terminate a word */

#define PEOF     256

#define USE_SIT_FUNCTION ENABLE_ASH_OPTIMIZE_FOR_SIZE

#if ENABLE_FEATURE_SH_MATH
# define SIT_ITEM(a,b,c,d) (a | (b << 4) | (c << 8) | (d << 12))
#else
# define SIT_ITEM(a,b,c,d) (a | (b << 4) | (c << 8))
#endif
static const uint16_t S_I_T[] ALIGN2 = {
	SIT_ITEM(CSPCL   , CWORD    , CWORD, CWORD  ),    /* 0, ' ' */
	SIT_ITEM(CNL     , CNL      , CNL  , CNL    ),    /* 1, \n */
	SIT_ITEM(CWORD   , CCTL     , CCTL , CWORD  ),    /* 2, !*-/:=?[]~ */
	SIT_ITEM(CDQUOTE , CENDQUOTE, CWORD, CWORD  ),    /* 3, '"' */
	SIT_ITEM(CVAR    , CVAR     , CWORD, CVAR   ),    /* 4, $ */
	SIT_ITEM(CSQUOTE , CWORD    , CENDQUOTE, CWORD),  /* 5, "'" */
	SIT_ITEM(CSPCL   , CWORD    , CWORD, CLP    ),    /* 6, ( */
	SIT_ITEM(CSPCL   , CWORD    , CWORD, CRP    ),    /* 7, ) */
	SIT_ITEM(CBACK   , CBACK    , CCTL , CBACK  ),    /* 8, \ */
	SIT_ITEM(CBQUOTE , CBQUOTE  , CWORD, CBQUOTE),    /* 9, ` */
	SIT_ITEM(CENDVAR , CENDVAR  , CWORD, CENDVAR),    /* 10, } */
#if !USE_SIT_FUNCTION
	SIT_ITEM(CENDFILE, CENDFILE , CENDFILE, CENDFILE),/* 11, PEOF */
	SIT_ITEM(CWORD   , CWORD    , CWORD, CWORD  ),    /* 12, 0-9A-Za-z */
	SIT_ITEM(CCTL    , CCTL     , CCTL , CCTL   )     /* 13, CTLESC ... */
#endif
#undef SIT_ITEM
};
/* Constants below must match table above */
enum {
	CSPCL_CWORD_CWORD_CWORD            , /*  0 */
	CNL_CNL_CNL_CNL                    , /*  1 */
	CWORD_CCTL_CCTL_CWORD              , /*  2 */
	CDQUOTE_CENDQUOTE_CWORD_CWORD      , /*  3 */
	CVAR_CVAR_CWORD_CVAR               , /*  4 */
	CSQUOTE_CWORD_CENDQUOTE_CWORD      , /*  5 */
	CSPCL_CWORD_CWORD_CLP              , /*  6 */
	CSPCL_CWORD_CWORD_CRP              , /*  7 */
	CBACK_CBACK_CCTL_CBACK             , /*  8 */
	CBQUOTE_CBQUOTE_CWORD_CBQUOTE      , /*  9 */
	CENDVAR_CENDVAR_CWORD_CENDVAR      , /* 10 */
	CENDFILE_CENDFILE_CENDFILE_CENDFILE, /* 11 */
	CWORD_CWORD_CWORD_CWORD            , /* 12 */
	CCTL_CCTL_CCTL_CCTL                , /* 13 */
};

/* c in SIT(c, syntax) must be an *unsigned char* or PEOF,
 * caller must ensure proper cast on it if c is *char_ptr!
 */
#if USE_SIT_FUNCTION

static int
SIT(int c, int syntax)
{
	/* Used to also have '/' in this string: "\t\n !\"$&'()*-/:;<=>?[\\]`|}~" */
	static const char spec_symbls[] ALIGN1 = "\t\n !\"$&'()*-:;<=>?[\\]`|}~";
	/*
	 * This causes '/' to be prepended with CTLESC in dquoted string,
	 * making "./file"* treated incorrectly because we feed
	 * ".\/file*" string to glob(), confusing it (see expandmeta func).
	 * The "homegrown" glob implementation is okay with that,
	 * but glibc one isn't. With '/' always treated as CWORD,
	 * both work fine.
	 */
	static const uint8_t syntax_index_table[] ALIGN1 = {
		0, 1, 0, 2, 3, 4, 0, 5,         /* "\t\n !\"$&'" */
		6, 7, 2, 2,/*2,*/2, 0, 0,       /* "()*-/:;<" */
		2, 0, 2, 2, 8, 2, 9, 0,         /* "=>?[\\]`|" */
		10, 2                           /* "}~" */
	};
	const char *s;
	int indx;

	if (c == PEOF)
		return CENDFILE;
	/* Cast is purely for paranoia here,
	 * just in case someone passed signed char to us */
	if ((unsigned char)c >= CTL_FIRST
	 && (unsigned char)c <= CTL_LAST
	) {
		return CCTL;
	}
	s = strchrnul(spec_symbls, c);
	if (*s == '\0')
		return CWORD;
	indx = syntax_index_table[s - spec_symbls];
	return (S_I_T[indx] >> (syntax*4)) & 0xf;
}

#else   /* !USE_SIT_FUNCTION */

static const uint8_t syntax_index_table[] ALIGN1 = {
	/* BASESYNTAX_DQSYNTAX_SQSYNTAX_ARISYNTAX */
	/*   0      */ CWORD_CWORD_CWORD_CWORD,
	/*   1      */ CWORD_CWORD_CWORD_CWORD,
	/*   2      */ CWORD_CWORD_CWORD_CWORD,
	/*   3      */ CWORD_CWORD_CWORD_CWORD,
	/*   4      */ CWORD_CWORD_CWORD_CWORD,
	/*   5      */ CWORD_CWORD_CWORD_CWORD,
	/*   6      */ CWORD_CWORD_CWORD_CWORD,
	/*   7      */ CWORD_CWORD_CWORD_CWORD,
	/*   8      */ CWORD_CWORD_CWORD_CWORD,
	/*   9 "\t" */ CSPCL_CWORD_CWORD_CWORD,
	/*  10 "\n" */ CNL_CNL_CNL_CNL,
	/*  11      */ CWORD_CWORD_CWORD_CWORD,
	/*  12      */ CWORD_CWORD_CWORD_CWORD,
	/*  13      */ CWORD_CWORD_CWORD_CWORD,
	/*  14      */ CWORD_CWORD_CWORD_CWORD,
	/*  15      */ CWORD_CWORD_CWORD_CWORD,
	/*  16      */ CWORD_CWORD_CWORD_CWORD,
	/*  17      */ CWORD_CWORD_CWORD_CWORD,
	/*  18      */ CWORD_CWORD_CWORD_CWORD,
	/*  19      */ CWORD_CWORD_CWORD_CWORD,
	/*  20      */ CWORD_CWORD_CWORD_CWORD,
	/*  21      */ CWORD_CWORD_CWORD_CWORD,
	/*  22      */ CWORD_CWORD_CWORD_CWORD,
	/*  23      */ CWORD_CWORD_CWORD_CWORD,
	/*  24      */ CWORD_CWORD_CWORD_CWORD,
	/*  25      */ CWORD_CWORD_CWORD_CWORD,
	/*  26      */ CWORD_CWORD_CWORD_CWORD,
	/*  27      */ CWORD_CWORD_CWORD_CWORD,
	/*  28      */ CWORD_CWORD_CWORD_CWORD,
	/*  29      */ CWORD_CWORD_CWORD_CWORD,
	/*  30      */ CWORD_CWORD_CWORD_CWORD,
	/*  31      */ CWORD_CWORD_CWORD_CWORD,
	/*  32  " " */ CSPCL_CWORD_CWORD_CWORD,
	/*  33  "!" */ CWORD_CCTL_CCTL_CWORD,
	/*  34  """ */ CDQUOTE_CENDQUOTE_CWORD_CWORD,
	/*  35  "#" */ CWORD_CWORD_CWORD_CWORD,
	/*  36  "$" */ CVAR_CVAR_CWORD_CVAR,
	/*  37  "%" */ CWORD_CWORD_CWORD_CWORD,
	/*  38  "&" */ CSPCL_CWORD_CWORD_CWORD,
	/*  39  "'" */ CSQUOTE_CWORD_CENDQUOTE_CWORD,
	/*  40  "(" */ CSPCL_CWORD_CWORD_CLP,
	/*  41  ")" */ CSPCL_CWORD_CWORD_CRP,
	/*  42  "*" */ CWORD_CCTL_CCTL_CWORD,
	/*  43  "+" */ CWORD_CWORD_CWORD_CWORD,
	/*  44  "," */ CWORD_CWORD_CWORD_CWORD,
	/*  45  "-" */ CWORD_CCTL_CCTL_CWORD,
	/*  46  "." */ CWORD_CWORD_CWORD_CWORD,
/* "/" was CWORD_CCTL_CCTL_CWORD, see comment in SIT() function why this is changed: */
	/*  47  "/" */ CWORD_CWORD_CWORD_CWORD,
	/*  48  "0" */ CWORD_CWORD_CWORD_CWORD,
	/*  49  "1" */ CWORD_CWORD_CWORD_CWORD,
	/*  50  "2" */ CWORD_CWORD_CWORD_CWORD,
	/*  51  "3" */ CWORD_CWORD_CWORD_CWORD,
	/*  52  "4" */ CWORD_CWORD_CWORD_CWORD,
	/*  53  "5" */ CWORD_CWORD_CWORD_CWORD,
	/*  54  "6" */ CWORD_CWORD_CWORD_CWORD,
	/*  55  "7" */ CWORD_CWORD_CWORD_CWORD,
	/*  56  "8" */ CWORD_CWORD_CWORD_CWORD,
	/*  57  "9" */ CWORD_CWORD_CWORD_CWORD,
	/*  58  ":" */ CWORD_CCTL_CCTL_CWORD,
	/*  59  ";" */ CSPCL_CWORD_CWORD_CWORD,
	/*  60  "<" */ CSPCL_CWORD_CWORD_CWORD,
	/*  61  "=" */ CWORD_CCTL_CCTL_CWORD,
	/*  62  ">" */ CSPCL_CWORD_CWORD_CWORD,
	/*  63  "?" */ CWORD_CCTL_CCTL_CWORD,
	/*  64  "@" */ CWORD_CWORD_CWORD_CWORD,
	/*  65  "A" */ CWORD_CWORD_CWORD_CWORD,
	/*  66  "B" */ CWORD_CWORD_CWORD_CWORD,
	/*  67  "C" */ CWORD_CWORD_CWORD_CWORD,
	/*  68  "D" */ CWORD_CWORD_CWORD_CWORD,
	/*  69  "E" */ CWORD_CWORD_CWORD_CWORD,
	/*  70  "F" */ CWORD_CWORD_CWORD_CWORD,
	/*  71  "G" */ CWORD_CWORD_CWORD_CWORD,
	/*  72  "H" */ CWORD_CWORD_CWORD_CWORD,
	/*  73  "I" */ CWORD_CWORD_CWORD_CWORD,
	/*  74  "J" */ CWORD_CWORD_CWORD_CWORD,
	/*  75  "K" */ CWORD_CWORD_CWORD_CWORD,
	/*  76  "L" */ CWORD_CWORD_CWORD_CWORD,
	/*  77  "M" */ CWORD_CWORD_CWORD_CWORD,
	/*  78  "N" */ CWORD_CWORD_CWORD_CWORD,
	/*  79  "O" */ CWORD_CWORD_CWORD_CWORD,
	/*  80  "P" */ CWORD_CWORD_CWORD_CWORD,
	/*  81  "Q" */ CWORD_CWORD_CWORD_CWORD,
	/*  82  "R" */ CWORD_CWORD_CWORD_CWORD,
	/*  83  "S" */ CWORD_CWORD_CWORD_CWORD,
	/*  84  "T" */ CWORD_CWORD_CWORD_CWORD,
	/*  85  "U" */ CWORD_CWORD_CWORD_CWORD,
	/*  86  "V" */ CWORD_CWORD_CWORD_CWORD,
	/*  87  "W" */ CWORD_CWORD_CWORD_CWORD,
	/*  88  "X" */ CWORD_CWORD_CWORD_CWORD,
	/*  89  "Y" */ CWORD_CWORD_CWORD_CWORD,
	/*  90  "Z" */ CWORD_CWORD_CWORD_CWORD,
	/*  91  "[" */ CWORD_CCTL_CCTL_CWORD,
	/*  92  "\" */ CBACK_CBACK_CCTL_CBACK,
	/*  93  "]" */ CWORD_CCTL_CCTL_CWORD,
	/*  94  "^" */ CWORD_CWORD_CWORD_CWORD,
	/*  95  "_" */ CWORD_CWORD_CWORD_CWORD,
	/*  96  "`" */ CBQUOTE_CBQUOTE_CWORD_CBQUOTE,
	/*  97  "a" */ CWORD_CWORD_CWORD_CWORD,
	/*  98  "b" */ CWORD_CWORD_CWORD_CWORD,
	/*  99  "c" */ CWORD_CWORD_CWORD_CWORD,
	/* 100  "d" */ CWORD_CWORD_CWORD_CWORD,
	/* 101  "e" */ CWORD_CWORD_CWORD_CWORD,
	/* 102  "f" */ CWORD_CWORD_CWORD_CWORD,
	/* 103  "g" */ CWORD_CWORD_CWORD_CWORD,
	/* 104  "h" */ CWORD_CWORD_CWORD_CWORD,
	/* 105  "i" */ CWORD_CWORD_CWORD_CWORD,
	/* 106  "j" */ CWORD_CWORD_CWORD_CWORD,
	/* 107  "k" */ CWORD_CWORD_CWORD_CWORD,
	/* 108  "l" */ CWORD_CWORD_CWORD_CWORD,
	/* 109  "m" */ CWORD_CWORD_CWORD_CWORD,
	/* 110  "n" */ CWORD_CWORD_CWORD_CWORD,
	/* 111  "o" */ CWORD_CWORD_CWORD_CWORD,
	/* 112  "p" */ CWORD_CWORD_CWORD_CWORD,
	/* 113  "q" */ CWORD_CWORD_CWORD_CWORD,
	/* 114  "r" */ CWORD_CWORD_CWORD_CWORD,
	/* 115  "s" */ CWORD_CWORD_CWORD_CWORD,
	/* 116  "t" */ CWORD_CWORD_CWORD_CWORD,
	/* 117  "u" */ CWORD_CWORD_CWORD_CWORD,
	/* 118  "v" */ CWORD_CWORD_CWORD_CWORD,
	/* 119  "w" */ CWORD_CWORD_CWORD_CWORD,
	/* 120  "x" */ CWORD_CWORD_CWORD_CWORD,
	/* 121  "y" */ CWORD_CWORD_CWORD_CWORD,
	/* 122  "z" */ CWORD_CWORD_CWORD_CWORD,
	/* 123  "{" */ CWORD_CWORD_CWORD_CWORD,
	/* 124  "|" */ CSPCL_CWORD_CWORD_CWORD,
	/* 125  "}" */ CENDVAR_CENDVAR_CWORD_CENDVAR,
	/* 126  "~" */ CWORD_CCTL_CCTL_CWORD,
	/* 127  del */ CWORD_CWORD_CWORD_CWORD,
	/* 128 0x80 */ CWORD_CWORD_CWORD_CWORD,
	/* 129 CTLESC       */ CCTL_CCTL_CCTL_CCTL,
	/* 130 CTLVAR       */ CCTL_CCTL_CCTL_CCTL,
	/* 131 CTLENDVAR    */ CCTL_CCTL_CCTL_CCTL,
	/* 132 CTLBACKQ     */ CCTL_CCTL_CCTL_CCTL,
	/* 133 CTLQUOTE     */ CCTL_CCTL_CCTL_CCTL,
	/* 134 CTLARI       */ CCTL_CCTL_CCTL_CCTL,
	/* 135 CTLENDARI    */ CCTL_CCTL_CCTL_CCTL,
	/* 136 CTLQUOTEMARK */ CCTL_CCTL_CCTL_CCTL,
#if BASH_PROCESS_SUBST
	/* 137 CTLTOPROC    */ CCTL_CCTL_CCTL_CCTL,
	/* 138 CTLFROMPROC  */ CCTL_CCTL_CCTL_CCTL,
#else
	/* 137      */ CWORD_CWORD_CWORD_CWORD,
	/* 138      */ CWORD_CWORD_CWORD_CWORD,
#endif
	/* 139      */ CWORD_CWORD_CWORD_CWORD,
	/* 140      */ CWORD_CWORD_CWORD_CWORD,
	/* 141      */ CWORD_CWORD_CWORD_CWORD,
	/* 142      */ CWORD_CWORD_CWORD_CWORD,
	/* 143      */ CWORD_CWORD_CWORD_CWORD,
	/* 144      */ CWORD_CWORD_CWORD_CWORD,
	/* 145      */ CWORD_CWORD_CWORD_CWORD,
	/* 146      */ CWORD_CWORD_CWORD_CWORD,
	/* 147      */ CWORD_CWORD_CWORD_CWORD,
	/* 148      */ CWORD_CWORD_CWORD_CWORD,
	/* 149      */ CWORD_CWORD_CWORD_CWORD,
	/* 150      */ CWORD_CWORD_CWORD_CWORD,
	/* 151      */ CWORD_CWORD_CWORD_CWORD,
	/* 152      */ CWORD_CWORD_CWORD_CWORD,
	/* 153      */ CWORD_CWORD_CWORD_CWORD,
	/* 154      */ CWORD_CWORD_CWORD_CWORD,
	/* 155      */ CWORD_CWORD_CWORD_CWORD,
	/* 156      */ CWORD_CWORD_CWORD_CWORD,
	/* 157      */ CWORD_CWORD_CWORD_CWORD,
	/* 158      */ CWORD_CWORD_CWORD_CWORD,
	/* 159      */ CWORD_CWORD_CWORD_CWORD,
	/* 160      */ CWORD_CWORD_CWORD_CWORD,
	/* 161      */ CWORD_CWORD_CWORD_CWORD,
	/* 162      */ CWORD_CWORD_CWORD_CWORD,
	/* 163      */ CWORD_CWORD_CWORD_CWORD,
	/* 164      */ CWORD_CWORD_CWORD_CWORD,
	/* 165      */ CWORD_CWORD_CWORD_CWORD,
	/* 166      */ CWORD_CWORD_CWORD_CWORD,
	/* 167      */ CWORD_CWORD_CWORD_CWORD,
	/* 168      */ CWORD_CWORD_CWORD_CWORD,
	/* 169      */ CWORD_CWORD_CWORD_CWORD,
	/* 170      */ CWORD_CWORD_CWORD_CWORD,
	/* 171      */ CWORD_CWORD_CWORD_CWORD,
	/* 172      */ CWORD_CWORD_CWORD_CWORD,
	/* 173      */ CWORD_CWORD_CWORD_CWORD,
	/* 174      */ CWORD_CWORD_CWORD_CWORD,
	/* 175      */ CWORD_CWORD_CWORD_CWORD,
	/* 176      */ CWORD_CWORD_CWORD_CWORD,
	/* 177      */ CWORD_CWORD_CWORD_CWORD,
	/* 178      */ CWORD_CWORD_CWORD_CWORD,
	/* 179      */ CWORD_CWORD_CWORD_CWORD,
	/* 180      */ CWORD_CWORD_CWORD_CWORD,
	/* 181      */ CWORD_CWORD_CWORD_CWORD,
	/* 182      */ CWORD_CWORD_CWORD_CWORD,
	/* 183      */ CWORD_CWORD_CWORD_CWORD,
	/* 184      */ CWORD_CWORD_CWORD_CWORD,
	/* 185      */ CWORD_CWORD_CWORD_CWORD,
	/* 186      */ CWORD_CWORD_CWORD_CWORD,
	/* 187      */ CWORD_CWORD_CWORD_CWORD,
	/* 188      */ CWORD_CWORD_CWORD_CWORD,
	/* 189      */ CWORD_CWORD_CWORD_CWORD,
	/* 190      */ CWORD_CWORD_CWORD_CWORD,
	/* 191      */ CWORD_CWORD_CWORD_CWORD,
	/* 192      */ CWORD_CWORD_CWORD_CWORD,
	/* 193      */ CWORD_CWORD_CWORD_CWORD,
	/* 194      */ CWORD_CWORD_CWORD_CWORD,
	/* 195      */ CWORD_CWORD_CWORD_CWORD,
	/* 196      */ CWORD_CWORD_CWORD_CWORD,
	/* 197      */ CWORD_CWORD_CWORD_CWORD,
	/* 198      */ CWORD_CWORD_CWORD_CWORD,
	/* 199      */ CWORD_CWORD_CWORD_CWORD,
	/* 200      */ CWORD_CWORD_CWORD_CWORD,
	/* 201      */ CWORD_CWORD_CWORD_CWORD,
	/* 202      */ CWORD_CWORD_CWORD_CWORD,
	/* 203      */ CWORD_CWORD_CWORD_CWORD,
	/* 204      */ CWORD_CWORD_CWORD_CWORD,
	/* 205      */ CWORD_CWORD_CWORD_CWORD,
	/* 206      */ CWORD_CWORD_CWORD_CWORD,
	/* 207      */ CWORD_CWORD_CWORD_CWORD,
	/* 208      */ CWORD_CWORD_CWORD_CWORD,
	/* 209      */ CWORD_CWORD_CWORD_CWORD,
	/* 210      */ CWORD_CWORD_CWORD_CWORD,
	/* 211      */ CWORD_CWORD_CWORD_CWORD,
	/* 212      */ CWORD_CWORD_CWORD_CWORD,
	/* 213      */ CWORD_CWORD_CWORD_CWORD,
	/* 214      */ CWORD_CWORD_CWORD_CWORD,
	/* 215      */ CWORD_CWORD_CWORD_CWORD,
	/* 216      */ CWORD_CWORD_CWORD_CWORD,
	/* 217      */ CWORD_CWORD_CWORD_CWORD,
	/* 218      */ CWORD_CWORD_CWORD_CWORD,
	/* 219      */ CWORD_CWORD_CWORD_CWORD,
	/* 220      */ CWORD_CWORD_CWORD_CWORD,
	/* 221      */ CWORD_CWORD_CWORD_CWORD,
	/* 222      */ CWORD_CWORD_CWORD_CWORD,
	/* 223      */ CWORD_CWORD_CWORD_CWORD,
	/* 224      */ CWORD_CWORD_CWORD_CWORD,
	/* 225      */ CWORD_CWORD_CWORD_CWORD,
	/* 226      */ CWORD_CWORD_CWORD_CWORD,
	/* 227      */ CWORD_CWORD_CWORD_CWORD,
	/* 228      */ CWORD_CWORD_CWORD_CWORD,
	/* 229      */ CWORD_CWORD_CWORD_CWORD,
	/* 230      */ CWORD_CWORD_CWORD_CWORD,
	/* 231      */ CWORD_CWORD_CWORD_CWORD,
	/* 232      */ CWORD_CWORD_CWORD_CWORD,
	/* 233      */ CWORD_CWORD_CWORD_CWORD,
	/* 234      */ CWORD_CWORD_CWORD_CWORD,
	/* 235      */ CWORD_CWORD_CWORD_CWORD,
	/* 236      */ CWORD_CWORD_CWORD_CWORD,
	/* 237      */ CWORD_CWORD_CWORD_CWORD,
	/* 238      */ CWORD_CWORD_CWORD_CWORD,
	/* 239      */ CWORD_CWORD_CWORD_CWORD,
	/* 230      */ CWORD_CWORD_CWORD_CWORD,
	/* 241      */ CWORD_CWORD_CWORD_CWORD,
	/* 242      */ CWORD_CWORD_CWORD_CWORD,
	/* 243      */ CWORD_CWORD_CWORD_CWORD,
	/* 244      */ CWORD_CWORD_CWORD_CWORD,
	/* 245      */ CWORD_CWORD_CWORD_CWORD,
	/* 246      */ CWORD_CWORD_CWORD_CWORD,
	/* 247      */ CWORD_CWORD_CWORD_CWORD,
	/* 248      */ CWORD_CWORD_CWORD_CWORD,
	/* 249      */ CWORD_CWORD_CWORD_CWORD,
	/* 250      */ CWORD_CWORD_CWORD_CWORD,
	/* 251      */ CWORD_CWORD_CWORD_CWORD,
	/* 252      */ CWORD_CWORD_CWORD_CWORD,
	/* 253      */ CWORD_CWORD_CWORD_CWORD,
	/* 254      */ CWORD_CWORD_CWORD_CWORD,
	/* 255      */ CWORD_CWORD_CWORD_CWORD,
	/* PEOF */     CENDFILE_CENDFILE_CENDFILE_CENDFILE,
};

#if 1
# define SIT(c, syntax) ((S_I_T[syntax_index_table[c]] >> ((syntax)*4)) & 0xf)
#else /* debug version, caught one signed char bug */
# define SIT(c, syntax) \
	({ \
		if ((c) < 0 || (c) > (PEOF + ENABLE_ASH_ALIAS)) \
			bb_error_msg_and_die("line:%d c:%d", __LINE__, (c)); \
		if ((syntax) < 0 || (syntax) > (2 + ENABLE_FEATURE_SH_MATH)) \
			bb_error_msg_and_die("line:%d c:%d", __LINE__, (c)); \
		((S_I_T[syntax_index_table[c]] >> ((syntax)*4)) & 0xf); \
	})
#endif

#endif  /* !USE_SIT_FUNCTION */


/* ============ Alias handling */

#if ENABLE_ASH_ALIAS

#define ALIASINUSE 1
#define ALIASDEAD  2

struct alias {
	struct alias *next;
	char *name;
	char *val;
	int flag;
};


static struct alias **atab; // [ATABSIZE];
#define INIT_G_alias() do { \
	atab = xzalloc(ATABSIZE * sizeof(atab[0])); \
} while (0)


static struct alias **
__lookupalias(const char *name)
{
	unsigned int hashval;
	struct alias **app;
	const char *p;
	unsigned int ch;

	p = name;

	ch = (unsigned char)*p;
	hashval = ch << 4;
	while (ch) {
		hashval += ch;
		ch = (unsigned char)*++p;
	}
	app = &atab[hashval % ATABSIZE];

	for (; *app; app = &(*app)->next) {
		if (strcmp(name, (*app)->name) == 0) {
			break;
		}
	}

	return app;
}

static struct alias *
lookupalias(const char *name, int check)
{
	struct alias *ap = *__lookupalias(name);

	if (check && ap && (ap->flag & ALIASINUSE))
		return NULL;
	return ap;
}

static struct alias *
freealias(struct alias *ap)
{
	struct alias *next;

	if (ap->flag & ALIASINUSE) {
		ap->flag |= ALIASDEAD;
		return ap;
	}

	next = ap->next;
	free(ap->name);
	free(ap->val);
	free(ap);
	return next;
}

static void
setalias(const char *name, const char *val)
{
	struct alias *ap, **app;

	app = __lookupalias(name);
	ap = *app;
	INT_OFF;
	if (ap) {
		if (!(ap->flag & ALIASINUSE)) {
			free(ap->val);
		}
		ap->val = ckstrdup(val);
		ap->flag &= ~ALIASDEAD;
	} else {
		/* not found */
		ap = ckzalloc(sizeof(struct alias));
		ap->name = ckstrdup(name);
		ap->val = ckstrdup(val);
		/*ap->flag = 0; - ckzalloc did it */
		/*ap->next = NULL;*/
		*app = ap;
	}
	INT_ON;
}

static int
unalias(const char *name)
{
	struct alias **app;

	app = __lookupalias(name);

	if (*app) {
		INT_OFF;
		*app = freealias(*app);
		INT_ON;
		return 0;
	}

	return 1;
}

static void
rmaliases(void)
{
	struct alias *ap, **app;
	int i;

	INT_OFF;
	for (i = 0; i < ATABSIZE; i++) {
		app = &atab[i];
		for (ap = *app; ap; ap = *app) {
			*app = freealias(*app);
			if (ap == *app) {
				app = &ap->next;
			}
		}
	}
	INT_ON;
}

static void
printalias(const struct alias *ap)
{
	out1fmt("%s=%s\n", ap->name, single_quote(ap->val));
}

/*
 * TODO - sort output
 */
static int FAST_FUNC
aliascmd(int argc UNUSED_PARAM, char **argv)
{
	char *n, *v;
	int ret = 0;
	struct alias *ap;

	if (!argv[1]) {
		int i;

		for (i = 0; i < ATABSIZE; i++) {
			for (ap = atab[i]; ap; ap = ap->next) {
				printalias(ap);
			}
		}
		return 0;
	}
	while ((n = *++argv) != NULL) {
		v = strchr(n+1, '=');
		if (v == NULL) { /* n+1: funny ksh stuff */
			ap = *__lookupalias(n);
			if (ap == NULL) {
				fprintf(stderr, "%s: %s not found\n", "alias", n);
				ret = 1;
			} else
				printalias(ap);
		} else {
			*v++ = '\0';
			setalias(n, v);
		}
	}

	return ret;
}

static int FAST_FUNC
unaliascmd(int argc UNUSED_PARAM, char **argv UNUSED_PARAM)
{
	int i;

	while (nextopt("a") != '\0') {
		rmaliases();
		return 0;
	}
	for (i = 0; *argptr; argptr++) {
		if (unalias(*argptr)) {
			fprintf(stderr, "%s: %s not found\n", "unalias", *argptr);
			i = 1;
		}
	}

	return i;
}

#endif /* ASH_ALIAS */


/* Mode argument to forkshell.  Don't change FORK_FG or FORK_BG. */
#define FORK_FG    0
#define FORK_BG    1
#define FORK_NOJOB 2

/* mode flags for showjob(s) */
#define SHOW_ONLY_PGID  0x01    /* show only pgid (jobs -p) */
#define SHOW_PIDS       0x02    /* show individual pids, not just one line per job */
#define SHOW_CHANGED    0x04    /* only jobs whose state has changed */
#define SHOW_STDERR     0x08    /* print to stderr (else stdout) */

/*
 * A job structure contains information about a job.  A job is either a
 * single process or a set of processes contained in a pipeline.  In the
 * latter case, pidlist will be non-NULL, and will point to a -1 terminated
 * array of pids.
 */
struct procstat {
	pid_t   ps_pid;         /* process id */
	int     ps_status;      /* last process status from wait() */
#if ENABLE_PLATFORM_POSIX || JOBS_WIN32
	char    *ps_cmd;        /* text of command being run */
#endif
#if ENABLE_PLATFORM_MINGW32
	HANDLE  ps_proc;
#endif
};

struct job {
	struct procstat ps0;    /* status of process */
	struct procstat *ps;    /* status of processes when more than one */
#if JOBS
	int stopstatus;         /* status of a stopped job */
#endif
	unsigned nprocs;        /* number of processes */

#define JOBRUNNING      0       /* at least one proc running */
#define JOBSTOPPED      1       /* all procs are stopped */
#define JOBDONE         2       /* all procs are completed */
	unsigned
		state: 8,
#if JOBS
		sigint: 1,      /* job was killed by SIGINT */
		jobctl: 1,      /* job running under job control */
#endif
		waited: 1,      /* true if this entry has been waited for */
		used: 1,        /* true if this entry is in used */
		changed: 1;     /* true if status has changed */
	struct job *prev_job;   /* previous job */
};

static struct job *makejob(/*union node *,*/ int);
#if !ENABLE_PLATFORM_MINGW32
static int forkshell(struct job *, union node *, int);
#endif
static int waitforjob(struct job *);

#if !JOBS && !JOBS_WIN32
enum { doing_jobctl = 0 };
#define setjobctl(on) do {} while (0)
#elif JOBS_WIN32
static smallint doing_jobctl; //references:8
#define setjobctl(on) do { if (rootshell) doing_jobctl = on; } while (0)
#else /* JOBS */
static smallint doing_jobctl; //references:8
static void setjobctl(int);
#endif

#if !ENABLE_PLATFORM_MINGW32
/*
 * Ignore a signal.
 */
static void
ignoresig(int signo)
{
	/* Avoid unnecessary system calls. Is it already SIG_IGNed? */
	if (sigmode[signo - 1] != S_IGN && sigmode[signo - 1] != S_HARD_IGN) {
		/* No, need to do it */
		signal(signo, SIG_IGN);
	}
	sigmode[signo - 1] = S_HARD_IGN;
}

/*
 * Only one usage site - in setsignal()
 */
static void
signal_handler(int signo)
{
	if (signo == SIGCHLD) {
		got_sigchld = 1;
		if (!trap[SIGCHLD])
			return;
	}

	gotsig[signo - 1] = 1;
	pending_sig = signo;

	if (signo == SIGINT && !trap[SIGINT]) {
		if (!suppress_int) {
			pending_sig = 0;
			raise_interrupt(); /* does not return */
		}
		pending_int = 1;
	}
}

/*
 * Set the signal handler for the specified signal.  The routine figures
 * out what it should be set to.
 */
static void
setsignal(int signo)
{
	char *t;
	char cur_act, new_act;
	struct sigaction act;

	t = trap[signo];
	new_act = S_DFL;
	if (t != NULL) { /* trap for this sig is set */
		new_act = S_CATCH;
		if (t[0] == '\0') /* trap is "": ignore this sig */
			new_act = S_IGN;
	}

	if (rootshell && new_act == S_DFL) {
		switch (signo) {
		case SIGINT:
			if (iflag || minusc || sflag == 0)
				new_act = S_CATCH;
			break;
		case SIGQUIT:
#if DEBUG
			if (debug)
				break;
#endif
			/* man bash:
			 * "In all cases, bash ignores SIGQUIT. Non-builtin
			 * commands run by bash have signal handlers
			 * set to the values inherited by the shell
			 * from its parent". */
			new_act = S_IGN;
			break;
		case SIGTERM:
			if (iflag)
				new_act = S_IGN;
			break;
#if JOBS
		case SIGTSTP:
		case SIGTTOU:
			if (mflag)
				new_act = S_IGN;
			break;
#endif
		}
	}
	/* if !rootshell, we reset SIGQUIT to DFL,
	 * whereas we have to restore it to what shell got on entry.
	 * This is handled by the fact that if signal was IGNored on entry,
	 * then cur_act is S_HARD_IGN and we never change its sigaction
	 * (see code below).
	 */

	if (signo == SIGCHLD)
		new_act = S_CATCH;

	t = &sigmode[signo - 1];
	cur_act = *t;
	if (cur_act == 0) {
		/* current setting is not yet known */
		if (sigaction(signo, NULL, &act)) {
			/* pretend it worked; maybe we should give a warning,
			 * but other shells don't. We don't alter sigmode,
			 * so we retry every time.
			 * btw, in Linux it never fails. --vda */
			return;
		}
		if (act.sa_handler == SIG_IGN) {
			cur_act = S_HARD_IGN;
			if (mflag
			 && (signo == SIGTSTP || signo == SIGTTIN || signo == SIGTTOU)
			) {
				cur_act = S_IGN;   /* don't hard ignore these */
			}
		}
		if (act.sa_handler == SIG_DFL && new_act == S_DFL) {
			/* installing SIG_DFL over SIG_DFL is a no-op */
			/* saves one sigaction call in each "sh -c SCRIPT" invocation */
			*t = S_DFL;
			return;
		}
	}
	if (cur_act == S_HARD_IGN || cur_act == new_act)
		return;

	*t = new_act;

	act.sa_handler = SIG_DFL;
	switch (new_act) {
	case S_CATCH:
		act.sa_handler = signal_handler;
		break;
	case S_IGN:
		act.sa_handler = SIG_IGN;
		break;
	}
	/* flags and mask matter only if !DFL and !IGN, but we do it
	 * for all cases for more deterministic behavior:
	 */
	act.sa_flags = 0; //TODO: why not SA_RESTART?
	sigfillset(&act.sa_mask);

	sigaction_set(signo, &act);
}
#else
#define setsignal(s)
#define ignoresig(s)
#endif

/* mode flags for set_curjob */
#define CUR_DELETE 2
#define CUR_RUNNING 1
#define CUR_STOPPED 0

#if JOBS
/* pgrp of shell on invocation */
static int initialpgrp; //references:2
static int ttyfd = -1; //5
#endif
/* array of jobs */
static struct job *jobtab; //5
/* size of array */
static unsigned njobs; //4
/* current job */
static struct job *curjob; //lots

#if 0
/* Bash has a feature: it restores termios after a successful wait for
 * a foreground job which had at least one stopped or sigkilled member.
 * The probable rationale is that SIGSTOP and SIGKILL can preclude task from
 * properly restoring tty state. Should we do this too?
 * A reproducer: ^Z an interactive python:
 *
 * # python
 * Python 2.7.12 (...)
 * >>> ^Z
 *	{ python leaves tty in -icanon -echo state. We do survive that... }
 *  [1]+  Stopped                    python
 *	{ ...however, next program (python #2) does not survive it well: }
 * # python
 * Python 2.7.12 (...)
 * >>> Traceback (most recent call last):
 *	{ above, I typed "qwerty<CR>", but -echo state is still in effect }
 *   File "<stdin>", line 1, in <module>
 * NameError: name 'qwerty' is not defined
 *
 * The implementation below is modeled on bash code and seems to work.
 * However, I'm not sure we should do this. For one: what if I'd fg
 * the stopped python instead? It'll be confused by "restored" tty state.
 */
static struct termios shell_tty_info;
static void
get_tty_state(void)
{
	if (rootshell && ttyfd >= 0)
		tcgetattr(ttyfd, &shell_tty_info);
}
static void
set_tty_state(void)
{
	/* if (rootshell) - caller ensures this */
	if (ttyfd >= 0)
		tcsetattr(ttyfd, TCSADRAIN, &shell_tty_info);
}
static int
job_signal_status(struct job *jp)
{
	int status;
	unsigned i;
	struct procstat *ps = jp->ps;
	for (i = 0; i < jp->nprocs; i++) {
		status = ps[i].ps_status;
		if (WIFSIGNALED(status) || WIFSTOPPED(status))
			return status;
	}
	return 0;
}
static void
restore_tty_if_stopped_or_signaled(struct job *jp)
{
//TODO: check what happens if we come from waitforjob() in expbackq()
	if (rootshell) {
		int s = job_signal_status(jp);
		if (s) /* WIFSIGNALED(s) || WIFSTOPPED(s) */
			set_tty_state();
	}
}
#else
# define get_tty_state() ((void)0)
# define restore_tty_if_stopped_or_signaled(jp) ((void)0)
#endif

static void
set_curjob(struct job *jp, unsigned mode)
{
	struct job *jp1;
	struct job **jpp, **curp;

	/* first remove from list */
	jpp = curp = &curjob;
	while (1) {
		jp1 = *jpp;
		if (jp1 == jp)
			break;
		jpp = &jp1->prev_job;
	}
	*jpp = jp1->prev_job;

	/* Then re-insert in correct position */
	jpp = curp;
	switch (mode) {
	default:
#if DEBUG
		abort();
#endif
	case CUR_DELETE:
		/* job being deleted */
		break;
	case CUR_RUNNING:
		/* newly created job or backgrounded job,
		 * put after all stopped jobs.
		 */
		while (1) {
			jp1 = *jpp;
#if JOBS
			if (!jp1 || jp1->state != JOBSTOPPED)
#endif
				break;
			jpp = &jp1->prev_job;
		}
		/* FALLTHROUGH */
#if JOBS
	case CUR_STOPPED:
#endif
		/* newly stopped job - becomes curjob */
		jp->prev_job = *jpp;
		*jpp = jp;
		break;
	}
}

#if JOBS || ENABLE_PLATFORM_MINGW32 || DEBUG
static int
jobno(const struct job *jp)
{
	return jp - jobtab + 1;
}
#endif

/*
 * Convert a job name to a job structure.
 */
#if !JOBS
#define getjob(name, getctl) getjob(name)
#endif
static struct job *
getjob(const char *name, int getctl)
{
	struct job *jp;
#if ENABLE_PLATFORM_POSIX || JOBS_WIN32
	struct job *found;
#endif
	const char *err_msg = "%s: no such job";
	unsigned num;
	int c;
	const char *p;
#if ENABLE_PLATFORM_POSIX || JOBS_WIN32
	char *(*match)(const char *, const char *);
#endif

	jp = curjob;
	p = name;
	if (!p)
		goto currentjob;

	if (*p != '%')
		goto err;

	c = *++p;
	if (!c)
		goto currentjob;

	if (!p[1]) {
		if (c == '+' || c == '%') {
 currentjob:
			err_msg = "No current job";
			goto check;
		}
		if (c == '-') {
			if (jp)
				jp = jp->prev_job;
			err_msg = "No previous job";
 check:
			if (!jp)
				goto err;
			goto gotit;
		}
	}

	if (is_number(p)) {
		num = atoi(p);
		if (num > 0 && num <= njobs) {
			jp = jobtab + num - 1;
			if (jp->used)
				goto gotit;
			goto err;
		}
	}

#if ENABLE_PLATFORM_POSIX || JOBS_WIN32
	match = prefix;
	if (*p == '?') {
		match = strstr;
		p++;
	}

	found = NULL;
	while (jp) {
		if (match(jp->ps[0].ps_cmd, p)) {
			if (found)
				goto err;
			found = jp;
			err_msg = "%s: ambiguous";
		}
		jp = jp->prev_job;
	}
	if (!found)
		goto err;
	jp = found;
#else
	goto err;
#endif

 gotit:
#if JOBS
	err_msg = "job %s not created under job control";
	if (getctl && jp->jobctl == 0)
		goto err;
#endif
	return jp;
 err:
	ash_msg_and_raise_error(err_msg, name);
}

/*
 * Mark a job structure as unused.
 */
static void
freejob(struct job *jp)
{
#if ENABLE_PLATFORM_POSIX || JOBS_WIN32
	struct procstat *ps;
	int i;
#endif

	INT_OFF;
#if ENABLE_PLATFORM_POSIX || JOBS_WIN32
	for (i = jp->nprocs, ps = jp->ps; --i >= 0; ps++) {
		if (ps->ps_cmd != nullstr)
			free(ps->ps_cmd);
	}
#endif
	if (jp->ps != &jp->ps0)
		free(jp->ps);
	jp->used = 0;
	set_curjob(jp, CUR_DELETE);
	INT_ON;
}

#if JOBS
static void
xtcsetpgrp(int fd, pid_t pgrp)
{
	if (tcsetpgrp(fd, pgrp))
		ash_msg_and_raise_perror("can't set tty process group");
}

/*
 * Turn job control on and off.
 *
 * Note:  This code assumes that the third arg to ioctl is a character
 * pointer, which is true on Berkeley systems but not System V.  Since
 * System V doesn't have job control yet, this isn't a problem now.
 *
 * Called with interrupts off.
 */
static void
setjobctl(int on)
{
	int fd;
	int pgrp;

	if (on == doing_jobctl || rootshell == 0)
		return;
	if (on) {
		int ofd;
		ofd = fd = open(_PATH_TTY, O_RDWR);
		if (fd < 0) {
	/* BTW, bash will try to open(ttyname(0)) if open("/dev/tty") fails.
	 * That sometimes helps to acquire controlling tty.
	 * Obviously, a workaround for bugs when someone
	 * failed to provide a controlling tty to bash! :) */
			fd = 2;
			while (!isatty(fd))
				if (--fd < 0)
					goto out;
		}
		/* fd is a tty at this point */
		fd = fcntl(fd, F_DUPFD_CLOEXEC, 10);
		if (ofd >= 0) /* if it is "/dev/tty", close. If 0/1/2, don't */
			close(ofd);
		if (fd < 0)
			goto out; /* F_DUPFD failed */
		if (F_DUPFD_CLOEXEC == F_DUPFD) /* if old libc (w/o F_DUPFD_CLOEXEC) */
			close_on_exec_on(fd);
		while (1) { /* while we are in the background */
			pgrp = tcgetpgrp(fd);
			if (pgrp < 0) {
 out:
				ash_msg("can't access tty; job control turned off");
				mflag = on = 0;
				goto close;
			}
			if (pgrp == getpgrp())
				break;
			killpg(0, SIGTTIN);
		}
		initialpgrp = pgrp;

		setsignal(SIGTSTP);
		setsignal(SIGTTOU);
		setsignal(SIGTTIN);
		pgrp = rootpid;
		setpgid(0, pgrp);
		xtcsetpgrp(fd, pgrp);
	} else {
		/* turning job control off */
		fd = ttyfd;
		pgrp = initialpgrp;
		/* was xtcsetpgrp, but this can make exiting ash
		 * loop forever if pty is already deleted */
		tcsetpgrp(fd, pgrp);
		setpgid(0, pgrp);
		setsignal(SIGTSTP);
		setsignal(SIGTTOU);
		setsignal(SIGTTIN);
 close:
		if (fd >= 0)
			close(fd);
		fd = -1;
	}
	ttyfd = fd;
	doing_jobctl = on;
}
#endif

#if JOBS || JOBS_WIN32
static int FAST_FUNC
killcmd(int argc, char **argv)
{
	if (argv[1] && strcmp(argv[1], "-l") != 0) {
		int i = 1;
		do {
			if (argv[i][0] == '%') {
				/*
				 * "kill %N" - job kill
				 * Converting to pgrp / pid kill
				 */
				struct job *jp;
				char *dst;
				int j, n;

				jp = getjob(argv[i], 0);
				/*
				 * In jobs started under job control, we signal
				 * entire process group by kill -PGRP_ID.
				 * This happens, f.e., in interactive shell.
				 *
				 * Otherwise, we signal each child via
				 * kill PID1 PID2 PID3.
				 * Testcases:
				 * sh -c 'sleep 1|sleep 1 & kill %1'
				 * sh -c 'true|sleep 2 & sleep 1; kill %1'
				 * sh -c 'true|sleep 1 & sleep 2; kill %1'
				 */
				n = jp->nprocs; /* can't be 0 (I hope) */
#if !ENABLE_PLATFORM_MINGW32
				if (jp->jobctl)
					n = 1;
#endif
				dst = alloca(n * sizeof(int)*4);
				argv[i] = dst;
				for (j = 0; j < n; j++) {
					struct procstat *ps = &jp->ps[j];
					/* Skip non-running and not-stopped members
					 * (i.e. dead members) of the job
					 */
					if (ps->ps_status != -1 && !WIFSTOPPED(ps->ps_status))
						continue;
					/*
					 * kill_main has matching code to expect
					 * leading space. Needed to not confuse
					 * negative pids with "kill -SIGNAL_NO" syntax
					 */
#if !ENABLE_PLATFORM_MINGW32
					dst += sprintf(dst, jp->jobctl ? " -%u" : " %u", (int)ps->ps_pid);
#else
					dst += sprintf(dst, " -%u", (int)ps->ps_pid);
#endif
				}
				*dst = '\0';
			}
		} while (argv[++i]);
	}
	return kill_main(argc, argv);
}
#endif

#if JOBS
static void
showpipe(struct job *jp /*, FILE *out*/)
{
	struct procstat *ps;
	struct procstat *psend;

	psend = jp->ps + jp->nprocs;
	for (ps = jp->ps + 1; ps < psend; ps++)
		printf(" | %s", ps->ps_cmd);
	newline_and_flush(stdout);
	flush_stdout_stderr();
}


static int
restartjob(struct job *jp, int mode)
{
	struct procstat *ps;
	int i;
	int status;
	pid_t pgid;

	INT_OFF;
	if (jp->state == JOBDONE)
		goto out;
	jp->state = JOBRUNNING;
	pgid = jp->ps[0].ps_pid;
	if (mode == FORK_FG) {
		get_tty_state();
		xtcsetpgrp(ttyfd, pgid);
	}
	killpg(pgid, SIGCONT);
	ps = jp->ps;
	i = jp->nprocs;
	do {
		if (WIFSTOPPED(ps->ps_status)) {
			ps->ps_status = -1;
		}
		ps++;
	} while (--i);
 out:
	status = (mode == FORK_FG) ? waitforjob(jp) : 0;
	INT_ON;
	return status;
}

static int FAST_FUNC
fg_bgcmd(int argc UNUSED_PARAM, char **argv)
{
	struct job *jp;
	int mode;
	int retval;

	mode = (**argv == 'f') ? FORK_FG : FORK_BG;
	nextopt(nullstr);
	argv = argptr;
	do {
		jp = getjob(*argv, 1);
		if (mode == FORK_BG) {
			set_curjob(jp, CUR_RUNNING);
			printf("[%d] ", jobno(jp));
		}
		out1str(jp->ps[0].ps_cmd);
		showpipe(jp /*, stdout*/);
		retval = restartjob(jp, mode);
	} while (*argv && *++argv);
	return retval;
}
#endif

static int
sprint_status48(char *os, int status, int sigonly)
{
	char *s = os;
	int st;

	if (!WIFEXITED(status)) {
#if JOBS
		if (WIFSTOPPED(status))
			st = WSTOPSIG(status);
		else
#endif
			st = WTERMSIG(status);
		if (sigonly) {
			if (st == SIGINT || st == SIGPIPE)
				goto out;
#if JOBS
			if (WIFSTOPPED(status))
				goto out;
#endif
		}
		st &= 0x7f;
		s = stpncpy(s, strsignal(st), 32);
		if (WCOREDUMP(status)) {
			s = stpcpy(s, " (core dumped)");
		}
	} else if (!sigonly) {
		st = WEXITSTATUS(status);
		s += fmtstr(s, 16, (st ? "Done(%d)" : "Done"), st);
	}
 out:
	return s - os;
}

#if ENABLE_PLATFORM_MINGW32
static BOOL WINAPI ctrl_handler(DWORD dwCtrlType)
{
	if (dwCtrlType == CTRL_C_EVENT || dwCtrlType == CTRL_BREAK_EVENT) {
		if (!suppress_int)
			raise_interrupt(); /* does not return */
		pending_int = 1;
		return TRUE;
	}
	return FALSE;
}

/*
 * Windows does not know about parent-child relationship
 * They don't support waitpid(-1)
 */
static pid_t
waitpid_child(int *status, int wait_flags)
{
	struct job *jb;
	struct procstat *ps;
	int pid_nr = 0;
	pid_t *pidlist;
	HANDLE *proclist;
	pid_t pid = -1;
	DWORD win_status, idx;
	int i;

	for (jb = curjob; jb; jb = jb->prev_job) {
		if (jb->state != JOBDONE)
			pid_nr += jb->nprocs;
	}
	if (pid_nr == 0)
		return -1;

	pidlist = ckmalloc(sizeof(*pidlist)*pid_nr);
	proclist = ckmalloc(sizeof(*proclist)*pid_nr);

	pid_nr = 0;
	for (jb = curjob; jb; jb = jb->prev_job) {
		if (jb->state == JOBDONE)
			continue;
		ps = jb->ps;
		for (i = 0; i < jb->nprocs; ++i) {
			if (ps[i].ps_proc) {
				pidlist[pid_nr] = ps[i].ps_pid;
				proclist[pid_nr++] = ps[i].ps_proc;
			}
		}
	}

	if (pid_nr == 0)
		goto done;

	idx = WaitForMultipleObjects(pid_nr, proclist, FALSE,
				wait_flags&WNOHANG ? 1 : INFINITE);
	if (idx < pid_nr) {
		GetExitCodeProcess(proclist[idx], &win_status);
		*status = (int)win_status << 8;
		if (win_status == 128 + SIGTERM || win_status == 128 + SIGKILL)
			*status += win_status - 128;
		pid = pidlist[idx];
	}
 done:
	free(pidlist);
	free(proclist);
	return pid;
}
#define waitpid(p, s, f) waitpid_child(s, f)
#endif

#define DOWAIT_NONBLOCK 0
#define DOWAIT_BLOCK    1
#define DOWAIT_BLOCK_OR_SIG 2
#if BASH_WAIT_N
# define DOWAIT_JOBSTATUS 0x10   /* OR this to get job's exitstatus instead of pid */
#endif

static int
waitproc(int block, int *status)
{
#if !ENABLE_PLATFORM_MINGW32
	sigset_t oldmask;
	int flags = block == DOWAIT_BLOCK ? 0 : WNOHANG;
	int err;

#if JOBS
	if (doing_jobctl)
		flags |= WUNTRACED;
#endif

	do {
		got_sigchld = 0;
		do
			err = waitpid(-1, status, flags);
		while (err < 0 && errno == EINTR);

		if (err || (err = -!block))
			break;

		sigfillset(&oldmask);
		sigprocmask2(SIG_SETMASK, &oldmask); /* mask is updated */
		while (!got_sigchld && !pending_sig)
			sigsuspend(&oldmask);
		sigprocmask(SIG_SETMASK, &oldmask, NULL);
		//simpler, but unsafe: a signal can set pending_sig after check, but before pause():
		//while (!got_sigchld && !pending_sig)
		//	pause();

	} while (got_sigchld);

	return err;
#else
	int flags = block == DOWAIT_BLOCK ? 0 : WNOHANG;
	*status = 0;
	return waitpid(-1, status, flags);
#endif
}

static int
waitone(int block, struct job *job)
{
	int pid;
	int status;
	struct job *jp;
	struct job *thisjob = NULL;
#if BASH_WAIT_N
	bool want_jobexitstatus = (block & DOWAIT_JOBSTATUS);
	block = (block & ~DOWAIT_JOBSTATUS);
#endif

	TRACE(("dowait(0x%x) called\n", block));

	/* It's wrong to call waitpid() outside of INT_OFF region:
	 * signal can arrive just after syscall return and handler can
	 * longjmp away, losing stop/exit notification processing.
	 * Thus, for "jobs" builtin, and for waiting for a fg job,
	 * we call waitpid() (blocking or non-blocking) inside INT_OFF.
	 *
	 * However, for "wait" builtin it is wrong to simply call waitpid()
	 * in INT_OFF region: "wait" needs to wait for any running job
	 * to change state, but should exit on any trap too.
	 * In INT_OFF region, a signal just before syscall entry can set
	 * pending_sig variables, but we can't check them, and we would
	 * either enter a sleeping waitpid() (BUG), or need to busy-loop.
	 *
	 * Because of this, we run inside INT_OFF, but use a special routine
	 * which combines waitpid() and sigsuspend().
	 * This is the reason why we need to have a handler for SIGCHLD:
	 * SIG_DFL handler does not wake sigsuspend().
	 */
	INT_OFF;
	pid = waitproc(block, &status);
	TRACE(("wait returns pid %d, status=%d\n", pid, status));
	if (pid <= 0)
		goto out;

	for (jp = curjob; jp; jp = jp->prev_job) {
		int jobstate;
		struct procstat *ps;
		struct procstat *psend;
		if (jp->state == JOBDONE)
			continue;
		jobstate = JOBDONE;
		ps = jp->ps;
		psend = ps + jp->nprocs;
		do {
			if (ps->ps_pid == pid) {
				TRACE(("Job %d: changing status of proc %d "
					"from 0x%x to 0x%x\n",
					jobno(jp), pid, ps->ps_status, status));
				ps->ps_status = status;
				thisjob = jp;
#if ENABLE_PLATFORM_MINGW32
				ps->ps_pid = -1;
				CloseHandle(ps->ps_proc);
				ps->ps_proc = NULL;
#endif
			}
			if (ps->ps_status == -1)
				jobstate = JOBRUNNING;
#if JOBS
			if (jobstate == JOBRUNNING)
				continue;
			if (WIFSTOPPED(ps->ps_status)) {
				jp->stopstatus = ps->ps_status;
				jobstate = JOBSTOPPED;
			}
#endif
		} while (++ps < psend);
		if (!thisjob)
			continue;

		/* Found the job where one of its processes changed its state.
		 * Is there at least one live and running process in this job? */
		if (jobstate != JOBRUNNING) {
			/* No. All live processes in the job are stopped
			 * (JOBSTOPPED) or there are no live processes (JOBDONE)
			 */
			thisjob->changed = 1;
			if (thisjob->state != jobstate) {
				TRACE(("Job %d: changing state from %d to %d\n",
					jobno(thisjob), thisjob->state, jobstate));
				thisjob->state = jobstate;
#if JOBS
				if (jobstate == JOBSTOPPED)
					set_curjob(thisjob, CUR_STOPPED);
#endif
			}
		}
		goto out;
	}
	/* The process wasn't found in job list */
 out:
	INT_ON;

#if BASH_WAIT_N
	if (want_jobexitstatus) {
		pid = -1;
		if (thisjob && thisjob->state == JOBDONE)
			pid = thisjob->ps[thisjob->nprocs - 1].ps_status;
	}
#endif
	if (thisjob && thisjob == job) {
		char s[48 + 1];
		int len;

		len = sprint_status48(s, status, 1);
		if (len) {
			s[len] = '\n';
			s[len + 1] = '\0';
			out2str(s);
		}
	}
	return pid;
}

static int
dowait(int block, struct job *jp)
{
#if !ENABLE_PLATFORM_MINGW32
	smallint gotchld = *(volatile smallint *)&got_sigchld;
	int rpid;
	int pid;

	if (jp && jp->state != JOBRUNNING)
		block = DOWAIT_NONBLOCK;

	if (block == DOWAIT_NONBLOCK && !gotchld)
		return 1;

	rpid = 1;

	do {
		pid = waitone(block, jp);
		rpid &= !!pid;

		if (!pid || (jp && jp->state != JOBRUNNING))
			block = DOWAIT_NONBLOCK;
	} while (pid >= 0);

	return rpid;
#else
	int pid = 1;

	while (jp ? jp->state == JOBRUNNING : pid > 0)
		pid = waitone(block, jp);

	return pid;
#endif
}

#if JOBS || JOBS_WIN32
static void
showjob(struct job *jp, int mode)
{
	struct procstat *ps;
	struct procstat *psend;
	int col;
	int indent_col;
	char s[16 + 16 + 48];
	FILE *out = (mode & SHOW_STDERR ? stderr : stdout);

	ps = jp->ps;

	if (mode & SHOW_ONLY_PGID) { /* jobs -p */
		/* just output process (group) id of pipeline */
		fprintf(out, "%"PID_FMT"d\n", ps->ps_pid);
		return;
	}

	col = fmtstr(s, 16, "[%d]   ", jobno(jp));
	indent_col = col;

	if (jp == curjob)
		s[col - 3] = '+';
	else if (curjob && jp == curjob->prev_job)
		s[col - 3] = '-';

	if (mode & SHOW_PIDS)
		col += fmtstr(s + col, 16, "%"PID_FMT"d ", ps->ps_pid);

	psend = ps + jp->nprocs;

	if (jp->state == JOBRUNNING) {
		strcpy(s + col, "Running");
		col += sizeof("Running") - 1;
	} else {
		int status = psend[-1].ps_status;
#if !ENABLE_PLATFORM_MINGW32
		if (jp->state == JOBSTOPPED)
			status = jp->stopstatus;
#endif
		col += sprint_status48(s + col, status, 0);
	}
	/* By now, "[JOBID]*  [maybe PID] STATUS" is printed */

	/* This loop either prints "<cmd1> | <cmd2> | <cmd3>" line
	 * or prints several "PID             | <cmdN>" lines,
	 * depending on SHOW_PIDS bit.
	 * We do not print status of individual processes
	 * between PID and <cmdN>. bash does it, but not very well:
	 * first line shows overall job status, not process status,
	 * making it impossible to know 1st process status.
	 */
	goto start;
	do {
		/* for each process */
		s[0] = '\0';
		col = 33;
		if (mode & SHOW_PIDS)
			col = fmtstr(s, 48, "\n%*c%"PID_FMT"d ", indent_col, ' ', ps->ps_pid) - 1;
 start:
#if ENABLE_PLATFORM_POSIX || JOBS_WIN32
		fprintf(out, "%s%*c%s%s",
				s,
				33 - col >= 0 ? 33 - col : 0, ' ',
				ps == jp->ps ? "" : "| ",
				ps->ps_cmd
		);
#else
		fprintf(out, "%s", s);
#endif
	} while (++ps != psend);
	newline_and_flush(out);

	jp->changed = 0;

	if (jp->state == JOBDONE) {
		TRACE(("showjob: freeing job %d\n", jobno(jp)));
		freejob(jp);
	}
}

/*
 * Print a list of jobs.  If "change" is nonzero, only print jobs whose
 * statuses have changed since the last call to showjobs.
 */
static void
showjobs(int mode)
{
	struct job *jp;

	TRACE(("showjobs(0x%x) called\n", mode));

	/* Handle all finished jobs */
	dowait(DOWAIT_NONBLOCK, NULL);

	for (jp = curjob; jp; jp = jp->prev_job) {
		if (!(mode & SHOW_CHANGED) || jp->changed) {
			showjob(jp, mode);
		}
	}
}

static int FAST_FUNC
jobscmd(int argc UNUSED_PARAM, char **argv)
{
	int mode, m;

	mode = 0;
	while ((m = nextopt("lp")) != '\0') {
		if (m == 'l')
			mode |= SHOW_PIDS;
		else
			mode |= SHOW_ONLY_PGID;
	}

	argv = argptr;
	if (*argv) {
		do
			showjob(getjob(*argv, 0), mode);
		while (*++argv);
	} else {
		showjobs(mode);
	}

	return 0;
}
#endif /* JOBS */

/* Called only on finished or stopped jobs (no members are running) */
static int
getstatus(struct job *job)
{
	int status;
	int retval;
	struct procstat *ps;

	/* Fetch last member's status */
	ps = job->ps + job->nprocs - 1;
	status = ps->ps_status;
	if (pipefail) {
		/* "set -o pipefail" mode: use last _nonzero_ status */
		while (status == 0 && --ps >= job->ps)
			status = ps->ps_status;
	}

	retval = WEXITSTATUS(status);
	if (!WIFEXITED(status)) {
#if JOBS
		retval = WSTOPSIG(status);
		if (!WIFSTOPPED(status))
#endif
		{
			/* XXX: limits number of signals */
			retval = WTERMSIG(status);
#if JOBS
			if (retval == SIGINT)
				job->sigint = 1;
#endif
		}
		retval |= 128;
	}
	TRACE(("getstatus: job %d, nproc %d, status 0x%x, retval 0x%x\n",
		jobno(job), job->nprocs, status, retval));
	return retval;
}

static int FAST_FUNC
waitcmd(int argc UNUSED_PARAM, char **argv)
{
	struct job *job;
	int retval;
	struct job *jp;
#if BASH_WAIT_N
	int status;
	char one = nextopt("n");
#else
	nextopt(nullstr);
#endif
	retval = 0;

	argv = argptr;
	if (!argv[0]) {
		/* wait for all jobs / one job if -n */
		for (;;) {
			jp = curjob;
#if BASH_WAIT_N
			if (one && !jp)
				/* exitcode of "wait -n" with nothing to wait for is 127, not 0 */
				retval = 127;
#endif
			while (1) {
				if (!jp) /* no running procs */
					goto ret;
				if (jp->state == JOBRUNNING)
					break;
				jp->waited = 1;
				jp = jp->prev_job;
			}
	/* man bash:
	 * "When bash is waiting for an asynchronous command via
	 * the wait builtin, the reception of a signal for which a trap
	 * has been set will cause the wait builtin to return immediately
	 * with an exit status greater than 128, immediately after which
	 * the trap is executed."
	 */
#if BASH_WAIT_N
			status = dowait(DOWAIT_BLOCK_OR_SIG | DOWAIT_JOBSTATUS, NULL);
#else
			dowait(DOWAIT_BLOCK_OR_SIG, NULL);
#endif
			/* if child sends us a signal *and immediately exits*,
			 * dowait() returns pid > 0. Check this case,
			 * not "if (dowait() < 0)"!
			 */
			if (pending_sig)
				goto sigout;
#if BASH_WAIT_N
			if (one) {
				/* wait -n waits for one _job_, not one _process_.
				 *  date; sleep 3 & sleep 2 | sleep 1 & wait -n; date
				 * should wait for 2 seconds. Not 1 or 3.
				 */
				if (status != -1 && !WIFSTOPPED(status)) {
					retval = WEXITSTATUS(status);
					if (WIFSIGNALED(status))
						retval = 128 | WTERMSIG(status);
					goto ret;
				}
			}
#endif
		}
	}

	retval = 127;
	do {
		if (**argv != '%') {
			pid_t pid = number(*argv);
			job = curjob;
			while (1) {
				if (!job)
					goto repeat;
				if (job->ps[job->nprocs - 1].ps_pid == pid)
					break;
				job = job->prev_job;
			}
		} else {
			job = getjob(*argv, 0);
		}
		/* loop until process terminated or stopped */
		dowait(DOWAIT_BLOCK_OR_SIG, job);
		if (pending_sig)
			goto sigout;
		job->waited = 1;
		retval = getstatus(job);
 repeat: ;
	} while (*++argv);

 ret:
	return retval;
 sigout:
	retval = 128 | pending_sig;
	return retval;
}

static struct job *
growjobtab(void)
{
	size_t len;
	ptrdiff_t offset;
	struct job *jp, *jq;

	len = njobs * sizeof(*jp);
	jq = jobtab;
	jp = ckrealloc(jq, len + 4 * sizeof(*jp));

	offset = (char *)jp - (char *)jq;
	if (offset) {
		/* Relocate pointers */
		size_t l = len;

		jq = (struct job *)((char *)jq + l);
		while (l) {
			l -= sizeof(*jp);
			jq--;
#define joff(p) ((struct job *)((char *)(p) + l))
#define jmove(p) (p) = (void *)((char *)(p) + offset)
			if (joff(jp)->ps == &jq->ps0)
				jmove(joff(jp)->ps);
			if (joff(jp)->prev_job)
				jmove(joff(jp)->prev_job);
		}
		if (curjob)
			jmove(curjob);
#undef joff
#undef jmove
	}

	njobs += 4;
	jobtab = jp;
	jp = (struct job *)((char *)jp + len);
	jq = jp + 3;
	do {
		jq->used = 0;
	} while (--jq >= jp);
	return jp;
}

/*
 * Return a new job structure.
 * Called with interrupts off.
 */
static struct job *
makejob(/*union node *node,*/ int nprocs)
{
	int i;
	struct job *jp;

	for (i = njobs, jp = jobtab; ; jp++) {
		if (--i < 0) {
			jp = growjobtab();
			break;
		}
		if (jp->used == 0)
			break;
		if (jp->state != JOBDONE || !jp->waited)
			continue;
#if JOBS || JOBS_WIN32
		if (doing_jobctl)
			continue;
#endif
		freejob(jp);
		break;
	}
	memset(jp, 0, sizeof(*jp));
#if JOBS
	/* jp->jobctl is a bitfield.
	 * "jp->jobctl |= doing_jobctl" likely to give awful code */
	if (doing_jobctl)
		jp->jobctl = 1;
#endif
	jp->prev_job = curjob;
	curjob = jp;
	jp->used = 1;
	jp->ps = &jp->ps0;
	if (nprocs > 1) {
		jp->ps = ckmalloc(nprocs * sizeof(struct procstat));
	}
	TRACE(("makejob(%d) returns %%%d\n", nprocs,
				jobno(jp)));
	return jp;
}

#if JOBS || JOBS_WIN32
/*
 * Return a string identifying a command (to be printed by the
 * jobs command).
 */
static char *cmdnextc;

static void
cmdputs(const char *s)
{
	static const char vstype[VSTYPE + 1][3] ALIGN1 = {
		"", "}", "-", "+", "?", "=",
		"%", "%%", "#", "##"
		IF_BASH_SUBSTR(, ":")
		IF_BASH_PATTERN_SUBST(, "/", "//")
	};

	const char *p, *str;
	char cc[2];
	char *nextc;
	unsigned char c;
	unsigned char subtype = 0;
	int quoted = 0;

	cc[1] = '\0';
	nextc = makestrspace((strlen(s) + 1) * 8, cmdnextc);
	p = s;
	while ((c = *p++) != '\0') {
		str = NULL;
		switch (c) {
		case CTLESC:
			c = *p++;
			break;
		case CTLVAR:
			subtype = *p++;
			if ((subtype & VSTYPE) == VSLENGTH)
				str = "${#";
			else
				str = "${";
			goto dostr;
		case CTLENDVAR:
			str = "\"}";
			str += !(quoted & 1);
			quoted >>= 1;
			subtype = 0;
			goto dostr;
#if BASH_PROCESS_SUBST
		case CTLBACKQ:
			c = '$';
			str = "(...)";
			break;
		case CTLTOPROC:
			c = '>';
			str = "(...)";
			break;
		case CTLFROMPROC:
			c = '<';
			str = "(...)";
			break;
#else
		case CTLBACKQ:
			str = "$(...)";
			goto dostr;
#endif
#if ENABLE_FEATURE_SH_MATH
		case CTLARI:
			str = "$((";
			goto dostr;
		case CTLENDARI:
			str = "))";
			goto dostr;
#endif
		case CTLQUOTEMARK:
			quoted ^= 1;
			c = '"';
			break;
		case '=':
			if (subtype == 0)
				break;
			if ((subtype & VSTYPE) != VSNORMAL)
				quoted <<= 1;
			str = vstype[subtype & VSTYPE];
			if (subtype & VSNUL)
				c = ':';
			else
				goto checkstr;
			break;
		case '\'':
		case '\\':
		case '"':
		case '$':
			/* These can only happen inside quotes */
			cc[0] = c;
			str = cc;
//FIXME:
// $ true $$ &
// $ <cr>
// [1]+  Done    true ${\$}   <<=== BUG: ${\$} is not a valid way to write $$ (${$} would be ok)
			c = '\\';
			break;
		default:
			break;
		}
		USTPUTC(c, nextc);
 checkstr:
		if (!str)
			continue;
 dostr:
		while ((c = *str++) != '\0') {
			USTPUTC(c, nextc);
		}
	} /* while *p++ not NUL */

	if (quoted & 1) {
		USTPUTC('"', nextc);
	}
	*nextc = 0;
	cmdnextc = nextc;
}

/* cmdtxt() and cmdlist() call each other */
static void cmdtxt(union node *n);

static void
cmdlist(union node *np, int sep)
{
	for (; np; np = np->narg.next) {
		if (!sep)
			cmdputs(" ");
		cmdtxt(np);
		if (sep && np->narg.next)
			cmdputs(" ");
	}
}

static void
cmdtxt(union node *n)
{
	union node *np;
	struct nodelist *lp;
	const char *p;

	if (!n)
		return;
	switch (n->type) {
	default:
#if DEBUG
		abort();
#endif
	case NPIPE:
		lp = n->npipe.cmdlist;
		for (;;) {
			cmdtxt(lp->n);
			lp = lp->next;
			if (!lp)
				break;
			cmdputs(" | ");
		}
		break;
	case NSEMI:
		p = "; ";
		goto binop;
	case NAND:
		p = " && ";
		goto binop;
	case NOR:
		p = " || ";
 binop:
		cmdtxt(n->nbinary.ch1);
		cmdputs(p);
		n = n->nbinary.ch2;
		goto donode;
	case NREDIR:
	case NBACKGND:
		n = n->nredir.n;
		goto donode;
	case NNOT:
		cmdputs("!");
		n = n->nnot.com;
 donode:
		cmdtxt(n);
		break;
	case NIF:
		cmdputs("if ");
		cmdtxt(n->nif.test);
		cmdputs("; then ");
		if (n->nif.elsepart) {
			cmdtxt(n->nif.ifpart);
			cmdputs("; else ");
			n = n->nif.elsepart;
		} else {
			n = n->nif.ifpart;
		}
		p = "; fi";
		goto dotail;
	case NSUBSHELL:
		cmdputs("(");
		n = n->nredir.n;
		p = ")";
		goto dotail;
	case NWHILE:
		p = "while ";
		goto until;
	case NUNTIL:
		p = "until ";
 until:
		cmdputs(p);
		cmdtxt(n->nbinary.ch1);
		n = n->nbinary.ch2;
		p = "; done";
 dodo:
		cmdputs("; do ");
 dotail:
		cmdtxt(n);
		goto dotail2;
	case NFOR:
		cmdputs("for ");
		cmdputs(n->nfor.var);
		cmdputs(" in ");
		cmdlist(n->nfor.args, 1);
		n = n->nfor.body;
		p = "; done";
		goto dodo;
	case NDEFUN:
		cmdputs(n->ndefun.text);
		p = "() { ... }";
		goto dotail2;
	case NCMD:
		cmdlist(n->ncmd.args, 1);
		cmdlist(n->ncmd.redirect, 0);
		break;
	case NARG:
		p = n->narg.text;
 dotail2:
		cmdputs(p);
		break;
	case NHERE:
	case NXHERE:
		p = "<<...";
		goto dotail2;
	case NCASE:
		cmdputs("case ");
		cmdputs(n->ncase.expr->narg.text);
		cmdputs(" in ");
		for (np = n->ncase.cases; np; np = np->nclist.next) {
			cmdtxt(np->nclist.pattern);
			cmdputs(") ");
			cmdtxt(np->nclist.body);
			cmdputs(";; ");
		}
		p = "esac";
		goto dotail2;
	case NTO:
		p = ">";
		goto redir;
	case NCLOBBER:
		p = ">|";
		goto redir;
	case NAPPEND:
		p = ">>";
		goto redir;
#if BASH_REDIR_OUTPUT
	case NTO2:
#endif
	case NTOFD:
		p = ">&";
		goto redir;
	case NFROM:
		p = "<";
		goto redir;
	case NFROMFD:
		p = "<&";
		goto redir;
	case NFROMTO:
		p = "<>";
 redir:
		cmdputs(utoa(n->nfile.fd));
		cmdputs(p);
		if (n->type == NTOFD || n->type == NFROMFD) {
			if (n->ndup.dupfd >= 0)
				cmdputs(utoa(n->ndup.dupfd));
			else
				cmdputs("-");
			break;
		}
		n = n->nfile.fname;
		goto donode;
	}
}

static char *
commandtext(union node *n)
{
	char *name;

	STARTSTACKSTR(cmdnextc);
	cmdtxt(n);
	name = stackblock();
	TRACE(("commandtext: name %p, end %p\n", name, cmdnextc));
	return ckstrdup(name);
}
#endif /* JOBS */

/*
 * Fork off a subshell.  If we are doing job control, give the subshell its
 * own process group.  Jp is a job structure that the job is to be added to.
 * N is the command that will be evaluated by the child.  Both jp and n may
 * be NULL.  The mode parameter can be one of the following:
 *      FORK_FG - Fork off a foreground process.
 *      FORK_BG - Fork off a background process.
 *      FORK_NOJOB - Like FORK_FG, but don't give the process its own
 *                   process group even if job control is on.
 *
 * When job control is turned off, background processes have their standard
 * input redirected to /dev/null (except for the second and later processes
 * in a pipeline).
 *
 * Called with interrupts off.
 */
#if !ENABLE_PLATFORM_MINGW32
/*
 * Clear traps on a fork.
 */
static void
clear_traps(void)
{
	char **tp;

	INT_OFF;
	for (tp = trap; tp <= &trap[NTRAP_LAST]; tp++) {
		if (*tp && **tp) {      /* trap not NULL or "" (SIG_IGN) */
			if (trap_ptr == trap)
				free(*tp);
			/* else: it "belongs" to trap_ptr vector, don't free */
			*tp = NULL;
			if ((tp - trap) != 0 && (tp - trap) < NSIG)
				setsignal(tp - trap);
		}
	}
	may_have_traps = 0;
	INT_ON;
}

/* Lives far away from here, needed for forkchild */
static void closescript(void);

/* Called after fork(), in child */
/* jp and n are NULL when called by openhere() for heredoc support */
static NOINLINE void
forkchild(struct job *jp, union node *n, int mode)
{
	int oldlvl;

	TRACE(("Child shell %d\n", getpid()));
	oldlvl = shlvl;
	shlvl++;

	/* man bash: "Non-builtin commands run by bash have signal handlers
	 * set to the values inherited by the shell from its parent".
	 * Do we do it correctly? */

	closescript();

	if (mode == FORK_NOJOB          /* is it `xxx` ? */
	 && n && n->type == NCMD        /* is it single cmd? */
	/* && n->ncmd.args->type == NARG - always true? */
	 && n->ncmd.args && strcmp(n->ncmd.args->narg.text, "trap") == 0
	 && n->ncmd.args->narg.next == NULL /* "trap" with no arguments */
	/* && n->ncmd.args->narg.backquote == NULL - do we need to check this? */
	) {
		TRACE(("Trap hack\n"));
		/* Awful hack for `trap` or $(trap).
		 *
		 * http://www.opengroup.org/onlinepubs/009695399/utilities/trap.html
		 * contains an example where "trap" is executed in a subshell:
		 *
		 * save_traps=$(trap)
		 * ...
		 * eval "$save_traps"
		 *
		 * Standard does not say that "trap" in subshell shall print
		 * parent shell's traps. It only says that its output
		 * must have suitable form, but then, in the above example
		 * (which is not supposed to be normative), it implies that.
		 *
		 * bash (and probably other shell) does implement it
		 * (traps are reset to defaults, but "trap" still shows them),
		 * but as a result, "trap" logic is hopelessly messed up:
		 *
		 * # trap
		 * trap -- 'echo Ho' SIGWINCH  <--- we have a handler
		 * # (trap)        <--- trap is in subshell - no output (correct, traps are reset)
		 * # true | trap   <--- trap is in subshell - no output (ditto)
		 * # echo `true | trap`    <--- in subshell - output (but traps are reset!)
		 * trap -- 'echo Ho' SIGWINCH
		 * # echo `(trap)`         <--- in subshell in subshell - output
		 * trap -- 'echo Ho' SIGWINCH
		 * # echo `true | (trap)`  <--- in subshell in subshell in subshell - output!
		 * trap -- 'echo Ho' SIGWINCH
		 *
		 * The rules when to forget and when to not forget traps
		 * get really complex and nonsensical.
		 *
		 * Our solution: ONLY bare $(trap) or `trap` is special.
		 */
		/* Save trap handler strings for trap builtin to print */
		trap_ptr = xmemdup(trap, sizeof(trap));
		/* Fall through into clearing traps */
	}
	clear_traps();
#if JOBS
	/* do job control only in root shell */
	doing_jobctl = 0;
	if (mode != FORK_NOJOB && jp->jobctl && oldlvl == 0) {
		pid_t pgrp;

		if (jp->nprocs == 0)
			pgrp = getpid();
		else
			pgrp = jp->ps[0].ps_pid;
		/* this can fail because we are doing it in the parent also */
		setpgid(0, pgrp);
		if (mode == FORK_FG)
			xtcsetpgrp(ttyfd, pgrp);
		setsignal(SIGTSTP);
		setsignal(SIGTTOU);
	} else
#endif
	if (mode == FORK_BG) {
		/* man bash: "When job control is not in effect,
		 * asynchronous commands ignore SIGINT and SIGQUIT" */
		ignoresig(SIGINT);
		ignoresig(SIGQUIT);
		if (jp->nprocs == 0) {
			close(0);
			if (open(bb_dev_null, O_RDONLY) != 0)
				ash_msg_and_raise_perror("can't open '%s'", bb_dev_null);
		}
	}
	if (oldlvl == 0) {
		if (iflag) { /* why if iflag only? */
			setsignal(SIGINT);
			setsignal(SIGTERM);
		}
		/* man bash:
		 * "In all cases, bash ignores SIGQUIT. Non-builtin
		 * commands run by bash have signal handlers
		 * set to the values inherited by the shell
		 * from its parent".
		 * Take care of the second rule: */
		setsignal(SIGQUIT);
	}
#if JOBS
	if (n && n->type == NCMD
	 && n->ncmd.args && strcmp(n->ncmd.args->narg.text, "jobs") == 0
	) {
		TRACE(("Job hack\n"));
		/* "jobs": we do not want to clear job list for it,
		 * instead we remove only _its_ own_ job from job list.
		 * This makes "jobs .... | cat" more useful.
		 */
		freejob(curjob);
		return;
	}
#endif
	for (jp = curjob; jp; jp = jp->prev_job)
		freejob(jp);
}
#endif

/* Called after fork(), in parent */
#if !JOBS && !JOBS_WIN32
#define forkparent(jp, n, mode, pid) forkparent(jp, mode, pid)
#endif
static void
#if !ENABLE_PLATFORM_MINGW32
forkparent(struct job *jp, union node *n, int mode, pid_t pid)
#else
forkparent(struct job *jp, union node *n, int mode, HANDLE proc)
#endif
{
#if ENABLE_PLATFORM_MINGW32
	pid_t pid = GetProcessId(proc);
#endif
	TRACE(("In parent shell: child = %d\n", pid));
	if (!jp) /* jp is NULL when called by openhere() for heredoc support */
		return;
#if JOBS
	if (mode != FORK_NOJOB && jp->jobctl) {
		int pgrp;

		if (jp->nprocs == 0)
			pgrp = pid;
		else
			pgrp = jp->ps[0].ps_pid;
		/* This can fail because we are doing it in the child also */
		setpgid(pid, pgrp);
	}
#endif
	if (mode == FORK_BG) {
		backgndpid = pid;               /* set $! */
		set_curjob(jp, CUR_RUNNING);
#if ENABLE_PLATFORM_MINGW32
		if (iflag && jp && jp->nprocs == 0)
			fprintf(stderr, "[%d] %"PID_FMT"d\n", jobno(jp), pid);
#endif
	}
	if (jp) {
		struct procstat *ps = &jp->ps[jp->nprocs++];
		ps->ps_pid = pid;
		ps->ps_status = -1;
#if ENABLE_PLATFORM_POSIX || JOBS_WIN32
		ps->ps_cmd = nullstr;
#endif
#if ENABLE_PLATFORM_MINGW32
		ps->ps_proc = proc;
#endif
#if JOBS || JOBS_WIN32
		if (doing_jobctl && n)
			ps->ps_cmd = commandtext(n);
#endif
	}
}

#if !ENABLE_PLATFORM_MINGW32
/* jp and n are NULL when called by openhere() for heredoc support */
static int
forkshell(struct job *jp, union node *n, int mode)
{
	int pid;

	TRACE(("forkshell(%%%d, %p, %d) called\n", jobno(jp), n, mode));
	pid = fork();
	if (pid < 0) {
		TRACE(("Fork failed, errno=%d", errno));
		if (jp)
			freejob(jp);
		ash_msg_and_raise_perror("can't fork");
	}
	if (pid == 0) {
		CLEAR_RANDOM_T(&random_gen); /* or else $RANDOM repeats in child */
		forkchild(jp, n, mode);
	} else {
		forkparent(jp, n, mode, pid);
	}
	return pid;
}
#endif

/*
 * Wait for job to finish.
 *
 * Under job control we have the problem that while a child process
 * is running interrupts generated by the user are sent to the child
 * but not to the shell.  This means that an infinite loop started by
 * an interactive user may be hard to kill.  With job control turned off,
 * an interactive user may place an interactive program inside a loop.
 * If the interactive program catches interrupts, the user doesn't want
 * these interrupts to also abort the loop.  The approach we take here
 * is to have the shell ignore interrupt signals while waiting for a
 * foreground process to terminate, and then send itself an interrupt
 * signal if the child process was terminated by an interrupt signal.
 * Unfortunately, some programs want to do a bit of cleanup and then
 * exit on interrupt; unless these processes terminate themselves by
 * sending a signal to themselves (instead of calling exit) they will
 * confuse this approach.
 *
 * Called with interrupts off.
 */
static int
waitforjob(struct job *jp)
{
	int st;

	TRACE(("waitforjob(%%%d) called\n", jp ? jobno(jp) : 0));

	/* In non-interactive shells, we _can_ get
	 * a keyboard signal here and be EINTRed, but we just loop
	 * inside dowait(), waiting for command to complete.
	 *
	 * man bash:
	 * "If bash is waiting for a command to complete and receives
	 * a signal for which a trap has been set, the trap
	 * will not be executed until the command completes."
	 *
	 * Reality is that even if trap is not set, bash
	 * will not act on the signal until command completes.
	 * Try this. sleep5intoff.c:
	 * #include <signal.h>
	 * #include <unistd.h>
	 * int main() {
	 *         sigset_t set;
	 *         sigemptyset(&set);
	 *         sigaddset(&set, SIGINT);
	 *         sigaddset(&set, SIGQUIT);
	 *         sigprocmask(SIG_BLOCK, &set, NULL);
	 *         sleep(5);
	 *         return 0;
	 * }
	 * $ bash -c './sleep5intoff; echo hi'
	 * ^C^C^C^C <--- pressing ^C once a second
	 * $ _
	 * $ bash -c './sleep5intoff; echo hi'
	 * ^\^\^\^\hi <--- pressing ^\ (SIGQUIT)
	 * $ _
	 */
	dowait(jp ? DOWAIT_BLOCK : DOWAIT_NONBLOCK, jp);
	if (!jp)
		return exitstatus;

	st = getstatus(jp);
#if JOBS
	if (jp->jobctl) {
		xtcsetpgrp(ttyfd, rootpid);
		restore_tty_if_stopped_or_signaled(jp);

		/*
		 * This is truly gross.
		 * If we're doing job control, then we did a TIOCSPGRP which
		 * caused us (the shell) to no longer be in the controlling
		 * session -- so we wouldn't have seen any ^C/SIGINT.  So, we
		 * intuit from the subprocess exit status whether a SIGINT
		 * occurred, and if so interrupt ourselves.  Yuck.  - mycroft
		 */
		if (jp->sigint) /* TODO: do the same with all signals */
			raise(SIGINT); /* ... by raise(jp->sig) instead? */
	}
	if (jp->state == JOBDONE)
#endif
		freejob(jp);
	return st;
}

/*
 * return 1 if there are stopped jobs, otherwise 0
 */
#if !ENABLE_PLATFORM_MINGW32
static int
stoppedjobs(void)
{
	struct job *jp;
	int retval;

	retval = 0;
	if (!iflag || job_warning)
		goto out;
	jp = curjob;
	if (jp && jp->state == JOBSTOPPED) {
		out2str("You have stopped jobs.\n");
		job_warning = 2;
		retval++;
	}
 out:
	return retval;
}
#else
static int
stoppedjobs(void)
{
	if (iflag && curjob) {
		out2str("You have background jobs.\n");
		return 1;
	}
	return 0;
}
#endif


/*
 * Code for dealing with input/output redirection.
 */

#undef EMPTY
#undef CLOSED
#define EMPTY -2                /* marks an unused slot in redirtab */
#define CLOSED -1               /* marks a slot of previously-closed fd */

/*
 * Handle here documents.  Normally we fork off a process to write the
 * data to a pipe.  If the document is short, we can stuff the data in
 * the pipe without forking.
 */
/* openhere needs this forward reference */
static void expandhere(union node *arg);
static int
openhere(union node *redir)
{
	char *p;
	int pip[2];
	size_t len = 0;
	IF_PLATFORM_MINGW32(struct forkshell fs);

	if (pipe(pip) < 0)
		ash_msg_and_raise_perror("can't create pipe");

	p = redir->nhere.doc->narg.text;
	if (redir->type == NXHERE) {
		expandhere(redir->nhere.doc);
		p = stackblock();
	}

	len = strlen(p);
	if (len <= PIPE_BUF) {
		xwrite(pip[1], p, len);
		goto out;
	}

#if ENABLE_PLATFORM_MINGW32
	memset(&fs, 0, sizeof(fs));
	fs.fpid = FS_OPENHERE;
	fs.fd[0] = pip[0];
	fs.fd[1] = pip[1];
	fs.path = p;
	spawn_forkshell(&fs, NULL, NULL, FORK_NOJOB);
#else
	if (forkshell((struct job *)NULL, (union node *)NULL, FORK_NOJOB) == 0) {
		/* child */
		close(pip[0]);
		ignoresig(SIGINT);  //signal(SIGINT, SIG_IGN);
		ignoresig(SIGQUIT); //signal(SIGQUIT, SIG_IGN);
		ignoresig(SIGHUP);  //signal(SIGHUP, SIG_IGN);
		ignoresig(SIGTSTP); //signal(SIGTSTP, SIG_IGN);
		signal(SIGPIPE, SIG_DFL);
		xwrite(pip[1], p, len);
		_exit(EXIT_SUCCESS);
	}
#endif
 out:
	close(pip[1]);
	return pip[0];
}

static int
openredirect(union node *redir)
{
	struct stat sb;
	char *fname;
	int f;

	switch (redir->nfile.type) {
/* Can't happen, our single caller does this itself */
//	case NTOFD:
//	case NFROMFD:
//		return -1;
	case NHERE:
	case NXHERE:
		return openhere(redir);
	}

	/* For N[X]HERE, reading redir->nfile.expfname would touch beyond
	 * allocated space. Do it only when we know it is safe.
	 */
	fname = redir->nfile.expfname;

	switch (redir->nfile.type) {
	default:
#if DEBUG
		abort();
#endif
	case NFROM:
		f = open(fname, O_RDONLY);
		if (f < 0)
			goto eopen;
		break;
	case NFROMTO:
		f = open(fname, O_RDWR|O_CREAT, 0666);
		if (f < 0)
			goto ecreate;
		break;
	case NTO:
#if BASH_REDIR_OUTPUT
	case NTO2:
#endif
		/* Take care of noclobber mode. */
		if (Cflag) {
			if (stat(fname, &sb) < 0) {
				f = open(fname, O_WRONLY|O_CREAT|O_EXCL, 0666);
				if (f < 0)
					goto ecreate;
			} else if (!S_ISREG(sb.st_mode)) {
				f = open(fname, O_WRONLY, 0666);
				if (f < 0)
					goto ecreate;
				if (!fstat(f, &sb) && S_ISREG(sb.st_mode)) {
					close(f);
					errno = EEXIST;
					goto ecreate;
				}
			} else {
				errno = EEXIST;
				goto ecreate;
			}
			break;
		}
		/* FALLTHROUGH */
	case NCLOBBER:
		f = open(fname, O_WRONLY|O_CREAT|O_TRUNC, 0666);
		if (f < 0)
			goto ecreate;
		break;
	case NAPPEND:
		f = open(fname, O_WRONLY|O_CREAT|O_APPEND, 0666);
		if (f < 0)
			goto ecreate;
#if ENABLE_PLATFORM_MINGW32
		lseek(f, 0, SEEK_END);
#endif
		break;
	}

	return f;
 ecreate:
	ash_msg_and_raise_error("can't create %s: %s", fname, errmsg(errno, "nonexistent directory"));
 eopen:
	ash_msg_and_raise_error("can't open %s: %s", fname, errmsg(errno, "no such file"));
}

/*
 * Copy a file descriptor to be >= 10. Throws exception on error.
 */
static int
savefd(int from)
{
	int newfd;
	int err;

	newfd = fcntl(from, F_DUPFD_CLOEXEC, 10);
	err = newfd < 0 ? errno : 0;
	if (err != EBADF) {
		if (err)
			ash_msg_and_raise_perror("%d", from);
		close(from);
		if (F_DUPFD_CLOEXEC == F_DUPFD)
			close_on_exec_on(newfd);
	}

	return newfd;
}
static int
dup2_or_raise(int from, int to)
{
	int newfd;

	newfd = (from != to) ? dup2(from, to) : to;
	if (newfd < 0) {
		/* Happens when source fd is not open: try "echo >&99" */
		ash_msg_and_raise_perror("%d", from);
	}
	return newfd;
}
static int
dup_CLOEXEC(int fd, int avoid_fd)
{
	int newfd;
 repeat:
	newfd = fcntl(fd, F_DUPFD_CLOEXEC, avoid_fd + 1);
	if (newfd >= 0) {
		if (F_DUPFD_CLOEXEC == F_DUPFD) /* if old libc (w/o F_DUPFD_CLOEXEC) */
			close_on_exec_on(newfd);
	} else { /* newfd < 0 */
		if (errno == EBUSY)
			goto repeat;
		if (errno == EINTR)
			goto repeat;
	}
	return newfd;
}
static int
xdup_CLOEXEC_and_close(int fd, int avoid_fd)
{
	int newfd;
 repeat:
	newfd = fcntl(fd, F_DUPFD_CLOEXEC, avoid_fd + 1);
	if (newfd < 0) {
		if (errno == EBUSY)
			goto repeat;
		if (errno == EINTR)
			goto repeat;
		/* fd was not open? */
		if (errno == EBADF)
			return fd;
		ash_msg_and_raise_perror("%d", newfd);
	}
	if (F_DUPFD_CLOEXEC == F_DUPFD)
		close_on_exec_on(newfd);
	close(fd);
	return newfd;
}

/* Struct def and variable are moved down to the first usage site */
struct squirrel {
	int orig_fd;
	int moved_to;
};
struct redirtab {
	struct redirtab *next;
	int pair_count;
	struct squirrel two_fd[];
};
#define redirlist (G_var.redirlist)

static void
add_squirrel_closed(struct redirtab *sq, int fd)
{
	int i;

	if (!sq)
		return;

	for (i = 0; sq->two_fd[i].orig_fd != EMPTY; i++) {
		/* If we collide with an already moved fd... */
		if (fd == sq->two_fd[i].orig_fd) {
			/* Examples:
			 * "echo 3>FILE 3>&- 3>FILE"
			 * "echo 3>&- 3>FILE"
			 * No need for last redirect to insert
			 * another "need to close 3" indicator.
			 */
			TRACE(("redirect_fd %d: already moved or closed\n", fd));
			return;
		}
	}
	TRACE(("redirect_fd %d: previous fd was closed\n", fd));
	sq->two_fd[i].orig_fd = fd;
	sq->two_fd[i].moved_to = CLOSED;
}

static int
save_fd_on_redirect(int fd, int avoid_fd, struct redirtab *sq)
{
	int i, new_fd;

	if (avoid_fd < 9) /* the important case here is that it can be -1 */
		avoid_fd = 9;

#if JOBS
	if (fd == ttyfd) {
		/* Testcase: "ls -l /proc/$$/fd 10>&-" should work */
		ttyfd = xdup_CLOEXEC_and_close(ttyfd, avoid_fd);
		TRACE(("redirect_fd %d: matches ttyfd, moving it to %d\n", fd, ttyfd));
		return 1; /* "we closed fd" */
	}
#endif
	/* Are we called from redirect(0)? E.g. redirect
	 * in a forked child. No need to save fds,
	 * we aren't going to use them anymore, ok to trash.
	 */
	if (!sq)
		return 0;

	/* If this one of script's fds? */
	if (fd != 0) {
		struct parsefile *pf = g_parsefile;
		while (pf) {
			/* We skip fd == 0 case because of the following:
			 * $ ash  # running ash interactively
			 * $ . ./script.sh
			 * and in script.sh: "exec 9>&0".
			 * Even though top-level pf_fd _is_ 0,
			 * it's still ok to use it: "read" builtin uses it,
			 * why should we cripple "exec" builtin?
			 */
			if (fd == pf->pf_fd) {
				pf->pf_fd = xdup_CLOEXEC_and_close(fd, avoid_fd);
				return 1; /* "we closed fd" */
			}
			pf = pf->prev;
		}
	}

	/* Check whether it collides with any open fds (e.g. stdio), save fds as needed */

	/* First: do we collide with some already moved fds? */
	for (i = 0; sq->two_fd[i].orig_fd != EMPTY; i++) {
		/* If we collide with an already moved fd... */
		if (fd == sq->two_fd[i].moved_to) {
			new_fd = dup_CLOEXEC(fd, avoid_fd);
			sq->two_fd[i].moved_to = new_fd;
			TRACE(("redirect_fd %d: already busy, moving to %d\n", fd, new_fd));
			if (new_fd < 0) /* what? */
				xfunc_die();
			return 0; /* "we did not close fd" */
		}
		if (fd == sq->two_fd[i].orig_fd) {
			/* Example: echo Hello >/dev/null 1>&2 */
			TRACE(("redirect_fd %d: already moved\n", fd));
			return 0; /* "we did not close fd" */
		}
	}

	/* If this fd is open, we move and remember it; if it's closed, new_fd = CLOSED (-1) */
	new_fd = dup_CLOEXEC(fd, avoid_fd);
	TRACE(("redirect_fd %d: previous fd is moved to %d (-1 if it was closed)\n", fd, new_fd));
	if (new_fd < 0) {
		if (errno != EBADF)
			xfunc_die();
		/* new_fd = CLOSED; - already is -1 */
	}
	sq->two_fd[i].moved_to = new_fd;
	sq->two_fd[i].orig_fd = fd;

	/* if we move stderr, let "set -x" code know */
	if (fd == preverrout_fd)
		preverrout_fd = new_fd;

	return 0; /* "we did not close fd" */
}

static int
internally_opened_fd(int fd, struct redirtab *sq)
{
	int i;
#if JOBS
	if (fd == ttyfd)
		return 1;
#endif
	/* If this one of script's fds? */
	if (fd != 0) {
		struct parsefile *pf = g_parsefile;
		while (pf) {
			if (fd == pf->pf_fd)
				return 1;
			pf = pf->prev;
		}
	}

	if (sq)	for (i = 0; i < sq->pair_count && sq->two_fd[i].orig_fd != EMPTY; i++) {
		if (fd == sq->two_fd[i].moved_to)
			return 1;
	}
	return 0;
}

/*
 * Process a list of redirection commands.  If the REDIR_PUSH flag is set,
 * old file descriptors are stashed away so that the redirection can be
 * undone by calling popredir.
 */
/* flags passed to redirect */
#define REDIR_PUSH    01        /* save previous values of file descriptors */
static void
redirect(union node *redir, int flags)
{
	struct redirtab *sv;

	if (!redir)
		return;

	sv = NULL;
	INT_OFF;
	if (flags & REDIR_PUSH)
		sv = redirlist;
	do {
		int fd;
		int newfd;
		int close_fd;
		int closed;

		fd = redir->nfile.fd;
		if (redir->nfile.type == NTOFD || redir->nfile.type == NFROMFD) {
			//bb_error_msg("doing %d > %d", fd, newfd);
			newfd = redir->ndup.dupfd;
			close_fd = -1;
		} else {
			newfd = openredirect(redir); /* always >= 0 */
			if (fd == newfd) {
				/* open() gave us precisely the fd we wanted.
				 * This means that this fd was not busy
				 * (not opened to anywhere).
				 * Remember to close it on restore:
				 */
				add_squirrel_closed(sv, fd);
				continue;
			}
			close_fd = newfd;
		}

		if (fd == newfd)
			continue;

		/* if "N>FILE": move newfd to fd */
		/* if "N>&M": dup newfd to fd */
		/* if "N>&-": close fd (newfd is -1) */

 IF_BASH_REDIR_OUTPUT(redirect_more:)

		closed = save_fd_on_redirect(fd, /*avoid:*/ newfd, sv);
		if (newfd == -1) {
			/* "N>&-" means "close me" */
			if (!closed) {
				/* ^^^ optimization: saving may already
				 * have closed it. If not... */
				close(fd);
			}
		} else {
			/* if newfd is a script fd or saved fd, simulate EBADF */
			if (internally_opened_fd(newfd, sv)) {
				errno = EBADF;
				ash_msg_and_raise_perror("%d", newfd);
			}
			dup2_or_raise(newfd, fd);
			if (close_fd >= 0) /* "N>FILE" or ">&FILE" or heredoc? */
				close(close_fd);
#if BASH_REDIR_OUTPUT
			if (redir->nfile.type == NTO2 && fd == 1) {
				/* ">&FILE". we already redirected to 1, now copy 1 to 2 */
				fd = 2;
				newfd = 1;
				close_fd = -1;
				goto redirect_more;
			}
#endif
		}
	} while ((redir = redir->nfile.next) != NULL);
	INT_ON;

//dash:#define REDIR_SAVEFD2 03        /* set preverrout */
#define REDIR_SAVEFD2 0
	// dash has a bug: since REDIR_SAVEFD2=3 and REDIR_PUSH=1, this test
	// triggers for pure REDIR_PUSH too. Thus, this is done almost always,
	// not only for calls with flags containing REDIR_SAVEFD2.
	// We do this unconditionally (see save_fd_on_redirect()).
	//if ((flags & REDIR_SAVEFD2) && copied_fd2 >= 0)
	//	preverrout_fd = copied_fd2;
}

static int
redirectsafe(union node *redir, int flags)
{
	int err;
	volatile int saveint;
	struct jmploc *volatile savehandler = exception_handler;
	struct jmploc jmploc;

	SAVE_INT(saveint);
	/* "echo 9>/dev/null; echo >&9; echo result: $?" - result should be 1, not 2! */
	err = setjmp(jmploc.loc); /* was = setjmp(jmploc.loc) * 2; */
	if (!err) {
		exception_handler = &jmploc;
		redirect(redir, flags);
	}
	exception_handler = savehandler;
	if (err && exception_type != EXERROR)
		longjmp(exception_handler->loc, 1);
	RESTORE_INT(saveint);
	return err;
}

#if BASH_PROCESS_SUBST
static void
pushfd(int fd)
{
	struct redirtab *sv;

	sv = ckzalloc(sizeof(*sv) + sizeof(sv->two_fd[0]));
	sv->pair_count = 1;
	sv->two_fd[0].orig_fd = fd;
	sv->two_fd[0].moved_to = CLOSED;
	sv->next = redirlist;
	redirlist = sv;
}
#endif

static struct redirtab*
pushredir(union node *redir)
{
	struct redirtab *sv;
	int i;

	if (!redir)
		return redirlist;

	i = 0;
	do {
		i++;
#if BASH_REDIR_OUTPUT
		if (redir->nfile.type == NTO2)
			i++;
#endif
		redir = redir->nfile.next;
	} while (redir);

	sv = ckzalloc(sizeof(*sv) + i * sizeof(sv->two_fd[0]));
	sv->pair_count = i;
	while (--i >= 0)
		sv->two_fd[i].orig_fd = sv->two_fd[i].moved_to = EMPTY;
	sv->next = redirlist;
	redirlist = sv;
	return sv->next;
}

/*
 * Undo the effects of the last redirection.
 */
static void
popredir(int drop)
{
	struct redirtab *rp;
	int i;

	if (redirlist == NULL)
		return;
	INT_OFF;
	rp = redirlist;
	for (i = 0; i < rp->pair_count; i++) {
		int fd = rp->two_fd[i].orig_fd;
		int copy = rp->two_fd[i].moved_to;
		if (copy == CLOSED) {
			if (!drop)
				close(fd);
			continue;
		}
		if (copy != EMPTY) {
			if (!drop) {
				/*close(fd);*/
				dup2_or_raise(copy, fd);
			}
			close(copy);
		}
	}
	redirlist = rp->next;
	free(rp);
	INT_ON;
}

static void
unwindredir(struct redirtab *stop)
{
	while (redirlist != stop)
		popredir(/*drop:*/ 0);
}


/* ============ Routines to expand arguments to commands
 *
 * We have to deal with backquotes, shell variables, and file metacharacters.
 */

#if ENABLE_FEATURE_SH_MATH
static arith_t
ash_arith(const char *s)
{
	arith_state_t math_state;
	arith_t result;

	math_state.lookupvar = lookupvar;
	math_state.setvar    = setvar0;
	//math_state.endofname = endofname;

	INT_OFF;
	result = arith(&math_state, s);
	if (math_state.errmsg)
		ash_msg_and_raise_error(math_state.errmsg);
	INT_ON;

	return result;
}
#endif
#if BASH_SUBSTR
# if ENABLE_FEATURE_SH_MATH
static int substr_atoi(const char *s)
{
	arith_t t = ash_arith(s);
	if (sizeof(t) > sizeof(int)) {
		/* clamp very large or very large negative nums for ${v:N:M}:
		 * else "${v:0:0x100000001}" would work as "${v:0:1}"
		 */
		if (t > INT_MAX)
			t = INT_MAX;
		if (t < INT_MIN)
			t = INT_MIN;
	}
	return t;
}
# else
#  define substr_atoi(s) number(s)
# endif
#endif

/*
 * expandarg flags
 */
#define EXP_FULL        0x1     /* perform word splitting & file globbing */
#define EXP_TILDE       0x2     /* do normal tilde expansion */
#define EXP_VARTILDE    0x4     /* expand tildes in an assignment */
#define EXP_REDIR       0x8     /* file glob for a redirection (1 match only) */
/* ^^^^^^^^^^^^^^ this is meant to support constructs such as "cmd >file*.txt"
 * POSIX says for this case:
 *  Pathname expansion shall not be performed on the word by a
 *  non-interactive shell; an interactive shell may perform it, but shall
 *  do so only when the expansion would result in one word.
 * Currently, our code complies to the above rule by never globbing
 * redirection filenames.
 * Bash performs globbing, unless it is non-interactive and in POSIX mode.
 * (this means that on a typical Linux distro, bash almost always
 * performs globbing, and thus diverges from what we do).
 */
#define EXP_CASE        0x10    /* keeps quotes around for CASE pattern */
#define EXP_VARTILDE2   0x20    /* expand tildes after colons only */
#define EXP_WORD        0x40    /* expand word in parameter expansion */
#define EXP_QUOTED      0x100   /* expand word in double quotes */
#define EXP_KEEPNUL     0x200   /* do not skip NUL characters */
#define EXP_DISCARD     0x400   /* discard result of expansion */

/*
 * rmescape() flags
 */
#define RMESCAPE_ALLOC  0x1     /* Allocate a new string */
#define RMESCAPE_GLOB   0x2     /* Add backslashes for glob */
#define RMESCAPE_GROW   0x8     /* Grow strings instead of stalloc */
#define RMESCAPE_HEAP   0x10    /* Malloc strings instead of stalloc */

/* Add CTLESC when necessary. */
#define QUOTES_ESC     (EXP_FULL | EXP_CASE)

/*
 * Structure specifying which parts of the string should be searched
 * for IFS characters.
 */
struct ifsregion {
	struct ifsregion *next; /* next region in list */
	int begoff;             /* offset of start of region */
	int endoff;             /* offset of end of region */
	int nulonly;            /* search for nul bytes only */
};

struct arglist {
	struct strlist *list;
	struct strlist **lastp;
};

/* output of current string */
static char *expdest;
/* list of back quote expressions */
static struct nodelist *argbackq;
/* first struct in list of ifs regions */
static struct ifsregion ifsfirst;
/* last struct in list */
static struct ifsregion *ifslastp;
/* holds expanded arg list */
static struct arglist exparg;

/*
 * Break the argument string into pieces based upon IFS and add the
 * strings to the argument list.  The regions of the string to be
 * searched for IFS characters have been stored by recordregion.
 */
static void
ifsbreakup(char *string, struct arglist *arglist)
{
	struct ifsregion *ifsp;
	struct strlist *sp;
	char *start;
	char *p;
	char *q;
	const char *ifs, *realifs;
	int ifsspc;
	int nulonly;

	start = string;
	if (ifslastp != NULL) {
		ifsspc = 0;
		nulonly = 0;
		realifs = ifsset() ? ifsval() : defifs;
		ifsp = &ifsfirst;
		do {
			int afternul;

			p = string + ifsp->begoff;
			afternul = nulonly;
			nulonly = ifsp->nulonly;
			ifs = nulonly ? nullstr : realifs;
			ifsspc = 0;
			while (p < string + ifsp->endoff) {
				q = p;
				if ((unsigned char)*p == CTLESC)
					p++;
				if (!strchr(ifs, *p)) {
					p++;
					continue;
				}
				if (!(afternul || nulonly))
					ifsspc = (strchr(defifs, *p) != NULL);
				/* Ignore IFS whitespace at start */
				if (q == start && ifsspc) {
					p++;
					start = p;
					continue;
				}
				*q = '\0';
				sp = stzalloc(sizeof(*sp));
				sp->text = start;
				*arglist->lastp = sp;
				arglist->lastp = &sp->next;
				p++;
				if (!nulonly) {
					for (;;) {
						if (p >= string + ifsp->endoff) {
							break;
						}
						q = p;
						if ((unsigned char)*p == CTLESC)
							p++;
						if (strchr(ifs, *p) == NULL) {
							p = q;
							break;
						}
						if (strchr(defifs, *p) == NULL) {
							if (ifsspc) {
								p++;
								ifsspc = 0;
							} else {
								p = q;
								break;
							}
						} else
							p++;
					}
				}
				start = p;
			} /* while */
			ifsp = ifsp->next;
		} while (ifsp != NULL);
		if (nulonly)
			goto add;
	}

	if (!*start)
		return;

 add:
	sp = stzalloc(sizeof(*sp));
	sp->text = start;
	*arglist->lastp = sp;
	arglist->lastp = &sp->next;
}

static void
ifsfree(void)
{
	struct ifsregion *p = ifsfirst.next;

	if (!p)
		goto out;

	INT_OFF;
	do {
		struct ifsregion *ifsp;
		ifsp = p->next;
		free(p);
		p = ifsp;
	} while (p);
	ifsfirst.next = NULL;
	INT_ON;
 out:
	ifslastp = NULL;
}

static size_t
esclen(const char *start, const char *p)
{
	size_t esc = 0;

	while (p > start && (unsigned char)*--p == CTLESC) {
		esc++;
	}
	return esc;
}

/*
 * Remove any CTLESC characters from a string.
 */
#if !BASH_PATTERN_SUBST
#define rmescapes(str, flag, slash_position) \
	rmescapes(str, flag)
#endif
static char *
rmescapes(char *str, int flag, int *slash_position)
{
	static const char qchars[] ALIGN1 = {
		IF_BASH_PATTERN_SUBST('/',) CTLESC, CTLQUOTEMARK, '\0' };

	char *p, *q, *r;
	unsigned protect_against_glob;
	unsigned globbing;

	p = strpbrk(str, qchars IF_BASH_PATTERN_SUBST(+ !slash_position));
	if (!p)
		return str;

	q = p;
	r = str;
	if (flag & RMESCAPE_ALLOC) {
		size_t len = p - str;
		size_t fulllen = len + strlen(p) + 1;

		if (flag & RMESCAPE_GROW) {
			int strloc = str - (char *)stackblock();
			r = makestrspace(fulllen, expdest);
			/* p and str may be invalidated by makestrspace */
			str = (char *)stackblock() + strloc;
			p = str + len;
		} else if (flag & RMESCAPE_HEAP) {
			r = ckmalloc(fulllen);
		} else {
			r = stalloc(fulllen);
		}
		q = r;
		if (len > 0) {
			q = (char *)mempcpy(q, str, len);
		}
	}

	globbing = flag & RMESCAPE_GLOB;
	protect_against_glob = globbing;
	while (*p) {
		if ((unsigned char)*p == CTLQUOTEMARK) {
// Note: protect_against_glob only affect whether
// CTLESC,<ch> gets converted to <ch> or to \<ch>
			p++;
			protect_against_glob = globbing;
			continue;
		}
		if (*p == '\\') {
			/* naked back slash */
			protect_against_glob = 0;
			goto copy;
		}
		if ((unsigned char)*p == CTLESC) {
			p++;
#if DEBUG
			if (*p == '\0')
				ash_msg_and_raise_error("CTLESC at EOL (shouldn't happen)");
#endif
			if (protect_against_glob) {
				/*
				 * We used to trust glob() and fnmatch() to eat
				 * superfluous escapes (\z where z has no
				 * special meaning anyway). But this causes
				 * bugs such as string of one greek letter rho
				 * (unicode-encoded as two bytes "cf,81")
				 * getting encoded as "cf,CTLESC,81"
				 * and here, converted to "cf,\,81" -
				 * which does not go well with some flavors
				 * of fnmatch() in unicode locales
				 * (for example, glibc <= 2.22).
				 *
				 * Lets add "\" only on the chars which need it.
				 * Testcases for less obvious chars are shown.
				 */
				if (*p == '*'
				 || *p == '?'
				 || *p == '['
				 || *p == '\\' /* case '\' in \\    ) echo ok;; *) echo WRONG;; esac */
				 || *p == ']'  /* case ']' in [a\]] ) echo ok;; *) echo WRONG;; esac */
				 || *p == '-'  /* case '-' in [a\-c]) echo ok;; *) echo WRONG;; esac */
				 || *p == '!'  /* case '!' in [\!]  ) echo ok;; *) echo WRONG;; esac */
				/* Some libc support [^negate], that's why "^" also needs love */
				 || *p == '^'  /* case '^' in [\^]  ) echo ok;; *) echo WRONG;; esac */
				) {
					*q++ = '\\';
				}
			}
		}
#if BASH_PATTERN_SUBST
		else if (slash_position && p == str + *slash_position) {
			/* stop handling globbing */
			globbing = 0;
			*slash_position = q - r;
			slash_position = NULL;
		}
#endif
		protect_against_glob = globbing;
 copy:
		*q++ = *p++;
	}
	*q = '\0';
	if (flag & RMESCAPE_GROW) {
		expdest = r;
		STADJUST(q - r + 1, expdest);
	}
	return r;
}
#define pmatch(a, b) !fnmatch((a), (b), 0)

/*
 * Prepare a pattern for a expmeta (internal glob(3)) call.
 *
 * Returns an stalloced string.
 */
static char *
preglob(const char *pattern, int flag)
{
	return rmescapes((char *)pattern, flag | RMESCAPE_GLOB, NULL);
}

/*
 * Put a string on the stack.
 */
static size_t
memtodest(const char *p, size_t len, int flags)
{
	int syntax = flags & EXP_QUOTED ? DQSYNTAX : BASESYNTAX;
	char *q;
	char *s;

	if (!len)
		return 0;

	q = makestrspace(len * 2, expdest);
	s = q;

	do {
		unsigned char c = *p++;
		if (c) {
			if (flags & QUOTES_ESC) {
				int n = SIT(c, syntax);
				if (n == CCTL
				 || ((flags & EXP_QUOTED) && n == CBACK)
				) {
					USTPUTC(CTLESC, q);
				}
			}
		} else if (!(flags & EXP_KEEPNUL))
			continue;
		USTPUTC(c, q);
	} while (--len);

	expdest = q;
	return q - s;
}

static size_t
strtodest(const char *p, int flags)
{
	size_t len = strlen(p);
	memtodest(p, len, flags);
	return len;
}

/*
 * Our own itoa().
 * cvtnum() is used even if math support is off (to prepare $? values and such).
 */
static int
cvtnum(arith_t num, int flags)
{
	/* 32-bit and wider ints require buffer size of bytes*3 (or less) */
	/* If narrower: worst case, 1-byte ints: need 5 bytes: "-127<NUL>" */
	int len = (sizeof(arith_t) >= 4) ? sizeof(arith_t) * 3 : sizeof(arith_t) * 3 + 2;
	char buf[len];

	len = fmtstr(buf, len, ARITH_FMT, num);
	return memtodest(buf, len, flags);
}

/*
 * Record the fact that we have to scan this region of the
 * string for IFS characters.
 */
static void
recordregion(int start, int end, int nulonly)
{
	struct ifsregion *ifsp;

	if (ifslastp == NULL) {
		ifsp = &ifsfirst;
	} else {
		INT_OFF;
		ifsp = ckzalloc(sizeof(*ifsp));
		/*ifsp->next = NULL; - ckzalloc did it */
		ifslastp->next = ifsp;
		INT_ON;
	}
	ifslastp = ifsp;
	ifslastp->begoff = start;
	ifslastp->endoff = end;
	ifslastp->nulonly = nulonly;
}

static void
removerecordregions(int endoff)
{
	if (ifslastp == NULL)
		return;

	if (ifsfirst.endoff > endoff) {
		while (ifsfirst.next) {
			struct ifsregion *ifsp;
			INT_OFF;
			ifsp = ifsfirst.next->next;
			free(ifsfirst.next);
			ifsfirst.next = ifsp;
			INT_ON;
		}
		if (ifsfirst.begoff > endoff) {
			ifslastp = NULL;
		} else {
			ifslastp = &ifsfirst;
			ifsfirst.endoff = endoff;
		}
		return;
	}

	ifslastp = &ifsfirst;
	while (ifslastp->next && ifslastp->next->begoff < endoff)
		ifslastp = ifslastp->next;
	while (ifslastp->next) {
		struct ifsregion *ifsp;
		INT_OFF;
		ifsp = ifslastp->next->next;
		free(ifslastp->next);
		ifslastp->next = ifsp;
		INT_ON;
	}
	if (ifslastp->endoff > endoff)
		ifslastp->endoff = endoff;
}

static char *
exptilde(char *startp, int flag)
{
	unsigned char c;
	char *name;
	struct passwd *pw;
	const char *home;
	char *p;

	p = startp;
	name = p + 1;

	while ((c = *++p) != '\0') {
		switch (c) {
		case CTLESC:
			return startp;
		case CTLQUOTEMARK:
			return startp;
		case ':':
			if (flag & EXP_VARTILDE)
				goto done;
			break;
		case '/':
		case CTLENDVAR:
			goto done;
		}
	}
 done:
	if (flag & EXP_DISCARD)
		goto out;
	*p = '\0';
	if (*name == '\0') {
		home = lookupvar("HOME");
	} else {
		pw = getpwnam(name);
		if (pw == NULL)
			goto lose;
		home = pw->pw_dir;
	}
	*p = c;
	if (!home)
		goto lose;
	strtodest(home, flag | EXP_QUOTED);
 out:
	return p;
 lose:
	return startp;
}

/*
 * Execute a command inside back quotes.  If it's a builtin command, we
 * want to save its output in a block obtained from malloc.  Otherwise
 * we fork off a subprocess and get the output of the command via a pipe.
 * Should be called with interrupts off.
 */
struct backcmd {                /* result of evalbackcmd */
	int fd;                 /* file descriptor to read from */
	int nleft;              /* number of chars in buffer */
	char *buf;              /* buffer */
	struct job *jp;         /* job structure for command */
};

/* These forward decls are needed to use "eval" code for backticks handling: */
/* flags in argument to evaltree */
#define EV_EXIT    01           /* exit after evaluating tree */
#define EV_TESTED  02           /* exit status is checked; ignore -e flag */
static int evaltree(union node *, int);

/* An evaltree() which is known to never return.
 * Used to use an alias:
 * static int evaltreenr(union node *, int) __attribute__((alias("evaltree"),__noreturn__));
 * but clang was reported to "transfer" noreturn-ness to evaltree() as well.
 */
static ALWAYS_INLINE NORETURN void
evaltreenr(union node *n, int flags)
{
	evaltree(n, flags);
	bb_unreachable(abort());
	/* NOTREACHED */
}

static void FAST_FUNC
evalbackcmd(union node *n, struct backcmd *result
				IF_BASH_PROCESS_SUBST(, int ctl))
{
	int pip[2];
	struct job *jp;
#if BASH_PROCESS_SUBST
	/* determine end of pipe used by parent (ip) and child (ic) */
	const int ip = (ctl == CTLTOPROC);
	const int ic = !(ctl == CTLTOPROC);
#else
	const int ctl = CTLBACKQ;
	const int ip = 0;
	const int ic = 1;
#endif
	IF_PLATFORM_MINGW32(struct forkshell fs);

	result->fd = -1;
	result->buf = NULL;
	result->nleft = 0;
	result->jp = NULL;
	if (n == NULL) {
		goto out;
	}

	if (pipe(pip) < 0)
		ash_msg_and_raise_perror("can't create pipe");
	/* process substitution uses NULL job/node, like openhere() */
	jp = (ctl == CTLBACKQ) ? makejob(/*n,*/ 1) : NULL;
#if ENABLE_PLATFORM_MINGW32
	memset(&fs, 0, sizeof(fs));
	fs.fpid = FS_EVALBACKCMD;
	fs.n = n;
	fs.fd[0] = pip[0];
	fs.fd[1] = pip[1];
	fs.fd[2] = ctl;
	spawn_forkshell(&fs, jp, (ctl == CTLBACKQ) ? n : NULL, FORK_NOJOB);
#else
	if (forkshell(jp, (ctl == CTLBACKQ) ? n : NULL, FORK_NOJOB) == 0) {
		/* child */
		FORCE_INT_ON;
		close(pip[ip]);
		/* ic is index of child end of pipe *and* fd to connect it to */
		if (pip[ic] != ic) {
			/*close(ic);*/
			dup2_or_raise(pip[ic], ic);
			close(pip[ic]);
		}
/* TODO: eflag clearing makes the following not abort:
 *  ash -c 'set -e; z=$(false;echo foo); echo $z'
 * which is what bash does (unless it is in POSIX mode).
 * dash deleted "eflag = 0" line in the commit
 *  Date: Mon, 28 Jun 2010 17:11:58 +1000
 *  [EVAL] Don't clear eflag in evalbackcmd
 * For now, preserve bash-like behavior, it seems to be somewhat more useful:
 */
		eflag = 0;
		ifsfree();
		evaltreenr(n, EV_EXIT);
		/* NOTREACHED */
	}
#endif
	/* parent */
#if BASH_PROCESS_SUBST
	if (ctl != CTLBACKQ) {
		int fd = fcntl(pip[ip], F_DUPFD, 64);
		if (fd > 0) {
			close(pip[ip]);
			pip[ip] = fd;
		}
		pushfd(pip[ip]);
	}
#endif
	close(pip[ic]);
	result->fd = pip[ip];
	result->jp = jp;

 out:
	TRACE(("evalbackcmd done: fd=%d buf=0x%x nleft=%d jp=0x%x\n",
		result->fd, result->buf, result->nleft, result->jp));
}

/*
 * Expand stuff in backwards quotes.
 */
static void
expbackq(union node *cmd, int flag IF_BASH_PROCESS_SUBST(, int ctl))
{
#if !BASH_PROCESS_SUBST
	const int ctl = CTLBACKQ;
#endif
	struct backcmd in;
	int i;
	char buf[128];
	char *p;
	char *dest;
	int startloc;
	struct stackmark smark;

	if (flag & EXP_DISCARD)
		goto out;

	INT_OFF;
	startloc = expdest - (char *)stackblock();
	pushstackmark(&smark, startloc);
	evalbackcmd(cmd, &in IF_BASH_PROCESS_SUBST(, ctl));
	popstackmark(&smark);

	if (ctl != CTLBACKQ) {
		sprintf(buf, DEV_FD_PREFIX"%d", in.fd);
		strtodest(buf, BASESYNTAX);
		goto done;
	}

	p = in.buf;
	i = in.nleft;
	if (i == 0)
		goto read;
	for (;;) {
		memtodest(p, i, flag);
 read:
		if (in.fd < 0)
			break;
		i = nonblock_immune_read(in.fd, buf, sizeof(buf));
		TRACE(("expbackq: read returns %d\n", i));
		if (i <= 0)
			break;
		p = buf;
	}

	free(in.buf);
	if (in.fd >= 0) {
		close(in.fd);
		back_exitstatus = waitforjob(in.jp);
	}
 done:
	INT_ON;

	/* Eat all trailing newlines */
	dest = expdest;
	for (; dest > ((char *)stackblock() + startloc) && (dest[-1] == '\n'
				IF_PLATFORM_MINGW32(|| dest[-1] == '\r'));)
		STUNPUTC(dest);
	expdest = dest;

	if (!(flag & EXP_QUOTED))
		recordregion(startloc, dest - (char *)stackblock(), 0);
	TRACE(("evalbackq: size:%d:'%.*s'\n",
		(int)((dest - (char *)stackblock()) - startloc),
		(int)((dest - (char *)stackblock()) - startloc),
		stackblock() + startloc));

 out:
	argbackq = argbackq->next;
}

/* expari needs it */
static char *argstr(char *p, int flag);

#if ENABLE_FEATURE_SH_MATH
/*
 * Expand arithmetic expression.  Backup to start of expression,
 * evaluate, place result in (backed up) result, adjust string position.
 */
static char *
expari(char *start, int flag)
{
	struct stackmark sm;
	int begoff;
	int endoff;
	int len;
	arith_t result;
	char *p;

	p = stackblock();
	begoff = expdest - p;
	p = argstr(start, flag & EXP_DISCARD);

	if (flag & EXP_DISCARD)
		goto out;

	start = stackblock();
	endoff = expdest - start;
	start += begoff;
	STADJUST(start - expdest, expdest);

	removerecordregions(begoff);

	if (flag & QUOTES_ESC)
		rmescapes(start, 0, NULL);

	pushstackmark(&sm, endoff);
	result = ash_arith(start);
	popstackmark(&sm);

	len = cvtnum(result, flag);

	if (!(flag & EXP_QUOTED))
		recordregion(begoff, begoff + len, 0);

 out:
	return p;
}
#endif

/* argstr needs it */
static char *evalvar(char *p, int flags);

/*
 * Perform variable and command substitution.  If EXP_FULL is set, output CTLESC
 * characters to allow for further processing.  Otherwise treat
 * $@ like $* since no splitting will be performed.
 */
static char *
argstr(char *p, int flag)
{
	static const char spclchars[] ALIGN1 = {
		'=',
		':',
		CTLQUOTEMARK,
		CTLENDVAR,
		CTLESC,
		CTLVAR,
		CTLBACKQ,
#if BASH_PROCESS_SUBST
		CTLTOPROC,
		CTLFROMPROC,
#endif
#if ENABLE_FEATURE_SH_MATH
		CTLARI,
		CTLENDARI,
#endif
		'\0'
	};
	const char *reject = spclchars;
	int breakall = (flag & (EXP_WORD | EXP_QUOTED)) == EXP_WORD;
	int inquotes;
	size_t length;
	int startloc;

	reject += !!(flag & EXP_VARTILDE2);
	reject += flag & EXP_VARTILDE ? 0 : 2;
	inquotes = 0;
	length = 0;
	if (flag & EXP_TILDE) {
		flag &= ~EXP_TILDE;
 tilde:
		if (*p == '~')
			p = exptilde(p, flag);
	}
 start:
	startloc = expdest - (char *)stackblock();
	for (;;) {
		int end;
		unsigned char c;

		length += strcspn(p + length, reject);
		end = 0;
		c = p[length];
		if (!(c & 0x80)
		 IF_FEATURE_SH_MATH(|| c == CTLENDARI)
		 || c == CTLENDVAR
		) {
			/*
			 * c == '=' || c == ':' || c == '\0' ||
			 * c == CTLENDARI || c == CTLENDVAR
			 */
			length++;
			/* c == '\0' || c == CTLENDARI || c == CTLENDVAR */
			end = !!((c - 1) & 0x80);
		}
		if (length > 0 && !(flag & EXP_DISCARD)) {
			int newloc;
			char *q;

			q = stnputs(p, length, expdest);
			q[-1] &= end - 1;
			expdest = q - (flag & EXP_WORD ? end : 0);
			newloc = q - (char *)stackblock() - end;
			if (breakall && !inquotes && newloc > startloc) {
				recordregion(startloc, newloc, 0);
			}
			startloc = newloc;
		}
		p += length + 1;
		length = 0;

		if (end)
			break;

		switch (c) {
		case '=':
			flag |= EXP_VARTILDE2;
			reject++;
			/* fall through */
		case ':':
			/*
			 * sort of a hack - expand tildes in variable
			 * assignments (after the first '=' and after ':'s).
			 */
			if (*--p == '~') {
				goto tilde;
			}
			continue;
		case CTLQUOTEMARK:
			/* "$@" syntax adherence hack */
			if (!inquotes && !memcmp(p, dolatstr + 1, DOLATSTRLEN - 1)) {
				p = evalvar(p + 1, flag | EXP_QUOTED) + 1;
				goto start;
			}
			inquotes ^= EXP_QUOTED;
 addquote:
			if (flag & QUOTES_ESC) {
				p--;
				length++;
				startloc++;
			}
			break;
		case CTLESC:
			startloc++;
			length++;
			goto addquote;
		case CTLVAR:
			TRACE(("argstr: evalvar('%s')\n", p));
			p = evalvar(p, flag | inquotes);
			TRACE(("argstr: evalvar:'%s'\n", (char *)stackblock()));
			goto start;
#if BASH_PROCESS_SUBST
		case CTLTOPROC:
		case CTLFROMPROC:
#endif
		case CTLBACKQ:
			expbackq(argbackq->n, flag | inquotes IF_BASH_PROCESS_SUBST(, c));
			goto start;
#if ENABLE_FEATURE_SH_MATH
		case CTLARI:
			p = expari(p, flag | inquotes);
			goto start;
#endif
		}
	}
	return p - 1;
}

static char *
scanleft(char *startp, char *rmesc, char *rmescend UNUSED_PARAM,
		char *pattern, int quotes, int zero)
{
	char *loc, *loc2;
	char c;

	loc = startp;
	loc2 = rmesc;
	do {
		int match;
		const char *s = loc2;

		c = *loc2;
		if (zero) {
			*loc2 = '\0';
			s = rmesc;
		}
		match = pmatch(pattern, s);

		*loc2 = c;
		if (match)
			return loc;
		if (quotes && (unsigned char)*loc == CTLESC)
			loc++;
		loc++;
		loc2++;
	} while (c);
	return NULL;
}

static char *
scanright(char *startp, char *rmesc, char *rmescend,
		char *pattern, int quotes, int match_at_start)
{
#if !ENABLE_ASH_OPTIMIZE_FOR_SIZE
	int try2optimize = match_at_start;
#endif
	int esc = 0;
	char *loc;
	char *loc2;

	/* If we called by "${v/pattern/repl}" or "${v//pattern/repl}":
	 * startp="escaped_value_of_v" rmesc="raw_value_of_v"
	 * rmescend=""(ptr to NUL in rmesc) pattern="pattern" quotes=match_at_start=1
	 * Logic:
	 * loc starts at NUL at the end of startp, loc2 starts at the end of rmesc,
	 * and on each iteration they go back two/one char until they reach the beginning.
	 * We try to match "raw_value_of_v", "raw_value_of_", "raw_value_of" etc.
	 * If one of these matches, return pointer past last matched char in startp.
	 */
	/* TODO: document in what other circumstances we are called. */

	for (loc = pattern - 1, loc2 = rmescend; loc >= startp; loc2--) {
		int match;
		char c = *loc2;
		const char *s = loc2;
		if (match_at_start) {
			*loc2 = '\0';
			s = rmesc;
		}
		match = pmatch(pattern, s);
		//bb_error_msg("pmatch(pattern:'%s',s:'%s'):%d", pattern, s, match);
		*loc2 = c;
		if (match)
			return loc;
#if !ENABLE_ASH_OPTIMIZE_FOR_SIZE
		if (try2optimize) {
			/* Maybe we can optimize this:
			 * if pattern ends with unescaped *, we can avoid checking
			 * shorter strings: if "foo*" doesn't match "raw_value_of_v",
			 * it won't match truncated "raw_value_of_" strings too.
			 */
			unsigned plen = strlen(pattern);
			/* Does it end with "*"? */
			if (plen != 0 && pattern[--plen] == '*') {
				/* "xxxx*" is not escaped */
				/* "xxx\*" is escaped */
				/* "xx\\*" is not escaped */
				/* "x\\\*" is escaped */
				int slashes = 0;
				while (plen != 0 && pattern[--plen] == '\\')
					slashes++;
				if (!(slashes & 1))
					break; /* ends with unescaped "*" */
			}
			try2optimize = 0;
		}
#endif
		loc--;
		if (quotes) {
			if (--esc < 0) {
				esc = esclen(startp, loc);
			}
			if (esc % 2) {
				esc--;
				loc--;
			}
		}
	}
	return NULL;
}

static void varunset(const char *, const char *, const char *, int) NORETURN;
static void
varunset(const char *end, const char *var, const char *umsg, int varflags)
{
	const char *msg;
	const char *tail;

	tail = nullstr;
	msg = "parameter not set";
	if (umsg) {
		if ((unsigned char)*end == CTLENDVAR) {
			if (varflags & VSNUL)
				tail = " or null";
		} else {
			msg = umsg;
		}
	}
	ash_msg_and_raise_error("%.*s: %s%s", (int)(end - var - 1), var, msg, tail);
}

static char *
subevalvar(char *start, char *str, int strloc,
		int startloc, int varflags, int flag)
{
	int subtype = varflags & VSTYPE;
	int quotes = flag & QUOTES_ESC;
	char *startp;
	char *loc;
	char *rmesc, *rmescend;
	long amount;
	int resetloc;
	int argstr_flags;
	IF_BASH_PATTERN_SUBST(int workloc;)
	IF_BASH_PATTERN_SUBST(int slash_pos;)
	IF_BASH_PATTERN_SUBST(char *repl;)
	int zero;
	char *(*scan)(char*, char*, char*, char*, int, int);
	char *p;

	//bb_error_msg("subevalvar(start:'%s',str:'%s',strloc:%d,startloc:%d,varflags:%x,quotes:%d)",
	//		start, str, strloc, startloc, varflags, quotes);

#if BASH_PATTERN_SUBST
	/* For "${v/pattern/repl}", we must find the delimiter _before_
	 * argstr() call expands possible variable references in pattern:
	 * think about "v=a; a=a/; echo ${v/$a/r}" case.
	 */
	repl = NULL;
	if (subtype == VSREPLACE || subtype == VSREPLACEALL) {
		/* Find '/' and replace with NUL */
		repl = start;
		/* The pattern can't be empty.
		 * IOW: if the first char after "${v//" is a slash,
		 * it does not terminate the pattern - it's the first char of the pattern:
		 *  v=/dev/ram; echo ${v////-}  prints -dev-ram (pattern is "/")
		 *  v=/dev/ram; echo ${v///r/-} prints /dev-am  (pattern is "/r")
		 */
		if (*repl == '/')
			repl++;
		for (;;) {
			if (*repl == '\0') {
				repl = NULL;
				break;
			}
			if (*repl == '/') {
				*repl = '\0';
				break;
			}
			/* Handle escaped slashes, e.g. "${v/\//_}" (they are CTLESC'ed by this point) */
			if ((unsigned char)*repl == CTLESC && repl[1])
				repl++;
			repl++;
		}
	}
#endif
	argstr_flags = (flag & EXP_DISCARD) | EXP_TILDE;
	if (!str
#if BASH_SUBSTR
	 && subtype != VSSUBSTR
#endif
	) {
		/* EXP_CASE keeps CTLESC's */
		argstr_flags |= EXP_CASE;
	}
	p = argstr(start, argstr_flags);

	//bb_error_msg("str0:'%s'", (char *)stackblock() + strloc);
#if BASH_PATTERN_SUBST
	slash_pos = -1;
	if (repl) {
		slash_pos = expdest - ((char *)stackblock() + strloc);
		if (!(flag & EXP_DISCARD))
			STPUTC('/', expdest);
		//bb_error_msg("repl+1:'%s'", repl + 1);
		p = argstr(repl + 1, (flag & EXP_DISCARD) | EXP_TILDE); /* EXP_TILDE: echo "${v/x/~}" expands ~ ! */
		*repl = '/';
	}
#endif
	if (flag & EXP_DISCARD)
		return p;

	startp = (char *)stackblock() + startloc;
	//bb_error_msg("str1:'%s'", (char *)stackblock() + strloc);

	switch (subtype) {
	case VSASSIGN:
		setvar0(str, startp);

		loc = startp;
		goto out;

	case VSQUESTION:
		varunset(start, str, startp, varflags);
		/* NOTREACHED */

#if BASH_SUBSTR
	case VSSUBSTR: {
		int pos, len, orig_len;
		char *colon;
		char *vstr;

		loc = vstr = stackblock() + strloc;

		/* Read POS in ${var:POS:LEN} */
		colon = strchr(loc, ':');
		if (colon) *colon = '\0';
		pos = substr_atoi(loc);
		if (colon) *colon = ':';

		/* Read LEN in ${var:POS:LEN} */
		len = vstr - startp - 1;
		/* *loc != '\0', guaranteed by parser */
		if (quotes) {
			char *ptr;
			/* Adjust the length by the number of escapes */
			for (ptr = startp; ptr < (vstr - 1); ptr++) {
				if ((unsigned char)*ptr == CTLESC) {
					len--;
					ptr++;
				}
			}
		}
		orig_len = len;
		if (*loc++ == ':') {
			/* ${var::LEN} */
			len = substr_atoi(loc);
		} else {
			/* Skip POS in ${var:POS:LEN} */
			len = orig_len;
			while (*loc && *loc != ':')
				loc++;
			if (*loc++ == ':')
				len = substr_atoi(loc);
		}
		if (pos < 0) {
			/* ${VAR:$((-n)):l} starts n chars from the end */
			pos = orig_len + pos;
		}
		if ((unsigned)pos >= orig_len) {
			/* apart from obvious ${VAR:999999:l},
			 * covers ${VAR:$((-9999999)):l} - result is ""
			 * (bash compat)
			 */
			pos = 0;
			len = 0;
		}
		if (len < 0) {
			/* ${VAR:N:-M} sets LEN to strlen()-M */
			len = (orig_len - pos) + len;
		}
		if ((unsigned)len > (orig_len - pos))
			len = orig_len - pos;

		if (!quotes) {
			loc = mempcpy(startp, startp + pos, len);
		} else {
			for (vstr = startp; pos != 0; pos--) {
				if ((unsigned char)*vstr == CTLESC)
					vstr++;
				vstr++;
			}
			for (loc = startp; len != 0; len--) {
				if ((unsigned char)*vstr == CTLESC)
					*loc++ = *vstr++;
				*loc++ = *vstr++;
			}
		}
		*loc = '\0';
		goto out;
	}
#endif /* BASH_SUBSTR */
	}

	resetloc = expdest - (char *)stackblock();

#if BASH_PATTERN_SUBST
	repl = NULL;

	/* We'll comeback here if we grow the stack while handling
	 * a VSREPLACE or VSREPLACEALL, since our pointers into the
	 * stack will need rebasing, and we'll need to remove our work
	 * areas each time
	 */
 restart:
#endif

	amount = expdest - ((char *)stackblock() + resetloc);
	STADJUST(-amount, expdest);
	startp = (char *)stackblock() + startloc;

	rmesc = startp;
	rmescend = (char *)stackblock() + strloc;
	//bb_error_msg("str7:'%s'", rmescend);
	if (quotes) {
//TODO: how to handle slash_pos here if string changes (shortens?)
		rmesc = rmescapes(startp, RMESCAPE_ALLOC | RMESCAPE_GROW, NULL);
		if (rmesc != startp) {
			rmescend = expdest;
			startp = (char *)stackblock() + startloc;
		}
	}
	rmescend--;
	str = (char *)stackblock() + strloc;
	/*
	 * Example: v='a\bc'; echo ${v/\\b/_\\_\z_}
	 * The result is a_\_z_c (not a\_\_z_c)!
	 *
	 * The search pattern and replace string treat backslashes differently!
	 * "&slash_pos" causes rmescapes() to work differently on the pattern
	 * and string.  It's only used on the first call.
	 */
	//bb_error_msg("str8:'%s' slash_pos:%d", str, slash_pos);
	rmescapes(str, RMESCAPE_GLOB,
		repl ? NULL : (slash_pos < 0 ? NULL : &slash_pos)
	);

#if BASH_PATTERN_SUBST
	workloc = expdest - (char *)stackblock();
	if (subtype == VSREPLACE || subtype == VSREPLACEALL) {
		size_t no_meta_len, first_escaped;
		int len;
		char *idx, *end;

		if (!repl) {
			//bb_error_msg("str9:'%s' slash_pos:%d", str, slash_pos);
			repl = nullstr;
			if (slash_pos >= 0) {
				repl = str + slash_pos;
				*repl++ = '\0';
			}
		}
		//bb_error_msg("str:'%s' repl:'%s'", str, repl);

		/* If there's no pattern to match, return the expansion unmolested */
		if (str[0] == '\0')
			goto out1;

		first_escaped = (str[0] == '\\' && str[1]);
		/* "first_escaped" trick allows to treat e.g. "\*no_glob_chars"
		 * as literal too (as it is semi-common, and easy to accomodate
		 * by just using str + 1).
		 */
		no_meta_len = strpbrk(str + first_escaped * 2, "*?[\\") ? 0 : strlen(str);
		len = 0;
		idx = startp;
		end = str - 1;
		while (idx <= end) {
 try_to_match:
			if (no_meta_len == 0) {
				/* pattern has meta chars, have to glob */
				loc = scanright(idx, rmesc, rmescend, str, quotes, /*match_at_start:*/ 1);
			} else {
				/* Testcase for very slow replace (performs about 22k replaces):
				 * x=::::::::::::::::::::::
				 * x=$x$x;x=$x$x;x=$x$x;x=$x$x;x=$x$x;x=$x$x;x=$x$x;x=$x$x;x=$x$x;x=$x$x;echo ${#x}
				 * echo "${x//:/|}"
				 * To test "first_escaped" logic, replace : with *.
				 */
				if (strncmp(rmesc, str + first_escaped, no_meta_len - first_escaped) != 0)
					goto no_match;
				loc = idx;
				if (!quotes) {
					loc += no_meta_len - first_escaped;
				} else {
					size_t n = no_meta_len - first_escaped;
					do {
						if ((unsigned char)*loc == CTLESC)
							loc++;
						loc++;
					} while (--n != 0);
				}
			}
			//bb_error_msg("scanright('%s'):'%s'", str, loc);
			if (!loc) {
				char *restart_detect;
 no_match:
				/* No match, advance */
				restart_detect = stackblock();
 skip_matching:
				if (idx >= end)
					break;
				STPUTC(*idx, expdest);
				if (quotes && (unsigned char)*idx == CTLESC) {
					idx++;
					len++;
					STPUTC(*idx, expdest);
				}
				if (stackblock() != restart_detect)
					goto restart;
				idx++;
				len++;
				rmesc++;
				/* continue; - prone to quadratic behavior, smarter code: */
				if (str[0] == '*') {
					/* Pattern is "*foo". If "*foo" does not match "long_string",
					 * it would never match "ong_string" etc, no point in trying.
					 */
					goto skip_matching;
				}
				goto try_to_match;
			}

			if (subtype == VSREPLACEALL) {
				while (idx < loc) {
					if (quotes && (unsigned char)*idx == CTLESC)
						idx++;
					idx++;
					rmesc++;
				}
			} else {
				idx = loc;
			}

			//bb_error_msg("repl:'%s'", repl);
			for (loc = (char*)repl; *loc; loc++) {
				char *restart_detect = stackblock();
				if (quotes && *loc == '\\') {
					STPUTC(CTLESC, expdest);
					len++;
				}
				STPUTC(*loc, expdest);
				if (stackblock() != restart_detect)
					goto restart;
				len++;
			}

			if (subtype == VSREPLACE) {
				//bb_error_msg("tail:'%s', quotes:%x", idx, quotes);
				while (*idx) {
					char *restart_detect = stackblock();
					STPUTC(*idx, expdest);
					if (stackblock() != restart_detect)
						goto restart;
					len++;
					idx++;
				}
				break;
			}
		}

		/* We've put the replaced text into a buffer at workloc, now
		 * move it to the right place and adjust the stack.
		 */
		STPUTC('\0', expdest);
		startp = (char *)stackblock() + startloc;
		memmove(startp, (char *)stackblock() + workloc, len + 1);
		//bb_error_msg("startp:'%s'", startp);
		loc = startp + len;
		goto out;
	}
#endif /* BASH_PATTERN_SUBST */

	subtype -= VSTRIMRIGHT;
#if DEBUG
	if (subtype < 0 || subtype > 7)
		abort();
#endif
	/* zero = (subtype == VSTRIMLEFT || subtype == VSTRIMLEFTMAX) */
	zero = subtype >> 1;
	/* VSTRIMLEFT/VSTRIMRIGHTMAX -> scanleft */
	scan = (subtype & 1) ^ zero ? scanleft : scanright;

	loc = scan(startp, rmesc, rmescend, str, quotes, zero);
	if (loc) {
		if (zero) {
			memmove(startp, loc, str - loc);
			loc = startp + (str - loc) - 1;
		}
		*loc = '\0';
	} else
		loc = str - 1;

 out:
	amount = loc - expdest;
	STADJUST(amount, expdest);
#if BASH_PATTERN_SUBST
 out1:
#endif
	/* Remove any recorded regions beyond start of variable */
	removerecordregions(startloc);

	return p;
}

/*
 * Add the value of a specialized variable to the stack string.
 * name parameter (examples):
 * ash -c 'echo $1'      name:'1='
 * ash -c 'echo $qwe'    name:'qwe='
 * ash -c 'echo $$'      name:'$='
 * ash -c 'echo ${$}'    name:'$='
 * ash -c 'echo ${$##q}' name:'$=q'
 * ash -c 'echo ${#$}'   name:'$='
 * note: examples with bad shell syntax:
 * ash -c 'echo ${#$1}'  name:'$=1'
 * ash -c 'echo ${#1#}'  name:'1=#'
 */
static NOINLINE ssize_t
varvalue(char *name, int varflags, int flags, int quoted)
{
	const char *p;
	int num;
	int i;
	ssize_t len = 0;
	int sep;
	int subtype = varflags & VSTYPE;
	int discard = (subtype == VSPLUS || subtype == VSLENGTH) | (flags & EXP_DISCARD);

	if (!subtype) {
		if (discard)
			return -1;

		raise_error_syntax("bad substitution");
	}

	flags |= EXP_KEEPNUL;
	flags &= discard ? ~QUOTES_ESC : ~0;
	sep = (flags & EXP_FULL) << CHAR_BIT;

	switch (*name) {
	case '$':
		num = rootpid;
		goto numvar;
	case '?':
		num = exitstatus;
		goto numvar;
	case '#':
		num = shellparam.nparam;
		goto numvar;
	case '!':
		num = backgndpid;
		if (num == 0)
			return -1;
 numvar:
		len = cvtnum(num, flags);
		goto check_1char_name;
	case '-':
		expdest = makestrspace(NOPTS, expdest);
		for (i = NOPTS - 1; i >= 0; i--) {
			if (optlist[i] && optletters(i)) {
				USTPUTC(optletters(i), expdest);
				len++;
			}
		}
 check_1char_name:
#if 0
		/* handles cases similar to ${#$1} */
		if (name[2] != '\0')
			raise_error_syntax("bad substitution");
#endif
		break;
	case '@':
		if (quoted && sep)
			goto param;
		/* fall through */
	case '*': {
		char **ap;
		char sepc;
		char c;

		/* We will set c to 0 or ~0 depending on whether
		 * we're doing field splitting.  We won't do field
		 * splitting if either we're quoted or sep is zero.
		 *
		 * Instead of testing (quoted || !sep) the following
		 * trick optimises away any branches by using the
		 * fact that EXP_QUOTED (which is the only bit that
		 * can be set in quoted) is the same as EXP_FULL <<
		 * CHAR_BIT (which is the only bit that can be set
		 * in sep).
		 */
#if EXP_QUOTED >> CHAR_BIT != EXP_FULL
#error The following two lines expect EXP_QUOTED == EXP_FULL << CHAR_BIT
#endif
		c = !((quoted | ~sep) & EXP_QUOTED) - 1;
		sep &= ~quoted;
		sep |= ifsset() ? (unsigned char)(c & ifsval()[0]) : ' ';
 param:
		sepc = sep;
		ap = shellparam.p;
		if (!ap)
			return -1;
		while ((p = *ap++) != NULL) {
			len += strtodest(p, flags);

			if (*ap && sep) {
				len++;
				memtodest(&sepc, 1, flags);
			}
		}
		break;
	} /* case '*' */
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		num = atoi(name); /* number(name) fails on ${N#str} etc */
		if (num < 0 || num > shellparam.nparam)
			return -1;
		p = num ? shellparam.p[num - 1] : arg0;
		goto value;
	default:
		/* NB: name has form "VAR=..." */
		p = lookupvar(name);
 value:
		if (!p)
			return -1;

		len = strtodest(p, flags);
#if ENABLE_UNICODE_SUPPORT
		if (subtype == VSLENGTH && len > 0) {
			reinit_unicode_for_ash();
			if (unicode_status == UNICODE_ON) {
				STADJUST(-len, expdest);
				discard = 0;
				len = unicode_strlen(p);
			}
		}
#endif
		break;
	}

	if (discard)
		STADJUST(-len, expdest);

	return len;
}

/*
 * Expand a variable, and return a pointer to the next character in the
 * input string.
 */
static char *
evalvar(char *p, int flag)
{
	char varflags;
	char subtype;
	char *var;
	int patloc;
	int startloc;
	ssize_t varlen;
	int discard;
	int quoted;

	varflags = (unsigned char) *p++;
	subtype = varflags & VSTYPE;

	quoted = flag & EXP_QUOTED;
	var = p;
	startloc = expdest - (char *)stackblock();
	p = strchr(p, '=') + 1; //TODO: use var_end(p)?

 again:
	varlen = varvalue(var, varflags, flag, quoted);
	if (varflags & VSNUL)
		varlen--;

	discard = varlen < 0 ? EXP_DISCARD : 0;

	switch (subtype) {
	case VSPLUS:
		discard ^= EXP_DISCARD;
		/* fall through */
	case 0:
	case VSMINUS:
		p = argstr(p, flag | EXP_TILDE | EXP_WORD | (discard ^ EXP_DISCARD));
		goto record;

	case VSASSIGN:
	case VSQUESTION:
		p = subevalvar(p, var, 0, startloc, varflags,
			(flag & ~QUOTES_ESC) | (discard ^ EXP_DISCARD));

		if ((flag | ~discard) & EXP_DISCARD)
			goto record;

		varflags &= ~VSNUL;
		subtype = VSNORMAL;
		goto again;
	}

	if ((discard & ~flag) && uflag)
		varunset(p, var, 0, 0);

	if (subtype == VSLENGTH) {
		p++;
		if (flag & EXP_DISCARD)
			return p;
		cvtnum(varlen > 0 ? varlen : 0, flag);
		goto really_record;
	}

	if (subtype == VSNORMAL)
		goto record;

#if DEBUG
	switch (subtype) {
	case VSTRIMLEFT:
	case VSTRIMLEFTMAX:
	case VSTRIMRIGHT:
	case VSTRIMRIGHTMAX:
#if BASH_SUBSTR
	case VSSUBSTR:
#endif
#if BASH_PATTERN_SUBST
	case VSREPLACE:
	case VSREPLACEALL:
#endif
		break;
	default:
		abort();
	}
#endif

	flag |= discard;
	if (!(flag & EXP_DISCARD)) {
		/*
		 * Terminate the string and start recording the pattern
		 * right after it
		 */
		STPUTC('\0', expdest);
	}

	patloc = expdest - (char *)stackblock();
	p = subevalvar(p, NULL, patloc, startloc, varflags, flag);

 record:
	if ((flag | discard) & EXP_DISCARD)
		return p;

 really_record:
	if (quoted) {
		quoted = *var == '@' && shellparam.nparam;
		if (!quoted)
			return p;
	}
	recordregion(startloc, expdest - (char *)stackblock(), quoted);
	return p;
}

/*
 * Add a file name to the list.
 */
static void
addfname(const char *name)
{
	struct strlist *sp;

	sp = stzalloc(sizeof(*sp));
	sp->text = sstrdup(name);
	*exparg.lastp = sp;
	exparg.lastp = &sp->next;
}

/* Avoid glob() (and thus, stat() et al) for words like "echo" */
static int
hasmeta(const char *p)
{
	static const char chars[] ALIGN1 = {
		'*', '?', '[', '\\', CTLQUOTEMARK, CTLESC, 0
	};

	for (;;) {
		p = strpbrk(p, chars);
		if (!p)
			break;
		switch ((unsigned char)*p) {
		case CTLQUOTEMARK:
			for (;;) {
				p++;
				if ((unsigned char)*p == CTLQUOTEMARK)
					break;
				if ((unsigned char)*p == CTLESC)
					p++;
				if (*p == '\0') /* huh? */
					return 0;
			}
			break;
		case '\\':
		case CTLESC:
			p++;
			if (*p == '\0')
				return 0;
			break;
		case '[':
			if (!strchr(p + 1, ']')) {
				/* It's not a properly closed [] pattern,
				 * but other metas may follow. Continue checking.
				 * my[file* _is_ globbed by bash
				 * and matches filenames like "my[file1".
				 */
				break;
			}
			/* fallthrough */
		default:
		/* case '*': */
		/* case '?': */
			return 1;
		}
		p++;
	}

	return 0;
}

/* If we want to use glob() from libc... */
#if !ENABLE_ASH_INTERNAL_GLOB

/* Add the result of glob() to the list */
static void
addglob(const glob_t *pglob)
{
	char **p = pglob->gl_pathv;

	do {
		addfname(*p);
	} while (*++p);
}
static void
expandmeta(struct strlist *str /*, int flag*/)
{
	/* TODO - EXP_REDIR */

	while (str) {
		char *p;
		glob_t pglob;
		int i;

		if (fflag)
			goto nometa;

		if (!hasmeta(str->text))
			goto nometa;

		INT_OFF;
		p = preglob(str->text, RMESCAPE_ALLOC | RMESCAPE_HEAP);
// GLOB_NOMAGIC (GNU): if no *?[ chars in pattern, return it even if no match
// GLOB_NOCHECK: if no match, return unchanged pattern (sans \* escapes?)
//
// glibc 2.24.90 glob(GLOB_NOMAGIC) does not remove backslashes used for escaping:
// if you pass it "file\?", it returns "file\?", not "file?", if no match.
// Which means you need to unescape the string, right? Not so fast:
// if there _is_ a file named "file\?" (with backslash), it is returned
// as "file\?" too (whichever pattern you used to find it, say, "file*").
// You DON'T KNOW by looking at the result whether you need to unescape it.
//
// Worse, globbing of "file\?" in a directory with two files, "file?" and "file\?",
// returns "file\?" - which is WRONG: "file\?" pattern matches "file?" file.
// Without GLOB_NOMAGIC, this works correctly ("file?" is returned as a match).
// With GLOB_NOMAGIC | GLOB_NOCHECK, this also works correctly.
//		i = glob(p, GLOB_NOMAGIC | GLOB_NOCHECK, NULL, &pglob);
//		i = glob(p, GLOB_NOMAGIC, NULL, &pglob);
		i = glob(p, 0, NULL, &pglob);
		//bb_error_msg("glob('%s'):%d '%s'...", p, i, pglob.gl_pathv ? pglob.gl_pathv[0] : "-");
		if (p != str->text)
			free(p);
		switch (i) {
		case 0:
#if 0 // glibc 2.24.90 bug? Patterns like "*/file", when match, don't set GLOB_MAGCHAR
			/* GLOB_MAGCHAR is set if *?[ chars were seen (GNU) */
			if (!(pglob.gl_flags & GLOB_MAGCHAR))
				goto nometa2;
#endif
			addglob(&pglob);
			globfree(&pglob);
			INT_ON;
			break;
		case GLOB_NOMATCH:
 //nometa2:
			globfree(&pglob);
			INT_ON;
 nometa:
			*exparg.lastp = str;
			rmescapes(str->text, 0, NULL);
			exparg.lastp = &str->next;
			break;
		default:	/* GLOB_NOSPACE */
			globfree(&pglob);
			INT_ON;
			ash_msg_and_raise_error(bb_msg_memory_exhausted);
		}
		str = str->next;
	}
}

#else
/* ENABLE_ASH_INTERNAL_GLOB: Homegrown globbing code. (dash also has both, uses homegrown one.) */

/*
 * Do metacharacter (i.e. *, ?, [...]) expansion.
 */
typedef struct exp_t {
	char *dir;
	unsigned dir_max;
} exp_t;
static void
expmeta(exp_t *exp, char *name, unsigned name_len, unsigned expdir_len)
{
#define expdir exp->dir
#define expdir_max exp->dir_max
	char *enddir = expdir + expdir_len;
	char *p;
	const char *cp;
	char *start;
	char *endname;
	int metaflag;
	struct stat statb;
	DIR *dirp;
	struct dirent *dp;
	int atend;
	int matchdot;
	int esc;

	metaflag = 0;
	start = name;
#if ENABLE_PLATFORM_MINGW32
	if (expdir_len == 0 && has_dos_drive_prefix(start) && start[2] != '/')
		start += 2;
#endif
	for (p = name; esc = 0, *p; p += esc + 1) {
		if (*p == '*' || *p == '?')
			metaflag = 1;
		else if (*p == '[') {
			char *q = p + 1;
			if (*q == '!')
				q++;
			for (;;) {
				if (*q == '\\')
					q++;
				if (*q == '/' || *q == '\0')
					break;
				if (*++q == ']') {
					metaflag = 1;
					break;
				}
			}
		} else {
			if (*p == '\\' && p[1])
				esc++;
			if (p[esc] == '/') {
				if (metaflag)
					break;
				start = p + esc + 1;
			}
		}
	}
	if (metaflag == 0) {    /* we've reached the end of the file name */
		if (!expdir_len)
			return;
		p = name;
		do {
			if (*p == '\\' && p[1])
				p++;
			*enddir++ = *p;
		} while (*p++);
		if (lstat(expdir, &statb) == 0)
			addfname(expdir);
		return;
	}
	endname = p;
	if (name < start) {
		p = name;
		do {
			if (*p == '\\' && p[1])
				p++;
			*enddir++ = *p++;
		} while (p < start);
	}
	*enddir = '\0';
	cp = expdir;
	expdir_len = enddir - cp;
	if (!expdir_len)
		cp = ".";
	dirp = opendir(cp);
	if (dirp == NULL)
		return;
	if (*endname == 0) {
		atend = 1;
	} else {
		atend = 0;
		*endname = '\0';
		endname += esc + 1;
	}
	name_len -= endname - name;
	matchdot = 0;
	p = start;
	if (*p == '\\')
		p++;
	if (*p == '.')
		matchdot++;
	while (!pending_int && (dp = readdir(dirp)) != NULL) {
		if (dp->d_name[0] == '.' && !matchdot)
			continue;
#if ENABLE_ASH_NOCASEGLOB
# undef pmatch
# define pmatch(a, b) !fnmatch((a), (b), nocaseglob ? FNM_CASEFOLD : 0)
#endif
		if (pmatch(start, dp->d_name)) {
			if (atend) {
				strcpy(enddir, dp->d_name);
				addfname(expdir);
			} else {
				unsigned offset;
				unsigned len;

				p = stpcpy(enddir, dp->d_name);
				*p = '/';

				offset = p - expdir + 1;
				len = offset + name_len + NAME_MAX;
				if (len > expdir_max) {
					len += PATH_MAX;
					expdir = ckrealloc(expdir, len);
					expdir_max = len;
				}

				expmeta(exp, endname, name_len, offset);
				enddir = expdir + expdir_len;
			}
		}
	}
	closedir(dirp);
	if (!atend)
		endname[-esc - 1] = esc ? '\\' : '/';
#undef expdir
#undef expdir_max
#if ENABLE_ASH_NOCASEGLOB
# undef pmatch
# define pmatch(a, b) !fnmatch((a), (b), 0)
#endif
}

static struct strlist *
msort(struct strlist *list, int len)
{
	struct strlist *p, *q = NULL;
	struct strlist **lpp;
	int half;
	int n;

	if (len <= 1)
		return list;
	half = len >> 1;
	p = list;
	for (n = half; --n >= 0;) {
		q = p;
		p = p->next;
	}
	q->next = NULL;                 /* terminate first half of list */
	q = msort(list, half);          /* sort first half of list */
	p = msort(p, len - half);               /* sort second half */
	lpp = &list;
	for (;;) {
#if ENABLE_LOCALE_SUPPORT
		if (strcoll(p->text, q->text) < 0)
#else
		if (strcmp(p->text, q->text) < 0)
#endif
						{
			*lpp = p;
			lpp = &p->next;
			p = *lpp;
			if (p == NULL) {
				*lpp = q;
				break;
			}
		} else {
			*lpp = q;
			lpp = &q->next;
			q = *lpp;
			if (q == NULL) {
				*lpp = p;
				break;
			}
		}
	}
	return list;
}

/*
 * Sort the results of file name expansion.  It calculates the number of
 * strings to sort and then calls msort (short for merge sort) to do the
 * work.
 */
static struct strlist *
expsort(struct strlist *str)
{
	int len;
	struct strlist *sp;

	len = 0;
	for (sp = str; sp; sp = sp->next)
		len++;
	return msort(str, len);
}

static void
expandmeta(struct strlist *str /*, int flag*/)
{
	/* TODO - EXP_REDIR */

	while (str) {
		exp_t exp;
		struct strlist **savelastp;
		struct strlist *sp;
		char *p;
		unsigned len;

		if (fflag)
			goto nometa;
		if (!hasmeta(str->text))
			goto nometa;
		savelastp = exparg.lastp;

		INT_OFF;
		p = preglob(str->text, RMESCAPE_ALLOC | RMESCAPE_HEAP);
		len = strlen(p);
		exp.dir_max = len + PATH_MAX;
		exp.dir = ckmalloc(exp.dir_max);

		expmeta(&exp, p, len, 0);
		free(exp.dir);
		if (p != str->text)
			free(p);
		INT_ON;
		if (exparg.lastp == savelastp) {
			/*
			 * no matches
			 */
 nometa:
			*exparg.lastp = str;
			rmescapes(str->text, 0, NULL);
			exparg.lastp = &str->next;
		} else {
			*exparg.lastp = NULL;
			*savelastp = sp = expsort(*savelastp);
			while (sp->next != NULL)
				sp = sp->next;
			exparg.lastp = &sp->next;
		}
		str = str->next;
	}
}
#endif /* ENABLE_ASH_INTERNAL_GLOB */

/*
 * Perform variable substitution and command substitution on an argument,
 * placing the resulting list of arguments in arglist.  If EXP_FULL is true,
 * perform splitting and file name expansion.  When arglist is NULL, perform
 * here document expansion.
 */
static void
expandarg(union node *arg, struct arglist *arglist, int flag)
{
	struct strlist *sp;
	char *p;

	argbackq = arg->narg.backquote;
	STARTSTACKSTR(expdest);
	TRACE(("expandarg: argstr('%s',flags:%x)\n", arg->narg.text, flag));
	argstr(arg->narg.text, flag);
	if (arglist == NULL) {
		/* here document expanded */
		goto out;
	}
	p = grabstackstr(expdest);
	TRACE(("expandarg: p:'%s'\n", p));
	exparg.lastp = &exparg.list;
	/*
	 * TODO - EXP_REDIR
	 */
	if (flag & EXP_FULL) {
		ifsbreakup(p, &exparg);
		*exparg.lastp = NULL;
		exparg.lastp = &exparg.list;
		expandmeta(exparg.list /*, flag*/);
	} else {
		sp = stzalloc(sizeof(*sp));
		sp->text = p;
		*exparg.lastp = sp;
		exparg.lastp = &sp->next;
	}
	*exparg.lastp = NULL;
	if (exparg.list) {
		*arglist->lastp = exparg.list;
		arglist->lastp = exparg.lastp;
	}

 out:
	ifsfree();
}

/*
 * Expand shell variables and backquotes inside a here document.
 */
static void
expandhere(union node *arg)
{
	expandarg(arg, (struct arglist *)NULL, EXP_QUOTED);
}

/*
 * Returns true if the pattern matches the string.
 */
static int
patmatch(char *pattern, const char *string)
{
	char *p = preglob(pattern, 0);
	int r = pmatch(p, string);
	//bb_error_msg("!fnmatch(pattern:'%s',str:'%s',0):%d", p, string, r);
	return r;
}

/*
 * See if a pattern matches in a case statement.
 */
static int
casematch(union node *pattern, char *val)
{
	struct stackmark smark;
	int result;

	setstackmark(&smark);
	argbackq = pattern->narg.backquote;
	STARTSTACKSTR(expdest);
	argstr(pattern->narg.text, EXP_TILDE | EXP_CASE);
	ifsfree();
	result = patmatch(stackblock(), val);
	popstackmark(&smark);
	return result;
}


/* ============ find_command */

struct builtincmd {
	const char *name;
	int (*builtin)(int, char **) FAST_FUNC;
	/* unsigned flags; */
};
#define IS_BUILTIN_SPECIAL(b) ((b)->name[0] & 1)
/* "regular" builtins always take precedence over commands,
 * regardless of PATH=....%builtin... position */
#define IS_BUILTIN_REGULAR(b) ((b)->name[0] & 2)
#define IS_BUILTIN_ASSIGN(b)  ((b)->name[0] & 4)

struct cmdentry {
	smallint cmdtype;       /* CMDxxx */
	union param {
		int index;
		/* index >= 0 for commands without path (slashes) */
		/* (TODO: what exactly does the value mean? PATH position?) */
		/* index == -1 for commands with slashes */
		/* index == (-2 - applet_no) for NOFORK applets */
		const struct builtincmd *cmd;
		struct funcnode *func;
	} u;
};
/* values of cmdtype */
#define CMDUNKNOWN      -1      /* no entry in table for command */
#define CMDNORMAL       0       /* command is an executable program */
#define CMDFUNCTION     1       /* command is a shell function */
#define CMDBUILTIN      2       /* command is a shell builtin */

/* action to find_command() */
#define DO_ERR          0x01    /* prints errors */
#define DO_ABS          0x02    /* checks absolute paths */
#define DO_NOFUNC       0x04    /* don't return shell functions, for command */
#define DO_ALTPATH      0x08    /* using alternate path */
#define DO_REGBLTIN     0x10    /* regular built-ins and functions only */

static void find_command(char *, struct cmdentry *, int, const char *);


/* ============ Hashing commands */

/*
 * When commands are first encountered, they are entered in a hash table.
 * This ensures that a full path search will not have to be done for them
 * on each invocation.
 *
 * We should investigate converting to a linear search, even though that
 * would make the command name "hash" a misnomer.
 */

struct tblentry {
	struct tblentry *next;  /* next entry in hash chain */
	union param param;      /* definition of builtin function */
	smallint cmdtype;       /* CMDxxx */
	char rehash;            /* if set, cd done since entry created */
	char cmdname[1];        /* name of command */
};

static struct tblentry **cmdtable;
#define INIT_G_cmdtable() do { \
	cmdtable = xzalloc(CMDTABLESIZE * sizeof(cmdtable[0])); \
} while (0)

static int builtinloc = -1;     /* index in path of %builtin, or -1 */


static void
tryexec(IF_FEATURE_SH_STANDALONE(int applet_no,) const char *cmd, char **argv, char **envp)
{
#if ENABLE_FEATURE_SH_STANDALONE
	if (applet_no >= 0) {
# if ENABLE_PLATFORM_MINGW32
		/* Treat all applets as NOEXEC, including the shell itself if
		 * this is a FS_SHELLEXEC shell. */
		struct forkshell *fs = (struct forkshell *)sticky_mem_start;
		if (applet_main[applet_no] != ash_main ||
				(fs && fs->fpid == FS_SHELLEXEC)) {
			/* mingw-w64's getopt() uses __argv[0] as the program name */
			__argv[0] = (char *)cmd;
# else
		if (APPLET_IS_NOEXEC(applet_no)) {
# endif
			clearenv();
			while (*envp)
				putenv(*envp++);
			popredir(/*drop:*/ 1);
			run_noexec_applet_and_exit(applet_no, cmd, argv);
		}
		/* re-exec ourselves with the new arguments */
		execve(bb_busybox_exec_path, argv, envp);
		/* If they called chroot or otherwise made the binary no longer
		 * executable, fall through */
	}
#endif

#if ENABLE_PLATFORM_MINGW32
	/* cmd was allocated on the stack with room for an extension */
	add_win32_extension((char *)cmd);
	execve(cmd, argv, envp);
	/* skip POSIX-mandated retry on ENOEXEC */
#else /* !ENABLE_PLATFORM_MINGW32 */
 repeat:
#ifdef SYSV
	do {
		execve(cmd, argv, envp);
	} while (errno == EINTR);
#else
	execve(cmd, argv, envp);
#endif

	if (cmd != bb_busybox_exec_path && errno == ENOEXEC) {
		/* Run "cmd" as a shell script:
		 * http://pubs.opengroup.org/onlinepubs/9699919799/utilities/V3_chap02.html
		 * "If the execve() function fails with ENOEXEC, the shell
		 * shall execute a command equivalent to having a shell invoked
		 * with the command name as its first operand,
		 * with any remaining arguments passed to the new shell"
		 *
		 * That is, do not use $SHELL, user's shell, or /bin/sh;
		 * just call ourselves.
		 *
		 * Note that bash reads ~80 chars of the file, and if it sees
		 * a zero byte before it sees newline, it doesn't try to
		 * interpret it, but fails with "cannot execute binary file"
		 * message and exit code 126. For one, this prevents attempts
		 * to interpret foreign ELF binaries as shell scripts.
		 */
		argv[0] = (char*) cmd;
		cmd = bb_busybox_exec_path;
		/* NB: this is only possible because all callers of shellexec()
		 * ensure that the argv[-1] slot exists!
		 */
		argv--;
		argv[0] = (char*) "ash";
		goto repeat;
	}
#endif /* !ENABLE_PLATFORM_MINGW32 */
}

/*
 * Exec a program.  Never returns.  If you change this routine, you may
 * have to change the find_command routine as well.
 * argv[-1] must exist and be writable! See tryexec() for why.
 */
static void shellexec(char *prog, char **argv, const char *path, int idx) NORETURN;
static void shellexec(char *prog, char **argv, const char *path, int idx)
{
	char *cmdname;
	int e;
	char **envp;
	int exerrno;
	int applet_no = -1; /* used only by FEATURE_SH_STANDALONE */

	envp = listvars(VEXPORT, VUNSET, /*strlist:*/ NULL, /*end:*/ NULL);
#if !ENABLE_PLATFORM_MINGW32
	if (strchr(prog, '/') != NULL
#else
	if (has_path(prog)
#endif
#if ENABLE_FEATURE_SH_STANDALONE
	 || (applet_no = find_applet_by_name(prog)) >= 0
#endif
	) {
#if ENABLE_PLATFORM_MINGW32
# if ENABLE_FEATURE_SH_STANDALONE
		char *oldprog = prog;
# endif
		prog = stack_add_system_drive(prog);
#endif
		tryexec(IF_FEATURE_SH_STANDALONE(applet_no,) prog, argv, envp);
		if (applet_no >= 0) {
			/* We tried execing ourself, but it didn't work.
			 * Maybe /proc/self/exe doesn't exist?
			 * Try $PATH search.
			 */
			goto try_PATH;
		}
		e = errno;
#if ENABLE_PLATFORM_MINGW32 && ENABLE_FEATURE_SH_STANDALONE
		if (unix_path(oldprog)) {
			const char *name = bb_basename(oldprog);
			if ((applet_no = find_applet_by_name(name)) >= 0) {
				tryexec(applet_no, name, argv, envp);
				e = errno;
			}
			else {
				e = ENOENT;
			}
		}
#endif
	} else {
 try_PATH:
		e = ENOENT;
		while (padvance(&path, argv[0]) >= 0) {
			cmdname = stackblock();
			if (--idx < 0 && pathopt == NULL) {
				tryexec(IF_FEATURE_SH_STANDALONE(-1,) cmdname, argv, envp);
				if (errno != ENOENT && errno != ENOTDIR)
					e = errno;
			}
		}
	}

	/* Map to POSIX errors */
	switch (e) {
	default:
		exerrno = 126;
		break;
	case ELOOP:
	case ENAMETOOLONG:
	case ENOENT:
	case ENOTDIR:
		exerrno = 127;
		break;
	}
	exitstatus = exerrno;
	TRACE(("shellexec failed for %s, errno %d, suppress_int %d\n",
		prog, e, suppress_int));
	ash_msg_and_raise(EXEND, "%s: %s", prog, errmsg(e, "not found"));
	/* NOTREACHED */
}

static void
printentry(struct tblentry *cmdp)
{
	int idx;
	const char *path;
	char *name;

	idx = cmdp->param.index;
	path = pathval();
	do {
		padvance(&path, cmdp->cmdname);
	} while (--idx >= 0);
	name = stackblock();
#if ENABLE_PLATFORM_MINGW32
	add_win32_extension(name);
#endif
	out1fmt("%s%s\n", name, (cmdp->rehash ? "*" : nullstr));
}

/*
 * Clear out command entries.
 */
static void
clearcmdentry(void)
{
	struct tblentry **tblp;
	struct tblentry **pp;
	struct tblentry *cmdp;

	INT_OFF;
	for (tblp = cmdtable; tblp < &cmdtable[CMDTABLESIZE]; tblp++) {
		pp = tblp;
		while ((cmdp = *pp) != NULL) {
			if (cmdp->cmdtype == CMDNORMAL
			 || (cmdp->cmdtype == CMDBUILTIN
			    && !IS_BUILTIN_REGULAR(cmdp->param.cmd)
			    && builtinloc > 0
			    )
			) {
				*pp = cmdp->next;
				free(cmdp);
			} else {
				pp = &cmdp->next;
			}
		}
	}
	INT_ON;
}

/*
 * Locate a command in the command hash table.  If "add" is nonzero,
 * add the command to the table if it is not already present.  The
 * variable "lastcmdentry" is set to point to the address of the link
 * pointing to the entry, so that delete_cmd_entry can delete the
 * entry.
 *
 * Interrupts must be off if called with add != 0.
 */
static struct tblentry **lastcmdentry;

static struct tblentry *
cmdlookup(const char *name, int add)
{
	unsigned int hashval;
	const char *p;
	struct tblentry *cmdp;
	struct tblentry **pp;

	p = name;
	hashval = (unsigned char)*p << 4;
	while (*p)
		hashval += (unsigned char)*p++;
	hashval &= 0x7FFF;
	pp = &cmdtable[hashval % CMDTABLESIZE];
	for (cmdp = *pp; cmdp; cmdp = cmdp->next) {
		if (strcmp(cmdp->cmdname, name) == 0)
			break;
		pp = &cmdp->next;
	}
	if (add && cmdp == NULL) {
		cmdp = *pp = ckzalloc(sizeof(struct tblentry)
				+ strlen(name)
				/* + 1 - already done because
				 * tblentry::cmdname is char[1] */);
		/*cmdp->next = NULL; - ckzalloc did it */
		cmdp->cmdtype = CMDUNKNOWN;
		strcpy(cmdp->cmdname, name);
	}
	lastcmdentry = pp;
	return cmdp;
}

/*
 * Delete the command entry returned on the last lookup.
 */
static void
delete_cmd_entry(void)
{
	struct tblentry *cmdp;

	INT_OFF;
	cmdp = *lastcmdentry;
	*lastcmdentry = cmdp->next;
	if (cmdp->cmdtype == CMDFUNCTION)
		freefunc(cmdp->param.func);
	free(cmdp);
	INT_ON;
}

/*
 * Add a new command entry, replacing any existing command entry for
 * the same name - except special builtins.
 */
static void
addcmdentry(char *name, struct cmdentry *entry)
{
	struct tblentry *cmdp;

	cmdp = cmdlookup(name, 1);
	if (cmdp->cmdtype == CMDFUNCTION) {
		freefunc(cmdp->param.func);
	}
	cmdp->cmdtype = entry->cmdtype;
	cmdp->param = entry->u;
	cmdp->rehash = 0;
}

static int FAST_FUNC
hashcmd(int argc UNUSED_PARAM, char **argv UNUSED_PARAM)
{
	struct tblentry **pp;
	struct tblentry *cmdp;
	int c;
	struct cmdentry entry;
	char *name;

	if (nextopt("r") != '\0') {
		clearcmdentry();
		return 0;
	}

	if (*argptr == NULL) {
		for (pp = cmdtable; pp < &cmdtable[CMDTABLESIZE]; pp++) {
			for (cmdp = *pp; cmdp; cmdp = cmdp->next) {
				if (cmdp->cmdtype == CMDNORMAL)
					printentry(cmdp);
			}
		}
		return 0;
	}

	c = 0;
	while ((name = *argptr) != NULL) {
		cmdp = cmdlookup(name, 0);
		if (cmdp != NULL
		 && (cmdp->cmdtype == CMDNORMAL
		    || (cmdp->cmdtype == CMDBUILTIN
			&& !IS_BUILTIN_REGULAR(cmdp->param.cmd)
			&& builtinloc > 0
			)
		    )
		) {
			delete_cmd_entry();
		}
		find_command(name, &entry, DO_ERR, pathval());
		if (entry.cmdtype == CMDUNKNOWN)
			c = 1;
		argptr++;
	}
	return c;
}

/*
 * Called when a cd is done.  Marks all commands so the next time they
 * are executed they will be rehashed.
 */
static void
hashcd(void)
{
	struct tblentry **pp;
	struct tblentry *cmdp;

	for (pp = cmdtable; pp < &cmdtable[CMDTABLESIZE]; pp++) {
		for (cmdp = *pp; cmdp; cmdp = cmdp->next) {
			if (cmdp->cmdtype == CMDNORMAL
			 || (cmdp->cmdtype == CMDBUILTIN
			     && !IS_BUILTIN_REGULAR(cmdp->param.cmd)
			     && builtinloc > 0)
			) {
				cmdp->rehash = 1;
			}
		}
	}
}

/*
 * Fix command hash table when PATH changed.
 * Called before PATH is changed.  The argument is the new value of PATH;
 * pathval() still returns the old value at this point.
 * Called with interrupts off.
 */
static void FAST_FUNC
changepath(const char *newval)
{
	const char *new;
	int idx;
	int bltin;

	new = newval;
	idx = 0;
	bltin = -1;
	for (;;) {
		if (*new == '%' && prefix(new + 1, "builtin")) {
			bltin = idx;
			break;
		}
		new = strchr(new, PATH_SEP);
		if (!new)
			break;
		idx++;
		new++;
	}
	builtinloc = bltin;
	clearcmdentry();
}
enum {
	TEOF,
	TNL,
	TREDIR,
	TWORD,
	TSEMI,
	TBACKGND,
	TAND,
	TOR,
	TPIPE,
	TLP,
	TRP,
	TENDCASE,
	TENDBQUOTE,
	TNOT,
	TCASE,
	TDO,
	TDONE,
	TELIF,
	TELSE,
	TESAC,
	TFI,
	TFOR,
#if BASH_FUNCTION
	TFUNCTION,
#endif
	TIF,
	TIN,
	TTHEN,
	TUNTIL,
	TWHILE,
	TBEGIN,
	TEND
};
typedef smallint token_id_t;

/* Nth bit indicates if token marks the end of a list */
enum {
	tokendlist = 0
	/*  0 */ | (1u << TEOF)
	/*  1 */ | (0u << TNL)
	/*  2 */ | (0u << TREDIR)
	/*  3 */ | (0u << TWORD)
	/*  4 */ | (0u << TSEMI)
	/*  5 */ | (0u << TBACKGND)
	/*  6 */ | (0u << TAND)
	/*  7 */ | (0u << TOR)
	/*  8 */ | (0u << TPIPE)
	/*  9 */ | (0u << TLP)
	/* 10 */ | (1u << TRP)
	/* 11 */ | (1u << TENDCASE)
	/* 12 */ | (1u << TENDBQUOTE)
	/* 13 */ | (0u << TNOT)
	/* 14 */ | (0u << TCASE)
	/* 15 */ | (1u << TDO)
	/* 16 */ | (1u << TDONE)
	/* 17 */ | (1u << TELIF)
	/* 18 */ | (1u << TELSE)
	/* 19 */ | (1u << TESAC)
	/* 20 */ | (1u << TFI)
	/* 21 */ | (0u << TFOR)
#if BASH_FUNCTION
	/* 22 */ | (0u << TFUNCTION)
#endif
	/* 23 */ | (0u << TIF)
	/* 24 */ | (0u << TIN)
	/* 25 */ | (1u << TTHEN)
	/* 26 */ | (0u << TUNTIL)
	/* 27 */ | (0u << TWHILE)
	/* 28 */ | (0u << TBEGIN)
	/* 29 */ | (1u << TEND)
	, /* thus far 29 bits used */
};

static const char *const tokname_array[] ALIGN_PTR = {
	"end of file",
	"newline",
	"redirection",
	"word",
	";",
	"&",
	"&&",
	"||",
	"|",
	"(",
	")",
	";;",
	"`",
#define KWDOFFSET 13
	/* the following are keywords */
	"!",
	"case",
	"do",
	"done",
	"elif",
	"else",
	"esac",
	"fi",
	"for",
#if BASH_FUNCTION
	"function",
#endif
	"if",
	"in",
	"then",
	"until",
	"while",
	"{",
	"}",
};

/* Wrapper around strcmp for qsort/bsearch/... */
static int
pstrcmp(const void *a, const void *b)
{
	return strcmp((char*)a, *(char**)b);
}

static const char *const *
findkwd(const char *s)
{
	return bsearch(s, tokname_array + KWDOFFSET,
			ARRAY_SIZE(tokname_array) - KWDOFFSET,
			sizeof(tokname_array[0]), pstrcmp);
}

/*
 * Locate and print what a word is...
 */
static int
describe_command(char *command, const char *path, int describe_command_verbose)
{
	struct cmdentry entry;
#if ENABLE_ASH_ALIAS
	const struct alias *ap;
#endif

	path = path ? path : pathval();

	if (describe_command_verbose) {
		out1str(command);
	}

	/* First look at the keywords */
	if (findkwd(command)) {
		out1str(describe_command_verbose ? " is a shell keyword" : command);
		goto out;
	}

#if ENABLE_ASH_ALIAS
	/* Then look at the aliases */
	ap = lookupalias(command, 0);
	if (ap != NULL) {
		if (!describe_command_verbose) {
			out1str("alias ");
			printalias(ap);
			return 0;
		}
		out1fmt(" is an alias for %s", ap->val);
		goto out;
	}
#endif
	/* Brute force */
	find_command(command, &entry, DO_ABS, path);

	switch (entry.cmdtype) {
	case CMDNORMAL: {
		int j = entry.u.index;
		char *p;
#if ENABLE_PLATFORM_MINGW32 && ENABLE_FEATURE_SH_STANDALONE
		if (j == INT_MIN) {
			p = (char *)bb_basename(command);
			goto describe;
		}
#endif
		if (j < 0) {
#if ENABLE_PLATFORM_MINGW32
			p = stack_add_system_drive(command);
#else
			p = command;
#endif
		} else {
			do {
				padvance(&path, command);
			} while (--j >= 0);
			p = stackblock();
		}
#if ENABLE_PLATFORM_MINGW32
		add_win32_extension(p);
		bs_to_slash(p);
 IF_FEATURE_SH_STANDALONE(describe:)
#endif
		if (describe_command_verbose) {
			out1fmt(" is %s", p);
		} else {
			out1str(p);
		}
		break;
	}

	case CMDFUNCTION:
		if (describe_command_verbose) {
			/*out1str(" is a shell function");*/
			out1str(" is a function"); /* bash says this */
		} else {
			out1str(command);
		}
		break;

	case CMDBUILTIN:
		if (describe_command_verbose) {
			out1fmt(" is a %sshell builtin",
				IS_BUILTIN_SPECIAL(entry.u.cmd) ?
					"special " : nullstr
			);
		} else {
			out1str(command);
		}
		break;

	default:
		if (describe_command_verbose) {
			out1str(": not found\n");
		}
		return 127;
	}
 out:
	out1str("\n");
	return 0;
}

static int FAST_FUNC
typecmd(int argc UNUSED_PARAM, char **argv)
{
	int i = 1;
	int err = 0;
	int verbose = 1;

	/* type -p ... ? (we don't bother checking for 'p') */
	if (argv[1] && argv[1][0] == '-') {
		i++;
		verbose = 0;
	}
	while (argv[i]) {
		err |= describe_command(argv[i++], NULL, verbose);
	}
	return err;
}

static struct strlist *
fill_arglist(struct arglist *arglist, union node **argpp)
{
	struct strlist **lastp = arglist->lastp;
	union node *argp;

	while ((argp = *argpp) != NULL) {
		expandarg(argp, arglist, EXP_FULL | EXP_TILDE);
		*argpp = argp->narg.next;
		if (*lastp)
			break;
	}

	return *lastp;
}

#if ENABLE_ASH_CMDCMD
/* Is it "command [-p] PROG ARGS" bltin, no other opts? Return ptr to "PROG" if yes */
static int
parse_command_args(struct arglist *arglist, union node **argpp, const char **path)
{
	struct strlist *sp = arglist->list;
	char *cp, c;

	for (;;) {
		sp = sp->next ? sp->next : fill_arglist(arglist, argpp);
		if (!sp)
			return 0;
		cp = sp->text;
		if (*cp++ != '-')
			break;
		c = *cp++;
		if (!c)
			break;
		if (c == '-' && !*cp) {
			if (!sp->next && !fill_arglist(arglist, argpp))
				return 0;
			sp = sp->next;
			break;
		}
		do {
			switch (c) {
			case 'p':
				*path = bb_default_path;
				break;
			default:
				/* run 'typecmd' for other options */
				return 0;
			}
			c = *cp++;
		} while (c);
	}

	arglist->list = sp;
	return DO_NOFUNC;
}

static int FAST_FUNC
commandcmd(int argc UNUSED_PARAM, char **argv UNUSED_PARAM)
{
	char *cmd;
	int c;
	enum {
		VERIFY_BRIEF = 1,
		VERIFY_VERBOSE = 2,
	} verify = 0;
	const char *path = NULL;

	/* "command [-p] PROG ARGS" (that is, without -V or -v)
	 * never reaches this function.
	 */

	while ((c = nextopt("pvV")) != '\0')
		if (c == 'V')
			verify |= VERIFY_VERBOSE;
		else if (c == 'v')
			/*verify |= VERIFY_BRIEF*/;
#if DEBUG
		else if (c != 'p')
			abort();
#endif
		else
			path = bb_default_path;

	/* Mimic bash: just "command -v" doesn't complain, it's a nop */
	cmd = *argptr;
	if (/*verify && */ cmd)
		return describe_command(cmd, path, verify /* - VERIFY_BRIEF*/);

	return 0;
}
#endif


/*static int funcblocksize;     // size of structures in function */
/*static int funcstringsize;    // size of strings in node */
static void *funcblock;         /* block to allocate function from */
static char *funcstring_end;    /* end of block to allocate strings from */
#if ENABLE_PLATFORM_MINGW32
static int fs_size;
# if FORKSHELL_DEBUG
static void *fs_start;
static const char **annot;
# endif
#endif

static const uint8_t nodesize[N_NUMBER] ALIGN1 = {
	[NCMD     ] = SHELL_ALIGN(sizeof(struct ncmd)),
	[NPIPE    ] = SHELL_ALIGN(sizeof(struct npipe)),
	[NREDIR   ] = SHELL_ALIGN(sizeof(struct nredir)),
	[NBACKGND ] = SHELL_ALIGN(sizeof(struct nredir)),
	[NSUBSHELL] = SHELL_ALIGN(sizeof(struct nredir)),
	[NAND     ] = SHELL_ALIGN(sizeof(struct nbinary)),
	[NOR      ] = SHELL_ALIGN(sizeof(struct nbinary)),
	[NSEMI    ] = SHELL_ALIGN(sizeof(struct nbinary)),
	[NIF      ] = SHELL_ALIGN(sizeof(struct nif)),
	[NWHILE   ] = SHELL_ALIGN(sizeof(struct nbinary)),
	[NUNTIL   ] = SHELL_ALIGN(sizeof(struct nbinary)),
	[NFOR     ] = SHELL_ALIGN(sizeof(struct nfor)),
	[NCASE    ] = SHELL_ALIGN(sizeof(struct ncase)),
	[NCLIST   ] = SHELL_ALIGN(sizeof(struct nclist)),
	[NDEFUN   ] = SHELL_ALIGN(sizeof(struct narg)),
	[NARG     ] = SHELL_ALIGN(sizeof(struct narg)),
	[NTO      ] = SHELL_ALIGN(sizeof(struct nfile)),
#if BASH_REDIR_OUTPUT
	[NTO2     ] = SHELL_ALIGN(sizeof(struct nfile)),
#endif
	[NCLOBBER ] = SHELL_ALIGN(sizeof(struct nfile)),
	[NFROM    ] = SHELL_ALIGN(sizeof(struct nfile)),
	[NFROMTO  ] = SHELL_ALIGN(sizeof(struct nfile)),
	[NAPPEND  ] = SHELL_ALIGN(sizeof(struct nfile)),
	[NTOFD    ] = SHELL_ALIGN(sizeof(struct ndup)),
	[NFROMFD  ] = SHELL_ALIGN(sizeof(struct ndup)),
	[NHERE    ] = SHELL_ALIGN(sizeof(struct nhere)),
	[NXHERE   ] = SHELL_ALIGN(sizeof(struct nhere)),
	[NNOT     ] = SHELL_ALIGN(sizeof(struct nnot)),
};

static int calcsize(int funcblocksize, union node *n);

static int
sizenodelist(int funcblocksize, struct nodelist *lp)
{
	while (lp) {
		funcblocksize += SHELL_ALIGN(sizeof(struct nodelist));
		funcblocksize = calcsize(funcblocksize, lp->n);
		lp = lp->next;
	}
	return funcblocksize;
}

static int
calcsize(int funcblocksize, union node *n)
{
	if (n == NULL)
		return funcblocksize;
	funcblocksize += nodesize[n->type];
	switch (n->type) {
	case NCMD:
		funcblocksize = calcsize(funcblocksize, n->ncmd.redirect);
		funcblocksize = calcsize(funcblocksize, n->ncmd.args);
		funcblocksize = calcsize(funcblocksize, n->ncmd.assign);
		break;
	case NPIPE:
		funcblocksize = sizenodelist(funcblocksize, n->npipe.cmdlist);
		break;
	case NREDIR:
	case NBACKGND:
	case NSUBSHELL:
		funcblocksize = calcsize(funcblocksize, n->nredir.redirect);
		funcblocksize = calcsize(funcblocksize, n->nredir.n);
		break;
	case NAND:
	case NOR:
	case NSEMI:
	case NWHILE:
	case NUNTIL:
		funcblocksize = calcsize(funcblocksize, n->nbinary.ch2);
		funcblocksize = calcsize(funcblocksize, n->nbinary.ch1);
		break;
	case NIF:
		funcblocksize = calcsize(funcblocksize, n->nif.elsepart);
		funcblocksize = calcsize(funcblocksize, n->nif.ifpart);
		funcblocksize = calcsize(funcblocksize, n->nif.test);
		break;
	case NFOR:
		funcblocksize += SHELL_ALIGN(strlen(n->nfor.var) + 1); /* was funcstringsize += ... */
		funcblocksize = calcsize(funcblocksize, n->nfor.body);
		funcblocksize = calcsize(funcblocksize, n->nfor.args);
		break;
	case NCASE:
		funcblocksize = calcsize(funcblocksize, n->ncase.cases);
		funcblocksize = calcsize(funcblocksize, n->ncase.expr);
		break;
	case NCLIST:
		funcblocksize = calcsize(funcblocksize, n->nclist.body);
		funcblocksize = calcsize(funcblocksize, n->nclist.pattern);
		funcblocksize = calcsize(funcblocksize, n->nclist.next);
		break;
	case NDEFUN:
		funcblocksize = calcsize(funcblocksize, n->ndefun.body);
		funcblocksize += SHELL_ALIGN(strlen(n->ndefun.text) + 1);
		break;
	case NARG:
		funcblocksize = sizenodelist(funcblocksize, n->narg.backquote);
		funcblocksize += SHELL_ALIGN(strlen(n->narg.text) + 1); /* was funcstringsize += ... */
		funcblocksize = calcsize(funcblocksize, n->narg.next);
		break;
	case NTO:
#if BASH_REDIR_OUTPUT
	case NTO2:
#endif
	case NCLOBBER:
	case NFROM:
	case NFROMTO:
	case NAPPEND:
		funcblocksize = calcsize(funcblocksize, n->nfile.fname);
		funcblocksize = calcsize(funcblocksize, n->nfile.next);
		break;
	case NTOFD:
	case NFROMFD:
		funcblocksize = calcsize(funcblocksize, n->ndup.vname);
		funcblocksize = calcsize(funcblocksize, n->ndup.next);
	break;
	case NHERE:
	case NXHERE:
		funcblocksize = calcsize(funcblocksize, n->nhere.doc);
		funcblocksize = calcsize(funcblocksize, n->nhere.next);
		break;
	case NNOT:
		funcblocksize = calcsize(funcblocksize, n->nnot.com);
		break;
	};
	return funcblocksize;
}

static char *
nodeckstrdup(const char *s)
{
#if ENABLE_PLATFORM_MINGW32
	if(!s)
		return NULL;
#endif
	funcstring_end -= SHELL_ALIGN(strlen(s) + 1);
	return strcpy(funcstring_end, s);
}

static union node *copynode(union node *);

#if ENABLE_PLATFORM_MINGW32
# if FORKSHELL_DEBUG
#  define FREE 1
#  define NO_FREE 2
#  define ANNOT(dst,note) { \
		if (annot) \
			annot[(char *)&dst - (char *)fs_start] = note; \
	}
# else
#  define FREE 1
#  define NO_FREE 1
#  define ANNOT(dst,note)
# endif

/* The relocation map is offset from the start of the forkshell data
 * block by 'fs_size' bytes.  The flag relating to a particular destination
 * pointer is thus at (dst+fs_size). */
# define MARK_PTR(dst,flag) {*((char *)&dst + fs_size) = flag;}

# define SAVE_PTR(dst,note,flag) { \
	if (fs_size) { \
		MARK_PTR(dst,flag); ANNOT(dst,note); \
	} \
}
# define SAVE_PTR2(dst1,note1,flag1,dst2,note2,flag2) { \
	if (fs_size) { \
		MARK_PTR(dst1,flag1); MARK_PTR(dst2,flag2); \
		ANNOT(dst1,note1); ANNOT(dst2,note2); \
	} \
}
# define SAVE_PTR3(dst1,note1,flag1,dst2,note2,flag2,dst3,note3,flag3) { \
	if (fs_size) { \
		MARK_PTR(dst1,flag1); MARK_PTR(dst2,flag2); MARK_PTR(dst3,flag3); \
		ANNOT(dst1,note1); ANNOT(dst2,note2); ANNOT(dst3,note3); \
	} \
}
#else
# define SAVE_PTR(dst,note,flag)
# define SAVE_PTR2(dst1,note1,flag1,dst2,note2,flag2)
# define SAVE_PTR3(dst1,note1,flag1,dst2,note2,flag2,dst3,note3,flag3)
#endif

static struct nodelist *
copynodelist(struct nodelist *lp)
{
	struct nodelist *start;
	struct nodelist **lpp;

	lpp = &start;
	while (lp) {
		*lpp = funcblock;
		funcblock = (char *) funcblock + SHELL_ALIGN(sizeof(struct nodelist));
		(*lpp)->n = copynode(lp->n);
		SAVE_PTR2((*lpp)->n, "(*lpp)->next", NO_FREE,
			(*lpp)->next, "(*lpp)->next", NO_FREE);
		lp = lp->next;
		lpp = &(*lpp)->next;
	}
	*lpp = NULL;
	return start;
}

static union node *
copynode(union node *n)
{
	union node *new;

	if (n == NULL)
		return NULL;
	new = funcblock;
	funcblock = (char *) funcblock + nodesize[n->type];

	switch (n->type) {
	case NCMD:
		new->ncmd.redirect = copynode(n->ncmd.redirect);
		new->ncmd.args = copynode(n->ncmd.args);
		new->ncmd.assign = copynode(n->ncmd.assign);
		new->ncmd.linno = n->ncmd.linno;
		SAVE_PTR3(new->ncmd.redirect, "ncmd.redirect", NO_FREE,
			new->ncmd.args, "ncmd.args", NO_FREE,
			new->ncmd.assign, "ncmd.assign", NO_FREE);
		break;
	case NPIPE:
		new->npipe.cmdlist = copynodelist(n->npipe.cmdlist);
		new->npipe.pipe_backgnd = n->npipe.pipe_backgnd;
		SAVE_PTR(new->npipe.cmdlist, "npipe.cmdlist", NO_FREE);
		break;
	case NREDIR:
	case NBACKGND:
	case NSUBSHELL:
		new->nredir.redirect = copynode(n->nredir.redirect);
		new->nredir.n = copynode(n->nredir.n);
		new->nredir.linno = n->nredir.linno;
		SAVE_PTR2(new->nredir.redirect, "nredir.redirect", NO_FREE,
			new->nredir.n, "nredir.n", NO_FREE);
		break;
	case NAND:
	case NOR:
	case NSEMI:
	case NWHILE:
	case NUNTIL:
		new->nbinary.ch2 = copynode(n->nbinary.ch2);
		new->nbinary.ch1 = copynode(n->nbinary.ch1);
		SAVE_PTR2(new->nbinary.ch1, "nbinary.ch1", NO_FREE,
			new->nbinary.ch2, "nbinary.ch2", NO_FREE);
		break;
	case NIF:
		new->nif.elsepart = copynode(n->nif.elsepart);
		new->nif.ifpart = copynode(n->nif.ifpart);
		new->nif.test = copynode(n->nif.test);
		SAVE_PTR3(new->nif.elsepart, "nif.elsepart", NO_FREE,
			new->nif.ifpart, "nif.ifpart", NO_FREE,
			new->nif.test, "nif.test", NO_FREE);
		break;
	case NFOR:
		new->nfor.var = nodeckstrdup(n->nfor.var);
		new->nfor.body = copynode(n->nfor.body);
		new->nfor.args = copynode(n->nfor.args);
		new->nfor.linno = n->nfor.linno;
		SAVE_PTR3(new->nfor.var,
				xasprintf("nfor.var '%s'", n->nfor.var ?: "NULL"), FREE,
			new->nfor.body, "nfor.body", NO_FREE,
			new->nfor.args, "nfor.args", NO_FREE);
		break;
	case NCASE:
		new->ncase.cases = copynode(n->ncase.cases);
		new->ncase.expr = copynode(n->ncase.expr);
		new->ncase.linno = n->ncase.linno;
		SAVE_PTR2(new->ncase.cases, "ncase.cases", NO_FREE,
			new->ncase.expr, "ncase.expr", NO_FREE);
		break;
	case NCLIST:
		new->nclist.body = copynode(n->nclist.body);
		new->nclist.pattern = copynode(n->nclist.pattern);
		new->nclist.next = copynode(n->nclist.next);
		SAVE_PTR3(new->nclist.body, "nclist.body", NO_FREE,
			new->nclist.pattern, "nclist.pattern", NO_FREE,
			new->nclist.next, "nclist.next", NO_FREE);
		break;
	case NDEFUN:
		new->ndefun.body = copynode(n->ndefun.body);
		new->ndefun.text = nodeckstrdup(n->ndefun.text);
		new->ndefun.linno = n->ndefun.linno;
		SAVE_PTR2(new->ndefun.body, "ndefun.body", NO_FREE,
			new->ndefun.text,
				xasprintf("ndefun.text '%s'", n->ndefun.text ?: "NULL"), FREE);
		break;
	case NARG:
		new->narg.backquote = copynodelist(n->narg.backquote);
		new->narg.text = nodeckstrdup(n->narg.text);
		new->narg.next = copynode(n->narg.next);
		SAVE_PTR3(new->narg.backquote, "narg.backquote", NO_FREE,
			new->narg.text,
				xasprintf("narg.text '%s'", n->narg.text ?: "NULL"), FREE,
			new->narg.next, "narg.next", NO_FREE);
		break;
	case NTO:
#if BASH_REDIR_OUTPUT
	case NTO2:
#endif
	case NCLOBBER:
	case NFROM:
	case NFROMTO:
	case NAPPEND:
		new->nfile.fname = copynode(n->nfile.fname);
		new->nfile.fd = n->nfile.fd;
		new->nfile.next = copynode(n->nfile.next);
		SAVE_PTR2(new->nfile.fname, "nfile.fname", NO_FREE,
			new->nfile.next, "nfile.next", NO_FREE);
		break;
	case NTOFD:
	case NFROMFD:
		new->ndup.vname = copynode(n->ndup.vname);
		new->ndup.dupfd = n->ndup.dupfd;
		new->ndup.fd = n->ndup.fd;
		new->ndup.next = copynode(n->ndup.next);
		SAVE_PTR2(new->ndup.vname, "ndup.vname", NO_FREE,
			new->ndup.next, "ndup.next", NO_FREE);
		break;
	case NHERE:
	case NXHERE:
		new->nhere.doc = copynode(n->nhere.doc);
		new->nhere.fd = n->nhere.fd;
		new->nhere.next = copynode(n->nhere.next);
		SAVE_PTR2(new->nhere.doc, "nhere.doc", NO_FREE,
			new->nhere.next, "nhere.next", NO_FREE);
		break;
	case NNOT:
		new->nnot.com = copynode(n->nnot.com);
		SAVE_PTR(new->nnot.com, "nnot.com", NO_FREE);
		break;
	};
	new->type = n->type;
	return new;
}

/*
 * Make a copy of a parse tree.
 */
static struct funcnode *
copyfunc(union node *n)
{
	struct funcnode *f;
	size_t blocksize;

	/*funcstringsize = 0;*/
	blocksize = offsetof(struct funcnode, n) + calcsize(0, n);
	f = ckzalloc(blocksize /* + funcstringsize */);
	funcblock = (char *) f + offsetof(struct funcnode, n);
	funcstring_end = (char *) f + blocksize;
	IF_PLATFORM_MINGW32(fs_size = 0);
	copynode(n);
	/* f->count = 0; - ckzalloc did it */
	return f;
}

/*
 * Define a shell function.
 */
static void
defun(union node *func)
{
	struct cmdentry entry;

	INT_OFF;
	entry.cmdtype = CMDFUNCTION;
	entry.u.func = copyfunc(func);
	addcmdentry(func->ndefun.text, &entry);
	INT_ON;
}

/* Reasons for skipping commands (see comment on breakcmd routine) */
#define SKIPBREAK      (1 << 0)
#define SKIPCONT       (1 << 1)
#define SKIPFUNC       (1 << 2)
#define SKIPFUNCDEF    (1 << 3)
static smallint evalskip;       /* set to SKIPxxx if we are skipping commands */
static int skipcount;           /* number of levels to skip */
#if ENABLE_PLATFORM_POSIX
static int loopnest;            /* current loop nesting level */
#endif
static int funcline;            /* starting line number of current function, or 0 if not in a function */

/* Forward decl way out to parsing code - dotrap needs it */
static int evalstring(char *s, int flags);

#if !ENABLE_PLATFORM_MINGW32
/* Called to execute a trap.
 * Single callsite - at the end of evaltree().
 * If we return non-zero, evaltree raises EXEXIT exception.
 *
 * Perhaps we should avoid entering new trap handlers
 * while we are executing a trap handler. [is it a TODO?]
 */
static void
dotrap(void)
{
	uint8_t *g;
	int sig;
	int status, last_status;

	if (!pending_sig)
		return;

	status = savestatus;
	last_status = status;
	if (status < 0) {
		status = exitstatus;
		savestatus = status;
	}
	pending_sig = 0;
	barrier();

	TRACE(("dotrap entered\n"));
	for (sig = 1, g = gotsig; sig < NSIG; sig++, g++) {
		char *p;

		if (!*g)
			continue;

		if (evalskip) {
			pending_sig = sig;
			break;
		}

		p = trap[sig];
		/* non-trapped SIGINT is handled separately by raise_interrupt,
		 * don't upset it by resetting gotsig[SIGINT-1] */
		if (sig == SIGINT && !p)
			continue;

		TRACE(("sig %d is active, will run handler '%s'\n", sig, p));
		*g = 0;
		if (!p)
			continue;
		trap_depth++;
		evalstring(p, 0);
		trap_depth--;
		if (evalskip != SKIPFUNC)
			exitstatus = status;
	}

	savestatus = last_status;
	TRACE(("dotrap returns\n"));
}
#else
# define dotrap()
#endif

/* forward declarations - evaluation is fairly recursive business... */
static int evalloop(union node *, int);
static int evalfor(union node *, int);
static int evalcase(union node *, int);
static int evalsubshell(union node *, int);
static void expredir(union node *);
static int evalpipe(union node *, int);
static int evalcommand(union node *, int);
static int evalbltin(const struct builtincmd *, int, char **, int);
static void prehash(union node *);

/*
 * Evaluate a parse tree.  The value is left in the global variable
 * exitstatus.
 */
static int
evaltree(union node *n, int flags)
{
	int checkexit = 0;
	int (*evalfn)(union node *, int);
	struct stackmark smark;
	int status = 0;

	setstackmark(&smark);

	if (nflag)
		goto out;

	if (n == NULL) {
		TRACE(("evaltree(NULL) called\n"));
		goto out;
	}
	TRACE(("evaltree(%p: %d, %d) called\n", n, n->type, flags));

	dotrap();

	switch (n->type) {
	default:
#if DEBUG
		out1fmt("Node type = %d\n", n->type);
		fflush_all();
		break;
#endif
	case NNOT:
		status = !evaltree(n->nnot.com, EV_TESTED);
		goto setstatus;
	case NREDIR:
		errlinno = lineno = n->nredir.linno;
		expredir(n->nredir.redirect);
		pushredir(n->nredir.redirect);
		status = redirectsafe(n->nredir.redirect, REDIR_PUSH);
		if (!status) {
			status = evaltree(n->nredir.n, flags & EV_TESTED);
		}
		if (n->nredir.redirect)
			popredir(/*drop:*/ 0);
		goto setstatus;
	case NCMD:
		evalfn = evalcommand;
 checkexit:
		checkexit = ~flags & EV_TESTED;
		goto calleval;
	case NFOR:
		evalfn = evalfor;
		goto calleval;
	case NWHILE:
	case NUNTIL:
		evalfn = evalloop;
		goto calleval;
	case NSUBSHELL:
	case NBACKGND:
		evalfn = evalsubshell;
		goto checkexit;
	case NPIPE:
		evalfn = evalpipe;
		goto checkexit;
	case NCASE:
		evalfn = evalcase;
		goto calleval;
	case NAND:
	case NOR:
	case NSEMI: {
#if NAND + 1 != NOR
#error NAND + 1 != NOR
#endif
#if NOR + 1 != NSEMI
#error NOR + 1 != NSEMI
#endif
		unsigned is_or = n->type - NAND;
		status = evaltree(
			n->nbinary.ch1,
			(flags | ((is_or >> 1) - 1)) & EV_TESTED
		);
		if ((!status) == is_or || evalskip)
			break;
		n = n->nbinary.ch2;
 evaln:
		evalfn = evaltree;
 calleval:
		status = evalfn(n, flags);
		goto setstatus;
	}
	case NIF:
		status = evaltree(n->nif.test, EV_TESTED);
		if (evalskip)
			break;
		if (!status) {
			n = n->nif.ifpart;
			goto evaln;
		} else if (n->nif.elsepart) {
			n = n->nif.elsepart;
			goto evaln;
		}
		status = 0;
		goto setstatus;
	case NDEFUN:
		defun(n);
		/* Not necessary. To test it:
		 * "false; f() { qwerty; }; echo $?" should print 0.
		 */
		/* status = 0; */
 setstatus:
		exitstatus = status;
		break;
	}
 out:
	/* Order of checks below is important:
	 * signal handlers trigger before exit caused by "set -e".
	 */
	dotrap();

	if (checkexit && status) {
		if (trap[NTRAP_ERR] && !in_trap_ERR) {
			int err;
			struct jmploc *volatile savehandler = exception_handler;
			struct jmploc jmploc;

			in_trap_ERR = 1;
			trap_depth++;
			err = setjmp(jmploc.loc);
			if (!err) {
				exception_handler = &jmploc;
				savestatus = exitstatus;
				evalstring(trap[NTRAP_ERR], 0);
			}
			trap_depth--;
			in_trap_ERR = 0;

			exception_handler = savehandler;
			if (err && exception_type != EXERROR)
				longjmp(exception_handler->loc, 1);

			exitstatus = savestatus;
		}
		if (eflag)
			goto exexit;
	}
	if (flags & EV_EXIT) {
 exexit:
		raise_exception(EXEND);
	}

	popstackmark(&smark);
	TRACE(("leaving evaltree (no interrupts)\n"));
	return exitstatus;
}

static int
skiploop(void)
{
	int skip = evalskip;

	switch (skip) {
	case 0:
		break;
	case SKIPBREAK:
	case SKIPCONT:
		if (--skipcount <= 0) {
			evalskip = 0;
			break;
		}
		skip = SKIPBREAK;
		break;
	}
	return skip;
}

static int
evalloop(union node *n, int flags)
{
	int skip;
	int status;

	loopnest++;
	status = 0;
	flags &= EV_TESTED;
	do {
		int i;

		i = evaltree(n->nbinary.ch1, EV_TESTED);
		skip = skiploop();
		if (skip == SKIPFUNC)
			status = i;
		if (skip)
			continue;
		if (n->type != NWHILE)
			i = !i;
		if (i != 0)
			break;
		status = evaltree(n->nbinary.ch2, flags);
		skip = skiploop();
	} while (!(skip & ~SKIPCONT));
	loopnest--;

	return status;
}

static int
evalfor(union node *n, int flags)
{
	struct arglist arglist;
	union node *argp;
	struct strlist *sp;
	int status = 0;

	errlinno = lineno = n->ncase.linno;

	arglist.list = NULL;
	arglist.lastp = &arglist.list;
	for (argp = n->nfor.args; argp; argp = argp->narg.next) {
		expandarg(argp, &arglist, EXP_FULL | EXP_TILDE);
	}
	*arglist.lastp = NULL;

	loopnest++;
	flags &= EV_TESTED;
	for (sp = arglist.list; sp; sp = sp->next) {
		setvar0(n->nfor.var, sp->text);
		status = evaltree(n->nfor.body, flags);
		if (skiploop() & ~SKIPCONT)
			break;
	}
	loopnest--;

	return status;
}

static int
evalcase(union node *n, int flags)
{
	union node *cp;
	union node *patp;
	struct arglist arglist;
	int status = 0;

	errlinno = lineno = n->ncase.linno;

	arglist.list = NULL;
	arglist.lastp = &arglist.list;
	expandarg(n->ncase.expr, &arglist, EXP_TILDE);
	for (cp = n->ncase.cases; cp && evalskip == 0; cp = cp->nclist.next) {
		for (patp = cp->nclist.pattern; patp; patp = patp->narg.next) {
			if (casematch(patp, arglist.list->text)) {
				/* Ensure body is non-empty as otherwise
				 * EV_EXIT may prevent us from setting the
				 * exit status.
				 */
				if (evalskip == 0 && cp->nclist.body) {
					status = evaltree(cp->nclist.body, flags);
				}
				goto out;
			}
		}
	}
 out:
	return status;
}

/*
 * Kick off a subshell to evaluate a tree.
 */
static int
evalsubshell(union node *n, int flags)
{
	IF_PLATFORM_MINGW32(struct forkshell fs;)
	struct job *jp;
	int backgnd = (n->type == NBACKGND); /* FORK_BG(1) if yes, else FORK_FG(0) */
	int status;

	errlinno = lineno = n->nredir.linno;

	expredir(n->nredir.redirect);
	if (!backgnd && (flags & EV_EXIT) && !may_have_traps)
		goto nofork;
	INT_OFF;
	if (backgnd == FORK_FG)
		get_tty_state();
	jp = makejob(/*n,*/ 1);
#if ENABLE_PLATFORM_MINGW32
	memset(&fs, 0, sizeof(fs));
	fs.fpid = FS_EVALSUBSHELL;
	fs.n = n;
	fs.flags = flags;
	spawn_forkshell(&fs, jp, n, backgnd);
	if ( 0 ) {
#else
	if (forkshell(jp, n, backgnd) == 0) {
		/* child */
		INT_ON;
		flags |= EV_EXIT;
		if (backgnd)
			flags &= ~EV_TESTED;
#endif
 nofork:
		redirect(n->nredir.redirect, 0);
		evaltreenr(n->nredir.n, flags);
		/* never returns */
	}
	/* parent */
	status = 0;
	if (backgnd == FORK_FG)
		status = waitforjob(jp);
	INT_ON;
	return status;
}

/*
 * Compute the names of the files in a redirection list.
 */
static void fixredir(union node *, const char *, int);
static void
expredir(union node *n)
{
	union node *redir;

	for (redir = n; redir; redir = redir->nfile.next) {
		struct arglist fn;

		fn.list = NULL;
		fn.lastp = &fn.list;
		switch (redir->type) {
		case NFROMTO:
		case NFROM:
		case NTO:
#if BASH_REDIR_OUTPUT
		case NTO2:
#endif
		case NCLOBBER:
		case NAPPEND:
			expandarg(redir->nfile.fname, &fn, EXP_TILDE | EXP_REDIR);
			TRACE(("expredir expanded to '%s'\n", fn.list->text));
#if BASH_REDIR_OUTPUT
 store_expfname:
#endif
#if 0
// By the design of stack allocator, the loop of this kind:
//	while true; do while true; do break; done </dev/null; done
// will look like a memory leak: ash plans to free expfname's
// of "/dev/null" as soon as it finishes running the loop
// (in this case, never).
// This "fix" is wrong:
			if (redir->nfile.expfname)
				stunalloc(redir->nfile.expfname);
// It results in corrupted state of stacked allocations.
#endif
			redir->nfile.expfname = fn.list->text;
			break;
		case NFROMFD:
		case NTOFD: /* >& */
			if (redir->ndup.vname) {
				expandarg(redir->ndup.vname, &fn, EXP_TILDE | EXP_REDIR);
				if (fn.list == NULL)
					ash_msg_and_raise_error("redir error");
#if BASH_REDIR_OUTPUT
				if (!isdigit_str9(fn.list->text)) {
					/* >&file, not >&fd */
					if (redir->nfile.fd != 1) /* 123>&file - BAD */
						ash_msg_and_raise_error("redir error");
					redir->type = NTO2;
					goto store_expfname;
				}
#endif
				fixredir(redir, fn.list->text, 1);
			}
			break;
		}
	}
}

/*
 * Evaluate a pipeline.  All the processes in the pipeline are children
 * of the process creating the pipeline.  (This differs from some versions
 * of the shell, which make the last process in a pipeline the parent
 * of all the rest.)
 */
static int
evalpipe(union node *n, int flags)
{
	IF_PLATFORM_MINGW32(struct forkshell fs;)
	struct job *jp;
	struct nodelist *lp;
	int pipelen;
	int prevfd;
	int pip[2];
	int status = 0;

	TRACE(("evalpipe(0x%lx) called\n", (long)n));
	pipelen = 0;
	for (lp = n->npipe.cmdlist; lp; lp = lp->next)
		pipelen++;
	flags |= EV_EXIT;
	INT_OFF;
	if (n->npipe.pipe_backgnd == 0)
		get_tty_state();
	jp = makejob(/*n,*/ pipelen);
	prevfd = -1;
	for (lp = n->npipe.cmdlist; lp; lp = lp->next) {
		prehash(lp->n);
		pip[1] = -1;
		if (lp->next) {
			if (pipe(pip) < 0) {
				close(prevfd);
				ash_msg_and_raise_perror("can't create pipe");
			}
		}
#if ENABLE_PLATFORM_MINGW32
		memset(&fs, 0, sizeof(fs));
		fs.fpid = FS_EVALPIPE;
		fs.flags = flags;
		fs.n = lp->n;
		fs.fd[0] = pip[0];
		fs.fd[1] = pip[1];
		fs.fd[2] = prevfd;
		spawn_forkshell(&fs, jp, lp->n, n->npipe.pipe_backgnd);
#else
		if (forkshell(jp, lp->n, n->npipe.pipe_backgnd) == 0) {
			/* child */
			INT_ON;
			if (pip[1] >= 0) {
				close(pip[0]);
			}
			if (prevfd > 0) {
				dup2(prevfd, 0);
				close(prevfd);
			}
			if (pip[1] > 1) {
				dup2(pip[1], 1);
				close(pip[1]);
			}
			evaltreenr(lp->n, flags);
			/* never returns */
		}
#endif
		/* parent */
		if (prevfd >= 0)
			close(prevfd);
		prevfd = pip[0];
		/* Don't want to trigger debugging */
		if (pip[1] != -1)
			close(pip[1]);
	}
	if (n->npipe.pipe_backgnd == 0) {
		status = waitforjob(jp);
		TRACE(("evalpipe:  job done exit status %d\n", status));
	}
	INT_ON;

	return status;
}

/* setinteractive needs this forward reference */
#if EDITING_HAS_get_exe_name
static const char *get_builtin_name(int i) FAST_FUNC;
#endif

/*
 * Controls whether the shell is interactive or not.
 */
static void
setinteractive(int on)
{
	static smallint is_interactive;

	if (++on == is_interactive)
		return;
	is_interactive = on;
	setsignal(SIGINT);
	setsignal(SIGQUIT);
	setsignal(SIGTERM);
	if (is_interactive > 1) {
#if !ENABLE_FEATURE_SH_EXTRA_QUIET
		/* Looks like they want an interactive shell */
		static smallint did_banner;

		if (!did_banner) {
			/* note: ash and hush share this string */
			out1fmt("\n\n%s %s\n"
				IF_ASH_HELP("Enter 'help' for a list of built-in commands.\n")
				"\n",
				bb_banner,
				"built-in shell (ash)"
			);
			did_banner = 1;
		}
#endif
#if ENABLE_FEATURE_EDITING
		if (!line_input_state) {
			line_input_state = new_line_input_t(FOR_SHELL | WITH_PATH_LOOKUP);
# if EDITING_HAS_get_exe_name
			line_input_state->get_exe_name = get_builtin_name;
# endif
		}
#endif
	}
}

static void
optschanged(void)
{
#if DEBUG
	opentrace();
#endif
	setinteractive(iflag);
	setjobctl(mflag);
#if ENABLE_FEATURE_EDITING_VI
	if (line_input_state) {
		if (viflag)
			line_input_state->flags |= VI_MODE;
		else
			line_input_state->flags &= ~VI_MODE;
	}
#else
	viflag = 0; /* forcibly keep the option off */
#endif
}

struct localvar_list {
	struct localvar_list *next;
	struct localvar *lv;
};

static struct localvar_list *localvar_stack;

/*
 * Called after a function returns.
 * Interrupts must be off.
 */
static void
poplocalvars(int keep)
{
	struct localvar_list *ll;
	struct localvar *lvp, *next;
	struct var *vp;

	INT_OFF;
	ll = localvar_stack;
	localvar_stack = ll->next;

	next = ll->lv;
	free(ll);

	while ((lvp = next) != NULL) {
		next = lvp->next;
		vp = lvp->vp;
		TRACE(("poplocalvar %s\n", vp ? vp->var_text : "-"));
		if (keep) {
			int bits = VSTRFIXED;

			if (lvp->flags != VUNSET) {
				if (vp->var_text == lvp->text)
					bits |= VTEXTFIXED;
				else if (!(lvp->flags & (VTEXTFIXED|VSTACK)))
					free((char*)lvp->text);
			}

			vp->flags &= ~bits;
			vp->flags |= (lvp->flags & bits);

			if ((vp->flags &
			     (VEXPORT|VREADONLY|VSTRFIXED|VUNSET)) == VUNSET)
				unsetvar(vp->var_text);
		} else if (vp == NULL) {	/* $- saved */
			memcpy(optlist, lvp->text, sizeof(optlist));
			free((char*)lvp->text);
			optschanged();
		} else if (lvp->flags == VUNSET) {
			vp->flags &= ~(VSTRFIXED|VREADONLY);
			unsetvar(vp->var_text);
		} else {
			if (vp->var_func)
				vp->var_func(var_end(lvp->text));
			if ((vp->flags & (VTEXTFIXED|VSTACK)) == 0)
				free((char*)vp->var_text);
			vp->flags = lvp->flags;
			vp->var_text = lvp->text;
		}
		free(lvp);
	}
	INT_ON;
}

/*
 * Create a new localvar environment.
 */
static struct localvar_list *
pushlocalvars(int push)
{
	struct localvar_list *ll;
	struct localvar_list *top;

	top = localvar_stack;
	if (!push)
		goto out;

	INT_OFF;
	ll = ckzalloc(sizeof(*ll));
	/*ll->lv = NULL; - zalloc did it */
	ll->next = top;
	localvar_stack = ll;
	INT_ON;
 out:
	return top;
}

static void
unwindlocalvars(struct localvar_list *stop)
{
	while (localvar_stack != stop)
		poplocalvars(0);
}

static int
evalfun(struct funcnode *func, int argc, char **argv, int flags)
{
	volatile struct shparam saveparam;
	struct jmploc *volatile savehandler;
	struct jmploc jmploc;
	int e;
	int savelineno;
	int savefuncline;
	char *savefuncname;
	char *savetrap = NULL;

	if (!Eflag) {
		savetrap = trap[NTRAP_ERR];
		trap[NTRAP_ERR] = NULL;
	}
	savelineno = lineno;
	saveparam = shellparam;
	savefuncline = funcline;
	savefuncname = funcname;
	savehandler = exception_handler;
	e = setjmp(jmploc.loc);
	if (e) {
		goto funcdone;
	}
	INT_OFF;
	exception_handler = &jmploc;
	shellparam.malloced = 0;
	func->count++;
	funcname = func->n.ndefun.text;
	funcline = func->n.ndefun.linno;
	INT_ON;
	shellparam.nparam = argc - 1;
	shellparam.p = argv + 1;
#if ENABLE_ASH_GETOPTS
	shellparam.optind = 1;
	shellparam.optoff = -1;
#endif
	evaltree(func->n.ndefun.body, flags & EV_TESTED);
 funcdone:
	INT_OFF;
	funcname = savefuncname;
	if (savetrap) {
		if (!trap[NTRAP_ERR])
			trap[NTRAP_ERR] = savetrap;
		else
			free(savetrap);
	}
	funcline = savefuncline;
	lineno = savelineno;
	freefunc(func);
	freeparam(&shellparam);
	shellparam = saveparam;
	exception_handler = savehandler;
	INT_ON;
	evalskip &= ~(SKIPFUNC | SKIPFUNCDEF);
	return e;
}

/*
 * Make a variable a local variable.  When a variable is made local, it's
 * value and flags are saved in a localvar structure.  The saved values
 * will be restored when the shell function returns.  We handle the name
 * "-" as a special case: it makes changes to "set +-options" local
 * (options will be restored on return from the function).
 */
static void
mklocal(char *name, int flags)
{
	struct localvar *lvp;
	struct var **vpp;
	struct var *vp;
	char *eq = strchr(name, '=');

	INT_OFF;
	/* Cater for duplicate "local". Examples:
	 * x=0; f() { local x=1; echo $x; local x; echo $x; }; f; echo $x
	 * x=0; f() { local x=1; echo $x; local x=2; echo $x; }; f; echo $x
	 */
	lvp = localvar_stack->lv;
	while (lvp) {
		if (lvp->vp && varcmp(lvp->vp->var_text, name) == 0) {
			if (eq)
				setvareq(name, 0);
			/* else:
			 * it's a duplicate "local VAR" declaration, do nothing
			 */
			goto ret;
		}
		lvp = lvp->next;
	}

	lvp = ckzalloc(sizeof(*lvp));
	if (LONE_DASH(name)) {
		char *p;
		p = ckmalloc(sizeof(optlist));
		lvp->text = memcpy(p, optlist, sizeof(optlist));
		vp = NULL;
	} else {
		vpp = hashvar(name);
		vp = *findvar(vpp, name);
		if (vp == NULL) {
			/* variable did not exist yet */
			if (eq)
				vp = setvareq(name, VSTRFIXED | flags);
			else
				vp = setvar(name, NULL, VSTRFIXED | flags);
			lvp->flags = VUNSET;
		} else {
			lvp->text = vp->var_text;
			lvp->flags = vp->flags;
			/* make sure neither "struct var" nor string gets freed
			 * during (un)setting:
			 */
			vp->flags |= VSTRFIXED|VTEXTFIXED;
			if (eq)
				setvareq(name, flags);
			else
				/* "local VAR" unsets VAR: */
				setvar0(name, NULL);
		}
	}
	lvp->vp = vp;
	lvp->next = localvar_stack->lv;
	localvar_stack->lv = lvp;
 ret:
	INT_ON;
}

/*
 * The "local" command.
 */
static int FAST_FUNC
localcmd(int argc UNUSED_PARAM, char **argv)
{
	char *name;

	if (!localvar_stack)
		ash_msg_and_raise_error("not in a function");

	argv = argptr;
	while ((name = *argv++) != NULL) {
		mklocal(name, 0);
	}
	return 0;
}

static int FAST_FUNC
falsecmd(int argc UNUSED_PARAM, char **argv UNUSED_PARAM)
{
	return 1;
}

static int FAST_FUNC
truecmd(int argc UNUSED_PARAM, char **argv UNUSED_PARAM)
{
	return 0;
}

static int FAST_FUNC
execcmd(int argc UNUSED_PARAM, char **argv)
{
	optionarg = NULL;
	while (nextopt("a:") != '\0')
		/* nextopt() sets optionarg to "-a ARGV0" */;

	argv = argptr;
	if (argv[0]) {
		char *prog;

		iflag = 0;              /* exit on error */
		mflag = 0;
		optschanged();
		/* We should set up signals for "exec CMD"
		 * the same way as for "CMD" without "exec".
		 * But optschanged->setinteractive->setsignal
		 * still thought we are a root shell. Therefore, for example,
		 * SIGQUIT is still set to IGN. Fix it:
		 */
		shlvl++;
		setsignal(SIGQUIT);
		/*setsignal(SIGTERM); - unnecessary because of iflag=0 */
		/*setsignal(SIGTSTP); - unnecessary because of mflag=0 */
		/*setsignal(SIGTTOU); - unnecessary because of mflag=0 */

		prog = argv[0];
		if (optionarg)
			argv[0] = optionarg;
		shellexec(prog, argv, pathval(), 0);
		/* NOTREACHED */
	}
	return 0;
}

/*
 * The return command.
 */
static int FAST_FUNC
returncmd(int argc UNUSED_PARAM, char **argv)
{
	int skip;
	int status;

	/*
	 * If called outside a function, do what ksh does;
	 * skip the rest of the file.
	 */
	if (argv[1]) {
		skip = SKIPFUNC;
		status = number(argv[1]);
	} else {
		skip = SKIPFUNCDEF;
		status = exitstatus;
	}
	evalskip = skip;

	return status;
}

/* Forward declarations for builtintab[] */
static int breakcmd(int, char **) FAST_FUNC;
static int dotcmd(int, char **) FAST_FUNC;
static int evalcmd(int, char **, int) FAST_FUNC;
static int exitcmd(int, char **) FAST_FUNC;
static int exportcmd(int, char **) FAST_FUNC;
#if ENABLE_ASH_GETOPTS
static int getoptscmd(int, char **) FAST_FUNC;
#endif
#if ENABLE_ASH_HELP
static int helpcmd(int, char **) FAST_FUNC;
#endif
#if MAX_HISTORY
static int historycmd(int, char **) FAST_FUNC;
#endif
#if ENABLE_FEATURE_SH_MATH
static int letcmd(int, char **) FAST_FUNC;
#endif
static int readcmd(int, char **) FAST_FUNC;
static int setcmd(int, char **) FAST_FUNC;
static int shiftcmd(int, char **) FAST_FUNC;
static int timescmd(int, char **) FAST_FUNC;
static int trapcmd(int, char **) FAST_FUNC;
static int umaskcmd(int, char **) FAST_FUNC;
static int unsetcmd(int, char **) FAST_FUNC;
static int ulimitcmd(int, char **) FAST_FUNC;

#define BUILTIN_NOSPEC          "0"
#define BUILTIN_SPECIAL         "1"
#define BUILTIN_REGULAR         "2"
#define BUILTIN_SPEC_REG        "3"
#define BUILTIN_ASSIGN          "4"
#define BUILTIN_SPEC_ASSG       "5"
#define BUILTIN_REG_ASSG        "6"
#define BUILTIN_SPEC_REG_ASSG   "7"

/* Stubs for calling non-FAST_FUNC's */
#if !ENABLE_PLATFORM_MINGW32
#if ENABLE_ASH_ECHO
static int FAST_FUNC echocmd(int argc, char **argv)   { return echo_main(argc, argv); }
#endif
#if ENABLE_ASH_PRINTF
static int FAST_FUNC printfcmd(int argc, char **argv) { return printf_main(argc, argv); }
#endif
#else
#if ENABLE_ASH_ECHO
static int FAST_FUNC echocmd(int argc, char **argv)
{
	int ret;
	INT_OFF;
	ret = echo_main(argc, argv);
	INT_ON;
	return ret;
}
#endif
#if ENABLE_ASH_PRINTF
static int FAST_FUNC printfcmd(int argc, char **argv)
{
	int ret;
	INT_OFF;
	ret = printf_main(argc, argv);
	INT_ON;
	return ret;
}
#endif
#endif
#if ENABLE_ASH_TEST || BASH_TEST2
static int FAST_FUNC testcmd(int argc, char **argv)   { return test_main(argc, argv); }
#endif

/* Keep these in proper order since it is searched via bsearch() */
static const struct builtincmd builtintab[] = {
	{ BUILTIN_SPEC_REG      "."       , dotcmd     },
	{ BUILTIN_SPEC_REG      ":"       , truecmd    },
#if ENABLE_ASH_TEST
	{ BUILTIN_REGULAR       "["       , testcmd    },
#endif
#if BASH_TEST2
	{ BUILTIN_REGULAR       "[["      , testcmd    },
#endif
#if ENABLE_ASH_ALIAS
	{ BUILTIN_REG_ASSG      "alias"   , aliascmd   },
#endif
#if JOBS
	{ BUILTIN_REGULAR       "bg"      , fg_bgcmd   },
#endif
	{ BUILTIN_SPEC_REG      "break"   , breakcmd   },
	{ BUILTIN_REGULAR       "cd"      , cdcmd      },
	{ BUILTIN_NOSPEC        "chdir"   , cdcmd      },
#if ENABLE_ASH_CMDCMD
	{ BUILTIN_REGULAR       "command" , commandcmd },
#endif
	{ BUILTIN_SPEC_REG      "continue", breakcmd   },
#if ENABLE_ASH_ECHO
	{ BUILTIN_REGULAR       "echo"    , echocmd    },
#endif
	{ BUILTIN_SPEC_REG      "eval"    , NULL       }, /*evalcmd() has a differing prototype*/
	{ BUILTIN_SPEC_REG      "exec"    , execcmd    },
	{ BUILTIN_SPEC_REG      "exit"    , exitcmd    },
	{ BUILTIN_SPEC_REG_ASSG "export"  , exportcmd  },
	{ BUILTIN_REGULAR       "false"   , falsecmd   },
#if JOBS
	{ BUILTIN_REGULAR       "fg"      , fg_bgcmd   },
#endif
#if ENABLE_ASH_GETOPTS
	{ BUILTIN_REGULAR       "getopts" , getoptscmd },
#endif
	{ BUILTIN_REGULAR       "hash"    , hashcmd    },
#if ENABLE_ASH_HELP
	{ BUILTIN_NOSPEC        "help"    , helpcmd    },
#endif
#if MAX_HISTORY
	{ BUILTIN_NOSPEC        "history" , historycmd },
#endif
#if JOBS || JOBS_WIN32
	{ BUILTIN_REGULAR       "jobs"    , jobscmd    },
	{ BUILTIN_REGULAR       "kill"    , killcmd    },
#endif
#if ENABLE_FEATURE_SH_MATH
	{ BUILTIN_NOSPEC        "let"     , letcmd     },
#endif
	{ BUILTIN_SPEC_REG_ASSG "local"   , localcmd   },
#if ENABLE_ASH_PRINTF
	{ BUILTIN_REGULAR       "printf"  , printfcmd  },
#endif
	{ BUILTIN_REGULAR       "pwd"     , pwdcmd     },
	{ BUILTIN_REGULAR       "read"    , readcmd    },
	{ BUILTIN_SPEC_REG_ASSG "readonly", exportcmd  },
	{ BUILTIN_SPEC_REG      "return"  , returncmd  },
	{ BUILTIN_SPEC_REG      "set"     , setcmd     },
	{ BUILTIN_SPEC_REG      "shift"   , shiftcmd   },
#if BASH_SOURCE
	{ BUILTIN_SPEC_REG      "source"  , dotcmd     },
#endif
#if ENABLE_ASH_TEST
	{ BUILTIN_REGULAR       "test"    , testcmd    },
#endif
	{ BUILTIN_SPEC_REG      "times"   , timescmd   },
	{ BUILTIN_SPEC_REG      "trap"    , trapcmd    },
	{ BUILTIN_REGULAR       "true"    , truecmd    },
	{ BUILTIN_REGULAR       "type"    , typecmd    },
	{ BUILTIN_REGULAR       "ulimit"  , ulimitcmd  },
	{ BUILTIN_REGULAR       "umask"   , umaskcmd   },
#if ENABLE_ASH_ALIAS
	{ BUILTIN_REGULAR       "unalias" , unaliascmd },
#endif
	{ BUILTIN_SPEC_REG      "unset"   , unsetcmd   },
	{ BUILTIN_REGULAR       "wait"    , waitcmd    },
};

/* Should match the above table! */
#define COMMANDCMD (builtintab + \
	/* . : */	2 + \
	/* [ */		1 * ENABLE_ASH_TEST + \
	/* [[ */	1 * BASH_TEST2 + \
	/* alias */	1 * ENABLE_ASH_ALIAS + \
	/* bg */	1 * JOBS + \
	/* break cd cddir  */	3)
#define EVALCMD (COMMANDCMD + \
	/* command */	1 * ENABLE_ASH_CMDCMD + \
	/* continue */	1 + \
	/* echo */	1 * ENABLE_ASH_ECHO + \
	0)
#define EXECCMD (EVALCMD + \
	/* eval */	1)

/*
 * Search the table of builtin commands.
 */
static int
pstrcmp1(const void *a, const void *b)
{
	return strcmp((char*)a, *(char**)b + 1);
}
static struct builtincmd *
find_builtin(const char *name)
{
	struct builtincmd *bp;

	bp = bsearch(
		name, builtintab, ARRAY_SIZE(builtintab), sizeof(builtintab[0]),
		pstrcmp1
	);
	return bp;
}

#if EDITING_HAS_get_exe_name
static const char * FAST_FUNC
get_builtin_name(int i)
{
	return /*i >= 0 &&*/ i < ARRAY_SIZE(builtintab) ? builtintab[i].name + 1 : NULL;
}
#endif

/*
 * Execute a simple command.
 */
static void unwindfiles(struct parsefile *stop);
static int
isassignment(const char *p)
{
	const char *q = endofname(p);
	if (p == q)
		return 0;
	return *q == '=';
}
static int FAST_FUNC
bltincmd(int argc UNUSED_PARAM, char **argv UNUSED_PARAM)
{
	/* Preserve exitstatus of a previous possible redirection
	 * as POSIX mandates */
	return back_exitstatus;
}

static int
evalcommand(union node *cmd, int flags)
{
	static const struct builtincmd null_bltin = {
		BUILTIN_REGULAR "", bltincmd
	};
	struct localvar_list *localvar_stop;
	struct parsefile *file_stop;
	struct redirtab *redir_stop;
	union node *argp;
	struct arglist arglist;
	struct arglist varlist;
	char **argv;
	int argc;
	struct strlist *osp;
	const struct strlist *sp;
	struct cmdentry cmdentry;
	struct job *jp;
	char *lastarg;
	const char *path;
	int spclbltin;
	int cmd_flag;
	int status;
	char **nargv;
	smallint cmd_is_exec;
	int vflags;
	int vlocal;

	errlinno = lineno = cmd->ncmd.linno;

	/* First expand the arguments. */
	TRACE(("evalcommand(0x%lx, %d) called\n", (long)cmd, flags));
#if BASH_PROCESS_SUBST
	redir_stop = redirlist;
#endif
	file_stop = g_parsefile;
	back_exitstatus = 0;

	cmdentry.cmdtype = CMDBUILTIN;
	cmdentry.u.cmd = &null_bltin;
	varlist.lastp = &varlist.list;
	*varlist.lastp = NULL;
	arglist.lastp = &arglist.list;
	*arglist.lastp = NULL;

	cmd_flag = 0;
	cmd_is_exec = 0;
	spclbltin = -1;
	vflags = 0;
	vlocal = 0;
	path = NULL;

	argc = 0;
	argp = cmd->ncmd.args;
	osp = fill_arglist(&arglist, &argp);
	if (osp) {
		int pseudovarflag = 0;

		for (;;) {
			find_command(arglist.list->text, &cmdentry,
					cmd_flag | DO_REGBLTIN, pathval());

			vlocal++;

			/* implement bltin and command here */
			if (cmdentry.cmdtype != CMDBUILTIN)
				break;

			pseudovarflag = IS_BUILTIN_ASSIGN(cmdentry.u.cmd);
			if (spclbltin < 0) {
				spclbltin = IS_BUILTIN_SPECIAL(cmdentry.u.cmd);
				vlocal = !spclbltin;
			}
			cmd_is_exec = cmdentry.u.cmd == EXECCMD;
#if ENABLE_ASH_CMDCMD
			if (cmdentry.u.cmd != COMMANDCMD)
				break;

			cmd_flag = parse_command_args(&arglist, &argp, &path);
			if (!cmd_flag)
#endif
				break;
		}

		for (; argp; argp = argp->narg.next)
			expandarg(argp, &arglist,
					pseudovarflag &&
					isassignment(argp->narg.text) ?
					EXP_VARTILDE : EXP_FULL | EXP_TILDE);

		for (sp = arglist.list; sp; sp = sp->next)
			argc++;

		if (cmd_is_exec && argc > 1)
			vflags = VEXPORT;
	}

	localvar_stop = pushlocalvars(vlocal);

	/* Reserve one extra spot at the front for shellexec. */
	nargv = stalloc(sizeof(char *) * (argc + 2));
	argv = ++nargv;
	for (sp = arglist.list; sp; sp = sp->next) {
		TRACE(("evalcommand arg: %s\n", sp->text));
		*nargv++ = sp->text;
	}
	*nargv = NULL;

	lastarg = NULL;
	if (iflag && funcline == 0 && argc > 0)
		lastarg = nargv[-1];

	expredir(cmd->ncmd.redirect);
#if !BASH_PROCESS_SUBST
	redir_stop = pushredir(cmd->ncmd.redirect);
#else
	pushredir(cmd->ncmd.redirect);
#endif
	preverrout_fd = 2;
	if (BASH_XTRACEFD && xflag) {
		/* NB: bash closes fd == $BASH_XTRACEFD when it is changed.
		 * we do not emulate this. We only use its value.
		 */
		const char *xtracefd = lookupvar("BASH_XTRACEFD");
		if (xtracefd && is_number(xtracefd))
			preverrout_fd = atoi(xtracefd);

	}
	status = redirectsafe(cmd->ncmd.redirect, REDIR_PUSH | REDIR_SAVEFD2);

	if (status) {
 bail:
		exitstatus = status;

		/* We have a redirection error. */
		if (spclbltin > 0)
			raise_exception(EXERROR);

		goto out;
	}

	for (argp = cmd->ncmd.assign; argp; argp = argp->narg.next) {
		struct strlist **spp;

		spp = varlist.lastp;
		expandarg(argp, &varlist, EXP_VARTILDE);

		if (vlocal)
			mklocal((*spp)->text, VEXPORT);
		else
			setvareq((*spp)->text, vflags);
	}

	/* Print the command if xflag is set. */
	if (xflag && !inps4) {
		const char *pfx = "";

		inps4 = 1;
		fdprintf(preverrout_fd, "%s", expandstr(ps4val(), DQSYNTAX));
		inps4 = 0;

		sp = varlist.list;
		while (sp) {
			char *varval = sp->text;
			char *eq = strchrnul(varval, '=');
			if (*eq)
				eq++;
			fdprintf(preverrout_fd, "%s%.*s%s",
				pfx,
				(int)(eq - varval), varval,
				maybe_single_quote(eq)
			);
			sp = sp->next;
			pfx = " ";
		}

		sp = arglist.list;
		while (sp) {
			fdprintf(preverrout_fd, "%s%s",
				pfx,
				/* always quote if matches reserved word: */
				findkwd(sp->text)
				? single_quote(sp->text)
				: maybe_single_quote(sp->text)
			);
			sp = sp->next;
			pfx = " ";
		}
		safe_write(preverrout_fd, "\n", 1);
	}

	/* Now locate the command. */
	if (cmdentry.cmdtype != CMDBUILTIN
	 || !(IS_BUILTIN_REGULAR(cmdentry.u.cmd))
	) {
		path = path ? path : pathval();
		find_command(argv[0], &cmdentry, cmd_flag | DO_ERR, path);
	}

	jp = NULL;

	/* Execute the command. */
	switch (cmdentry.cmdtype) {
	case CMDUNKNOWN:
		status = 127;
		flush_stdout_stderr();
		goto bail;

	default: {

#if ENABLE_FEATURE_SH_STANDALONE \
 && ENABLE_FEATURE_SH_NOFORK \
 && NUM_APPLETS > 1
/* (1) BUG: if variables are set, we need to fork, or save/restore them
 *     around run_nofork_applet() call.
 * (2) Should this check also be done in forkshell()?
 *     (perhaps it should, so that "VAR=VAL nofork" at least avoids exec...)
 */
		/* find_command() encodes applet_no as (-2 - applet_no) */
		int applet_no = (- cmdentry.u.index - 2);
		if (applet_no >= 0 && APPLET_IS_NOFORK(applet_no)) {
			char **sv_environ;

			INT_OFF;
			sv_environ = environ;
			environ = listvars(VEXPORT, VUNSET, varlist.list, /*end:*/ NULL);
			/*
			 * Run <applet>_main().
			 * Signals (^C) can't interrupt here.
			 * Otherwise we can mangle stdio or malloc internal state.
			 * This makes applets which can run for a long time
			 * and/or wait for user input ineligible for NOFORK:
			 * for example, "yes" or "rm" (rm -i waits for input).
			 */
			exitstatus = run_nofork_applet(applet_no, argv);
			environ = sv_environ;
			/*
			 * Try enabling NOFORK for "yes" applet.
			 * ^C _will_ stop it (write returns EINTR),
			 * but this causes stdout FILE to be stuck
			 * and needing clearerr(). What if other applets
			 * also can get EINTRs? Do we need to switch
			 * our signals to SA_RESTART?
			 */
			/*clearerr(stdout);*/
			INT_ON;
			break;
		}
#endif
		/* Can we avoid forking? For example, very last command
		 * in a script or a subshell does not need forking,
		 * we can just exec it.
		 */
#if ENABLE_PLATFORM_MINGW32
		if (!(flags & EV_EXIT) || may_have_traps) {
			/* No, forking off a child is necessary */
			struct forkshell fs;

			INT_OFF;
			memset(&fs, 0, sizeof(fs));
			fs.fpid = FS_SHELLEXEC;
			fs.argv = argv;
			fs.path = (char*)path;
			fs.fd[0] = cmdentry.u.index;
			jp = makejob(/*cmd,*/ 1);
			spawn_forkshell(&fs, jp, cmd, FORK_FG);
			break;
		}
#else
		if (!(flags & EV_EXIT) || may_have_traps) {
			/* No, forking off a child is necessary */
			INT_OFF;
			get_tty_state();
			jp = makejob(/*cmd,*/ 1);
			if (forkshell(jp, cmd, FORK_FG) != 0) {
				/* parent */
				break;
			}
			/* child */
			FORCE_INT_ON;
			/* fall through to exec'ing external program */
		}
#endif
		shellexec(argv[0], argv, path, cmdentry.u.index);
		/* NOTREACHED */
	} /* default */
	case CMDBUILTIN:
		if (evalbltin(cmdentry.u.cmd, argc, argv, flags)
		 && !(exception_type == EXERROR && spclbltin <= 0)
		) {
 raise:
			longjmp(exception_handler->loc, 1);
		}
		break;

	case CMDFUNCTION:
		if (evalfun(cmdentry.u.func, argc, argv, flags))
			goto raise;
		break;
	} /* switch */

	status = waitforjob(jp);
	if (jp)
		TRACE(("forked child exited with %d\n", status));
	FORCE_INT_ON;

 out:
	if (cmd->ncmd.redirect)
		popredir(/*drop:*/ cmd_is_exec);
	unwindredir(redir_stop);
	unwindfiles(file_stop);
	unwindlocalvars(localvar_stop);
	if (lastarg) {
		/* dsl: I think this is intended to be used to support
		 * '_' in 'vi' command mode during line editing...
		 * However I implemented that within libedit itself.
		 */
		setvar0("_", lastarg);
	}

	return status;
}

static int
evalbltin(const struct builtincmd *cmd, int argc, char **argv, int flags)
{
	char *volatile savecmdname;
	struct jmploc *volatile savehandler;
	struct jmploc jmploc;
	int status;
	int i;

	savecmdname = commandname;
	savehandler = exception_handler;
	i = setjmp(jmploc.loc);
	if (i)
		goto cmddone;
	exception_handler = &jmploc;
	commandname = argv[0];
	argptr = argv + 1;
	optptr = NULL;                  /* initialize nextopt */
	if (cmd == EVALCMD)
		status = evalcmd(argc, argv, flags);
	else
		status = (*cmd->builtin)(argc, argv);
	flush_stdout_stderr();
	status |= ferror(stdout);
	exitstatus = status;
 cmddone:
	clearerr(stdout);
	commandname = savecmdname;
	exception_handler = savehandler;

	return i;
}

static int
goodname(const char *p)
{
	return endofname(p)[0] == '\0';
}


/*
 * Search for a command.  This is called before we fork so that the
 * location of the command will be available in the parent as well as
 * the child.  The check for "goodname" is an overly conservative
 * check that the name will not be subject to expansion.
 */
static void
prehash(union node *n)
{
	struct cmdentry entry;

	if (n->type == NCMD && n->ncmd.args && goodname(n->ncmd.args->narg.text))
		find_command(n->ncmd.args->narg.text, &entry, 0, pathval());
}


/* ============ Builtin commands
 *
 * Builtin commands whose functions are closely tied to evaluation
 * are implemented here.
 */

/*
 * Handle break and continue commands.  Break, continue, and return are
 * all handled by setting the evalskip flag.  The evaluation routines
 * above all check this flag, and if it is set they start skipping
 * commands rather than executing them.  The variable skipcount is
 * the number of loops to break/continue, or the number of function
 * levels to return.  (The latter is always 1.)  It should probably
 * be an error to break out of more loops than exist, but it isn't
 * in the standard shell so we don't make it one here.
 */
static int FAST_FUNC
breakcmd(int argc UNUSED_PARAM, char **argv)
{
	int n = argv[1] ? number(argv[1]) : 1;

	if (n <= 0)
		ash_msg_and_raise_error(msg_illnum, argv[1]);
	if (n > loopnest)
		n = loopnest;
	if (n > 0) {
		evalskip = (**argv == 'c') ? SKIPCONT : SKIPBREAK;
		skipcount = n;
	}
	return 0;
}


/*
 * This implements the input routines used by the parser.
 */

enum {
	INPUT_PUSH_FILE = 1,
	INPUT_NOFILE_OK = 2,
};

static smallint checkkwd;
/* values of checkkwd variable */
#define CHKALIAS        0x1
#define CHKKWD          0x2
#define CHKNL           0x4
#define CHKEOFMARK      0x8

/*
 * Push a string back onto the input at this current parsefile level.
 * We handle aliases this way.
 */
#if !ENABLE_ASH_ALIAS
#define pushstring(s, ap) pushstring(s)
#endif
static void
pushstring(char *s, struct alias *ap)
{
	struct strpush *sp;
	int len;

	len = strlen(s);
	INT_OFF;
	if (g_parsefile->strpush || g_parsefile->spfree) {
		sp = ckzalloc(sizeof(*sp));
		sp->prev = g_parsefile->strpush;
	} else {
		sp = &(g_parsefile->basestrpush);
	}
	g_parsefile->strpush = sp;
	sp->prev_string = g_parsefile->next_to_pgetc;
	sp->prev_left_in_line = g_parsefile->left_in_line;
	sp->unget = g_parsefile->unget;
	sp->spfree = g_parsefile->spfree;
	memcpy(sp->lastc, g_parsefile->lastc, sizeof(sp->lastc));
#if ENABLE_ASH_ALIAS
	sp->ap = ap;
	if (ap) {
		ap->flag |= ALIASINUSE;
		sp->string = s;
	}
#endif
	g_parsefile->next_to_pgetc = s;
	g_parsefile->left_in_line = len;
	g_parsefile->unget = 0;
	g_parsefile->spfree = NULL;
	INT_ON;
}

static void popstring(void)
{
	struct strpush *sp = g_parsefile->strpush;

	INT_OFF;
#if ENABLE_ASH_ALIAS
	if (sp->ap) {
		if (g_parsefile->next_to_pgetc[-1] == ' '
		 || g_parsefile->next_to_pgetc[-1] == '\t'
		) {
			checkkwd |= CHKALIAS;
		}
		if (sp->string != sp->ap->val) {
			free(sp->string);
		}
	}
#endif
	g_parsefile->next_to_pgetc = sp->prev_string;
	g_parsefile->left_in_line = sp->prev_left_in_line;
	g_parsefile->unget = sp->unget;
	memcpy(g_parsefile->lastc, sp->lastc, sizeof(sp->lastc));
	g_parsefile->strpush = sp->prev;
	g_parsefile->spfree = sp;
	INT_ON;
}

static int
preadfd(void)
{
	int nr;
	char *buf = g_parsefile->buf;

	g_parsefile->next_to_pgetc = buf;
#if ENABLE_FEATURE_EDITING
 /* retry: */
	if (!iflag || g_parsefile->pf_fd != STDIN_FILENO)
		nr = nonblock_immune_read(g_parsefile->pf_fd, buf, IBUFSIZ - 1);
	else {
# if ENABLE_ASH_IDLE_TIMEOUT
		int timeout = -1;
		const char *tmout_var = lookupvar("TMOUT");
		if (tmout_var) {
			timeout = atoi(tmout_var) * 1000;
			if (timeout <= 0)
				timeout = -1;
		}
		line_input_state->timeout = timeout;
# endif
# if ENABLE_FEATURE_TAB_COMPLETION
		line_input_state->path_lookup = pathval();
# endif
		reinit_unicode_for_ash();
		nr = read_line_input(line_input_state, cmdedit_prompt, buf, IBUFSIZ);
		if (nr == 0) {
			/* ^C pressed, "convert" to SIGINT */
# if !ENABLE_PLATFORM_MINGW32
			write(STDOUT_FILENO, "^C", 2);
			raise(SIGINT);
# else
			raise_interrupt();
# endif
			if (trap[SIGINT]) {
				buf[0] = '\n';
				buf[1] = '\0';
				return 1;
			}
			exitstatus = 128 + SIGINT;
			return -1;
		}
		if (nr < 0) {
			if (errno == 0) {
				/* Ctrl+D pressed */
				nr = 0;
			}
# if ENABLE_ASH_IDLE_TIMEOUT
			else if (errno == EAGAIN && timeout > 0) {
				puts("\007timed out waiting for input: auto-logout");
				exitshell();
			}
# endif
		}
	}
#else
	nr = nonblock_immune_read(g_parsefile->pf_fd, buf, IBUFSIZ - 1);
#endif

#if 0 /* disabled: nonblock_immune_read() handles this problem */
	if (nr < 0) {
		if (parsefile->fd == 0 && errno == EWOULDBLOCK) {
			int flags = fcntl(0, F_GETFL);
			if (flags >= 0 && (flags & O_NONBLOCK)) {
				flags &= ~O_NONBLOCK;
				if (fcntl(0, F_SETFL, flags) >= 0) {
					out2str("sh: turning off NDELAY mode\n");
					goto retry;
				}
			}
		}
	}
#endif
	return nr;
}

/*
 * Refill the input buffer and return the next input character:
 *
 * 1) If a string was pushed back on the input, pop it;
 * 2) If we are reading from a string we can't refill the buffer, return EOF.
 * 3) If there is more stuff in this buffer, use it else call read to fill it.
 * 4) Process input up to the next newline, deleting nul characters.
 */
//#define pgetc_debug(...) bb_error_msg(__VA_ARGS__)
#define pgetc_debug(...) ((void)0)
static int __pgetc(void);
static int
preadbuffer(void)
{
	char *q;
	int more;

	if (unlikely(g_parsefile->strpush)) {
		popstring();
		return __pgetc();
	}

	if (g_parsefile->buf == NULL) {
		pgetc_debug("preadbuffer PEOF1");
		return PEOF;
	}

	more = g_parsefile->left_in_buffer;
	if (more <= 0) {
		flush_stdout_stderr();
 again:
		more = preadfd();
		if (more <= 0) {
			g_parsefile->left_in_buffer = g_parsefile->left_in_line = 0;
			pgetc_debug("preadbuffer PEOF2");
			return PEOF;
		}
	}

	/* Find out where's the end of line.
	 * Set g_parsefile->left_in_line
	 * and g_parsefile->left_in_buffer acordingly.
	 * NUL chars are deleted.
	 */
	q = g_parsefile->next_to_pgetc;
	for (;;) {
		char c;

		more--;

		c = *q;
		if (c == '\0' || (ENABLE_PLATFORM_MINGW32 && c == '\r')) {
			memmove(q, q + 1, more);
		} else {
			q++;
			if (c == '\n') {
				g_parsefile->left_in_line = q - g_parsefile->next_to_pgetc - 1;
				break;
			}
		}

		if (more <= 0) {
			g_parsefile->left_in_line = q - g_parsefile->next_to_pgetc - 1;
			if (g_parsefile->left_in_line < 0)
				goto again;
			break;
		}
	}
	g_parsefile->left_in_buffer = more;

	if (vflag) {
		char save = *q;
		*q = '\0';
		out2str(g_parsefile->next_to_pgetc);
		*q = save;
	}

	pgetc_debug("preadbuffer at %d:%p'%s'",
			g_parsefile->left_in_line,
			g_parsefile->next_to_pgetc,
			g_parsefile->next_to_pgetc);
	return (unsigned char)*g_parsefile->next_to_pgetc++;
}

static void
nlprompt(void)
{
	if (trap_depth == 0)
		g_parsefile->linno++;
	setprompt_if(doprompt, 2);
}
static void
nlnoprompt(void)
{
	if (trap_depth == 0)
		g_parsefile->linno++;
	needprompt = doprompt;
}

static void freestrings(struct strpush *sp)
{
	INT_OFF;
	do {
		struct strpush *psp;
#if ENABLE_ASH_ALIAS
		if (sp->ap) {
			sp->ap->flag &= ~ALIASINUSE;
			if (sp->ap->flag & ALIASDEAD) {
				unalias(sp->ap->name);
			}
		}
#endif
		psp = sp;
		sp = sp->spfree;

		if (psp != &(g_parsefile->basestrpush))
			free(psp);
	} while (sp);

	g_parsefile->spfree = NULL;
	INT_ON;
}

static int __pgetc(void)
{
	int c;

	pgetc_debug("pgetc at %d:%p'%s'",
			g_parsefile->left_in_line,
			g_parsefile->next_to_pgetc,
			g_parsefile->next_to_pgetc);
	if (g_parsefile->unget)
		return g_parsefile->lastc[--g_parsefile->unget];

	if (--g_parsefile->left_in_line >= 0)
		c = (unsigned char)*g_parsefile->next_to_pgetc++;
	else
		c = preadbuffer();

	g_parsefile->lastc[1] = g_parsefile->lastc[0];
	g_parsefile->lastc[0] = c;

	return c;
}

/*
 * Read a character from the script, returning PEOF on end of file.
 * Nul characters in the input are silently discarded.
 */
static int pgetc(void)
{
	struct strpush *sp = g_parsefile->spfree;

	if (unlikely(sp))
		freestrings(sp);

	return __pgetc();
}

/*
 * Undo a call to pgetc.  Only two characters may be pushed back.
 * PEOF may be pushed back.
 */
static void
pungetc(void)
{
	g_parsefile->unget++;
}

/* This one eats backslash+newline */
static int
pgetc_eatbnl(void)
{
	int c;

	while ((c = pgetc()) == '\\') {
		if (pgetc() != '\n') {
			pungetc();
			break;
		}

		nlprompt();
	}

	return c;
}

struct synstack {
	smalluint syntax;
	uint8_t innerdq   :1;
	uint8_t varpushed :1;
	uint8_t dblquote  :1;
	int varnest;		/* levels of variables expansion */
	int dqvarnest;		/* levels of variables expansion within double quotes */
	int parenlevel;		/* levels of parens in arithmetic */
	struct synstack *prev;
	struct synstack *next;
};

static int
pgetc_top(struct synstack *stack)
{
	return stack->syntax == SQSYNTAX ? pgetc() : pgetc_eatbnl();
}

static void
synstack_push(struct synstack **stack, struct synstack *next, int syntax)
{
	memset(next, 0, sizeof(*next));
	next->syntax = syntax;
	next->next = *stack;
	(*stack)->prev = next;
	*stack = next;
}

static ALWAYS_INLINE void
synstack_pop(struct synstack **stack)
{
	*stack = (*stack)->next;
}

/*
 * To handle the "." command, a stack of input files is used.  Pushfile
 * adds a new entry to the stack and popfile restores the previous level.
 */
static void
pushfile(void)
{
	struct parsefile *pf;

	pf = ckzalloc(sizeof(*pf));
	pf->prev = g_parsefile;
	pf->pf_fd = -1;
	/*pf->strpush = NULL; - ckzalloc did it */
	/*pf->spfree = NULL;*/
	/*pf->basestrpush.prev = NULL;*/
	/*pf->unget = 0;*/
	g_parsefile = pf;
}

static void
popfile(void)
{
	struct parsefile *pf = g_parsefile;

	if (pf == &basepf)
		return;

	INT_OFF;
	if (pf->pf_fd >= 0)
		close(pf->pf_fd);
	free(pf->buf);
	if (g_parsefile->spfree)
		freestrings(g_parsefile->spfree);
	while (pf->strpush) {
		popstring();
		freestrings(g_parsefile->spfree);
	}
	g_parsefile = pf->prev;
	free(pf);
	INT_ON;
}

static void
unwindfiles(struct parsefile *stop)
{
	while (g_parsefile != stop)
		popfile();
}

/*
 * Return to top level.
 */
static void
popallfiles(void)
{
	unwindfiles(&basepf);
}

#if !ENABLE_PLATFORM_MINGW32
/*
 * Close the file(s) that the shell is reading commands from.  Called
 * after a fork is done.
 */
static void
closescript(void)
{
	popallfiles();
	if (g_parsefile->pf_fd > 0) {
		close(g_parsefile->pf_fd);
		g_parsefile->pf_fd = 0;
	}
}
#endif

/*
 * Like setinputfile, but takes an open file descriptor.  Call this with
 * interrupts off.
 */
static void
setinputfd(int fd, int push)
{
	if (push) {
		pushfile();
		g_parsefile->buf = NULL;
	}
	g_parsefile->pf_fd = fd;
	if (g_parsefile->buf == NULL)
		g_parsefile->buf = ckmalloc(IBUFSIZ);
	g_parsefile->left_in_buffer = 0;
	g_parsefile->left_in_line = 0;
	g_parsefile->linno = 1;
}

/*
 * Set the input to take input from a file.  If push is set, push the
 * old input onto the stack first.
 */
static int
setinputfile(const char *fname, int flags)
{
	int fd;

	INT_OFF;
	fd = open(fname, O_RDONLY | O_CLOEXEC);
	if (fd < 0) {
		if (flags & INPUT_NOFILE_OK)
			goto out;
		exitstatus = 127;
		ash_msg_and_raise_perror("can't open '%s'", fname);
	}
	if (fd < 10)
		fd = savefd(fd);
	else if (O_CLOEXEC == 0) /* old libc */
		close_on_exec_on(fd);

	setinputfd(fd, flags & INPUT_PUSH_FILE);
 out:
	INT_ON;
	return fd;
}

/*
 * Like setinputfile, but takes input from a string.
 */
static void
setinputstring(char *string)
{
	INT_OFF;
	pushfile();
	g_parsefile->next_to_pgetc = string;
	g_parsefile->left_in_line = strlen(string);
	g_parsefile->buf = NULL;
	g_parsefile->linno = lineno;
	INT_ON;
}


/*
 * Routines to check for mail.
 */

#if ENABLE_ASH_MAIL

/* Hash of mtimes of mailboxes */
static unsigned mailtime_hash;
/* Set if MAIL or MAILPATH is changed. */
static smallint mail_var_path_changed;

/*
 * Print appropriate message(s) if mail has arrived.
 * If mail_var_path_changed is set,
 * then the value of MAIL has mail_var_path_changed,
 * so we just update the values.
 */
static void
chkmail(void)
{
	const char *mpath;
	char *p;
	char *q;
	unsigned new_hash;
	struct stackmark smark;
	struct stat statb;

	setstackmark(&smark);
	mpath = mpathset() ? mpathval() : mailval();
	new_hash = 0;
	for (;;) {
		int len;

		len = padvance_magic(&mpath, nullstr, 2);
		if (!len)
			break;
		p = stackblock();
			break;
		if (*p == '\0')
			continue;
		for (q = p; *q; q++)
			continue;
#if DEBUG
		if (q[-1] != '/')
			abort();
#endif
		q[-1] = '\0';                   /* delete trailing '/' */
		if (stat(p, &statb) < 0) {
			continue;
		}
		/* Very simplistic "hash": just a sum of all mtimes */
		new_hash += (unsigned)statb.st_mtime;
	}
	if (!mail_var_path_changed && mailtime_hash != new_hash) {
		if (mailtime_hash != 0)
			out2str("you have mail\n");
		mailtime_hash = new_hash;
	}
	mail_var_path_changed = 0;
	popstackmark(&smark);
}

static void FAST_FUNC
changemail(const char *val UNUSED_PARAM)
{
	mail_var_path_changed = 1;
}

#endif /* ASH_MAIL */


/* ============ ??? */

/*
 * Set the shell parameters.
 */
static void
setparam(char **argv)
{
	char **newparam;
	char **ap;
	int nparam;

	for (nparam = 0; argv[nparam]; nparam++)
		continue;
	ap = newparam = ckmalloc((nparam + 1) * sizeof(*ap));
	while (*argv) {
		*ap++ = ckstrdup(*argv++);
	}
	*ap = NULL;
	freeparam(&shellparam);
	shellparam.malloced = 1;
	shellparam.nparam = nparam;
	shellparam.p = newparam;
#if ENABLE_ASH_GETOPTS
	shellparam.optind = 1;
	shellparam.optoff = -1;
#endif
}

/*
 * Process shell options.  The global variable argptr contains a pointer
 * to the argument list; we advance it past the options.
 *
 * SUSv3 section 2.8.1 "Consequences of Shell Errors" says:
 * For a non-interactive shell, an error condition encountered
 * by a special built-in ... shall cause the shell to write a diagnostic message
 * to standard error and exit as shown in the following table:
 * Error                                           Special Built-In
 * ...
 * Utility syntax error (option or operand error)  Shall exit
 * ...
 * However, in bug 1142 (http://busybox.net/bugs/view.php?id=1142)
 * we see that bash does not do that (set "finishes" with error code 1 instead,
 * and shell continues), and people rely on this behavior!
 * Testcase:
 * set -o barfoo 2>/dev/null
 * echo $?
 *
 * Oh well. Let's mimic that.
 */
static int
plus_minus_o(char *name, int val)
{
	int i;

	if (name) {
		for (i = 0; i < NOPTS; i++) {
			if (strcmp(name, optnames(i)) == 0) {
				optlist[i] = val;
				return 0;
			}
		}
		ash_msg("illegal option %co %s", val ? '-' : '+', name);
		return 1;
	}
	for (i = 0; i < NOPTS; i++) {
		if (optnames(i)[0] == '\0')
			continue;
		if (val) {
			out1fmt("%-16s%s\n", optnames(i), optlist[i] ? "on" : "off");
		} else {
			out1fmt("set %co %s\n", optlist[i] ? '-' : '+', optnames(i));
		}
	}
	return 0;
}
static void
setoption(int flag, int val)
{
	int i;

	for (i = 0; i < NOPTS; i++) {
		if (optletters(i) == flag && optnames(i)[0] != '\0') {
			optlist[i] = val;
			return;
		}
	}
	ash_msg_and_raise_error("illegal option %c%c", val ? '-' : '+', flag);
	/* NOTREACHED */
}
/* If login_sh is not NULL, we are called to parse command line opts,
 * not "set -opts"
 */
static int
options(int *login_sh)
{
	char *p;
	int val;
	int c;

	if (login_sh) {
		minusc = NULL;
#if ENABLE_PLATFORM_MINGW32
		dirarg = NULL;
		title = NULL;
#endif
	}
	while ((p = *argptr) != NULL) {
		c = *p++;
		if (c != '-' && c != '+')
			break;
		argptr++;
		val = 0; /* val = 0 if c == '+' */
		if (c == '-') {
			val = 1;
			if (p[0] == '\0' || LONE_DASH(p)) {
				if (!login_sh) {
					/* "-" means turn off -x and -v */
					if (p[0] == '\0')
						xflag = vflag = 0;
					/* "--" means reset params */
					else if (*argptr == NULL)
						setparam(argptr);
				}
				break;    /* "-" or "--" terminates options */
			}
		}
		/* first char was + or - */
		while ((c = *p++) != '\0') {
			if (login_sh) {
				/* bash 3.2 indeed handles -c CMD and +c CMD the same */
				if (c == 'c') {
					minusc = p; /* command is after shell args */
					cflag = 1;
					continue;
				}
#if ENABLE_PLATFORM_MINGW32
				/* Undocumented flags;
				 *   -d force current directory
				 *   -t title to display in console window
				 * Must appear before -s or -c. */
				if (c == 'd' && val == 1) {
					if (*argptr == NULL)
						ash_msg_and_raise_error(bb_msg_requires_arg, "-d");
					dirarg = *argptr++;
					continue;
				}
				if (c == 't' && val == 1) {
					if (*argptr == NULL)
						ash_msg_and_raise_error(bb_msg_requires_arg, "-t");
					title = *argptr++;
					continue;
				}
#endif
				if (c == 's') { /* -s, +s */
					sflag = 1;
					continue;
				}
				if (c == 'i') { /* -i, +i */
					iflag = 1;
					continue;
				}
				if (c == 'l') {
					*login_sh = 1; /* -l or +l == --login */
					continue;
				}
				/* bash does not accept +-login, we also won't */
				if (val && (c == '-')) { /* long options */
					if (strcmp(p, "login") == 0) {
						*login_sh = 1;
					}
					break;
				}
			}
			if (c == 'o') {
				if (plus_minus_o(*argptr, val)) {
					/* it already printed err message */
					return 1; /* error */
				}
				if (*argptr)
					argptr++;
			} else {
				setoption(c, val);
			}
		}
	}
	return 0;
}

/*
 * The shift builtin command.
 */
static int FAST_FUNC
shiftcmd(int argc UNUSED_PARAM, char **argv)
{
	int n;
	char **ap1, **ap2;

	n = 1;
	if (argv[1])
		n = number(argv[1]);
	if (n > shellparam.nparam)
		return 1;
	INT_OFF;
	shellparam.nparam -= n;
	for (ap1 = shellparam.p; --n >= 0; ap1++) {
		if (shellparam.malloced)
			free(*ap1);
	}
	ap2 = shellparam.p;
	while ((*ap2++ = *ap1++) != NULL)
		continue;
#if ENABLE_ASH_GETOPTS
	shellparam.optind = 1;
	shellparam.optoff = -1;
#endif
	INT_ON;
	return 0;
}

/*
 * POSIX requires that 'set' (but not export or readonly) output the
 * variables in lexicographic order - by the locale's collating order (sigh).
 * Maybe we could keep them in an ordered balanced binary tree
 * instead of hashed lists.
 * For now just roll 'em through qsort for printing...
 */
static int
showvars(const char *sep_prefix, int on, int off)
{
	const char *sep;
	char **ep, **epend;

	ep = listvars(on, off, /*strlist:*/ NULL, &epend);
	qsort(ep, epend - ep, sizeof(char *), vpcmp);

	sep = *sep_prefix ? " " : sep_prefix;

	for (; ep < epend; ep++) {
		const char *p;
		const char *q;

		p = endofname(*ep);
/* Used to have simple "p = strchrnul(*ep, '=')" here instead, but this
 * makes "export -p" to have output not suitable for "eval":
 * import os
 * os.environ["test-test"]="test"
 * if os.fork() == 0:
 *   os.execv("ash", [ 'ash', '-c', 'eval $(export -p); echo OK' ])  # fixes this
 * os.execv("ash", [ 'ash', '-c', 'env | grep test-test' ])
 */
		q = nullstr;
		if (*p == '=')
			q = single_quote(++p);
		out1fmt("%s%s%.*s%s\n", sep_prefix, sep, (int)(p - *ep), *ep, q);
	}
	return 0;
}

/*
 * The set command builtin.
 */
static int FAST_FUNC
setcmd(int argc UNUSED_PARAM, char **argv UNUSED_PARAM)
{
	int retval;

	if (!argv[1])
		return showvars(nullstr, 0, VUNSET);

	INT_OFF;
	retval = options(/*login_sh:*/ NULL);
	if (retval == 0) { /* if no parse error... */
		optschanged();
		if (*argptr != NULL) {
			setparam(argptr);
		}
	}
	INT_ON;
	return retval;
}

#if ENABLE_ASH_RANDOM_SUPPORT
static void FAST_FUNC
change_random(const char *value)
{
	uint32_t t;

	if (value == NULL) {
		/* "get", generate */
		t = next_random(&random_gen);
		/* set without recursion */
		setvar(vrandom.var_text, utoa(t), VNOFUNC);
		vrandom.flags &= ~VNOFUNC;
	} else {
		/* set/reset */
		t = strtoul(value, NULL, 10);
		INIT_RANDOM_T(&random_gen, (t ? t : 1), t);
	}
}
#endif

#if BASH_EPOCH_VARS
static void FAST_FUNC
change_epoch(struct var *vepoch, const char *fmt)
{
	struct timeval tv;
	char buffer[sizeof("%llu.nnnnnn") + sizeof(long long)*3];

	xgettimeofday(&tv);
	sprintf(buffer, fmt, (unsigned long long)tv.tv_sec, (unsigned)tv.tv_usec);
	setvar(vepoch->var_text, buffer, VNOFUNC);
	vepoch->flags &= ~VNOFUNC;
}

static void FAST_FUNC
change_seconds(const char *value UNUSED_PARAM)
{
	change_epoch(&vepochs, "%llu");
}

static void FAST_FUNC
change_realtime(const char *value UNUSED_PARAM)
{
	change_epoch(&vepochr, "%llu.%06u");
}
#endif

#if ENABLE_ASH_GETOPTS
static int
getopts(char *optstr, char *optvar, char **optfirst)
{
	char *p, *q;
	char c = '?';
	int done = 0;
	char sbuf[2];
	char **optnext;
	int ind = shellparam.optind;
	int off = shellparam.optoff;

	sbuf[1] = '\0';

	shellparam.optind = -1;
	optnext = optfirst + ind - 1;

	if (ind <= 1 || off < 0 || (int)strlen(optnext[-1]) < off)
		p = NULL;
	else
		p = optnext[-1] + off;
	if (p == NULL || *p == '\0') {
		/* Current word is done, advance */
		p = *optnext;
		if (p == NULL || *p != '-' || *++p == '\0') {
 atend:
			unsetvar("OPTARG");
			p = NULL;
			done = 1;
			goto out;
		}
		optnext++;
		if (LONE_DASH(p))        /* check for "--" */
			goto atend;
	}

	c = *p++;
	for (q = optstr; *q != c;) {
		if (*q == '\0') {
			/* OPTERR is a bashism */
			const char *cp = lookupvar("OPTERR");
			if ((cp && LONE_CHAR(cp, '0'))
			 || (optstr[0] == ':')
			) {
				sbuf[0] = c;
				/*sbuf[1] = '\0'; - already is */
				setvar0("OPTARG", sbuf);
			} else {
				fprintf(stderr, "Illegal option -%c\n", c);
				unsetvar("OPTARG");
			}
			c = '?';
			goto out;
		}
		if (*++q == ':')
			q++;
	}

	if (*++q == ':') {
		if (*p == '\0' && (p = *optnext) == NULL) {
			/* OPTERR is a bashism */
			const char *cp = lookupvar("OPTERR");
			if ((cp && LONE_CHAR(cp, '0'))
			 || (optstr[0] == ':')
			) {
				sbuf[0] = c;
				/*sbuf[1] = '\0'; - already is */
				setvar0("OPTARG", sbuf);
				c = ':';
			} else {
				fprintf(stderr, "No arg for -%c option\n", c);
				unsetvar("OPTARG");
				c = '?';
			}
			goto out;
		}

		if (p == *optnext)
			optnext++;
		setvar0("OPTARG", p);
		p = NULL;
	} else
		setvar0("OPTARG", nullstr);
 out:
	ind = optnext - optfirst + 1;
	setvar("OPTIND", itoa(ind), VNOFUNC);
	sbuf[0] = c;
	/*sbuf[1] = '\0'; - already is */
	setvar0(optvar, sbuf);

	shellparam.optoff = p ? p - *(optnext - 1) : -1;
	shellparam.optind = ind;

	return done;
}

/*
 * The getopts builtin.  Shellparam.optnext points to the next argument
 * to be processed.  Shellparam.optptr points to the next character to
 * be processed in the current argument.  If shellparam.optnext is NULL,
 * then it's the first time getopts has been called.
 */
static int FAST_FUNC
getoptscmd(int argc, char **argv)
{
	char **optbase;

	if (argc < 3)
		ash_msg_and_raise_error("usage: getopts optstring var [arg]");
	if (argc == 3) {
		optbase = shellparam.p;
		if ((unsigned)shellparam.optind > shellparam.nparam + 1) {
			shellparam.optind = 1;
			shellparam.optoff = -1;
		}
	} else {
		optbase = &argv[3];
		if ((unsigned)shellparam.optind > argc - 2) {
			shellparam.optind = 1;
			shellparam.optoff = -1;
		}
	}

	return getopts(argv[1], argv[2], optbase);
}
#endif /* ASH_GETOPTS */


/* ============ Shell parser */

struct heredoc {
	struct heredoc *next;   /* next here document in list */
	union node *here;       /* redirection node */
	char *eofmark;          /* string indicating end of input */
	smallint striptabs;     /* if set, strip leading tabs */
};

static smallint tokpushback;           /* last token pushed back */
static smallint quoteflag;             /* set if (part of) last token was quoted */
static token_id_t lasttoken;           /* last token read (integer id Txxx) */
static struct heredoc *heredoclist;    /* list of here documents to read */
static char *wordtext;                 /* text of last word returned by readtoken */
static struct nodelist *backquotelist;
static union node *redirnode;
static struct heredoc *heredoc;

static const char *
tokname(char *buf, int tok)
{
	if (tok < TSEMI)
		return tokname_array[tok];
	sprintf(buf, "\"%s\"", tokname_array[tok]);
	return buf;
}

/* raise_error_unexpected_syntax:
 * Called when an unexpected token is read during the parse.  The argument
 * is the token that is expected, or -1 if more than one type of token can
 * occur at this point.
 */
static void raise_error_unexpected_syntax(int) NORETURN;
static void
raise_error_unexpected_syntax(int token)
{
	char msg[64];
	char buf[16];
	int l;

	l = sprintf(msg, "unexpected %s", tokname(buf, lasttoken));
	if (token >= 0)
		sprintf(msg + l, " (expecting %s)", tokname(buf, token));
	raise_error_syntax(msg);
	/* NOTREACHED */
}

/* parsing is heavily cross-recursive, need these forward decls */
static union node *andor(void);
static union node *pipeline(void);
static union node *parse_command(void);
static void parseheredoc(void);
static int readtoken(void);

static union node *
list(int nlflag)
{
	int chknl = nlflag & 1 ? 0 : CHKNL;
	union node *n1, *n2, *n3;
	int tok;

	n1 = NULL;
	for (;;) {
		checkkwd = chknl | CHKKWD | CHKALIAS;
		tok = readtoken();
		switch (tok) {
		case TNL:
			parseheredoc();
			return n1;

		case TEOF:
			if (!n1 && !chknl)
				n1 = NODE_EOF;
 out_eof:
			parseheredoc();
			tokpushback++;
			lasttoken = TEOF;
			return n1;
		}

		tokpushback++;
		if (nlflag == 2 && ((1 << tok) & tokendlist))
			return n1;
		nlflag |= 2;

		n2 = andor();
		tok = readtoken();
		if (tok == TBACKGND) {
			if (n2->type == NPIPE) {
				n2->npipe.pipe_backgnd = 1;
			} else {
				if (n2->type != NREDIR) {
					n3 = stzalloc(sizeof(struct nredir));
					n3->nredir.n = n2;
					/*n3->nredir.redirect = NULL; - stzalloc did it */
					n2 = n3;
				}
				n2->type = NBACKGND;
			}
		}
		if (n1 == NULL) {
			n1 = n2;
		} else {
			n3 = stzalloc(sizeof(struct nbinary));
			n3->type = NSEMI;
			n3->nbinary.ch1 = n1;
			n3->nbinary.ch2 = n2;
			n1 = n3;
		}
		switch (tok) {
		case TEOF:
			goto out_eof;
		case TNL:
			tokpushback = 1;
			/* fall through */
		case TBACKGND:
		case TSEMI:
			break;
		default:
			if (!chknl)
				raise_error_unexpected_syntax(-1);
			tokpushback = 1;
			return n1;
		}
	}
}

static union node *
andor(void)
{
	union node *n1, *n2, *n3;
	int t;

	n1 = pipeline();
	for (;;) {
		t = readtoken();
		if (t == TAND) {
			t = NAND;
		} else if (t == TOR) {
			t = NOR;
		} else {
			tokpushback = 1;
			return n1;
		}
		checkkwd = CHKNL | CHKKWD | CHKALIAS;
		n2 = pipeline();
		n3 = stzalloc(sizeof(struct nbinary));
		n3->type = t;
		n3->nbinary.ch1 = n1;
		n3->nbinary.ch2 = n2;
		n1 = n3;
	}
}

static union node *
pipeline(void)
{
	union node *n1, *n2, *pipenode;
	struct nodelist *lp, *prev;
	int negate;

	negate = 0;
	TRACE(("pipeline: entered\n"));
	if (readtoken() == TNOT) {
		negate = !negate;
		checkkwd = CHKKWD | CHKALIAS;
	} else
		tokpushback = 1;
	n1 = parse_command();
	if (readtoken() == TPIPE) {
		pipenode = stzalloc(sizeof(struct npipe));
		pipenode->type = NPIPE;
		/*pipenode->npipe.pipe_backgnd = 0; - stzalloc did it */
		lp = stzalloc(sizeof(struct nodelist));
		pipenode->npipe.cmdlist = lp;
		lp->n = n1;
		do {
			prev = lp;
			lp = stzalloc(sizeof(struct nodelist));
			checkkwd = CHKNL | CHKKWD | CHKALIAS;
			lp->n = parse_command();
			prev->next = lp;
		} while (readtoken() == TPIPE);
		lp->next = NULL;
		n1 = pipenode;
	}
	tokpushback = 1;
	if (negate) {
		n2 = stzalloc(sizeof(struct nnot));
		n2->type = NNOT;
		n2->nnot.com = n1;
		return n2;
	}
	return n1;
}

static union node *
makename(void)
{
	union node *n;

	n = stzalloc(sizeof(struct narg));
	n->type = NARG;
	/*n->narg.next = NULL; - stzalloc did it */
	n->narg.text = wordtext;
	n->narg.backquote = backquotelist;
	return n;
}

static void
fixredir(union node *n, const char *text, int err)
{
	int fd;

	TRACE(("Fix redir %s %d\n", text, err));
	if (!err)
		n->ndup.vname = NULL;

	fd = bb_strtou(text, NULL, 10);
	if (!errno && fd >= 0)
		n->ndup.dupfd = fd;
	else if (LONE_DASH(text))
		n->ndup.dupfd = -1;
	else {
		if (err)
			raise_error_syntax("bad fd number");
		n->ndup.vname = makename();
	}
}

static void
parsefname(void)
{
	union node *n = redirnode;

	if (n->type == NHERE)
		checkkwd = CHKEOFMARK;
	if (readtoken() != TWORD)
		raise_error_unexpected_syntax(-1);
	if (n->type == NHERE) {
		struct heredoc *here = heredoc;
		struct heredoc *p;

		if (quoteflag == 0)
			n->type = NXHERE;
		TRACE(("Here document %d\n", n->type));
		rmescapes(wordtext, 0, NULL);
		here->eofmark = wordtext;
		here->next = NULL;
		if (heredoclist == NULL)
			heredoclist = here;
		else {
			for (p = heredoclist; p->next; p = p->next)
				continue;
			p->next = here;
		}
	} else if (n->type == NTOFD || n->type == NFROMFD) {
		fixredir(n, wordtext, 0);
	} else {
		n->nfile.fname = makename();
	}
}

static union node *
simplecmd(void)
{
	union node *args, **app;
	union node *n = NULL;
	union node *vars, **vpp;
	union node **rpp, *redir;
	int savecheckkwd;
	int savelinno;
#if BASH_TEST2
	smallint double_brackets_flag = 0;
#endif
	IF_BASH_FUNCTION(smallint function_flag = 0;)

	args = NULL;
	app = &args;
	vars = NULL;
	vpp = &vars;
	redir = NULL;
	rpp = &redir;

	savecheckkwd = CHKALIAS;
	savelinno = g_parsefile->linno;
	for (;;) {
		int t;
		checkkwd = savecheckkwd;
		t = readtoken();
		switch (t) {
#if BASH_FUNCTION
		case TFUNCTION:
			if (readtoken() != TWORD)
				raise_error_unexpected_syntax(TWORD);
			tokpushback = 1;
			function_flag = 1;
			break;
#endif
#if BASH_TEST2
		case TAND: /* "&&" */
		case TOR: /* "||" */
			if (!double_brackets_flag) {
				tokpushback = 1;
				goto out;
			}
			/* pass "&&" or "||" to [[ ]] as literal args */
			wordtext = (char *) (t == TAND ? "&&" : "||");
#endif
		case TWORD:
			n = stzalloc(sizeof(struct narg));
			n->type = NARG;
			/*n->narg.next = NULL; - stzalloc did it */
			n->narg.text = wordtext;
#if BASH_TEST2
			if (strcmp("[[", wordtext) == 0)
				double_brackets_flag = 1;
			else if (strcmp("]]", wordtext) == 0)
				double_brackets_flag = 0;
#endif
			n->narg.backquote = backquotelist;
			if (savecheckkwd && isassignment(wordtext)) {
				*vpp = n;
				vpp = &n->narg.next;
			} else {
				*app = n;
				app = &n->narg.next;
				savecheckkwd = 0;
			}
#if BASH_FUNCTION
			if (function_flag) {
				checkkwd = CHKNL | CHKKWD;
				t = readtoken();
				tokpushback = 1;
				switch (t) {
				case TBEGIN:
				case TIF:
				case TCASE:
				case TUNTIL:
				case TWHILE:
				case TFOR:
					goto do_func;
				case TLP:
					function_flag = 0;
					break;
# if BASH_TEST2
				case TWORD:
					if (strcmp("[[", wordtext) == 0)
						goto do_func;
					/* fall through */
# endif
				default:
					raise_error_unexpected_syntax(-1);
				}
			}
#endif
			break;
		case TREDIR:
			*rpp = n = redirnode;
			rpp = &n->nfile.next;
			parsefname();   /* read name of redirection file */
			break;
		case TLP:
 IF_BASH_FUNCTION(do_func:)
			if (args && app == &args->narg.next
			 && !vars && !redir
			) {
				struct builtincmd *bcmd;
				const char *name;

				/* We have a function */
				if (IF_BASH_FUNCTION(!function_flag &&) readtoken() != TRP)
					raise_error_unexpected_syntax(TRP);
				name = n->narg.text;
				if (!goodname(name)
				 || ((bcmd = find_builtin(name)) && IS_BUILTIN_SPECIAL(bcmd))
				) {
					raise_error_syntax("bad function name");
				}
				n->type = NDEFUN;
				checkkwd = CHKNL | CHKKWD | CHKALIAS;
				n->ndefun.text = n->narg.text;
				n->ndefun.linno = g_parsefile->linno;
				n->ndefun.body = parse_command();
				return n;
			}
			IF_BASH_FUNCTION(function_flag = 0;)
			/* fall through */
		default:
			tokpushback = 1;
			goto out;
		}
	}
 out:
	*app = NULL;
	*vpp = NULL;
	*rpp = NULL;
	n = stzalloc(sizeof(struct ncmd));
	if (NCMD != 0)
		n->type = NCMD;
	n->ncmd.linno = savelinno;
	n->ncmd.args = args;
	n->ncmd.assign = vars;
	n->ncmd.redirect = redir;
	return n;
}

static union node *
parse_command(void)
{
	union node *n1, *n2;
	union node *ap, **app;
	union node *cp, **cpp;
	union node *redir, **rpp;
	union node **rpp2;
	int t;
	int savelinno;

	redir = NULL;
	rpp2 = &redir;

	savelinno = g_parsefile->linno;

	switch (readtoken()) {
	default:
		raise_error_unexpected_syntax(-1);
		/* NOTREACHED */
	case TIF:
		n1 = stzalloc(sizeof(struct nif));
		n1->type = NIF;
		n1->nif.test = list(0);
		if (readtoken() != TTHEN)
			raise_error_unexpected_syntax(TTHEN);
		n1->nif.ifpart = list(0);
		n2 = n1;
		while (readtoken() == TELIF) {
			n2->nif.elsepart = stzalloc(sizeof(struct nif));
			n2 = n2->nif.elsepart;
			n2->type = NIF;
			n2->nif.test = list(0);
			if (readtoken() != TTHEN)
				raise_error_unexpected_syntax(TTHEN);
			n2->nif.ifpart = list(0);
		}
		if (lasttoken == TELSE)
			n2->nif.elsepart = list(0);
		else {
			n2->nif.elsepart = NULL;
			tokpushback = 1;
		}
		t = TFI;
		break;
	case TWHILE:
	case TUNTIL: {
		int got;
		n1 = stzalloc(sizeof(struct nbinary));
		n1->type = (lasttoken == TWHILE) ? NWHILE : NUNTIL;
		n1->nbinary.ch1 = list(0);
		got = readtoken();
		if (got != TDO) {
			TRACE(("expecting DO got '%s' %s\n", tokname_array[got],
					got == TWORD ? wordtext : ""));
			raise_error_unexpected_syntax(TDO);
		}
		n1->nbinary.ch2 = list(0);
		t = TDONE;
		break;
	}
	case TFOR:
		if (readtoken() != TWORD || quoteflag || !goodname(wordtext))
			raise_error_syntax("bad for loop variable");
		n1 = stzalloc(sizeof(struct nfor));
		n1->type = NFOR;
		n1->nfor.linno = savelinno;
		n1->nfor.var = wordtext;
		checkkwd = CHKNL | CHKKWD | CHKALIAS;
		if (readtoken() == TIN) {
			app = &ap;
			while (readtoken() == TWORD) {
				n2 = stzalloc(sizeof(struct narg));
				n2->type = NARG;
				/*n2->narg.next = NULL; - stzalloc did it */
				n2->narg.text = wordtext;
				n2->narg.backquote = backquotelist;
				*app = n2;
				app = &n2->narg.next;
			}
			*app = NULL;
			n1->nfor.args = ap;
			if (lasttoken != TNL && lasttoken != TSEMI)
				raise_error_unexpected_syntax(-1);
		} else {
			n2 = stzalloc(sizeof(struct narg));
			n2->type = NARG;
			/*n2->narg.next = NULL; - stzalloc did it */
			n2->narg.text = (char *)dolatstr;
			/*n2->narg.backquote = NULL;*/
			n1->nfor.args = n2;
			/*
			 * Newline or semicolon here is optional (but note
			 * that the original Bourne shell only allowed NL).
			 */
			if (lasttoken != TSEMI)
				tokpushback = 1;
		}
		checkkwd = CHKNL | CHKKWD | CHKALIAS;
		if (readtoken() != TDO)
			raise_error_unexpected_syntax(TDO);
		n1->nfor.body = list(0);
		t = TDONE;
		break;
	case TCASE:
		n1 = stzalloc(sizeof(struct ncase));
		n1->type = NCASE;
		n1->ncase.linno = savelinno;
		if (readtoken() != TWORD)
			raise_error_unexpected_syntax(TWORD);
		n1->ncase.expr = n2 = stzalloc(sizeof(struct narg));
		n2->type = NARG;
		/*n2->narg.next = NULL; - stzalloc did it */
		n2->narg.text = wordtext;
		n2->narg.backquote = backquotelist;
		checkkwd = CHKNL | CHKKWD | CHKALIAS;
		if (readtoken() != TIN)
			raise_error_unexpected_syntax(TIN);
		cpp = &n1->ncase.cases;
 next_case:
		checkkwd = CHKNL | CHKKWD;
		t = readtoken();
		while (t != TESAC) {
			if (lasttoken == TLP)
				readtoken();
			*cpp = cp = stzalloc(sizeof(struct nclist));
			cp->type = NCLIST;
			app = &cp->nclist.pattern;
			for (;;) {
				*app = ap = stzalloc(sizeof(struct narg));
				ap->type = NARG;
				/*ap->narg.next = NULL; - stzalloc did it */
				ap->narg.text = wordtext;
				ap->narg.backquote = backquotelist;
				if (readtoken() != TPIPE)
					break;
				app = &ap->narg.next;
				readtoken();
			}
			//ap->narg.next = NULL;
			if (lasttoken != TRP)
				raise_error_unexpected_syntax(TRP);
			cp->nclist.body = list(2);

			cpp = &cp->nclist.next;

			checkkwd = CHKNL | CHKKWD;
			t = readtoken();
			if (t != TESAC) {
				if (t != TENDCASE)
					raise_error_unexpected_syntax(TENDCASE);
				goto next_case;
			}
		}
		*cpp = NULL;
		goto redir;
	case TLP:
		n1 = stzalloc(sizeof(struct nredir));
		n1->type = NSUBSHELL;
		n1->nredir.linno = savelinno;
		n1->nredir.n = list(0);
		/*n1->nredir.redirect = NULL; - stzalloc did it */
		t = TRP;
		break;
	case TBEGIN:
		n1 = list(0);
		t = TEND;
		break;
	IF_BASH_FUNCTION(case TFUNCTION:)
	case TWORD:
	case TREDIR:
		tokpushback = 1;
		return simplecmd();
	}

	if (readtoken() != t)
		raise_error_unexpected_syntax(t);

 redir:
	/* Now check for redirection which may follow command */
	checkkwd = CHKKWD | CHKALIAS;
	rpp = rpp2;
	while (readtoken() == TREDIR) {
		*rpp = n2 = redirnode;
		rpp = &n2->nfile.next;
		parsefname();
	}
	tokpushback = 1;
	*rpp = NULL;
	if (redir) {
		if (n1->type != NSUBSHELL) {
			n2 = stzalloc(sizeof(struct nredir));
			n2->type = NREDIR;
			n2->nredir.linno = savelinno;
			n2->nredir.n = n1;
			n1 = n2;
		}
		n1->nredir.redirect = redir;
	}
	return n1;
}

#if BASH_DOLLAR_SQUOTE
static int
decode_dollar_squote(void)
{
	static const char C_escapes[] ALIGN1 = "nrbtfav""x\\01234567";
	int c, cnt;
	char *p;
	char buf[4];

	c = pgetc();
	p = strchr(C_escapes, c);
	if (p) {
		buf[0] = c;
		p = buf;
		cnt = 3;
		if ((unsigned char)(c - '0') <= 7) { /* \ooo */
			do {
				c = pgetc();
				*++p = c;
			} while ((unsigned char)(c - '0') <= 7 && --cnt);
			pungetc();
		} else if (c == 'x') { /* \xHH */
			do {
				c = pgetc();
				*++p = c;
			} while (isxdigit(c) && --cnt);
			pungetc();
			if (cnt == 3) { /* \x but next char is "bad" */
				c = 'x';
				goto unrecognized;
			}
		} else { /* simple seq like \\ or \t */
			p++;
		}
		*p = '\0';
		p = buf;
		c = bb_process_escape_sequence((void*)&p);
	} else { /* unrecognized "\z": print both chars unless ' or " */
		if (c != '\'' && c != '"') {
 unrecognized:
			c |= 0x100; /* "please encode \, then me" */
		}
	}
	return c;
}
#endif

/* Used by expandstr to get here-doc like behaviour. */
#define FAKEEOFMARK ((char*)(uintptr_t)1)

static ALWAYS_INLINE int
realeofmark(const char *eofmark)
{
	return eofmark && eofmark != FAKEEOFMARK;
}

/*
 * If eofmark is NULL, read a word or a redirection symbol.  If eofmark
 * is not NULL, read a here document.  In the latter case, eofmark is the
 * word which marks the end of the document and striptabs is true if
 * leading tabs should be stripped from the document.  The argument c
 * is the first character of the input token or document.
 *
 * Because C does not have internal subroutines, I have simulated them
 * using goto's to implement the subroutine linkage.  The following macros
 * will run code that appears at the end of readtoken1.
 */
#define CHECKEND()      {goto checkend; checkend_return:;}
#define PARSEREDIR()    {goto parseredir; parseredir_return:;}
#define PARSESUB()      {goto parsesub; parsesub_return:;}
#define PARSEBACKQOLD() {style = OLD; goto parsebackq; parsebackq_oldreturn:;}
#define PARSEBACKQNEW() {style = NEW; goto parsebackq; parsebackq_newreturn:;}
#define PARSEPROCSUB()  {style = PSUB; goto parsebackq; parsebackq_psreturn:;}
#define PARSEARITH()    {goto parsearith; parsearith_return:;}
static int
readtoken1(int c, int syntax, char *eofmark, int striptabs)
{
	/* NB: syntax parameter fits into smallint */
	/* c parameter is an unsigned char or PEOF */
	char *out;
	size_t len;
	struct nodelist *bqlist;
	smallint quotef;
	smallint style;
	enum { OLD, NEW, PSUB };
#define oldstyle (style == OLD)
	smallint pssyntax;   /* we are expanding a prompt string */
	IF_BASH_DOLLAR_SQUOTE(smallint bash_dollar_squote = 0;)
	/* syntax stack */
	struct synstack synbase = { };
	struct synstack *synstack = &synbase;

#if ENABLE_ASH_EXPAND_PRMT
	pssyntax = (syntax == PSSYNTAX);
	if (pssyntax)
		syntax = DQSYNTAX;
#else
	pssyntax = 0; /* constant */
#endif
	synstack->syntax = syntax;

	if (syntax == DQSYNTAX)
		synstack->dblquote = 1;
	quotef = 0;
	bqlist = NULL;

	STARTSTACKSTR(out);
 loop:
	/* For each line, until end of word */
	CHECKEND();     /* set c to PEOF if at end of here document */
	for (;;) {      /* until end of line or end of word */
		CHECKSTRSPACE(4, out);  /* permit 4 calls to USTPUTC */
		switch (SIT(c, synstack->syntax)) {
		case CNL:       /* '\n' */
			if (synstack->syntax == BASESYNTAX
			 && !synstack->varnest
			) {
				goto endword;   /* exit outer loop */
			}
			USTPUTC(c, out);
			nlprompt();
			c = pgetc_top(synstack);
			goto loop;              /* continue outer loop */
		case CWORD:
			USTPUTC(c, out);
			break;
		case CCTL:
#if BASH_DOLLAR_SQUOTE
			if (c == '\\' && bash_dollar_squote) {
				c = decode_dollar_squote();
				if (c == '\0') {
					/* skip $'\000', $'\x00' (like bash) */
					break;
				}
				if (c & 0x100) {
					/* Unknown escape. Encode as '\z' */
					c = (unsigned char)c;
					if (eofmark == NULL || synstack->dblquote)
						USTPUTC(CTLESC, out);
					USTPUTC('\\', out);
				}
			}
#endif
			if (!eofmark || synstack->dblquote || synstack->varnest)
				USTPUTC(CTLESC, out);
			USTPUTC(c, out);
			break;
		case CBACK:     /* backslash */
			c = pgetc();
			if (c == PEOF) {
				USTPUTC(CTLESC, out);
				USTPUTC('\\', out);
				pungetc();
			} else {
				if (pssyntax && c == '$') {
					USTPUTC(CTLESC, out);
					USTPUTC('\\', out);
				}
				/* Backslash is retained if we are in "str"
				 * and next char isn't dquote-special.
				 */
				if (synstack->dblquote
				 && c != '\\'
				 && c != '`'
				 && c != '$'
				 && (c != '"' || (eofmark != NULL && !synstack->varnest))
				 && (c != '}' || !synstack->varnest)
				) {
					USTPUTC(CTLESC, out); /* protect '\' from glob */
					USTPUTC('\\', out);
				}
				USTPUTC(CTLESC, out);
				USTPUTC(c, out);
				quotef = 1;
			}
			break;
		case CSQUOTE:
			synstack->syntax = SQSYNTAX;
 quotemark:
			if (eofmark == NULL) {
				USTPUTC(CTLQUOTEMARK, out);
			}
			break;
		case CDQUOTE:
			synstack->syntax = DQSYNTAX;
			synstack->dblquote = 1;
 toggledq:
			if (synstack->varnest)
				synstack->innerdq ^= 1;
			goto quotemark;
		case CENDQUOTE:
			IF_BASH_DOLLAR_SQUOTE(bash_dollar_squote = 0;)
			if (eofmark != NULL && synstack->varnest == 0) {
				USTPUTC(c, out);
				break;
			}

			if (synstack->dqvarnest == 0) {
				synstack->syntax = BASESYNTAX;
				synstack->dblquote = 0;
			}

			quotef = 1;

			if (c == '"')
				goto toggledq;

			goto quotemark;
		case CVAR:      /* '$' */
			PARSESUB();             /* parse substitution */
			break;
		case CENDVAR:   /* '}' */
			if (!synstack->innerdq && synstack->varnest > 0) {
				if (!--synstack->varnest && synstack->varpushed)
					synstack_pop(&synstack);
				else if (synstack->dqvarnest > 0)
					synstack->dqvarnest--;
				c = CTLENDVAR;
			}
			USTPUTC(c, out);
			break;
#if ENABLE_FEATURE_SH_MATH
		case CLP:       /* '(' in arithmetic */
			synstack->parenlevel++;
			USTPUTC(c, out);
			break;
		case CRP:       /* ')' in arithmetic */
			if (synstack->parenlevel > 0) {
				synstack->parenlevel--;
			} else {
				if (pgetc_eatbnl() == ')') {
					c = CTLENDARI;
					synstack_pop(&synstack);
				} else {
					/*
					 * unbalanced parens
					 * (don't 2nd guess - no error)
					 */
					pungetc();
				}
			}
			USTPUTC(c, out);
			break;
#endif
		case CBQUOTE:   /* '`' */
			if (checkkwd & CHKEOFMARK) {
				quotef = 1;
				USTPUTC('`', out);
				break;
			}

			PARSEBACKQOLD();
			break;
		case CENDFILE:
			goto endword;           /* exit outer loop */
		default:
			if (synstack->varnest == 0) {
#if BASH_REDIR_OUTPUT
				if (c == '&') {
//Can't call pgetc_eatbnl() here, this requires three-deep pungetc()
					if (pgetc() == '>')
						c = 0x100 + '>'; /* flag &> */
					pungetc();
				}
#endif
#if BASH_PROCESS_SUBST
				if (c == '<' || c == '>') {
					if (pgetc() == '(') {
						PARSEPROCSUB();
						break;
					}
					pungetc();
				}
#endif
				goto endword;   /* exit outer loop */
			}
			USTPUTC(c, out);
		}
		c = pgetc_top(synstack);
	} /* for (;;) */
 endword:

#if ENABLE_FEATURE_SH_MATH
	if (synstack->syntax == ARISYNTAX)
		raise_error_syntax("missing '))'");
#endif
	if (synstack->syntax != BASESYNTAX && eofmark == NULL)
		raise_error_syntax("unterminated quoted string");
	if (synstack->varnest != 0) {
		/* { */
		raise_error_syntax("missing '}'");
	}
	USTPUTC('\0', out);
	len = out - (char *)stackblock();
	out = stackblock();
	if (eofmark == NULL) {
		if ((c == '>' || c == '<' IF_BASH_REDIR_OUTPUT( || c == 0x100 + '>'))
		 && quotef == 0
		) {
			if (isdigit_str9(out)) {
				PARSEREDIR(); /* passed as params: out, c */
				lasttoken = TREDIR;
				return lasttoken;
			}
			/* else: non-number X seen, interpret it
			 * as "NNNX>file" = "NNNX >file" */
		}
		pungetc();
	}
	quoteflag = quotef;
	backquotelist = bqlist;
	grabstackblock(len);
	wordtext = out;
	lasttoken = TWORD;
	return lasttoken;
/* end of readtoken routine */

/*
 * Check to see whether we are at the end of the here document.  When this
 * is called, c is set to the first character of the next input line.  If
 * we are at the end of the here document, this routine sets the c to PEOF.
 */
checkend: {
	if (realeofmark(eofmark)) {
		int markloc;
		char *p;

		if (striptabs) {
			while (c == '\t')
				c = pgetc();
		}

		markloc = out - (char *)stackblock();
		for (p = eofmark; STPUTC(c, out), *p; p++) {
			if (c != *p)
				goto more_heredoc;
			/* FIXME: fails for backslash-newlined terminator:
			 * cat <<EOF
			 * ...
			 * EO\
			 * F
			 * (see heredoc_bkslash_newline2.tests)
			 */
			c = pgetc();
		}

		if (c == '\n' || c == PEOF) {
			c = PEOF;
			if (trap_depth == 0)
				g_parsefile->linno++;
			needprompt = doprompt;
		} else {
			int len_here;

 more_heredoc:
			p = (char *)stackblock() + markloc + 1;
			len_here = out - p;

			if (len_here) {
				len_here -= (c >= PEOF);
				c = p[-1];

				if (len_here) {
					char *str;

					str = alloca(len_here + 1);
					*(char *)mempcpy(str, p, len_here) = '\0';

					pushstring(str, NULL);
				}
			}
		}

		STADJUST((char *)stackblock() + markloc - out, out);
	}
	goto checkend_return;
}

/*
 * Parse a redirection operator.  The variable "out" points to a string
 * specifying the fd to be redirected.  The variable "c" contains the
 * first character of the redirection operator.
 */
parseredir: {
	/* out is already checked to be a valid number or "" */
	int fd = (*out == '\0' ? -1 : atoi(out));
	union node *np;

	np = stzalloc(sizeof(struct nfile));
	if (c == '>') {
		np->nfile.fd = 1;
		c = pgetc_eatbnl();
		if (c == '>')
			np->type = NAPPEND;
		else if (c == '|')
			np->type = NCLOBBER;
		else if (c == '&')
			np->type = NTOFD;
			/* it also can be NTO2 (>&file), but we can't figure it out yet */
		else {
			np->type = NTO;
			pungetc();
		}
	}
#if BASH_REDIR_OUTPUT
	else if (c == 0x100 + '>') { /* this flags &> redirection */
		np->nfile.fd = 1;
		pgetc(); /* this is '>', no need to check */
		np->type = NTO2;
	}
#endif
	else { /* c == '<' */
		/*np->nfile.fd = 0; - stzalloc did it */
		c = pgetc_eatbnl();
		switch (c) {
		case '<':
			if (sizeof(struct nfile) != sizeof(struct nhere)) {
				np = stzalloc(sizeof(struct nhere));
				/*np->nfile.fd = 0; - stzalloc did it */
			}
			np->type = NHERE;
			heredoc = stzalloc(sizeof(struct heredoc));
			heredoc->here = np;
			c = pgetc_eatbnl();
			if (c == '-') {
				heredoc->striptabs = 1;
			} else {
				/*heredoc->striptabs = 0; - stzalloc did it */
				pungetc();
			}
			break;

		case '&':
			np->type = NFROMFD;
			break;

		case '>':
			np->type = NFROMTO;
			break;

		default:
			np->type = NFROM;
			pungetc();
			break;
		}
	}
	if (fd >= 0)
		np->nfile.fd = fd;
	redirnode = np;
	goto parseredir_return;
}

/*
 * Parse a substitution.  At this point, we have read the dollar sign
 * and nothing else.
 */

/* is_special(c) evaluates to 1 for c in "!#$*-0123456789?@"; 0 otherwise
 * (assuming ascii char codes, as the original implementation did) */
#define is_special(c) \
	(((unsigned)(c) - 33 < 32) \
			&& ((0xc1ff920dU >> ((unsigned)(c) - 33)) & 1))
parsesub: {
	unsigned char subtype;
	int typeloc;

	c = pgetc_eatbnl();
	if ((checkkwd & CHKEOFMARK)
	 || (c != '(' && c != '{' && !is_name(c) && !is_special(c))
	) {
#if BASH_DOLLAR_SQUOTE
		if (synstack->syntax != DQSYNTAX && c == '\'')
			bash_dollar_squote = 1;
		else
#endif
			USTPUTC('$', out);
		pungetc();
	} else if (c == '(') {
		/* $(command) or $((arith)) */
		if (pgetc_eatbnl() == '(') {
#if ENABLE_FEATURE_SH_MATH
			PARSEARITH();
#else
			raise_error_syntax("support for $((arith)) is disabled");
#endif
		} else {
			pungetc();
			PARSEBACKQNEW();
		}
	} else {
		/* $VAR, $<specialchar>, ${...}, or PEOF */
		smalluint newsyn = synstack->syntax;

		USTPUTC(CTLVAR, out);
		typeloc = out - (char *)stackblock();
		STADJUST(1, out);
		subtype = VSNORMAL;
		if (c == '{') {
			c = pgetc_eatbnl();
			subtype = 0;
		}
 varname:
		if (is_name(c)) {
			/* $[{[#]]NAME[}] */
			do {
				STPUTC(c, out);
				c = pgetc_eatbnl();
			} while (is_in_name(c));
		} else if (isdigit(c)) {
			/* $[{[#]]NUM[}] */
			do {
				STPUTC(c, out);
				c = pgetc_eatbnl();
			} while ((subtype == 0 || subtype == VSLENGTH) && isdigit(c));
		} else if (c != '}') {
			/* $[{[#]]<specialchar>[}] */
			int cc = c;

			c = pgetc_eatbnl();
			if (!subtype && cc == '#') {
				subtype = VSLENGTH;
				if (c == '_' || isalnum(c))
					goto varname;
				cc = c;
				c = pgetc_eatbnl();
				if (cc == '}' || c != '}') {
					pungetc();
					subtype = 0;
					c = cc;
					cc = '#';
				}
			}

			if (!is_special(cc)) {
				if (subtype == VSLENGTH)
					subtype = 0;
				goto badsub;
			}

			USTPUTC(cc, out);
		} else
			goto badsub;

		if (subtype == 0) {
			static const char types[] ALIGN1 = "}-+?=";
			/* ${VAR...} but not $VAR or ${#VAR} */
			/* c == first char after VAR */
			int cc = c;

			switch (c) {
			case ':':
				c = pgetc_eatbnl();
#if BASH_SUBSTR
				/* This check is only needed to not misinterpret
				 * ${VAR:-WORD}, ${VAR:+WORD}, ${VAR:=WORD}, ${VAR:?WORD}
				 * constructs.
				 */
				if (!strchr(types, c)) {
					subtype = VSSUBSTR;
					pungetc();
					break; /* "goto badsub" is bigger (!) */
				}
#endif
				subtype = VSNUL;
				/*FALLTHROUGH*/
			default: {
				const char *p = strchr(types, c);
				if (p == NULL)
					break;
				subtype |= p - types + VSNORMAL;
				break;
			}
			case '%':
			case '#':
				subtype = (c == '#' ? VSTRIMLEFT : VSTRIMRIGHT);
				c = pgetc_eatbnl();
				if (c == cc)
					subtype++;
				else
					pungetc();

				newsyn = BASESYNTAX;
				break;
#if BASH_PATTERN_SUBST
			case '/':
				/* ${v/[/]pattern/repl} */
//TODO: encode pattern and repl separately.
// Currently cases like: v=1;echo ${v/$((1/1))/ONE}
// are broken (should print "ONE")
				subtype = VSREPLACE;
				newsyn = BASESYNTAX;
				c = pgetc_eatbnl();
				if (c != '/')
					goto badsub;
				subtype++; /* VSREPLACEALL */
				break;
#endif
			}
		} else {
			if (subtype == VSLENGTH && c != '}')
				subtype = 0;
 badsub:
			pungetc();
		}

		if (newsyn == ARISYNTAX)
			newsyn = DQSYNTAX;

		if ((newsyn != synstack->syntax || synstack->innerdq)
		 && subtype != VSNORMAL
		) {
			synstack_push(&synstack,
				synstack->prev ?: alloca(sizeof(*synstack)),
				newsyn);

			synstack->varpushed = 1;
			synstack->dblquote = newsyn != BASESYNTAX;
		}

		((unsigned char *)stackblock())[typeloc] = subtype;
		if (subtype != VSNORMAL) {
			synstack->varnest++;
			if (synstack->dblquote)
				synstack->dqvarnest++;
		}
		STPUTC('=', out);
	}
	goto parsesub_return;
}

/*
 * Called to parse command substitutions.  Newstyle is set if the command
 * is enclosed inside $(...); nlpp is a pointer to the head of the linked
 * list of commands (passed by reference), and savelen is the number of
 * characters on the top of the stack which must be preserved.
 */
parsebackq: {
	struct nodelist **nlpp;
	union node *n;
	char *str;
	size_t savelen;
	struct heredoc *saveheredoclist;
	smallint saveprompt = 0;

	str = NULL;
	savelen = out - (char *)stackblock();
	if (savelen > 0) {
		/*
		 * FIXME: this can allocate very large block on stack and SEGV.
		 * Example:
		 * echo "..<100kbytes>..`true` $(true) `true` ..."
		 * allocates 100kb for every command subst. With about
		 * a hundred command substitutions stack overflows.
		 * With larger prepended string, SEGV happens sooner.
		 */
		str = alloca(savelen);
		memcpy(str, stackblock(), savelen);
	}

	if (oldstyle) {
		/* We must read until the closing backquote, giving special
		 * treatment to some slashes, and then push the string and
		 * reread it as input, interpreting it normally.
		 */
		char *pout;
		size_t psavelen;
		char *pstr;

		STARTSTACKSTR(pout);
		for (;;) {
			int pc;

			setprompt_if(needprompt, 2);
			pc = pgetc_eatbnl();
			switch (pc) {
			case '`':
				goto done;

			case '\\':
				pc = pgetc(); /* not pgetc_eatbnl! */
				if (pc != '\\' && pc != '`' && pc != '$'
				 && (!synstack->dblquote || pc != '"')
				) {
					STPUTC('\\', pout);
				}
				break;

			case PEOF:
				raise_error_syntax("EOF in backquote substitution");

			case '\n':
				nlnoprompt();
				break;

			default:
				break;
			}
			STPUTC(pc, pout);
		}
 done:
		STPUTC('\0', pout);
		psavelen = pout - (char *)stackblock();
		if (psavelen > 0) {
			pstr = grabstackstr(pout);
			setinputstring(pstr);
		}
	}
	nlpp = &bqlist;
	while (*nlpp)
		nlpp = &(*nlpp)->next;
	*nlpp = stzalloc(sizeof(**nlpp));
	/* (*nlpp)->next = NULL; - stzalloc did it */

	saveheredoclist = heredoclist;
	heredoclist = NULL;

	if (oldstyle) {
		saveprompt = doprompt;
		doprompt = 0;
	}

	n = list(2);

	if (oldstyle)
		doprompt = saveprompt;
	else {
		if (readtoken() != TRP)
			raise_error_unexpected_syntax(TRP);
		setinputstring(nullstr);
	}

	parseheredoc();
	heredoclist = saveheredoclist;

	(*nlpp)->n = n;
	/* Start reading from old file again. */
	popfile();
	/* Ignore any pushed back tokens left from the backquote parsing. */
	if (oldstyle)
		tokpushback = 0;
	out = growstackto(savelen + 1);
	if (str) {
		memcpy(out, str, savelen);
		STADJUST(savelen, out);
	}
#if BASH_PROCESS_SUBST
	if (style == PSUB)
		USTPUTC(c == '<' ? CTLFROMPROC : CTLTOPROC, out);
	else
#endif
		USTPUTC(CTLBACKQ, out);
	if (oldstyle)
		goto parsebackq_oldreturn;
#if BASH_PROCESS_SUBST
	else if (style == PSUB)
		goto parsebackq_psreturn;
#endif
	goto parsebackq_newreturn;
}

#if ENABLE_FEATURE_SH_MATH
/*
 * Parse an arithmetic expansion (indicate start of one and set state)
 */
parsearith: {

	synstack_push(&synstack,
			synstack->prev ?: alloca(sizeof(*synstack)),
			ARISYNTAX);
	synstack->dblquote = 1;
	USTPUTC(CTLARI, out);
	goto parsearith_return;
}
#endif
} /* end of readtoken */

/*
 * Read the next input token.
 * If the token is a word, we set backquotelist to the list of cmds in
 *      backquotes.  We set quoteflag to true if any part of the word was
 *      quoted.
 * If the token is TREDIR, then we set redirnode to a structure containing
 *      the redirection.
 *
 * [Change comment:  here documents and internal procedures]
 * [Readtoken shouldn't have any arguments.  Perhaps we should make the
 *  word parsing code into a separate routine.  In this case, readtoken
 *  doesn't need to have any internal procedures, but parseword does.
 *  We could also make parseoperator in essence the main routine, and
 *  have parseword (readtoken1?) handle both words and redirection.]
 */
#define NEW_xxreadtoken
#ifdef NEW_xxreadtoken
/* singles must be first! */
static const char xxreadtoken_chars[7] ALIGN1 = {
	'\n', '(', ')', /* singles */
	'&', '|', ';',  /* doubles */
	0
};

#define xxreadtoken_singles 3
#define xxreadtoken_doubles 3

static const char xxreadtoken_tokens[] ALIGN1 = {
	TNL, TLP, TRP,          /* only single occurrence allowed */
	TBACKGND, TPIPE, TSEMI, /* if single occurrence */
	TEOF,                   /* corresponds to trailing nul */
	TAND, TOR, TENDCASE     /* if double occurrence */
};

static int
xxreadtoken(void)
{
	int c;

	if (tokpushback) {
		tokpushback = 0;
		return lasttoken;
	}
	setprompt_if(needprompt, 2);
	for (;;) {                      /* until token or start of word found */
		c = pgetc_eatbnl();
		if (c == ' ' || c == '\t')
			continue;

		if (c == '#') {
			while ((c = pgetc()) != '\n' && c != PEOF)
				continue;
			pungetc();
		} else if (c == '\\') {
			break; /* return readtoken1(...) */
		} else {
			const char *p;

			p = xxreadtoken_chars + sizeof(xxreadtoken_chars) - 1;
			if (c != PEOF) {
				if (c == '\n') {
					nlnoprompt();
				}

				p = strchr(xxreadtoken_chars, c);
				if (p == NULL)
					break; /* return readtoken1(...) */

				if ((int)(p - xxreadtoken_chars) >= xxreadtoken_singles) {
					int cc = pgetc_eatbnl();
					if (cc == c) {    /* double occurrence? */
						p += xxreadtoken_doubles + 1;
					} else {
						pungetc();
#if BASH_REDIR_OUTPUT
						if (c == '&' && cc == '>') /* &> */
							break; /* return readtoken1(...) */
#endif
					}
				}
			}
			lasttoken = xxreadtoken_tokens[p - xxreadtoken_chars];
			return lasttoken;
		}
	} /* for (;;) */

	return readtoken1(c, BASESYNTAX, (char *) NULL, 0);
}
#else /* old xxreadtoken */
#define RETURN(token)   return lasttoken = token
static int
xxreadtoken(void)
{
	int c;

	if (tokpushback) {
		tokpushback = 0;
		return lasttoken;
	}
	setprompt_if(needprompt, 2);
	for (;;) {      /* until token or start of word found */
		c = pgetc_eatbnl();
		switch (c) {
		case ' ': case '\t':
			continue;
		case '#':
			while ((c = pgetc()) != '\n' && c != PEOF)
				continue;
			pungetc();
			continue;
		case '\n':
			nlnoprompt();
			RETURN(TNL);
		case PEOF:
			RETURN(TEOF);
		case '&':
			if (pgetc_eatbnl() == '&')
				RETURN(TAND);
			pungetc();
			RETURN(TBACKGND);
		case '|':
			if (pgetc_eatbnl() == '|')
				RETURN(TOR);
			pungetc();
			RETURN(TPIPE);
		case ';':
			if (pgetc_eatbnl() == ';')
				RETURN(TENDCASE);
			pungetc();
			RETURN(TSEMI);
		case '(':
			RETURN(TLP);
		case ')':
			RETURN(TRP);
		}
		break;
	}
	return readtoken1(c, BASESYNTAX, (char *)NULL, 0);
#undef RETURN
}
#endif /* old xxreadtoken */

static int
readtoken(void)
{
	int t;
	int kwd = checkkwd;
#if DEBUG
	smallint alreadyseen = tokpushback;
#endif

#if ENABLE_ASH_ALIAS
 top:
#endif

	t = xxreadtoken();

	/*
	 * eat newlines
	 */
	if (kwd & CHKNL) {
		while (t == TNL) {
			parseheredoc();
			checkkwd = 0;
			t = xxreadtoken();
		}
	}

	kwd |= checkkwd;
	checkkwd = 0;

	if (t != TWORD || quoteflag) {
		goto out;
	}

	/*
	 * check for keywords
	 */
	if (kwd & CHKKWD) {
		const char *const *pp;

		pp = findkwd(wordtext);
		if (pp) {
			lasttoken = t = pp - tokname_array;
			TRACE(("keyword '%s' recognized\n", tokname_array[t]));
			goto out;
		}
	}

	if (kwd & CHKALIAS) {
#if ENABLE_ASH_ALIAS
		struct alias *ap;
		ap = lookupalias(wordtext, 1);
		if (ap != NULL) {
			if (*ap->val) {
				pushstring(ap->val, ap);
			}
			goto top;
		}
#endif
	}
 out:
#if DEBUG
	if (!alreadyseen)
		TRACE(("token '%s' %s\n", tokname_array[t], t == TWORD ? wordtext : ""));
	else
		TRACE(("reread token '%s' %s\n", tokname_array[t], t == TWORD ? wordtext : ""));
#endif
	return t;
}

/*
 * Read and parse a command.  Returns NODE_EOF on end of file.
 * (NULL is a valid parse tree indicating a blank line.)
 */
static union node *
parsecmd(int interact)
{
	tokpushback = 0;
	checkkwd = 0;
	heredoclist = 0;
	doprompt = interact;
	setprompt_if(doprompt, doprompt);
	needprompt = 0;
	return list(1);
}

/*
 * Input any here documents.
 */
static void
parseheredoc(void)
{
	struct heredoc *here;
	union node *n;

	here = heredoclist;
	heredoclist = NULL;

	while (here) {
		tokpushback = 0;
		setprompt_if(needprompt, 2);
		if (here->here->type == NHERE)
			readtoken1(pgetc(), SQSYNTAX, here->eofmark, here->striptabs);
		else
			readtoken1(pgetc_eatbnl(), DQSYNTAX, here->eofmark, here->striptabs);
		n = stzalloc(sizeof(struct narg));
		n->narg.type = NARG;
		/*n->narg.next = NULL; - stzalloc did it */
		n->narg.text = wordtext;
		n->narg.backquote = backquotelist;
		here->here->nhere.doc = n;
		here = here->next;
	}
}


static const char *
expandstr(const char *ps, int syntax_type)
{
	struct parsefile *file_stop;
	struct jmploc *volatile savehandler;
	struct heredoc *saveheredoclist;
	const char *result;
	int saveprompt;
	struct jmploc jmploc;
	union node n;
	int err;

	file_stop = g_parsefile;

	/* XXX Fix (char *) cast. */
	setinputstring((char *)ps);

	saveheredoclist = heredoclist;
	heredoclist = NULL;
	saveprompt = doprompt;
	doprompt = 0;
	result = ps;
	savehandler = exception_handler;
	err = setjmp(jmploc.loc);
	if (err)
		goto out;

	/* readtoken1() might die horribly.
	 * Try a prompt with syntactically wrong command:
	 * PS1='$(date "+%H:%M:%S) > '
	 */
	exception_handler = &jmploc;
	readtoken1(pgetc_eatbnl(), syntax_type, FAKEEOFMARK, 0);

	n.narg.type = NARG;
	n.narg.next = NULL;
	n.narg.text = wordtext;
	n.narg.backquote = backquotelist;

	/* expandarg() might fail too:
	 * PS1='$((123+))'
	 */
	expandarg(&n, NULL, EXP_QUOTED);
	result = stackblock();

out:
	exception_handler = savehandler;
	if (err && exception_type != EXERROR)
		longjmp(exception_handler->loc, 1);

	doprompt = saveprompt;
	/* Try: PS1='`xxx(`' */
	unwindfiles(file_stop);
	heredoclist = saveheredoclist;

	return result;
}

static inline int
parser_eof(void)
{
	return tokpushback && lasttoken == TEOF;
}

/*
 * Execute a command or commands contained in a string.
 */
static int
evalstring(char *s, int flags)
{
	struct jmploc *volatile savehandler;
	struct jmploc jmploc;
	int ex;

	union node *n;
	struct stackmark smark;
	int status;

	s = sstrdup(s);
#if ENABLE_PLATFORM_MINGW32
	remove_cr(s, strlen(s)+1);
#endif
	setinputstring(s);
	setstackmark(&smark);

	status = 0;
	/* On exception inside execution loop, we must popfile().
	 * Try interactively:
	 *	readonly a=a
	 *	command eval "a=b"  # throws "is read only" error
	 * "command BLTIN" is not supposed to abort (even in non-interactive use).
	 * But if we skip popfile(), we hit EOF in eval's string, and exit.
	 */
	savehandler = exception_handler;
	ex = setjmp(jmploc.loc);
	if (ex)
		goto out;
	exception_handler = &jmploc;

	while ((n = parsecmd(0)) != NODE_EOF) {
		int i;

		i = evaltree(n, flags & ~(parser_eof() ? 0 : EV_EXIT));
		if (n)
			status = i;
		popstackmark(&smark);
		if (evalskip)
			break;
	}
 out:
	popstackmark(&smark);
	popfile();
	stunalloc(s);

	exception_handler = savehandler;
	if (ex)
		longjmp(exception_handler->loc, ex);

	return status;
}

/*
 * The eval command.
 */
static int FAST_FUNC
evalcmd(int argc UNUSED_PARAM, char **argv, int flags)
{
	char *p;
	char *concat;

	if (argv[1]) {
		p = argv[1];
		argv += 2;
		if (argv[0]) {
			STARTSTACKSTR(concat);
			for (;;) {
				concat = stack_putstr(p, concat);
				p = *argv++;
				if (p == NULL)
					break;
				STPUTC(' ', concat);
			}
			STPUTC('\0', concat);
			p = grabstackstr(concat);
		}
		return evalstring(p, flags & EV_TESTED);
	}
	return 0;
}

/*
 * Read and execute commands.
 * "Top" is nonzero for the top level command loop;
 * it turns on prompting if the shell is interactive.
 */
static int
cmdloop(int top)
{
	union node *n;
	struct stackmark smark;
	int inter;
	int status = 0;
	int numeof = 0;

	TRACE(("cmdloop(%d) called\n", top));
	for (;;) {
		int skip;

		setstackmark(&smark);
#if JOBS || JOBS_WIN32
		if (doing_jobctl)
			showjobs(SHOW_CHANGED|SHOW_STDERR);
#endif
		inter = 0;
		if (iflag && top) {
			inter++;
			chkmail();
		}
		n = parsecmd(inter);
#if DEBUG
		if (DEBUG > 2 && debug && (n != NODE_EOF))
			showtree(n);
#endif
		if (n == NODE_EOF) {
			if (!top || numeof >= 50)
				break;
			if (!stoppedjobs()) {
				if (!iflag)
					break;
				if (!Iflag) {
					newline_and_flush(stderr);
					break;
				}
				/* "set -o ignoreeof" active, do not exit command loop on ^D */
				out2str("\nUse \"exit\" to leave shell.\n");
			}
			numeof++;
		} else {
			int i;

#if !ENABLE_PLATFORM_MINGW32
			/* job_warning can only be 2,1,0. Here 2->1, 1/0->0 */
			job_warning >>= 1;
#endif
			numeof = 0;
			i = evaltree(n, 0);
			if (n)
				status = i;
		}
		popstackmark(&smark);
		skip = evalskip;

		if (skip) {
			evalskip &= ~(SKIPFUNC | SKIPFUNCDEF);
			break;
		}
	}
	return status;
}

/*
 * Take commands from a file.  To be compatible we should do a path
 * search for the file, which is necessary to find sub-commands.
 */
static char *
find_dot_file(char *basename)
{
	char *fullname;
	const char *path = pathval();
	struct stat statb;
	int len;

	/* don't try this for absolute or relative paths */
	if (strchr(basename, '/') IF_PLATFORM_MINGW32(|| strchr(basename, '\\')))
		return basename;

	while ((len = padvance(&path, basename)) >= 0) {
		fullname = stackblock();
		if ((!pathopt || *pathopt == 'f')
		 && !stat(fullname, &statb) && S_ISREG(statb.st_mode)
		) {
			/* This will be freed by the caller. */
			return stalloc(len);
		}
	}
	/* not found in PATH */

#if ENABLE_ASH_BASH_SOURCE_CURDIR
	return basename;
#else
	ash_msg_and_raise_error("%s: not found", basename);
	/* NOTREACHED */
#endif
}

static int FAST_FUNC
dotcmd(int argc_ UNUSED_PARAM, char **argv_ UNUSED_PARAM)
{
	/* "false; . empty_file; echo $?" should print 0, not 1: */
	int status = 0;
	char *fullname;
	char **argv;
	char *args_need_save;
	volatile struct shparam saveparam;

//???
//	struct strlist *sp;
//	for (sp = cmdenviron; sp; sp = sp->next)
//		setvareq(ckstrdup(sp->text), VSTRFIXED | VTEXTFIXED);

	nextopt(nullstr); /* handle possible "--" */
	argv = argptr;

	if (!argv[0]) {
		/* bash says: "bash: .: filename argument required" */
		return 2; /* bash compat */
	}

	/* This aborts if file isn't found, which is POSIXly correct.
	 * bash returns exitcode 1 instead.
	 */
	fullname = find_dot_file(argv[0]);
	argv++;
	args_need_save = argv[0];
	if (args_need_save) { /* ". FILE ARGS", and ARGS are not empty */
		int argc;
		saveparam = shellparam;
		shellparam.malloced = 0;
		argc = 1;
		while (argv[argc])
			argc++;
		shellparam.nparam = argc;
		shellparam.p = argv;
	};

	/* This aborts if file can't be opened, which is POSIXly correct.
	 * bash returns exitcode 1 instead.
	 */
	setinputfile(fullname, INPUT_PUSH_FILE);
	commandname = fullname;
	status = cmdloop(0);
	popfile();

	if (args_need_save) {
		freeparam(&shellparam);
		shellparam = saveparam;
	};

	return status;
}

static int FAST_FUNC
exitcmd(int argc UNUSED_PARAM, char **argv)
{
	if (stoppedjobs())
		return 0;

	if (argv[1])
		savestatus = number(argv[1]);

//TODO: this script
// trap 'echo trap:$FUNCNAME' EXIT
// f() { exit; }
// f
//prints "trap:f" in bash. We can call exitshell() here to achieve this.
//For now, keeping dash code:
	raise_exception(EXEXIT);
	/* NOTREACHED */
}

/*
 * Read a file containing shell functions.
 */
static void
readcmdfile(char *name)
{
	setinputfile(name, INPUT_PUSH_FILE);
	cmdloop(0);
	popfile();
}


/* ============ find_command inplementation */

/*
 * Resolve a command name.  If you change this routine, you may have to
 * change the shellexec routine as well.
 */
static void
find_command(char *name, struct cmdentry *entry, int act, const char *path)
{
	struct tblentry *cmdp;
	int idx;
	int prev;
	char *fullname;
	struct stat statb;
	int e;
	int updatetbl;
	struct builtincmd *bcmd;
	int len;

#if !ENABLE_PLATFORM_MINGW32
	/* If name contains a slash, don't use PATH or hash table */
	if (strchr(name, '/') != NULL) {
		entry->u.index = -1;
		if (act & DO_ABS) {
			while (stat(name, &statb) < 0) {
#ifdef SYSV
				if (errno == EINTR)
					continue;
#endif
				entry->cmdtype = CMDUNKNOWN;
				return;
			}
		}
		entry->cmdtype = CMDNORMAL;
		return;
	}
#else /* ENABLE_PLATFORM_MINGW32 */
	/* If name contains a slash or drive prefix, don't use PATH or hash table */
	if (has_path(name)) {
		fullname = stack_add_system_drive(name);
		entry->u.index = -1;
		if (act & DO_ABS) {
			if (!add_win32_extension(fullname) && stat(fullname, &statb) < 0) {
# if ENABLE_FEATURE_SH_STANDALONE
				if (unix_path(name) &&
						find_applet_by_name(bb_basename(name)) >= 0) {
					entry->cmdtype = CMDNORMAL;
					entry->u.index = INT_MIN;
					return;
				}
# endif
				entry->cmdtype = CMDUNKNOWN;
				return;
			}
		}
		entry->cmdtype = CMDNORMAL;
		return;
	}
#endif /* ENABLE_PLATFORM_MINGW32 */

/* #if ENABLE_FEATURE_SH_STANDALONE... moved after builtin check */

	updatetbl = (path == pathval());
	if (!updatetbl)
		act |= DO_ALTPATH;

	/* If name is in the table, check answer will be ok */
	cmdp = cmdlookup(name, 0);
	if (cmdp != NULL) {
		int bit;

		switch (cmdp->cmdtype) {
		default:
#if DEBUG
			abort();
#endif
		case CMDNORMAL:
			bit = DO_ALTPATH | DO_REGBLTIN;
			break;
		case CMDFUNCTION:
			bit = DO_NOFUNC;
			break;
		case CMDBUILTIN:
			bit = IS_BUILTIN_REGULAR(cmdp->param.cmd) ? 0 : DO_REGBLTIN;
			break;
		}
		if (act & bit) {
			if (act & bit & DO_REGBLTIN)
				goto fail;

			updatetbl = 0;
			cmdp = NULL;
		} else if (cmdp->rehash == 0)
			/* if not invalidated by cd, we're done */
			goto success;
	}

	/* If %builtin not in path, check for builtin next */
	bcmd = find_builtin(name);
	if (bcmd) {
		if (IS_BUILTIN_REGULAR(bcmd))
			goto builtin_success;
		if (act & DO_ALTPATH)
			goto builtin_success;
		if (builtinloc <= 0)
			goto builtin_success;
	}

	if (act & DO_REGBLTIN)
		goto fail;

#if ENABLE_FEATURE_SH_STANDALONE
	{
		int applet_no = find_applet_by_name(name);
		if (applet_no >= 0) {
			entry->cmdtype = CMDNORMAL;
			entry->u.index = -2 - applet_no;
			return;
		}
	}
#endif

	/* We have to search path. */
	prev = -1;              /* where to start */
	if (cmdp && cmdp->rehash) {     /* doing a rehash */
		if (cmdp->cmdtype == CMDBUILTIN)
			prev = builtinloc;
		else
			prev = cmdp->param.index;
	}

	e = ENOENT;
	idx = -1;
 loop:
	while ((len = padvance(&path, name)) >= 0) {
		const char *lpathopt = pathopt;

		fullname = stackblock();
		idx++;
		if (lpathopt) {
			if (*lpathopt == 'b') {
				if (bcmd)
					goto builtin_success;
				continue;
			} else if (!(act & DO_NOFUNC)) {
				/* handled below */
			} else {
				/* ignore unimplemented options */
				continue;
			}
		}
		/* if rehash, don't redo absolute path names */
		if (!is_relative_path(fullname) && idx <= prev) {
			if (idx < prev)
				continue;
			TRACE(("searchexec \"%s\": no change\n", name));
			goto success;
		}
#if ENABLE_PLATFORM_MINGW32
		add_win32_extension(fullname);
#endif
		while (stat(fullname, &statb) < 0) {
#ifdef SYSV
			if (errno == EINTR)
				continue;
#endif
			if (errno != ENOENT && errno != ENOTDIR)
				e = errno;
			goto loop;
		}
		e = EACCES;     /* if we fail, this will be the error */
		if (!S_ISREG(statb.st_mode))
			continue;
		if (lpathopt) {          /* this is a %func directory */
			stalloc(len);
			/* NB: stalloc will return space pointed by fullname
			 * (because we don't have any intervening allocations
			 * between stunalloc above and this stalloc) */
			readcmdfile(fullname);
			cmdp = cmdlookup(name, 0);
			if (cmdp == NULL || cmdp->cmdtype != CMDFUNCTION)
				ash_msg_and_raise_error("%s not defined in %s", name, fullname);
			stunalloc(fullname);
			goto success;
		}
		TRACE(("searchexec \"%s\" returns \"%s\"\n", name, fullname));
		if (!updatetbl) {
			entry->cmdtype = CMDNORMAL;
			entry->u.index = idx;
			return;
		}
		INT_OFF;
		cmdp = cmdlookup(name, 1);
		cmdp->cmdtype = CMDNORMAL;
		cmdp->param.index = idx;
		INT_ON;
		goto success;
	}

	/* We failed.  If there was an entry for this command, delete it */
	if (cmdp && updatetbl)
		delete_cmd_entry();
	if (act & DO_ERR) {
#if ENABLE_ASH_BASH_NOT_FOUND_HOOK
		struct tblentry *hookp = cmdlookup("command_not_found_handle", 0);
		if (hookp && hookp->cmdtype == CMDFUNCTION) {
			char *argv[3];
			argv[0] = (char*) "command_not_found_handle";
			argv[1] = name;
			argv[2] = NULL;
			evalfun(hookp->param.func, 2, argv, 0);
			entry->cmdtype = CMDUNKNOWN;
			return;
		}
#endif
		ash_msg("%s: %s", name, errmsg(e, "not found"));
	}
 fail:
	entry->cmdtype = CMDUNKNOWN;
	return;

 builtin_success:
	if (!updatetbl) {
		entry->cmdtype = CMDBUILTIN;
		entry->u.cmd = bcmd;
		return;
	}
	INT_OFF;
	cmdp = cmdlookup(name, 1);
	cmdp->cmdtype = CMDBUILTIN;
	cmdp->param.cmd = bcmd;
	INT_ON;
 success:
	cmdp->rehash = 0;
	entry->cmdtype = cmdp->cmdtype;
	entry->u = cmdp->param;
}


/*
 * The trap builtin.
 */
static int FAST_FUNC
trapcmd(int argc UNUSED_PARAM, char **argv UNUSED_PARAM)
{
	char *action;
	char **ap;
	int signo, exitcode;

	nextopt(nullstr);
	ap = argptr;
	if (!*ap) {
		for (signo = 0; signo <= NTRAP_LAST; signo++) {
			char *tr = trap_ptr[signo];
			if (tr) {
				/* note: bash adds "SIG", but only if invoked
				 * as "bash". If called as "sh", or if set -o posix,
				 * then it prints short signal names.
				 * We are printing short names: */
				out1fmt("trap -- %s %s\n",
						single_quote(tr),
						(signo == NTRAP_ERR) ? "ERR" : get_signame(signo));
		/* trap_ptr != trap only if we are in special-cased `trap` code.
		 * In this case, we will exit very soon, no need to free(). */
				/* if (trap_ptr != trap && tp[0]) */
				/*	free(tr); */
			}
		}
		/*
		if (trap_ptr != trap) {
			free(trap_ptr);
			trap_ptr = trap;
		}
		*/
		return 0;
	}

	/* Why the second check?
	 * "trap NUM [sig2]..." is the same as "trap - NUM [sig2]..."
	 * In this case, NUM is signal no, not an action.
	 */
	action = NULL;
	if (ap[1] && !is_number(ap[0]))
		action = *ap++;

	exitcode = 0;
	while (*ap) {
		signo = strcmp(*ap, "ERR") == 0 ? NTRAP_ERR : get_signum(*ap);
		if (signo < 0) {
			/* Mimic bash message exactly */
			ash_msg("%s: invalid signal specification", *ap);
			exitcode = 1;
			goto next;
		}
		INT_OFF;
		if (action) {
			if (LONE_DASH(action))
				action = NULL;
			else {
#if !ENABLE_PLATFORM_MINGW32
				if (action[0]) /* not NULL and not "" and not "-" */
					may_have_traps = 1;
#endif
				action = ckstrdup(action);
			}
		}
		free(trap[signo]);
		trap[signo] = action;
		if (signo != 0 && signo < NSIG)
			setsignal(signo);
		INT_ON;
 next:
		ap++;
	}
	return exitcode;
}


/* ============ Builtins */

#if ENABLE_ASH_HELP
static int FAST_FUNC
helpcmd(int argc UNUSED_PARAM, char **argv UNUSED_PARAM)
{
	unsigned col;
	unsigned i;

	out1fmt(
		"Built-in commands:\n"
		"------------------\n");
	for (col = 0, i = 0; i < ARRAY_SIZE(builtintab); i++) {
		col += out1fmt("%c%s", ((col == 0) ? '\t' : ' '),
					builtintab[i].name + 1);
		if (col > 60) {
			out1fmt("\n");
			col = 0;
		}
	}
# if ENABLE_FEATURE_SH_STANDALONE
	{
		const char *a = applet_names;
		while (*a) {
			col += out1fmt("%c%s", ((col == 0) ? '\t' : ' '), a);
			if (col > 60) {
				out1fmt("\n");
				col = 0;
			}
			while (*a++ != '\0')
				continue;
		}
	}
# endif
	newline_and_flush(stdout);
	return EXIT_SUCCESS;
}
#endif

#if MAX_HISTORY
static int FAST_FUNC
historycmd(int argc UNUSED_PARAM, char **argv UNUSED_PARAM)
{
	show_history(line_input_state);
	return EXIT_SUCCESS;
}
#endif

/*
 * The export and readonly commands.
 */
static int FAST_FUNC
exportcmd(int argc UNUSED_PARAM, char **argv)
{
	struct var *vp;
	char *name;
	const char *p;
	char **aptr;
	char opt;
	int flag;
	int flag_off;

	/* "readonly" in bash accepts, but ignores -n.
	 * We do the same: it saves a conditional in nextopt's param.
	 */
	flag_off = 0;
	while ((opt = nextopt("np")) != '\0') {
		if (opt == 'n')
			flag_off = VEXPORT;
	}
	flag = VEXPORT;
	if (argv[0][0] == 'r') {
		flag = VREADONLY;
		flag_off = 0; /* readonly ignores -n */
	}
	flag_off = ~flag_off;

	/*if (opt_p_not_specified) - bash doesn't check this. Try "export -p NAME" */
	{
		aptr = argptr;
		name = *aptr;
		if (name) {
			do {
				p = strchr(name, '=');
				if (p != NULL) {
					p++;
				} else {
					vp = *findvar(hashvar(name), name);
					if (vp) {
						vp->flags = ((vp->flags | flag) & flag_off);
						continue;
					}
				}
				setvar(name, p, (flag & flag_off));
			} while ((name = *++aptr) != NULL);
			return 0;
		}
	}

	/* No arguments. Show the list of exported or readonly vars.
	 * -n is ignored.
	 */
	showvars(argv[0], flag, 0);
	return 0;
}

/*
 * Delete a function if it exists.
 */
static void
unsetfunc(const char *name)
{
	struct tblentry *cmdp;

	cmdp = cmdlookup(name, 0);
	if (cmdp != NULL && cmdp->cmdtype == CMDFUNCTION)
		delete_cmd_entry();
}

/*
 * The unset builtin command.  We unset the function before we unset the
 * variable to allow a function to be unset when there is a readonly variable
 * with the same name.
 */
static int FAST_FUNC
unsetcmd(int argc UNUSED_PARAM, char **argv UNUSED_PARAM)
{
	char **ap;
	int i;
	int flag = 0;

	while ((i = nextopt("vf")) != 0) {
		flag = i;
	}

	for (ap = argptr; *ap; ap++) {
		if (flag != 'f') {
			unsetvar(*ap);
			continue;
		}
		if (flag != 'v')
			unsetfunc(*ap);
	}
	return 0;
}

static const unsigned char timescmd_str[] ALIGN1 = {
	' ',  offsetof(struct tms, tms_utime),
	'\n', offsetof(struct tms, tms_stime),
	' ',  offsetof(struct tms, tms_cutime),
	'\n', offsetof(struct tms, tms_cstime),
	0
};
static int FAST_FUNC
timescmd(int argc UNUSED_PARAM, char **argv UNUSED_PARAM)
{
	unsigned clk_tck;
	const unsigned char *p;
	struct tms buf;

	clk_tck = bb_clk_tck();

	times(&buf);
	p = timescmd_str;
	do {
		unsigned sec, frac;
		unsigned long t;
		t = *(clock_t *)(((char *) &buf) + p[1]);
		sec = t / clk_tck;
		frac = t % clk_tck;
		out1fmt("%um%u.%03us%c",
			sec / 60, sec % 60,
			(frac * 1000) / clk_tck,
			p[0]);
		p += 2;
	} while (*p);

	return 0;
}

#if ENABLE_FEATURE_SH_MATH
/*
 * The let builtin. Partially stolen from GNU Bash, the Bourne Again SHell.
 * Copyright (C) 1987, 1989, 1991 Free Software Foundation, Inc.
 *
 * Copyright (C) 2003 Vladimir Oleynik <dzo@simtreas.ru>
 */
static int FAST_FUNC
letcmd(int argc UNUSED_PARAM, char **argv)
{
	arith_t i;

	argv++;
	if (!*argv)
		ash_msg_and_raise_error("expression expected");
	do {
		i = ash_arith(*argv);
	} while (*++argv);

	return !i;
}
#endif

/*
 * The read builtin. Options:
 *      -r              Do not interpret '\' specially
 *      -s              Turn off echo (tty only)
 *      -n NCHARS       Read NCHARS max
 *      -p PROMPT       Display PROMPT on stderr (if input is from tty)
 *      -t SECONDS      Timeout after SECONDS (tty or pipe only)
 *      -u FD           Read from given FD instead of fd 0
 *      -d DELIM        End on DELIM char, not newline
 * This uses unbuffered input, which may be avoidable in some cases.
 * TODO: bash also has:
 *      -a ARRAY        Read into array[0],[1],etc
 *      -e              Use line editing (tty only)
 */
static int FAST_FUNC
readcmd(int argc UNUSED_PARAM, char **argv UNUSED_PARAM)
{
	struct builtin_read_params params;
	const char *r;
	int i;

	memset(&params, 0, sizeof(params));

	while ((i = nextopt("p:u:rt:n:sd:")) != '\0') {
		switch (i) {
		case 'p':
			params.opt_p = optionarg;
			break;
		case 'n':
			params.opt_n = optionarg;
			break;
		case 's':
			params.read_flags |= BUILTIN_READ_SILENT;
			break;
		case 't':
			params.opt_t = optionarg;
			break;
		case 'r':
			params.read_flags |= BUILTIN_READ_RAW;
			break;
		case 'u':
			params.opt_u = optionarg;
			break;
#if BASH_READ_D
		case 'd':
			params.opt_d = optionarg;
			break;
#endif
		default:
			break;
		}
	}

	if (!ENABLE_ASH_BASH_COMPAT && !argptr) {
		bb_simple_error_msg("read: need variable name");
		return 1;
	}
	params.argv = argptr;
	params.setvar = setvar0;
	params.ifs = bltinlookup("IFS"); /* can be NULL */

	/* "read -s" needs to save/restore termios, can't allow ^C
	 * to jump out of it.
	 */
 again:
	INT_OFF;
	r = shell_builtin_read(&params);
	INT_ON;

	if ((uintptr_t)r == 1 && errno == EINTR) {
		/* To get SIGCHLD: sleep 1 & read x; echo $x
		 * Correct behavior is to not exit "read"
		 */
		if (pending_sig == 0)
			goto again;
	}

#if ENABLE_PLATFORM_MINGW32
	if ((uintptr_t)r == 2) {
		/* ^C pressed, propagate event */
		if (iflag) {
			raise_interrupt();
		}
		else {
			GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0);
			exitshell();
		}
		return (uintptr_t)r;
	}
#endif

	if ((uintptr_t)r > 1)
		ash_msg_and_raise_error(r);

	return (uintptr_t)r;
}

static int FAST_FUNC
umaskcmd(int argc UNUSED_PARAM, char **argv UNUSED_PARAM)
{
	static const char permuser[3] ALIGN1 = "ogu";

	mode_t mask;
	int symbolic_mode = 0;

	while (nextopt("S") != '\0') {
		symbolic_mode = 1;
	}

	INT_OFF;
	mask = umask(0);
	umask(mask);
	INT_ON;

	if (*argptr == NULL) {
		if (symbolic_mode) {
			char buf[sizeof(",u=rwx,g=rwx,o=rwx")];
			char *p = buf;
			int i;

			i = 2;
			for (;;) {
				*p++ = ',';
				*p++ = permuser[i];
				*p++ = '=';
				/* mask is 0..0uuugggooo. i=2 selects uuu bits */
				if (!(mask & 0400)) *p++ = 'r';
				if (!(mask & 0200)) *p++ = 'w';
				if (!(mask & 0100)) *p++ = 'x';
				mask <<= 3;
				if (--i < 0)
					break;
			}
			*p = '\0';
			puts(buf + 1);
		} else {
			out1fmt("%04o\n", mask);
		}
	} else {
		char *modestr = *argptr;
		/* numeric umasks are taken as-is */
		/* symbolic umasks are inverted: "umask a=rx" calls umask(222) */
		if (!isdigit(modestr[0]))
			mask ^= 0777;
		mask = bb_parse_mode(modestr, mask);
		if ((unsigned)mask > 0777) {
			ash_msg_and_raise_error("illegal mode: %s", modestr);
		}
		if (!isdigit(modestr[0]))
			mask ^= 0777;
		umask(mask);
#if ENABLE_PLATFORM_MINGW32
		setvareq(xasprintf("BB_UMASK=0%o", mask), VEXPORT|VNOSAVE);
#endif
	}
	return 0;
}

static int FAST_FUNC
ulimitcmd(int argc UNUSED_PARAM, char **argv)
{
	return shell_builtin_ulimit(argv);
}

/* ============ main() and helpers */

/*
 * This routine is called when an error or an interrupt occurs in an
 * interactive shell and control is returned to the main command loop
 * but prior to exitshell.
 */
static void
exitreset(void)
{
	/* from eval.c: */
	if (savestatus >= 0) {
		if (exception_type == EXEXIT || evalskip == SKIPFUNCDEF)
			exitstatus = savestatus;
		savestatus = -1;
	}
	evalskip = 0;
	loopnest = 0;
	inps4 = 0;

	/* from expand.c: */
	ifsfree();

	/* from redir.c: */
	unwindredir(NULL);
}

/*
 * This routine is called when an error or an interrupt occurs in an
 * interactive shell and control is returned to the main command loop.
 * (In dash, this function is auto-generated by build machinery).
 */
static void
reset(void)
{
	/* from input.c: */
	g_parsefile->left_in_buffer = 0;
	g_parsefile->left_in_line = 0;      /* clear input buffer */
	g_parsefile->unget = 0;
	popallfiles();

	/* from var.c: */
	unwindlocalvars(NULL);
}

/*
 * Called to exit the shell.
 */
static void
exitshell(void)
{
	struct jmploc loc;
	char *p;

#if ENABLE_FEATURE_EDITING_SAVE_ON_EXIT
	save_history(line_input_state); /* may be NULL */
#endif
	savestatus = exitstatus;
	TRACE(("pid %d, exitshell(%d)\n", getpid(), savestatus));
	if (setjmp(loc.loc))
		goto out;
	exception_handler = &loc;
	p = trap[0];
	if (p) {
		trap[0] = NULL;
		evalskip = 0;
		trap_depth++;
		evalstring(p, 0);
		trap_depth--;
		evalskip = SKIPFUNCDEF;
		/*free(p); - we'll exit soon */
	}
 out:
	exitreset();
	/* dash wraps setjobctl(0) in "if (setjmp(loc.loc) == 0) {...}".
	 * our setjobctl(0) does not panic if tcsetpgrp fails inside it.
	 */
	setjobctl(0);
	flush_stdout_stderr();
	_exit(exitstatus);
	/* NOTREACHED */
}

#if ENABLE_PLATFORM_MINGW32
static void xsetenv_if_unset(const char *key, const char *value)
{
	if (!getenv(key))
		xsetenv(key, value);
}
#endif

/* Don't inline: conserve stack of caller from having our locals too */
static NOINLINE void
init(void)
{
#if !ENABLE_PLATFORM_MINGW32
	/* we will never free this */
	basepf.next_to_pgetc = basepf.buf = ckmalloc(IBUFSIZ);
	basepf.linno = 1;

	sigmode[SIGCHLD - 1] = S_DFL; /* ensure we install handler even if it is SIG_IGNed */
	setsignal(SIGCHLD);
#endif

	{
		char **envp;
		const char *p;

		initvar();

#if ENABLE_PLATFORM_MINGW32
		/*
		 * case insensitive env names from Windows world
		 *
		 * Some standard env names such as PATH is named Path and so on
		 * ash itself is case sensitive, so "Path" will confuse it, as
		 * MSVC getenv() is case insensitive.
		 *
		 * We may end up having both Path and PATH. Then Path will be chosen
		 * because it appears first.
		 */
		for (envp = environ; envp && *envp; envp++) {
			if (is_prefixed_with_case(*envp, "PATH=") &&
					!is_prefixed_with(*envp, "PATH=")) {
				break;
			}
		}

		if (envp && *envp) {
			/*
			 * If we get here it's because the environment contains a path
			 * variable called something other than PATH.  This suggests we
			 * haven't been invoked from an earlier instance of BusyBox.
			 */
			char *start, *end;
			struct passwd *pw;

			for (envp = environ; envp && *envp; envp++) {
				if (!(end=strchr(*envp, '=')))
					continue;

				/* make all variable names uppercase */
				for (start = *envp;start < end;start++)
					*start = toupper(*start);

				/* Convert backslashes to forward slashes in value but
				 * not if we're on Windows XP or for variables known to
				 * cause problems */
				if (!winxp && !is_prefixed_with(*envp, "SYSTEMROOT=") &&
						!is_prefixed_with(*envp, "COMSPEC=")) {
					bs_to_slash(end+1);
				}

				/* check for invalid characters in name */
				for (start = *envp;start < end;start++) {
					if (!isdigit(*start) && !isalpha(*start) && *start != '_') {
						break;
					}
				}

				if (start != end) {
					/*
					 * Make a copy of the variable, replacing invalid
					 * characters in the name with underscores.
					 */
					char *var = xstrdup(*envp);

					for (start = var;*start != '=';start++) {
						if (!isdigit(*start) && !isalpha(*start)) {
							*start = '_';
						}
					}
					setvareq(var, VEXPORT|VNOSAVE);
				}
			}

			/* Initialise some variables normally set at login, but
			 * only if someone hasn't already set them. */
			pw = xgetpwuid(getuid());
			xsetenv_if_unset("USER",    pw->pw_name);
			xsetenv_if_unset("LOGNAME", pw->pw_name);
			xsetenv_if_unset("HOME",    pw->pw_dir);
			xsetenv_if_unset("SHELL",   DEFAULT_SHELL);
		}
#endif
		for (envp = environ; envp && *envp; envp++) {
/* Used to have
 *			p = endofname(*envp);
 *			if (p != *envp && *p == '=') {
 * here to weed out badly-named variables, but this breaks
 * scenarios where people do want them passed to children:
 * import os
 * os.environ["test-test"]="test"
 * if os.fork() == 0:
 *   os.execv("ash", [ 'ash', '-c', 'eval $(export -p); echo OK' ])  # fixes this
 * os.execv("ash", [ 'ash', '-c', 'env | grep test-test' ])  # breaks this
 */
			if (strchr(*envp, '=')) {
#if !ENABLE_PLATFORM_MINGW32
				setvareq(*envp, VEXPORT|VTEXTFIXED);
#else
				setvareq(*envp, VEXPORT);
#endif
			}
		}

		setvareq((char*)defifsvar, VTEXTFIXED);
		setvareq((char*)defoptindvar, VTEXTFIXED);

		setvar0("PPID", utoa(getppid()));
#if BASH_SHLVL_VAR
		p = lookupvar("SHLVL");
		setvar("SHLVL", utoa((p ? atoi(p) : 0) + 1), VEXPORT);
#endif
#if BASH_HOSTNAME_VAR
		if (!lookupvar("HOSTNAME")) {
			struct utsname uts;
			uname(&uts);
			setvar0("HOSTNAME", uts.nodename);
		}
#endif
		p = lookupvar("PWD");
		if (p) {
			struct stat st1, st2;
			if (p[0] != '/' || stat(p, &st1) || stat(".", &st2)
			 || st1.st_dev != st2.st_dev || st1.st_ino != st2.st_ino
			) {
				p = NULL;
			}
		}
		setpwd(p, 0);
	}
}


//usage:#define ash_trivial_usage
//usage:	"[-il] [-|+Cabefmnuvx] [-|+o OPT]... [-c 'SCRIPT' [ARG0 ARGS] | FILE ARGS | -s ARGS]"
////////	comes from ^^^^^^^^^^optletters
//usage:#define ash_full_usage "\n\n"
//usage:	"Unix shell interpreter"

/*
 * Process the shell command line arguments.
 */
static int
procargs(char **argv)
{
	int i;
	const char *xminusc;
	char **xargv;
	int login_sh;

	xargv = argv;
	login_sh = xargv[0] && xargv[0][0] == '-';
#if NUM_SCRIPTS > 0
	if (minusc)
		goto setarg0;
#endif
	arg0 = xargv[0];
	/* if (xargv[0]) - mmm, this is always true! */
		xargv++;
	argptr = xargv;
	for (i = 0; i < NOPTS; i++)
		optlist[i] = 2;
	if (options(&login_sh)) {
		/* it already printed err message */
		raise_exception(EXERROR);
	}
	xargv = argptr;
	xminusc = minusc;
	if (*xargv == NULL) {
		if (xminusc)
			ash_msg_and_raise_error(bb_msg_requires_arg, "-c");
		sflag = 1;
	}
	if (iflag == 2 /* no explicit -i given */
	 && sflag == 1 /* -s given (or implied) */
#if !ENABLE_PLATFORM_MINGW32
	 && !minusc /* bash compat: ash -sc 'echo $-' is not interactive (dash is) */
#endif
	 && isatty(0) && isatty(1) /* we are on tty */
	) {
		iflag = 1;
	}
	if (mflag == 2)
		mflag = iflag;
	/* Unset options which weren't explicitly set or unset */
	for (i = 0; i < NOPTS; i++)
		optlist[i] &= 1; /* same effect as "if (optlist[i] == 2) optlist[i] = 0;" */
#if DEBUG == 2
	debug = 1;
#endif
	/* POSIX 1003.2: first arg after "-c CMD" is $0, remainder $1... */
	if (xminusc) {
		minusc = *xargv++;
		if (*xargv)
			goto setarg0;
	} else if (!sflag) {
		setinputfile(*xargv, 0);
#if ENABLE_PLATFORM_MINGW32
		bs_to_slash(*xargv);
#endif
 setarg0:
		arg0 = *xargv++;
		commandname = arg0;
	}

	shellparam.p = xargv;
#if ENABLE_ASH_GETOPTS
	shellparam.optind = 1;
	shellparam.optoff = -1;
#endif
	/* assert(shellparam.malloced == 0 && shellparam.nparam == 0); */
	while (*xargv) {
		shellparam.nparam++;
		xargv++;
	}

	/* Interactive bash re-enables SIGHUP which is SIG_IGNed on entry.
	 * Try:
	 * trap '' hup; bash; echo RET	# type "kill -hup $$", see SIGHUP having effect
	 * trap '' hup; bash -c 'kill -hup $$; echo ALIVE'  # here SIGHUP is SIG_IGNed
	 * NB: must do it before setting up signals (in optschanged())
	 * and reading .profile etc (after we return from here):
	 */
#if !ENABLE_PLATFORM_MINGW32
	if (iflag)
		signal(SIGHUP, SIG_DFL);
#endif

	optschanged();

	return login_sh;
}

/*
 * Read /etc/profile, ~/.profile, $ENV.
 */
static void
read_profile(const char *name)
{
	name = expandstr(name, DQSYNTAX);
	if (setinputfile(name, INPUT_PUSH_FILE | INPUT_NOFILE_OK) < 0)
		return;
	cmdloop(0);
	popfile();
}

#if PROFILE
static short profile_buf[16384];
extern int etext();
#endif

/*
 * Main routine.  We initialize things, parse the arguments, execute
 * profiles if we're a login shell, and then call cmdloop to execute
 * commands.  The setjmp call sets up the location to jump to when an
 * exception occurs.  When an exception occurs the variable "state"
 * is used to figure out how far we had gotten.
 */
int ash_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
#if NUM_SCRIPTS > 0
int ash_main(int argc, char **argv)
#else
int ash_main(int argc UNUSED_PARAM, char **argv)
#endif
/* note: 'argc' is used only if embedded scripts are enabled */
{
	volatile smallint state;
	struct jmploc jmploc;
	struct stackmark smark;
	int login_sh;

#if ENABLE_PLATFORM_MINGW32
	INIT_G_memstack();

	/* from init() */
	basepf.next_to_pgetc = basepf.buf = ckmalloc(IBUFSIZ);
	basepf.linno = 1;

	if (argc == 3 && !strcmp(argv[1], "--fs")) {
		forkshell_init(argv[2]);
		/* only reached in case of error */
		bb_error_msg_and_die("forkshell failed");
	}
#endif

	/* Initialize global data */
	INIT_G_misc();
#if !ENABLE_PLATFORM_MINGW32
	INIT_G_memstack();
#endif
	INIT_G_var();
#if ENABLE_ASH_ALIAS
	INIT_G_alias();
#endif
	INIT_G_cmdtable();

#if PROFILE
	monitor(4, etext, profile_buf, sizeof(profile_buf), 50);
#endif

	state = 0;
	if (setjmp(jmploc.loc)) {
		smallint e;
		smallint s;

		exitreset();

		e = exception_type;
		s = state;
		if (e == EXEND || e == EXEXIT || s == 0 || iflag == 0 || shlvl) {
			exitshell();
		}

		reset();

		if (e == EXINT) {
			newline_and_flush(stderr);
		}

		popstackmark(&smark);
		FORCE_INT_ON; /* enable interrupts */
		if (s == 1)
			goto state1;
		if (s == 2)
			goto state2;
		if (s == 3)
			goto state3;
		goto state4;
	}
	exception_handler = &jmploc;
	rootpid = getpid();

#if ENABLE_PLATFORM_MINGW32
	winxp = (argv[1] != NULL && strcmp(argv[1], "-X") == 0);
#endif
	init();
	setstackmark(&smark);

#if ENABLE_PLATFORM_MINGW32
	SetConsoleCtrlHandler(ctrl_handler, TRUE);
#endif

#if NUM_SCRIPTS > 0
	if (argc < 0)
		/* Non-NULL minusc tells procargs that an embedded script is being run */
		minusc = get_script_content(-argc - 1);
#endif
	login_sh = procargs(argv);
#if DEBUG
	TRACE(("Shell args: "));
	trace_puts_args(argv);
#endif

#if ENABLE_ASH_NOCONSOLE
	if (noconsole)
		hide_console();
#endif

#if ENABLE_PLATFORM_MINGW32
	if (dirarg) {
		chdir(dirarg);
		setpwd(NULL, 0);
	}
	else if (!login_sh && iflag) {
		char *cwd = getcwd(NULL, 0);
		if (cwd) {
			docd(cwd, 0);
			free(cwd);
		}
	}

	if (title)
		set_title(title);
#endif

	if (login_sh) {
		const char *hp;

#if ENABLE_PLATFORM_MINGW32
		if (!dirarg) {
			chdir(xgetpwuid(getuid())->pw_dir);
			setpwd(NULL, 0);
		}
#endif

		state = 1;
#if ENABLE_PLATFORM_MINGW32
		hp = xasprintf("%s/etc/profile", get_system_drive() ?: "");
		read_profile(hp);
		free((void *)hp);
#else
		read_profile("/etc/profile");
#endif
 state1:
		state = 2;
		hp = lookupvar("HOME");
		if (hp)
			read_profile("$HOME/.profile");
	}
 state2:
	state = 3;
	if (iflag
#if ENABLE_PLATFORM_POSIX
#ifndef linux
	 && getuid() == geteuid() && getgid() == getegid()
#endif
#endif
	) {
		const char *shinit = lookupvar("ENV");
		if (shinit != NULL && *shinit != '\0')
			read_profile(shinit);
	}
	popstackmark(&smark);
 state3:
	state = 4;
	if (minusc) {
		/* evalstring pushes parsefile stack.
		 * Ensure we don't falsely claim that 0 (stdin)
		 * is one of stacked source fds.
		 * Testcase: ash -c 'exec 1>&0' must not complain. */

		// if (!sflag) g_parsefile->pf_fd = -1;
		// ^^ not necessary since now we special-case fd 0
		// in save_fd_on_redirect()

		lineno = 0; // bash compat
		// dash: evalstring(minusc, sflag ? 0 : EV_EXIT);
		// The above makes
		//  ash -sc 'echo $-'
		// continue reading input from stdin after running 'echo'.
		// bash does not do this: it prints "hBcs" and exits.
#if !ENABLE_PLATFORM_MINGW32
		evalstring(minusc, EV_EXIT);
#else
		evalstring(minusc, sflag ? 0 : EV_EXIT);
#endif
	}

	if (sflag || minusc == NULL) {
#if MAX_HISTORY > 0 && ENABLE_FEATURE_EDITING_SAVEHISTORY
		if (line_input_state) {
			const char *hp = lookupvar("HISTFILE");
			if (!hp) {
				hp = lookupvar("HOME");
				if (hp) {
					INT_OFF;
					hp = concat_path_file(hp, ".ash_history");
					setvar0("HISTFILE", hp);
					free((char*)hp);
					INT_ON;
					hp = lookupvar("HISTFILE");
				}
			}
			if (hp)
				line_input_state->hist_file = xstrdup(hp);
# if ENABLE_FEATURE_SH_HISTFILESIZE
			hp = lookupvar("HISTFILESIZE");
			line_input_state->max_history = size_from_HISTFILESIZE(hp);
# endif
		}
#endif
 state4: /* XXX ??? - why isn't this before the "if" statement */
		cmdloop(1);
	}
#if PROFILE
	monitor(0);
#endif
#ifdef GPROF
	{
		extern void _mcleanup(void);
		_mcleanup();
	}
#endif
	TRACE(("End of main reached\n"));
	exitshell();
	/* NOTREACHED */
}

#if ENABLE_PLATFORM_MINGW32
static void
forkshell_openhere(struct forkshell *fs)
{
	const char *p = fs->path;
	size_t len = strlen(p);
	int pip[2];

	pip[0] = fs->fd[0];
	pip[1] = fs->fd[1];

	TRACE(("ash: subshell: %s\n",__PRETTY_FUNCTION__));

	close(pip[0]);
	ignoresig(SIGINT);  //signal(SIGINT, SIG_IGN);
	ignoresig(SIGQUIT); //signal(SIGQUIT, SIG_IGN);
	ignoresig(SIGHUP);  //signal(SIGHUP, SIG_IGN);
	ignoresig(SIGTSTP); //signal(SIGTSTP, SIG_IGN);
	signal(SIGPIPE, SIG_DFL);
	xwrite(pip[1], p, len);
	_exit(EXIT_SUCCESS);
}

static void
forkshell_evalbackcmd(struct forkshell *fs)
{
#if BASH_PROCESS_SUBST
	/* determine end of pipe used by parent (ip) and child (ic) */
	const int ctl = fs->fd[2];
	const int ip = (ctl == CTLTOPROC);
	const int ic = !(ctl == CTLTOPROC);
#else
	const int ip = 0;
	const int ic = 1;
#endif
	union node *n = fs->n;
	int pip[2];

	pip[ip] = fs->fd[ip];
	pip[ic] = fs->fd[ic];

	FORCE_INT_ON;
	close(pip[ip]);
	if (pip[ic] != ic) {
		/*close(ic);*/
		dup2_or_raise(pip[ic], ic);
		close(pip[ic]);
	}
	eflag = 0;
	ifsfree();
	evaltreenr(n, EV_EXIT);
	/* NOTREACHED */
}

static void
forkshell_evalsubshell(struct forkshell *fs)
{
	union node *n = fs->n;
	int flags = fs->flags;

	TRACE(("ash: subshell: %s\n",__PRETTY_FUNCTION__));
	INT_ON;
	flags |= EV_EXIT;
	expredir(n->nredir.redirect);
	redirect(n->nredir.redirect, 0);
	evaltreenr(n->nredir.n, flags);
	/* never returns */
}

static void
forkshell_evalpipe(struct forkshell *fs)
{
	union node *n = fs->n;
	int flags = fs->flags;
	int prevfd = fs->fd[2];
	int pip[2] = {fs->fd[0], fs->fd[1]};

	TRACE(("ash: subshell: %s\n",__PRETTY_FUNCTION__));
	INT_ON;
	if (pip[1] >= 0) {
		close(pip[0]);
	}
	if (prevfd > 0) {
		dup2(prevfd, 0);
		close(prevfd);
	}
	if (pip[1] > 1) {
		dup2(pip[1], 1);
		close(pip[1]);
	}
	evaltreenr(n, flags);
}

static void
forkshell_shellexec(struct forkshell *fs)
{
	int idx = fs->fd[0];
	char **argv = fs->argv;
	char *path = fs->path;

	FORCE_INT_ON;
	shellexec(argv[0], argv, path, idx);
}

static void
forkshell_child(struct forkshell *fs)
{
	switch ( fs->fpid ) {
	case FS_OPENHERE:
		forkshell_openhere(fs);
		break;
	case FS_EVALBACKCMD:
		forkshell_evalbackcmd(fs);
		break;
	case FS_EVALSUBSHELL:
		forkshell_evalsubshell(fs);
		break;
	case FS_EVALPIPE:
		forkshell_evalpipe(fs);
		break;
	case FS_SHELLEXEC:
		forkshell_shellexec(fs);
		break;
	}
}

/*
 * Reinitialise the builtin environment variables in varinit.  Their
 * current settings have been copied from the parent in vartab.  Look
 * these up using the names from varinit_data, copy the details from
 * vartab to varinit and replace the old copy in vartab with the new
 * one in varinit.
 *
 * Also reinitialise the function pointers and line number variable.
 */
static void
reinitvar(void)
{
	int i;
	const char *name;
	struct var **vpp, **old;

	for (i=0; i<ARRAY_SIZE(varinit); ++i) {
		if (i == LINENO_INDEX)
			name = "LINENO=";
		else if (i == FUNCNAME_INDEX)
			name = "FUNCNAME=";
		else
			name = varinit_data[i].var_text;
		vpp = hashvar(name);
		if ( (old=findvar(vpp, name)) != NULL ) {
			varinit[i] = **old;
			*old = varinit+i;
		}
		varinit[i].var_func = varinit_data[i].var_func;
	}
	vlineno.var_text = linenovar;
	vfuncname.var_text = funcnamevar;
}

static void
spawn_forkshell(struct forkshell *fs, struct job *jp, union node *n, int mode)
{
	struct forkshell *new;
	char buf[32];
	const char *argv[] = { "sh", "--fs", NULL, NULL };
	intptr_t ret;

	new = forkshell_prepare(fs);
	if (new == NULL)
		goto fail;

	new->mode = mode;
	new->nprocs = jp == NULL ? 0 : jp->nprocs;
	sprintf(buf, "%p", new->hMapFile);
	argv[2] = buf;
	ret = spawnve(P_NOWAIT, bb_busybox_exec_path, (char *const *)argv, NULL);
	CloseHandle(new->hMapFile);
	UnmapViewOfFile(new);
	if (ret == -1) {
 fail:
		if (jp)
			freejob(jp);
		ash_msg_and_raise_error("unable to spawn shell");
	}
	forkparent(jp, n, mode, (HANDLE)ret);
}

/*
 * forkshell_prepare() and friends
 *
 * The sequence is as follows:
 * - funcblocksize is initialized
 * - forkshell_size(fs) is called to calculate the exact memory needed
 * - a new struct is allocated
 * - funcblock, funcstring, relocate are initialized from the new block
 * - forkshell_copy(fs) is called to copy recursively everything over
 *   it will record all relocations along the way
 *
 * When this memory is mapped elsewhere, pointer fixup will be needed
 */

/* redefine without test that fs_size is nonzero */
#undef SAVE_PTR
#undef SAVE_PTR2
#undef SAVE_PTR3
#define SAVE_PTR(dst,note,flag) {MARK_PTR(dst,flag); ANNOT(dst,note);}

static int align_len(const char *s)
{
	return s ? SHELL_ALIGN(strlen(s)+1) : 0;
}

struct datasize {
	int funcblocksize;
	int funcstringsize;
};

#define SLIST_SIZE_BEGIN(name,type) \
static struct datasize \
name(struct datasize ds, type *p) \
{ \
	while (p) { \
		ds.funcblocksize += sizeof(type);
		/* do something here with p */
#define SLIST_SIZE_END() \
		p = p->next; \
	} \
	return ds; \
}

#define SLIST_COPY_BEGIN(name,type) \
static type * \
name(type *vp) \
{ \
	type *start; \
	type **vpp; \
	vpp = &start; \
	while (vp) { \
		*vpp = funcblock; \
		funcblock = (char *) funcblock + sizeof(type);
		/* do something here with vpp and vp */
#define SLIST_COPY_END() \
		SAVE_PTR((*vpp)->next, "(*vpp)->next", NO_FREE); \
		vp = vp->next; \
		vpp = &(*vpp)->next; \
	} \
	*vpp = NULL; \
	return start; \
}

/*
 * struct var
 */
SLIST_SIZE_BEGIN(var_size,struct var)
ds.funcstringsize += align_len(p->var_text);
SLIST_SIZE_END()

SLIST_COPY_BEGIN(var_copy,struct var)
(*vpp)->var_text = nodeckstrdup(vp->var_text);
(*vpp)->flags = vp->flags;
(*vpp)->var_func = NULL;
SAVE_PTR((*vpp)->var_text, xasprintf("(*vpp)->var_text '%s'", vp->var_text ?: "NULL"), FREE);
SLIST_COPY_END()

/*
 * struct tblentry
 */
static struct datasize
tblentry_size(struct datasize ds, struct tblentry *tep)
{
	while (tep) {
		ds.funcblocksize += sizeof(struct tblentry) + align_len(tep->cmdname);
		/* CMDBUILTIN, e->param.cmd needs no pointer relocation */
		if (tep->cmdtype == CMDFUNCTION) {
			ds.funcblocksize += offsetof(struct funcnode, n);
			ds.funcblocksize = calcsize(ds.funcblocksize, &tep->param.func->n);
		}
		tep = tep->next;
	}
	return ds;
}

static struct tblentry *
tblentry_copy(struct tblentry *tep)
{
	struct tblentry *start;
	struct tblentry **newp;
	int size;

	newp = &start;
	while (tep) {
		*newp = funcblock;
		size = sizeof(struct tblentry) + align_len(tep->cmdname);

		funcblock = (char *) funcblock + size;
		memcpy(*newp, tep, sizeof(struct tblentry)+strlen(tep->cmdname));
		switch (tep->cmdtype) {
		case CMDBUILTIN:
			/* Save index of builtin, not pointer; fixed by forkshell_init() */
			(*newp)->param.index = tep->param.cmd - builtintab;
			break;
		case CMDFUNCTION:
			(*newp)->param.func = funcblock;
			funcblock = (char *) funcblock + offsetof(struct funcnode, n);
			copynode(&tep->param.func->n);
			SAVE_PTR((*newp)->param.func, "param.func", NO_FREE);
			break;
		default:
			break;
		}
		SAVE_PTR((*newp)->next, xasprintf("cmdname '%s'", tep->cmdname), FREE);
		tep = tep->next;
		newp = &(*newp)->next;
	}
	*newp = NULL;
	return start;
}

static struct datasize
cmdtable_size(struct datasize ds, struct tblentry **cmdtablep)
{
	int i;
	ds.funcblocksize += sizeof(struct tblentry *)*CMDTABLESIZE;
	for (i = 0; i < CMDTABLESIZE; i++)
		ds = tblentry_size(ds, cmdtablep[i]);
	return ds;
}

static struct tblentry **
cmdtable_copy(struct tblentry **cmdtablep)
{
	struct tblentry **new = funcblock;
	int i;

	funcblock = (char *) funcblock + sizeof(struct tblentry *)*CMDTABLESIZE;
	for (i = 0; i < CMDTABLESIZE; i++) {
		new[i] = tblentry_copy(cmdtablep[i]);
		SAVE_PTR(new[i], xasprintf("cmdtable[%d]", i), FREE);
	}
	return new;
}

#if ENABLE_ASH_ALIAS
/*
 * struct alias
 */
SLIST_SIZE_BEGIN(alias_size,struct alias)
ds.funcstringsize += align_len(p->name);
ds.funcstringsize += align_len(p->val);
SLIST_SIZE_END()

SLIST_COPY_BEGIN(alias_copy,struct alias)
(*vpp)->name = nodeckstrdup(vp->name);
(*vpp)->val = nodeckstrdup(vp->val);
(*vpp)->flag = vp->flag;
SAVE_PTR((*vpp)->name, xasprintf("(*vpp)->name '%s'", vp->name ?: "NULL"), FREE);
SAVE_PTR((*vpp)->val, xasprintf("(*vpp)->val '%s'", vp->val ?: "NULL"), FREE);
SLIST_COPY_END()

static struct datasize
atab_size(struct datasize ds, struct alias **atabp)
{
	int i;
	ds.funcblocksize += sizeof(struct alias *)*ATABSIZE;
	for (i = 0; i < ATABSIZE; i++)
		ds = alias_size(ds, atabp[i]);
	return ds;
}

static struct alias **
atab_copy(struct alias **atabp)
{
	struct alias **new = funcblock;
	int i;

	funcblock = (char *) funcblock + sizeof(struct alias *)*ATABSIZE;
	for (i = 0; i < ATABSIZE; i++) {
		new[i] = alias_copy(atabp[i]);
		SAVE_PTR(new[i], xasprintf("atab[%d]", i), FREE);
	}
	return new;
}
#endif

/*
 * char **
 */
static struct datasize
argv_size(struct datasize ds, char **p)
{
	if (p) {
		while (*p) {
			ds.funcblocksize += sizeof(char *);
			ds.funcstringsize += align_len(*p);
			p++;
		}
		ds.funcblocksize += sizeof(char *);
	}
	return ds;
}

static char **
argv_copy(char **p)
{
	char **new, **start = funcblock;
#if FORKSHELL_DEBUG
	int i = 0;
#endif

	if (p) {
		while (*p) {
			new = funcblock;
			funcblock = (char *) funcblock + sizeof(char *);
			*new = nodeckstrdup(*p);
			SAVE_PTR(*new, xasprintf("argv[%d] '%s'", i++, *p), FREE);
			p++;
		}
		new = funcblock;
		funcblock = (char *) funcblock + sizeof(char *);
		*new = NULL;
		return start;
	}
	return NULL;
}

#if MAX_HISTORY
static struct datasize
history_size(struct datasize ds, line_input_t *st)
{
	int i;

	ds.funcblocksize += sizeof(char *) * st->cnt_history;
	for (i = 0; i < st->cnt_history; i++) {
		ds.funcstringsize += align_len(st->history[i]);
	}
	return ds;
}

static char **
history_copy(line_input_t *st)
{
	char **new = funcblock;
	int i;

	funcblock = (char *)funcblock + sizeof(char *) * st->cnt_history;
	for (i = 0; i < st->cnt_history; i++) {
		new[i] = nodeckstrdup(st->history[i]);
		SAVE_PTR(new[i],
			xasprintf("history[%d] '%s'", i, st->history[i]), FREE);
	}
	return new;
}
#endif

/*
 * struct redirtab
 */
static int
redirtab_size(int funcblocksize, struct redirtab *rdtp)
{
	while (rdtp) {
		funcblocksize += sizeof(*rdtp)+sizeof(rdtp->two_fd[0])*rdtp->pair_count;
		rdtp = rdtp->next;
	}
	return funcblocksize;
}

static struct redirtab *
redirtab_copy(struct redirtab *rdtp)
{
	struct redirtab *start;
	struct redirtab **vpp;

	vpp = &start;
	while (rdtp) {
		int size = sizeof(*rdtp)+sizeof(rdtp->two_fd[0])*rdtp->pair_count;
		*vpp = funcblock;
		funcblock = (char *) funcblock + size;
		memcpy(*vpp, rdtp, size);
		SAVE_PTR((*vpp)->next, "(*vpp)->next", NO_FREE);
		rdtp = rdtp->next;
		vpp = &(*vpp)->next;
	}
	*vpp = NULL;
	return start;
}

#undef funcname
#undef shellparam
#undef redirlist
#undef vartab
static struct datasize
globals_var_size(struct datasize ds, struct globals_var *gvp)
{
	int i;

	ds.funcblocksize += sizeof(struct globals_var);
	ds.funcstringsize += align_len(gvp->funcname);
	ds = argv_size(ds, gvp->shellparam.p);
	ds.funcblocksize = redirtab_size(ds.funcblocksize, gvp->redirlist);
	for (i = 0; i < VTABSIZE; i++)
		ds = var_size(ds, gvp->vartab[i]);
	return ds;
}

static struct globals_var *
globals_var_copy(struct globals_var *gvp)
{
	int i;
	struct globals_var *new;

	new = funcblock;
	funcblock = (char *) funcblock + sizeof(struct globals_var);
	memcpy(new, gvp, sizeof(struct globals_var));

	new->funcname = nodeckstrdup(gvp->funcname);
	SAVE_PTR(new->funcname, xasprintf("funcname '%s'", gvp->funcname ?: "NULL"), FREE);

	/* shparam */
	new->shellparam.malloced = 0;
	new->shellparam.p = argv_copy(gvp->shellparam.p);
	SAVE_PTR(new->shellparam.p, "shellparam.p", NO_FREE);

	new->redirlist = redirtab_copy(gvp->redirlist);
	SAVE_PTR(new->redirlist, "redirlist", NO_FREE);

	for (i = 0; i < VTABSIZE; i++) {
		new->vartab[i] = var_copy(gvp->vartab[i]);
		SAVE_PTR(new->vartab[i], xasprintf("vartab[%d]", i), FREE);
	}

	return new;
}

#undef minusc
#undef curdir
#undef physdir
#undef arg0
#undef commandname
#undef nullstr
static struct datasize
globals_misc_size(struct datasize ds, struct globals_misc *p)
{
	ds.funcblocksize += sizeof(struct globals_misc);
	ds.funcstringsize += align_len(p->minusc);
	if (p->curdir != p->nullstr)
		ds.funcstringsize += align_len(p->curdir);
	if (p->physdir != p->nullstr)
		ds.funcstringsize += align_len(p->physdir);
	ds.funcstringsize += align_len(p->arg0);
	ds.funcstringsize += align_len(p->commandname);
	return ds;
}

static struct globals_misc *
globals_misc_copy(struct globals_misc *p)
{
	struct globals_misc *new = funcblock;

	funcblock = (char *) funcblock + sizeof(struct globals_misc);
	memcpy(new, p, sizeof(struct globals_misc));

	new->minusc = nodeckstrdup(p->minusc);
	new->curdir = p->curdir != p->nullstr ? nodeckstrdup(p->curdir) : new->nullstr;
	new->physdir = p->physdir != p->nullstr ? nodeckstrdup(p->physdir) : new->nullstr;
	new->arg0 = nodeckstrdup(p->arg0);
	new->commandname = nodeckstrdup(p->commandname);
	SAVE_PTR(new->minusc, xasprintf("minusc '%s'", p->minusc ?: "NULL"), FREE);
	SAVE_PTR(new->curdir,
			xasprintf("curdir '%s'", new->curdir ?: "NULL"), FREE);
	SAVE_PTR(new->physdir,
			xasprintf("physdir '%s'", new->physdir ?: "NULL"), FREE);
	SAVE_PTR(new->arg0, xasprintf("arg0 '%s'", p->arg0 ?: "NULL"), FREE);
	SAVE_PTR(new->commandname,
			xasprintf("commandname '%s'", p->commandname ?: "NULL"), FREE);
	return new;
}

static struct datasize
forkshell_size(struct forkshell *fs)
{
	struct datasize ds = {0, 0};

	ds.funcstringsize += align_len(fs->path);
	if (fs->fpid == FS_OPENHERE)
		return ds;

	ds = globals_var_size(ds, ash_ptr_to_globals_var);
	ds = globals_misc_size(ds, ash_ptr_to_globals_misc);
	ds = cmdtable_size(ds, cmdtable);

	ds.funcblocksize = calcsize(ds.funcblocksize, fs->n);
	ds = argv_size(ds, fs->argv);

	if ((ENABLE_ASH_ALIAS || MAX_HISTORY) && fs->fpid != FS_SHELLEXEC) {
#if ENABLE_ASH_ALIAS
		ds = atab_size(ds, atab);
#endif
#if MAX_HISTORY
		if (line_input_state)
			ds = history_size(ds, line_input_state);
#endif
	}
	return ds;
}

static void
forkshell_copy(struct forkshell *fs, struct forkshell *new)
{
	memcpy(new, fs, sizeof(struct forkshell)); /* non-pointer stuff */

	new->path = nodeckstrdup(fs->path);
	SAVE_PTR(new->path, xasprintf("path '%s'", fs->path ?: "NULL"), FREE);
	if (fs->fpid == FS_OPENHERE)
		return;

	new->gvp = globals_var_copy(ash_ptr_to_globals_var);
	new->gmp = globals_misc_copy(ash_ptr_to_globals_misc);
	new->cmdtable = cmdtable_copy(cmdtable);
	SAVE_PTR(new->gvp, "gvp", NO_FREE);
	SAVE_PTR(new->gmp, "gmp", NO_FREE);
	SAVE_PTR(new->cmdtable, "cmdtable", NO_FREE);

	new->n = copynode(fs->n);
	new->argv = argv_copy(fs->argv);
	SAVE_PTR(new->n, "n", NO_FREE);
	SAVE_PTR(new->argv, "argv", NO_FREE);

	if ((ENABLE_ASH_ALIAS || MAX_HISTORY) && fs->fpid != FS_SHELLEXEC) {
#if ENABLE_ASH_ALIAS
		new->atab = atab_copy(atab);
		SAVE_PTR(new->atab, "atab", NO_FREE);
#endif
#if MAX_HISTORY
		if (line_input_state) {
			new->history = history_copy(line_input_state);
			SAVE_PTR(new->history, "history", NO_FREE);
			new->cnt_history = line_input_state->cnt_history;
		}
#endif
	}
}

#if FORKSHELL_DEBUG
/* fp and notes can each be NULL */
#define NUM_BLOCKS 7
static void
forkshell_print(FILE *fp0, struct forkshell *fs, const char **notes)
{
	FILE *fp;
	void *lfuncblock;
	char *lfuncstring;
	char *lrelocate;
	char *s;
	int count, i, total;
	int size[NUM_BLOCKS];
	char *lptr[NUM_BLOCKS+1];
	enum {GVP, GMP, CMDTABLE, NODE, ARGV, ATAB, HISTORY, FUNCSTRING};
	const char *fsname[] = {
		"FS_OPENHERE",
		"FS_EVALBACKCMD",
		"FS_EVALSUBSHELL",
		"FS_EVALPIPE",
		"FS_SHELLEXEC"
	};

	if (fp0 != NULL) {
		fp = fp0;
	}
	else {
		char name[64];
		static int num = 0;

		sprintf(name, "fs_%d_%03d.out", getpid(), ++num % 100);
		if ((fp=fopen(name, "w")) == NULL)
			return;
	}

	total = sizeof(struct forkshell) + fs->funcblocksize +
				fs->funcstringsize + fs->relocatesize;
	fprintf(fp, "total size    %6d = %d + %d + %d + %d = %d\n",
				fs->size + fs->relocatesize,
				(int)sizeof(struct forkshell), fs->funcblocksize,
				fs->funcstringsize, fs->relocatesize, total);

	lfuncblock = (char *)(fs + 1);
	lfuncstring = (char *)lfuncblock + fs->funcblocksize;
	lrelocate = (char *)lfuncstring + fs->funcstringsize;

	/* funcblocksize is zero for FS_OPENHERE */
	if (fs->funcblocksize != 0) {
		/* Depending on the configuration and the type of forkshell
		 * some items may not be present. */
		lptr[FUNCSTRING] = lfuncstring;
#if MAX_HISTORY
		lptr[HISTORY] = fs->history ? (char *)fs->history : lptr[FUNCSTRING];
#else
		lptr[HISTORY] = lptr[FUNCSTRING];
#endif
		lptr[ATAB] = IF_ASH_ALIAS(fs->atab ? (char *)fs->atab :) lptr[HISTORY];
		lptr[ARGV] = fs->argv ? (char *)fs->argv : lptr[ATAB];
		lptr[NODE] = fs->n ? (char *)fs->n : lptr[ARGV];
		lptr[CMDTABLE] = (char *)fs->cmdtable;
		lptr[GMP] = (char *)fs->gmp;
		lptr[GVP] = (char *)fs->gvp;

		fprintf(fp, "funcblocksize %6d = ", fs->funcblocksize);
		total = 0;
		for (i=0; i<NUM_BLOCKS; ++i) {
			size[i] = (int)(lptr[i+1] - lptr[i]);
			total += size[i];
			fprintf(fp, "%d %c ", size[i], i == NUM_BLOCKS - 1 ? '=' : '+');
		}
		fprintf(fp, "%d\n\n", total);
	}
	else {
		fprintf(fp, "\n");
	}

	fprintf(fp, "%s\n\n", fsname[fs->fpid]);
	fprintf(fp, "--- relocate ---\n");
	count = 0;
	for (i = 0; i < fs->relocatesize; ++i) {
		if (lrelocate[i]) {
			char **ptr = (char **)((char *)fs + i);
			fprintf(fp, "%p %p %s\n", ptr, *ptr,
					notes && notes[i] ? notes[i] : "");
			++count;
		}
	}
	fprintf(fp, "--- %d relocations ---\n\n", count);

	fprintf(fp, "--- funcstring ---\n");
	count = 0;
	s = lfuncstring;
	while (s-lfuncstring < fs->funcstringsize) {
		if (!*s) {
			++s;
			continue;
		}
		fprintf(fp, "%p '%s'\n", s, s);
		s += strlen(s)+1;
		++count;
	}
	fprintf(fp, "--- %d strings ---\n", count);

	if (fp0 == NULL)
		fclose(fp);
}
#endif

static struct forkshell *
forkshell_prepare(struct forkshell *fs)
{
	struct forkshell *new;
	struct datasize ds;
	int size, relocatesize;
	HANDLE h;
	SECURITY_ATTRIBUTES sa;
#if FORKSHELL_DEBUG
	char *relocate;
	char name[64];
	FILE *fp;
	static int num = 0;
#endif

	/* calculate size of structure, funcblock and funcstring */
	ds = forkshell_size(fs);
	size = sizeof(struct forkshell) + ds.funcblocksize + ds.funcstringsize;
	relocatesize = sizeof(struct forkshell) + ds.funcblocksize;

	/* Allocate shared memory region */
	memset(&sa, 0, sizeof(sa));
	sa.nLength = sizeof(sa);
	sa.lpSecurityDescriptor = NULL;
	sa.bInheritHandle = TRUE;
	h = CreateFileMapping(INVALID_HANDLE_VALUE, &sa, PAGE_READWRITE, 0,
			size+relocatesize, NULL);

	/* Initialise pointers */
	new = (struct forkshell *)MapViewOfFile(h, FILE_MAP_WRITE, 0,0, 0);
	if (new == NULL)
		return NULL;
	fs_size = size;
	funcblock = (char *)(new + 1);
	funcstring_end = (char *)new + size;
#if FORKSHELL_DEBUG
	fs_start = new;
	relocate = (char *)new + size;
	annot = (const char **)xzalloc(sizeof(char *)*relocatesize);
#endif

	/* Now pack them all */
	forkshell_copy(fs, new);

	/* Finish it up */
	new->size = size;
	new->relocatesize = relocatesize;
	new->old_base = (char *)new;
	new->hMapFile = h;
#if FORKSHELL_DEBUG
	sprintf(name, "fs_%d_%03d.out", getpid(), ++num % 100);
	if ((fp=fopen(name, "w")) != NULL) {
		int i;

		new->funcblocksize = (char *)funcblock - (char *)(new + 1);
		new->funcstringsize = (char *)new + size - funcstring_end;

		/* perform some sanity checks on pointers */
		fprintf(fp, "forkshell   %p  %6d\n", new, (int)sizeof(*new));
		fprintf(fp, "funcblock   %p  %6d\n", new+1, new->funcblocksize);
		fprintf(fp, "funcstring  %p  %6d\n", funcstring_end,
				new->funcstringsize);
		if ((char *)funcblock != funcstring_end)
			fprintf(fp, "   funcstring != end funcblock + 1 %p\n", funcblock);
		fprintf(fp, "relocate    %p  %6d\n\n", relocate, new->relocatesize);

		forkshell_print(fp, new, annot);

		for (i = 0; i < relocatesize; ++i) {
			/* check relocations are only present for structure and funcblock */
			if (i >= sizeof(*new)+new->funcblocksize && annot[i] != NULL) {
				fprintf(fp, "\nnon-NULL annotation at offset %d (> %d) %s\n",
						i, (int)sizeof(*new)+new->funcblocksize, annot[i]);
				break;
			}
			if (relocate[i] == FREE) {
				free((void *)annot[i]);
			}
		}
		free(annot);
		annot = NULL;
		fclose(fp);
	}
#endif
	return new;
}

#undef trap
#undef trap_ptr
static void
forkshell_init(const char *idstr)
{
	struct forkshell *fs;
	void *map_handle;
	HANDLE h;
	struct globals_var **gvpp;
	struct globals_misc **gmpp;
	int i;
	char **ptr;
	char *lrelocate;
	struct jmploc jmploc;

	if (sscanf(idstr, "%p", &map_handle) != 1)
		return;

	h = (HANDLE)map_handle;
	fs = (struct forkshell *)MapViewOfFile(h, FILE_MAP_WRITE, 0,0, 0);
	if (!fs)
		return;

	/* this memory can't be freed */
	sticky_mem_start = fs;
	sticky_mem_end = (char *) fs + fs->size;

	/* pointer fixup */
	lrelocate = (char *)fs + fs->size;
	for (i = 0; i < fs->relocatesize; i++) {
		if (lrelocate[i]) {
			ptr = (char **)((char *)fs + i);
			if (*ptr)
				*ptr = (char *)fs + (*ptr - fs->old_base);
		}
	}

	if (fs->fpid == FS_OPENHERE)
		goto end;

	/* Now fix up stuff that can't be transferred */
	for (i = 0; i < CMDTABLESIZE; i++) {
		struct tblentry *e = fs->cmdtable[i];
		while (e) {
			if (e->cmdtype == CMDBUILTIN)
				e->param.cmd = builtintab + e->param.index;
			e = e->next;
		}
	}
	memset(fs->gmp->trap, 0, sizeof(fs->gmp->trap[0])*NSIG);
	/* fs->gmp->trap_ptr = fs->gmp->trap; */

	/* Set global variables */
	gvpp = (struct globals_var **)&ash_ptr_to_globals_var;
	*gvpp = fs->gvp;
	gmpp = (struct globals_misc **)&ash_ptr_to_globals_misc;
	*gmpp = fs->gmp;
	cmdtable = fs->cmdtable;
#if ENABLE_ASH_ALIAS
	atab = fs->atab;	/* will be NULL for FS_SHELLEXEC */
#endif
#if MAX_HISTORY
	if (fs->cnt_history) {
		line_input_state = new_line_input_t(FOR_SHELL);
		line_input_state->cnt_history = fs->cnt_history;
		for (i = 0; i < line_input_state->cnt_history; i++)
			line_input_state->history[i] = fs->history[i];
	}
#endif

	CLEAR_RANDOM_T(&random_gen); /* or else $RANDOM repeats in child */

	reinitvar();

	if (setjmp(jmploc.loc)) {
		exitreset();
		exitshell();
	}
	exception_handler = &jmploc;

	shlvl++;
	if (fs->mode == FORK_BG) {
		SetConsoleCtrlHandler(NULL, TRUE);
		if (fs->nprocs == 0) {
			close(0);
			if (open(bb_dev_null, O_RDONLY) != 0)
				ash_msg_and_raise_perror("can't open '%s'", bb_dev_null);
		}
	}
	else {
		SetConsoleCtrlHandler(ctrl_handler, TRUE);
	}
#if JOBS_WIN32
	/* do job control only in root shell */
	doing_jobctl = 0;
#endif
 end:
	forkshell_child(fs);
}

#undef free
static void
sticky_free(void *base)
{
	if (base >= sticky_mem_start && base < sticky_mem_end)
		return;
	free(base);
}
#endif

/*-
 * Copyright (c) 1989, 1991, 1993, 1994
 *      The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kenneth Almquist.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ''AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
