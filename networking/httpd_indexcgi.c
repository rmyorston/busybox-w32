/*
 * Copyright (c) 2007 Denys Vlasenko <vda.linux@googlemail.com>
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */

/*
 * This program is a CGI application. It outputs directory index page.
 * Put it into cgi-bin/index.cgi and chmod 0755.
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
httpd_indexcgi.c -o index.cgi
*/

/* We don't use printf, as it pulls in >12 kb of code from uclibc (i386). */
/* Currently malloc machinery is the biggest part of libc we pull in. */
/* We have only one realloc and one strdup, any idea how to do without? */
/* 2026: */
/* Answer: by using getdents and mmap */

/* Sizes
 * 2007: i386, static uclibc, approximate:
 *   text    data     bss     dec     hex filename
 *  13036      44    3052   16132    3f04 index.cgi
 *   2576       4    2048    4628    1214 index.cgi.o
 * 2026: i386, static musl, rewritten to not use malloc:
 *  19222      44    2988   22254    56ee index.cgi_before_mmap_rewrite
 *  12790      40     416   13246    33be index.cgi
 *   3291       0       4    3295     cdf index.cgi.o
 */

#define _GNU_SOURCE 1  /* for strchrnul */
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <dirent.h>
#include <time.h>
#include <sys/mman.h>

/* In order to avoid using malloc, we have to avoid opendir()
 * which in most implementations allocates its return value.
 * Thus, have to use Linux's getdents syscall directly.
 * It has an added benefit of a more efficient storage of filenames
 * (no pointers needed).
 */
#include <fcntl.h>
#include <sys/syscall.h>
struct linux_dirent64 {
	ino64_t        d_ino;
	off64_t        d_off;
	unsigned short d_reclen;
	unsigned char  d_type;
	char           d_name[];
};

/* Appearance of the table is controlled by style sheet *ONLY*,
 * formatting code uses <TAG class=CLASS> to apply style
 * to elements. Edit stylesheet to your liking and recompile. */

#define STYLE_STR \
"<style>"                                              \
"table {"                                              \
  "width:100%;"                                        \
  "background-color:#fff5ee;"                          \
  "border-width:1px;" /* 1px 1px 1px 1px; */           \
  "border-spacing:2px;"                                \
  "border-style:solid;" /* solid solid solid solid; */ \
  "border-color:black;" /* black black black black; */ \
  "border-collapse:collapse"                           \
"}"                                                    \
"th {"                                                 \
  "border-width:1px;" /* 1px 1px 1px 1px; */           \
  "padding:1px;" /* 1px 1px 1px 1px; */                \
  "border-style:solid;" /* solid solid solid solid; */ \
  "border-color:black" /* black black black black; */  \
"}"                                                    \
"td {"                                                 \
             /* top right bottom left */               \
  "border-width:0 1px 0 1px;"                          \
  "padding:1px;" /* 1px 1px 1px 1px; */                \
  "border-style:solid;" /* solid solid solid solid; */ \
  "border-color:black;" /* black black black black; */ \
  "white-space:nowrap"                                 \
"}"                                                    \
"tr:nth-child(odd) { background-color:#ffffff }"       \
"tr.hdr { background-color:#eee5de }"                  \
"tr.foot { background-color:#eee5de }"                 \
"th.cnt { text-align:left }"                           \
"th.sz { text-align:right }"                           \
"th.dt { text-align:right }"                           \
"td.sz { text-align:right }"                           \
"td.dt { text-align:right }"                           \
"col.nm { width:98% }"                                 \
"col.sz { width:1% }"                                  \
"col.dt { width:1% }"                                  \
"</style>"                                             \

typedef struct linux_dirent64 dir_list_t;
/* reuse these fields for storing other values: */
#define D_MODE  d_reclen
#define D_SIZE  d_off
#define D_MTIME d_ino

static int compare_dl(dir_list_t **aa, dir_list_t **bb)
{
	dir_list_t *a = *aa;
	dir_list_t *b = *bb;

	/* ".." is 'less than' any other dir entry */
	if (strcmp(a->d_name, "..") == 0) {
		return -1;
	}
	if (strcmp(b->d_name, "..") == 0) {
		return 1;
	}
	if (S_ISDIR(a->D_MODE) != S_ISDIR(b->D_MODE)) {
		/* 1 if b is a dir (and thus a is 'after' b, a > b),
		 * else -1 (a < b) */
		return (S_ISDIR(b->D_MODE) != 0) ? 1 : -1;
	}
	return strcmp(a->d_name, b->d_name);
}

enum {
	/* Must be >= 64k (all Linux arches have pages <= 64k) */
	BUFFER_SIZE = 8 * 1024*1024,
//one dirent is typically <= 100 bytes, 1M is enough for ~10k files
//FIXME: change code to *iterate* getdents64 if need to support giant file lists
	HEADROOM = 64,
};

