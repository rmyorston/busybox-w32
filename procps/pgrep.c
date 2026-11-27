/* vi: set sw=4 ts=4: */
/*
 * Mini pgrep/pkill implementation for busybox
 *
 * Copyright (C) 2007 Loic Grenie <loic.grenie@gmail.com>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
//config:config PGREP
//config:	bool "pgrep (6.8 kb)"
//config:	default y
//config:	help
//config:	Look for processes by name.
//config:
//config:config PKILL
//config:	bool "pkill (7.8 kb)"
//config:	default y
//config:	help
//config:	Send signals to processes by name.

//applet:IF_PGREP(APPLET_ODDNAME(pgrep, pgrep, BB_DIR_USR_BIN, BB_SUID_DROP, pgrep))
//                APPLET_ODDNAME:name   main   location        suid_type     help
//applet:IF_PKILL(APPLET_ODDNAME(pkill, pgrep, BB_DIR_USR_BIN, BB_SUID_DROP, pkill))
/* can't be noexec: can find _itself_ under wrong name, since after fork only,
 * /proc/PID/cmdline and comm are wrong! Can fix comm (prctl(PR_SET_NAME)),
 * but cmdline?
 */

//kbuild:lib-$(CONFIG_PGREP) += pgrep.o
//kbuild:lib-$(CONFIG_PKILL) += pgrep.o

//usage:#define pgrep_trivial_usage
//usage:     IF_NOT_PLATFORM_MINGW32(
//usage:       "[-nofxval] -s SID|-P PPID|-u EUID|-U UID|PATTERN"
//usage:     )
//usage:     IF_PLATFORM_MINGW32(
//usage:       "[-xvl] [-P PPID|PATTERN]"
//usage:     )
//usage:#define pgrep_full_usage "\n\n"
//usage:       "Display processes selected by regex PATTERN\n"
//usage:     IF_NOT_PLATFORM_MINGW32(
//usage:     "\n	-n	Show newest process only"
//usage:     "\n	-o	Show oldest process only"
//usage:     "\n	-f	Match against entire command line"
//usage:     )
//usage:     "\n	-x	Match whole name (not substring)"
//usage:     IF_NOT_PLATFORM_MINGW32(
//usage:     "\n	-s SID	Match session ID (0 for current)"
//usage:     )
//usage:     "\n	-P PPID	Match parent process ID"
//usage:     IF_NOT_PLATFORM_MINGW32(
//usage:     "\n	-u EUID Match effective UID"
//usage:     "\n	-U UID  Match UID"
//usage:     )
//usage:     "\n	-v	Negate match"
//usage:     IF_NOT_PLATFORM_MINGW32(
//usage:     "\n	-a	Show command line too"
//usage:     )
//usage:     "\n	-l	Show command name too"
//usage:
//usage:#define pkill_trivial_usage
//usage:     IF_NOT_PLATFORM_MINGW32(
//usage:       "[-SIGNAL] [-nofxve] -s SID|-P PPID|-u EUID|-U UID|PATTERN"
//usage:     )
//usage:     IF_PLATFORM_MINGW32(
//usage:       "[-SIGNAL] [-xve] -P PPID|PATTERN"
//usage:     )
//usage:#define pkill_full_usage "\n\n"
//usage:       "Send signal to processes selected by regex PATTERN\n"
//usage:     IF_NOT_PLATFORM_MINGW32(
//usage:     "\n	-n	Signal newest process only"
//usage:     "\n	-o	Signal oldest process only"
//usage:     "\n	-f	Match against entire command line"
//usage:     )
//usage:     "\n	-x	Match whole name (not substring)"
//usage:     IF_NOT_PLATFORM_MINGW32(
//usage:     "\n	-s SID	Match session ID (0 for current)"
//usage:     )
//usage:     "\n	-P PPID	Match parent process ID"
//usage:     IF_NOT_PLATFORM_MINGW32(
//usage:     "\n	-u EUID Match effective UID"
//usage:     "\n	-U UID  Match UID"
//usage:     )
//usage:     "\n	-v	Negate match"
//usage:     "\n	-e	Display name and PID of process being killed"

#include "libbb.h"
#include "xregex.h"

/* Idea taken from kill.c */
#define pgrep (ENABLE_PGREP && (!ENABLE_PKILL || applet_name[1] == 'g'))
#define pkill (ENABLE_PKILL && (!ENABLE_PGREP || applet_name[1] == 'k'))

