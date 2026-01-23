/*
 * Copyright (c) 2026 Denys Vlasenko <vda.linux@googlemail.com>
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */

/*
 * This program is a CGI application. It is intended to rate-limit
 * invocations of another, presumably resource-intensive CGI
 * which you want to only allow less than N instances at any one time.
 *
 * Any extra clients who try to run the CGI will get the
 * "429 Too Many Requests" HTTP response.
 *
 * The most efficient way to do so is to use a shebang-style executable file:
 * #!/path/to/httpd_ratelimit_cgi /tmp/lockdir 99 /path/to/expensive_cgi
 */

/* Build a-la
i486-linux-uclibc-gcc \
-static -static-libgcc \
-D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 \
-Wall -Wshadow -Wwrite-strings -Wundef -Wstrict-prototypes -Werror \
-Wold-style-definition -Wdeclaration-after-statement -Wno-pointer-sign \
-Wmissing-prototypes -Wmissing-declarations \
-Os -fno-builtin-strlen -finline-limit=0 -fomit-frame-pointer \
-ffunction-sections -fdata-sections -fno-guess-branch-probability \
-funsigned-char \
-falign-functions=1 -falign-jumps=1 -falign-labels=1 -falign-loops=1 \
-march=i386 -mpreferred-stack-boundary=2 \
-Wl,-Map -Wl,link.map -Wl,--warn-common -Wl,--sort-common -Wl,--gc-sections \
httpd_ratelimit_cgi.c -o httpd_ratelimit_cgi
*/
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h> /* mkdir */
#include <limits.h>

static ssize_t full_write(int fd, const void *buf, size_t len)
{
	ssize_t cc;
	ssize_t total;

	total = 0;

	while (len) {
		cc = write(fd, buf, len);

		if (cc < 0) {
			if (total) {
				/* we already wrote some! */
				/* user can do another write to know the error code */
				return total;
			}
			return cc;  /* write() returns -1 on failure. */
		}

		total += cc;
		buf = ((const char *)buf) + cc;
		len -= cc;
	}

	return total;
}

static void full_write2(int fd, const char *msg, const char *msg2)
{
	full_write(fd, msg, strlen(msg));
	full_write(fd, " '", 2);
	full_write(fd, msg2, strlen(msg2));
	full_write(fd, "'\n", 2);
}

static void write_and_die(int fd, const char *msg)
{
	full_write(fd, msg, strlen(msg));
	exit(0);
}

static void write_and_die2(int fd, const char *msg, const char *msg2)
{
	full_write2(fd, msg, msg2);
	exit(0);
}

static void fmt_ul(char *dst, unsigned long n)
{
	char buf[sizeof(n)*3 + 2];
	char *p;

	p = buf + sizeof(buf) - 1;
	*p = '\0';
	do {
		*--p = (n % 10) + '0';
		n /= 10;
	} while (n);
	strcpy(dst, p);
}

static long get_no(const char *s)
{
	const char *start = s;
	long v = 0;
	while (*s >= '0' && *s <= '9')
		v = v * 10 + (*s++ - '0');
	if (start == s || *s != '\0' /*|| v < 0*/)
		return -1;
	return v;
}

int main(int argc, char **argv)
{
	const char *lock_dir = ".";
	unsigned long max_slots;
	char *sp;
	char *symno;
	unsigned slot_num;
	pid_t my_pid;
	char my_pid_str[sizeof(long)*3];

	argv++;
	if (!argv[0] || !argv[1])
		write_and_die(2, "Usage: ratelimit [LOCKDIR] MAX_PROCS PROG [ARGS]\n");

	/* ratelimit "[LOCKDIR] MAX_PROCS PROG" SHEBANG [ARGS] syntax?
	 * This happens if we are running as shebang file
	 * of the form "!#/path/to/ratelimit [/tmp/cgit] 10 CGI_BINARY"
	 * (in this case argv[1] is the shebang's filename) */
	sp = strchr(argv[0], ' ');
	if (sp) {
		*sp++ = '\0';
		/* convert to ratelimit "SOME\0THING" SHEBANG [ARGS] form */
		/*                       argv1 ^ */
		argv[1] = sp;
		sp = strchr(sp, ' ');
		if (sp) { /* "THING" also has a space? There is a LOCKDIR! */
			*sp++ = '\0';
			/* convert to ratelimit "SOME\0THI\0G" SHEBANG [ARGS] form */
			/*                        argv0^    ^argv1 */
			lock_dir = argv[0];
			argv[0] = argv[1];
			argv[1] = sp;
			goto get_max;
		}
	}

	max_slots = get_no(argv[0]);
	if (max_slots > 9999) {
		/* ratelimit LOCKDIR MAX_PROCS PROG [ARGS] */
		lock_dir = argv[0];
		if (!lock_dir[0])
			write_and_die2(2, "Bad LOCKDIR", argv[0]);
		argv++;
 get_max:
		max_slots = get_no(argv[0]);
		if (max_slots > 9999)
			write_and_die2(2, "Bad MAX_PROCS", argv[0]);
	}
	argv++; /* points to PROG [ARGS] */

    {
	char slot_path[strlen(lock_dir) + 16];
	symno = stpcpy(stpcpy(slot_path, lock_dir), "/lock.");

	my_pid = getpid();
	fmt_ul(my_pid_str, my_pid);

	/* Ensure lock directory exists (idempotent, ignores errors) */
	if (lock_dir[0] != '.' || lock_dir[1]) /* Don't bother with "." */
		mkdir(lock_dir, 0755);

	/* Starting slot varies per process */
	slot_num = my_pid;

	/* max_slots = 0 is allowed for testing */
	if (max_slots != 0) for (int i = 0; i < max_slots; i++) {
		slot_num = (slot_num + 1) % max_slots;
		fmt_ul(symno, slot_num);

		while (1) {
			char buf[32];
			ssize_t len;
			long old_pid;

			/* Try to claim atomically */
			if (symlink(my_pid_str, slot_path) == 0)
				goto exec;

			/* Only handle EEXIST - other errors skip to next slot */
			if (errno != EEXIST)
				break;

			/* Read existing target PID */
			len = readlink(slot_path, buf, sizeof(buf) - 1);
			if (len < 1) {
				/* Broken/empty - clean up and retry */
				unlink(slot_path);
				continue;
			}
			buf[len] = '\0';

			/* Parse PID */
			old_pid = get_no(buf);
			if (old_pid <= 0 || old_pid > INT_MAX) {
				/* Invalid PID string - clean up and retry */
				unlink(slot_path);
				continue;
			}

			/* Check if old process is alive */
			if (kill(old_pid, 0) == 0 || errno != ESRCH) {
				/* Alive (or unexpected error): slot in use, try next */
				break;
			}

			/* Dead: clean up and retry this slot */
			unlink(slot_path);
			/* Loop continues to retry symlink() */
		}
	}

	/* No slot available, return 429 */
	write_and_die(1, "Status: 429 Too Many Requests\r\n"
	"Content-Type: text/plain\r\n"
	"Retry-After: 60\r\n"
	"Connection: close\r\n\r\n"
	"Too many concurrent requests\n"
	);
	return 0;
    }

exec:
	execv(argv[0], argv);
	full_write2(2, "can't execute", argv[0]);
	write_and_die(1, "Status: 500 Internal Server Error\r\n"
	"Content-Type: text/plain\r\n\r\n"
	"Failed to execute binary\n");
	return 1;
}