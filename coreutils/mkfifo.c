/* vi: set sw=4 ts=4: */
/*
 * Mini mkfifo implementation for busybox
 *
 * Copyright (C) 1999 by Randolph Chung <tausq@debian.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include "internal.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

static const char mkfifo_usage[] = "mkfifo [OPTIONS] name\n"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\nCreates a named pipe (identical to 'mknod name p')\n\n"
	"Options:\n"
	"\t-m\tcreate the pipe using the specified mode (default a=rw)\n"
#endif
	;

extern int mkfifo_main(int argc, char **argv)
{
	char *thisarg;
	mode_t mode = 0666;

	argc--;
	argv++;

	/* Parse any options */
	while (argc > 1) {
		if (**argv != '-')
			usage(mkfifo_usage);
		thisarg = *argv;
		thisarg++;
		switch (*thisarg) {
		case 'm':
			argc--;
			argv++;
			parse_mode(*argv, &mode);
			break;
		default:
			usage(mkfifo_usage);
		}
		argc--;
		argv++;
	}
	if (argc < 1)
		usage(mkfifo_usage);
	if (mkfifo(*argv, mode) < 0) {
		perror("mkfifo");
		exit(255);
	} else {
		exit(TRUE);
	}
}