enum {
	/* "vlafxoneu:U:s:+P:+" */
	/* "vlxeP:+" for Windows */
	OPTBIT_v = 0, /* must be first, we need OPT_INVERT = 0/1 */
	OPTBIT_l, /* should be pgrep-only, do we care? */
#if !ENABLE_PLATFORM_MINGW32
	OPTBIT_a, /* should be pgrep-only, do we care? */
	OPTBIT_f,
#else
# define OPTBIT_a OPTBIT_l
#endif
	OPTBIT_x,
#if !ENABLE_PLATFORM_MINGW32
	OPTBIT_o,
	OPTBIT_n,
#endif
	OPTBIT_e, /* should be pkill-only, do we care? */
#if !ENABLE_PLATFORM_MINGW32
	OPTBIT_u,
	OPTBIT_U,
	OPTBIT_s,
#endif
	OPTBIT_P,
};

#define OPT_INVERT	(1 << OPTBIT_v)
#define OPT_LIST	(1 << OPTBIT_l)
#if !ENABLE_PLATFORM_MINGW32
#define OPT_LISTFULL	(1 << OPTBIT_a)
#define OPT_FULL	(1 << OPTBIT_f)
#else
#define OPT_LISTFULL	(0)
#define OPT_FULL	(0)
#endif
#define OPT_ANCHOR	(1 << OPTBIT_x)
#if !ENABLE_PLATFORM_MINGW32
#define OPT_FIRST	(1 << OPTBIT_o)
#define OPT_LAST	(1 << OPTBIT_n)
#else
#define OPT_FIRST	(0)
#define OPT_LAST	(0)
#endif
#define OPT_ECHO	(1 << OPTBIT_e)
#if !ENABLE_PLATFORM_MINGW32
#define OPT_EUID	(1 << OPTBIT_u)
#define OPT_RUID	(1 << OPTBIT_U)
#define OPT_SID		(1 << OPTBIT_s)
#else
#define OPT_EUID	(0)
#define OPT_RUID	(0)
#define OPT_SID		(0)
#endif
#define OPT_PPID	(1 << OPTBIT_P)

static void act(unsigned pid, char *cmd, int signo)
{
	if (pgrep) {
		if (option_mask32 & (OPT_LIST|OPT_LISTFULL)) /* -l or -a */
			printf("%u %s\n", pid, cmd);
		else
			printf("%u\n", pid);
	} else {
		kill(pid, signo);
		if (option_mask32 & OPT_ECHO)
			printf("%s killed (pid %u)\n", cmd, pid);
	}
}

