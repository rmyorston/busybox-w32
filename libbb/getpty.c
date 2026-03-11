/* vi: set sw=4 ts=4: */
/*
 * Mini getpty implementation for busybox
 * Bjorn Wesen, Axis Communications AB (bjornw@axis.com)
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
#include "libbb.h"

#define DEBUG 0

// On modern systems (old ways were more kludgy with setuid "pt_chown" binary):
// ptyfd = open("/dev/ptmx"):
// Kernel creates slave /dev/pts/N with owner = real UID of opener,
// group and mode as configured by /dev/pts mount options.
// The mode is safe with standard mount options (gid=5,mode=620).
//
// grantpt(ptyfd) is now only a verification step. In glibc:
// Calls ioctl(fd, TIOCGPTN, &minor) to confirm ptyfd is valid master pty.
// If ioctl fails with ENOTTY, remaps to EINVAL (POSIX).
// Otherwise returns 0. (No chown/chmod occurs.)
//
// unlockpt(ptyfd) is ioctl(TIOCSPTLCK, 0): clears optional protection flag
// on slave. If flag was set, open(slave) fails with EIO for everyone (even root)
// until unlocked. [Do modern kernels still set the lock in open("/dev/ptmx")?]
// The lock historically prevented opening slave before grantpt() fixes perms
// in misconfigured/old setups where kernel didn't set 0620 atomically.
// Today it's mostly belt-and-suspenders + POSIX compatibility.
//
// The attack thwarted by the lock (assuming unprivileged adversary):
// you open /dev/ptmx, kernel allocates /dev/pts/N with overly permissive mode.
// Adversary manages to open /dev/pts/N before your grantpt() call
// locks down permissions.
// Typically, /dev/pts mount options are gid=5,mode=620, 5 = "tty" group,
// system is set up with only trusted binaries and no users in "tty" group.
// In this case, the locking mechanism is overkill (would be safe without it).
// To fortify more, you can mount with gid=0,mode=600. Tools like "wall",
// "mesg" would stop working.
// Processes with my own UID can still race against me, but I probably
// trust myself...
//
// ptsname(ptyfd), internally ioctl(TIOCGPTN): get index, build
// "/dev/pts/NN" string which refers to the slave pty.
int FAST_FUNC xgetpty(char *line)
{
#if ENABLE_FEATURE_DEVPTS
	int ptyfd;

	ptyfd = open("/dev/ptmx", O_RDWR);
	if (ptyfd < 0)
		bb_simple_perror_msg_and_die("can't find free pty");
	grantpt(ptyfd); /* chmod+chown corresponding slave pty */
	unlockpt(ptyfd); /* allow open() on slave /dev node */
# ifndef HAVE_PTSNAME_R
	{
		const char *name;
		name = ptsname(ptyfd); /* find out the name of slave pty */
		if (!name) {
			bb_simple_perror_msg_and_die("ptsname error (is /dev/pts mounted?)");
		}
		safe_strncpy(line, name, GETPTY_BUFSIZE);
	}
# else
	/* find out the name of slave pty */
	if (ptsname_r(ptyfd, line, GETPTY_BUFSIZE-1) != 0) {
		bb_simple_perror_msg_and_die("ptsname error (is /dev/pts mounted?)");
	}
	line[GETPTY_BUFSIZE-1] = '\0';
# endif
	return ptyfd;

#else
	struct stat stb;
	int ptyfd;
	int i;
	int j;

	strcpy(line, "/dev/ptyXX");

	for (i = 0; i < 16; i++) {
		line[8] = "pqrstuvwxyzabcde"[i];
		line[9] = '0';
		if (stat(line, &stb) < 0) {
			continue;
		}
		for (j = 0; j < 16; j++) {
			line[9] = j < 10 ? j + '0' : j - 10 + 'a';
			if (DEBUG)
				fprintf(stderr, "Trying to open device: %s\n", line);
			ptyfd = open(line, O_RDWR | O_NOCTTY);
			if (ptyfd >= 0) {
				line[5] = 't';
				return ptyfd;
			}
		}
	}
	bb_simple_error_msg_and_die("can't find free pty");
#endif /* FEATURE_DEVPTS */
}