static char *buffer;
#if 0
/* Use global "dst" pointer */
static char *dst;
# define INIT_DST   dst = buffer;
# define CHARP      void
# define CHARP_DST  /*nothing*/
# define DST        /*nothing*/
# define SET_DST    /*nothing*/
# define RETURN_DST ((void)0)
#else
/* Propagate "dst" pointer as parameter */
/* This is usually more efficient (uses a register for it) */
# define INIT_DST   char *dst = buffer;
# define CHARP      char*
# define CHARP_DST  char *dst,
# define DST        dst,
# define SET_DST    dst =
# define RETURN_DST return dst
#endif

/* After this call, you have at least size + HEADROOM bytes available
 * ahead of dst */
static CHARP guarantee(CHARP_DST int size)
{
	if (buffer + (BUFFER_SIZE-HEADROOM) - dst < size) {
		write(STDOUT_FILENO, buffer, dst - buffer);
		dst = buffer;
	}
	RETURN_DST;
}

/* NB: formatters do not store terminating NUL! */

static CHARP fmt_str(CHARP_DST const char *src)
{
	unsigned len = strlen(src);
	SET_DST guarantee(DST len);
	dst = mempcpy(dst, src, len);
	RETURN_DST;
}

/* HEADROOM bytes after dst are available after this call */
static CHARP fmt_url(CHARP_DST const char *name)
{
	while (*name) {
		unsigned c = (unsigned char)*name++;
		SET_DST guarantee(DST 3);
		if ((c - '0') > 9 /* not a digit */
		 && ((c|0x20) - 'a') > ('z' - 'a') /* not A-Z or a-z */
		 && !strchr("._-+@", c)
		) {
			*dst++ = '%';
			*dst++ = "0123456789ABCDEF"[c >> 4];
			c = "0123456789ABCDEF"[c & 0xf];
		}
		*dst++ = c;
	}
	RETURN_DST;
}

/* HEADROOM bytes are available after dst after this call */
static CHARP fmt_html(CHARP_DST const char *name)
{
	while (*name) {
		char c = *name++;
		if (c == '<')
			SET_DST fmt_str(DST "&lt;");
		else if (c == '>')
			SET_DST fmt_str(DST "&gt;");
		else if (c == '&') {
			SET_DST fmt_str(DST "&amp;");
		} else {
			SET_DST guarantee(DST 1);
			*dst++ = c;
			continue;
		}
	}
	RETURN_DST;
}

/* HEADROOM bytes are available after dst after this call */
static CHARP fmt_ull(CHARP_DST unsigned long long n)
{
	char buf[sizeof(n)*3 + 2];
	char *p;

	p = buf + sizeof(buf) - 1;
	*p = '\0';
	do {
		*--p = (n % 10) + '0';
		n /= 10;
	} while (n);
	SET_DST fmt_str(DST  p);
	RETURN_DST;
}

/* Does not call guarantee - eats into headroom instead */
static CHARP fmt_02u(CHARP_DST unsigned n)
{
	/* n %= 100; - not needed, callers don't pass big n */
	dst[0] = (n / 10) + '0';
	dst[1] = (n % 10) + '0';
	dst += 2;
	RETURN_DST;
}

/* Does not call guarantee - eats into headroom instead */
static CHARP fmt_04u(CHARP_DST unsigned n)
{
	/* n %= 10000; - not needed, callers don't pass big n */
	SET_DST fmt_02u(DST n / 100);
	SET_DST fmt_02u(DST n % 100);
	RETURN_DST;
}