int pgrep_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int pgrep_main(int argc UNUSED_PARAM, char **argv)
{
	unsigned pid;
	int signo;
	unsigned opt;
	int scan_mask;
	int matched_pid;
#if ENABLE_PLATFORM_MINGW32
	int ppid2match;
#else
	int sid2match, ppid2match, uid2match, euid2match;
	char *uid_arg, *euid_arg;
#endif
	char *cmd_last;
	procps_status_t *proc;
	/* These are initialized to 0 */
	struct {
		regex_t re_buffer;
		regmatch_t re_match[1];
	} Z;
#define re_buffer (Z.re_buffer)
#define re_match  (Z.re_match )

	memset(&Z, 0, sizeof(Z));

	/* Parse -SIGNAL for pkill. Must be first option, if present */
	signo = SIGTERM;
	if (pkill && argv[1] && argv[1][0] == '-') {
		int temp = get_signum(argv[1]+1);
		if (temp != -1) {
			signo = temp;
			argv++;
		}
	}

	/* Parse remaining options */
	ppid2match = -1;
#if ENABLE_PLATFORM_MINGW32
	opt = getopt32(argv, "vlxeP:+", &ppid2match);
#else
	sid2match = -1;
	uid2match = -1;
	euid2match = -1;
	opt = getopt32(argv, "^" "vlafxoneu:U:s:+P:+" "\0" "?1"/* 1 arg max*/,
				&euid_arg, &uid_arg, &sid2match, &ppid2match);
#endif
	argv += optind;

	pid = getpid();
#if !ENABLE_PLATFORM_MINGW32
	if (sid2match == 0)
		sid2match = getsid(pid);
#endif

	scan_mask = PSSCAN_COMM | PSSCAN_ARGV0;
	if (opt & OPT_FULL)
		scan_mask |= PSSCAN_ARGVN;
#if !ENABLE_PLATFORM_MINGW32
	if (opt & OPT_EUID) {
		scan_mask |= PSSCAN_UIDGID;
		euid2match = get_ug_id(euid_arg, xuname2uid);
	}
	if (opt & OPT_RUID) {
		scan_mask |= PSSCAN_RUIDGID;
		uid2match = get_ug_id(uid_arg, xuname2uid);
	}
#endif

	/* One pattern is required, if no -s/-P/-u/-U */
#if ENABLE_PLATFORM_MINGW32
	if (ppid2match < 0
#else
	if ((sid2match & ppid2match & euid2match & uid2match) < 0 // -s/-P/-u/-U: all not set
#endif
	 && !argv[0] // no PATTERN
	) {
		bb_show_usage();
	}

	if (argv[0])
		xregcomp(&re_buffer, argv[0], opt & OPT_ANCHOR ? REG_EXTENDED : (REG_EXTENDED|REG_NOSUB));

	matched_pid = 0;
	cmd_last = NULL;
	proc = NULL;
	while ((proc = procps_scan(proc, scan_mask)) != NULL) {
		char *cmd;
		int cmdlen, match;

		if (proc->pid == pid)
			continue;

		if (!opt & OPT_INVERT) {
			/* Quickly reject -sN -PN -uN -UN mismatches... unless -v */
			if (ppid2match >= 0 && ppid2match != proc->ppid)
				continue;
#if !ENABLE_PLATFORM_MINGW32
			if (sid2match >= 0 && sid2match != proc->sid)
				continue;
			if (euid2match >= 0 && euid2match != proc->uid)
				continue;
			if (uid2match >= 0 && uid2match != proc->ruid)
				continue;
#endif
		}

		cmdlen = -1;
#if ENABLE_PLATFORM_MINGW32
		cmd = proc->comm;
#else
		cmd = proc->argv0;
		if (!cmd) {
			cmd = proc->comm;
		} else {
			int i = proc->argv_len;

			if (!(opt & OPT_LISTFULL))
				cmdlen = strlen(cmd); /* not -a: find first NUL */
			/*
			 * "sleep 11" looks like "sleep""\0""11""\0" in argv0.
			 * Make sure last "\0" does not get converted to " ":
			 */
			if (i && cmd[i-1] == '\0')
				i--;
			while (--i >= 0) {
				if ((unsigned char)cmd[i] < ' ')
					cmd[i] = ' ';
			}
		}
#endif

		if (opt & OPT_INVERT) {
			/* "pgrep -v -P1 firefox" means "not (ppid=1 AND name=firefox)"
			 * or equivalently "ppid!=1 OR name!=firefox".
			 * Check the first condition and if true, skip matching.
			 */
			if (ppid2match >= 0 && ppid2match != proc->ppid)
				goto got_it;
#if !ENABLE_PLATFORM_MINGW32
			if (sid2match >= 0 && sid2match != proc->sid)
				goto got_it;
			if (euid2match >= 0 && euid2match != proc->uid)
				goto got_it;
			if (uid2match >= 0 && uid2match != proc->ruid)
				goto got_it;
#endif
		}

		match = !argv[0]; /* if no PATTERN, then it's a match, else... */
		if (!match) {
 again:
			match = (regexec(&re_buffer, cmd, 1, re_match, 0) == 0);
			if (match && (opt & OPT_ANCHOR)) {
				/* -x requires full string match */
				match = (re_match[0].rm_so == 0 && cmd[re_match[0].rm_eo] == '\0');
			}
			if (!match && cmd != proc->comm) {
				/* if argv[] did not match, try comm */
				cmdlen = -1;
				cmd = proc->comm;
				goto again;
			}
		}

		/* NB: OPT_INVERT is always 0 or 1 */
		if (match ^ (opt & OPT_INVERT)) {
 got_it:
			matched_pid = proc->pid;
			if (opt & OPT_LAST) {
				free(cmd_last);
				cmd_last = xstrdup(cmd);
				continue;
			}
			if (cmdlen >= 0)
				cmd[cmdlen] = '\0';
			act(proc->pid, cmd, signo);
			if (opt & OPT_FIRST)
				break;
		}
	}

	if (cmd_last) {
		act(matched_pid, cmd_last, signo);
		if (ENABLE_FEATURE_CLEAN_UP)
			free(cmd_last);
	}
	return matched_pid == 0; /* return 1 if no processes listed/signaled */
}
