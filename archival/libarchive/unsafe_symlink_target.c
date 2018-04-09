/* vi: set sw=4 ts=4: */
/*
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
#include "libbb.h"
#include "bb_archive.h"

void FAST_FUNC create_or_remember_symlink(llist_t **symlink_placeholders
		IF_PLATFORM_MINGW32(UNUSED_PARAM),
		const char *target,
		const char *linkname)
{
#if !ENABLE_PLATFORM_MINGW32
	if (target[0] == '/' || strstr(target, "..")) {
		llist_add_to(symlink_placeholders,
			xasprintf("%s%c%s", linkname, '\0', target)
		);
		return;
	}
	if (symlink(target, linkname) != 0) {
		/* shared message */
		bb_perror_msg_and_die("can't create %slink '%s' to '%s'",
			"sym", linkname, target
		);
	}
#else
	/* symlink isn't implemented for WIN32, just issue a warning */
	bb_perror_msg("can't create %slink '%s' to '%s'", "sym", linkname, target);
#endif
}

#if !ENABLE_PLATFORM_MINGW32
void FAST_FUNC create_symlinks_from_list(llist_t *list)
{
	while (list) {
		char *target;

		target = list->data + strlen(list->data) + 1;
		if (symlink(target, list->data)) {
			/* shared message */
			bb_error_msg_and_die("can't create %slink '%s' to '%s'",
				"sym",
				list->data, target
			);
		}
		list = list->link;
	}
}
#endif
