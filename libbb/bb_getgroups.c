/*
 * Utility routines.
 *
 * Copyright (C) 2017 Denys Vlasenko
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */

//kbuild:lib-y += bb_getgroups.o

#include "libbb.h"

gid_t* FAST_FUNC bb_getgroups(int *ngroups, gid_t *group_array)
{
	int n = ngroups ? *ngroups : 0;

	/* getgroups may be a bit expensive, try to use it only once */
	if (n < 32)
		n = 32;

	for (;;) {
// FIXME: ash tries so hard to not die on OOM (when we are called from test),
// and we spoil it with just one xrealloc here
		group_array = xrealloc(group_array, (n+1) * sizeof(group_array[0]));
		n = getgroups(n, group_array);
		/*
		 * If buffer is too small, kernel does not return new_n > n.
		 * It returns -1 and EINVAL:
		 */
		if (n >= 0) {
			/* Terminator for bb_getgroups(NULL, NULL) usage */
			group_array[n] = (gid_t) -1;
			break;
		}
		if (errno == EINVAL) { /* too small? */
			/* This is the way to ask kernel how big the array is */
			n = getgroups(0, group_array);
			continue;
		}
		/* Some other error (should never happen on Linux) */
		bb_simple_perror_msg_and_die("getgroups");
	}

	if (ngroups)
		*ngroups = n;
	return group_array;
}

/* Return non-zero if GID is in our supplementary group list. */
int FAST_FUNC is_in_supplementary_groups(int *pngroups, gid_t **pgroup_array, gid_t gid)
{
	int i;
	int ngroups;
	gid_t *group_array;

	if (*pngroups == 0)
		*pgroup_array = bb_getgroups(pngroups, NULL);
	ngroups = *pngroups;
	group_array = *pgroup_array;

	/* Search through the list looking for GID. */
	for (i = 0; i < ngroups; i++)
		if (gid == group_array[i])
			return 1;

	return 0;
}