int main(int argc, char **argv)
{
	char *location;
	dir_list_t **dir_list;
	dir_list_t *cdir;
	int dir_list_count;
	unsigned count_dirs;
	unsigned count_files;
	unsigned long long size_total;
	int dirfd;
	long nread, byte_pos;

	location = getenv("REQUEST_URI");
	if (!location)
		return 1;

	/* drop URL arguments if any */
	strchrnul(location, '?')[0] = '\0';

	if (location[0] != '/'
	 || strstr(location, "//")
	 || strstr(location, "/../")
	 || strcmp(strrchr(location, '/'), "/..") == 0
	) {
		return 1;
	}

	if (chdir("..")
	 || (location[1] && chdir(location + 1))
	) {
		return 1;
	}

	dirfd = open(".", O_RDONLY | O_DIRECTORY);
	if (dirfd == -1)
		return - dirfd;

	/* Allocate 3 BUFFER_SIZE regions, with holes between them:
	 * 1: *dir_list_t[] array
	 * 2: sequence of dir_list_t's as read by getdents (not an array - variable length items)
	 * 3: output char buffer
	 * Importantly, the mmap size is large, but mmap is NOT physically preallocated.
	 * THis way, we'll use only three pages of data is there are not that many files
	 * (the rest of mapped regions will be left unmapped, since it is not accessed).
	 */
//TODO: add MAP_NORESERVE, MAP_UNINITIALIZED flags?
	buffer = mmap(NULL, 5*BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
	if (buffer == MAP_FAILED)
		return 1;

	dir_list = (void*)buffer;
	munmap(buffer + BUFFER_SIZE, BUFFER_SIZE);

	buffer += 2*BUFFER_SIZE;
	munmap(buffer + BUFFER_SIZE, BUFFER_SIZE);

	nread = syscall(SYS_getdents64, dirfd, buffer, BUFFER_SIZE);
	if (nread <= 0)
		return - nread;
//FIXME: we simply won't see any files which did not fit into BUFFER_SIZE

	dir_list_count = 0;
	byte_pos = 0;
	while (byte_pos < nread) {
		struct linux_dirent64 *dp;
		struct stat sb;

		dp = (struct linux_dirent64 *) (buffer + byte_pos);
		byte_pos += dp->d_reclen; /* NB: using this before overwriting it with mode! */

		if (dp->d_name[0] == '.' && !dp->d_name[1])
			continue;
		if (stat(dp->d_name, &sb) != 0)
			continue;
//fprintf(stderr, "%d '%s'\n", dir_list_count, dp->d_name);

		dp->d_off    = sb.st_size;
		dp->d_reclen = sb.st_mode;
		dp->d_ino    = sb.st_mtime;
		dir_list[dir_list_count] = dp;
		dir_list_count++;
	}
	//close(dirfd);
	qsort(dir_list, dir_list_count, sizeof(dir_list[0]), (void*)compare_dl);

	buffer += 2*BUFFER_SIZE;
    {
	INIT_DST
	SET_DST fmt_str(DST
		"" /* Additional headers (currently none) */
		"\r\n" /* Mandatory empty line after headers */
		"<html><head><title>Index of ");
	/* Guard against directories with &, > etc */
	SET_DST fmt_html(DST location);
	SET_DST fmt_str(DST
		"</title>\n"
		STYLE_STR
		"</head>" "\n"
		"<body>" "\n"
		"<h1>Index of ");
	SET_DST fmt_html(DST location);
	SET_DST fmt_str(DST
		"</h1>" "\n"
		"<table>" "\n"
		"<col class=nm><col class=sz><col class=dt>" "\n"
		"<tr class=hdr><th class=cnt>Name<th class=sz>Size<th class=dt>Last modified" "\n");

	count_dirs = 0;
	count_files = 0;
	size_total = 0;
	while (--dir_list_count >= 0) {
		struct tm *ptm;
		time_t tt;

		cdir = *dir_list++;
//fprintf(stderr, "%d '%s'\n", dir_list_count, cdir->d_name);
		if (S_ISDIR(cdir->D_MODE)) {
			count_dirs++;
		} else if (S_ISREG(cdir->D_MODE)) {
			count_files++;
			size_total += cdir->D_SIZE;
		} else
			continue;
//fprintf(stderr, "%d '%s'\n", dir_list_count, cdir->d_name);

		SET_DST fmt_str(DST "<tr><td class=nm><a href='");
		SET_DST fmt_url(DST cdir->d_name); /* %20 etc */
		if (S_ISDIR(cdir->D_MODE))
			*dst++ = '/';
		SET_DST fmt_str(DST "'>");
		SET_DST fmt_html(DST cdir->d_name); /* &lt; etc */
		if (S_ISDIR(cdir->D_MODE))
			*dst++ = '/';
		SET_DST fmt_str(DST "</a><td class=sz>");
		if (S_ISREG(cdir->D_MODE))
			SET_DST fmt_ull(DST cdir->D_SIZE);
		SET_DST fmt_str(DST "<td class=dt>");
		if (sizeof(cdir->D_MTIME) == sizeof(tt))
			ptm = gmtime(&cdir->D_MTIME);
		else {
			tt = cdir->D_MTIME;
			ptm = gmtime(&tt);
		}
		SET_DST fmt_04u(DST 1900 + ptm->tm_year); *dst++ = '-';
		SET_DST fmt_02u(DST ptm->tm_mon + 1); *dst++ = '-';
		SET_DST fmt_02u(DST ptm->tm_mday); *dst++ = ' ';
		SET_DST fmt_02u(DST ptm->tm_hour); *dst++ = ':';
		SET_DST fmt_02u(DST ptm->tm_min); *dst++ = ':';
		SET_DST fmt_02u(DST ptm->tm_sec);
		*dst++ = '\n';
	}

	SET_DST fmt_str(DST "<tr class=foot><th class=cnt>Files: ");
	SET_DST fmt_ull(DST count_files);
	/* count_dirs - 1: we don't want to count ".." */
	SET_DST fmt_str(DST ", directories: ");
	SET_DST fmt_ull(DST count_dirs - 1);
	SET_DST fmt_str(DST "<th class=sz>");
	SET_DST fmt_ull(DST size_total);
	SET_DST fmt_str(DST "<th class=dt>\n");
	/* "</table></body></html>" - why bother? */
	SET_DST guarantee(DST BUFFER_SIZE * 2); /* flush */

	return 0;
    }
}
