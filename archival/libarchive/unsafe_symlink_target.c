/* vi: set sw=4 ts=4: */
/*
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
#include "libbb.h"
#include "bb_archive.h"

void FAST_FUNC create_or_remember_link(llist_t **link_placeholders,
		const char *target,
		const char *linkname,
		int hard_link)
{
#if ENABLE_PLATFORM_MINGW32
	/* defer reporting error if symlink(2) fails on Windows */
	if (hard_link || target[0] == '/' || strstr(target, "..") ||
			symlink(target, linkname) != 0) {
#else
	if (hard_link || target[0] == '/' || strstr(target, "..")) {
#endif
		llist_add_to_end(link_placeholders,
			xasprintf("%c%s%c%s", hard_link, linkname, '\0', target)
		);
		return;
	}
#if !ENABLE_PLATFORM_MINGW32
	if (symlink(target, linkname) != 0) {
		// shared message
		bb_perror_msg_and_die("can't create %slink '%s' to '%s'",
			"sym", linkname, target
		);
	}
#endif
}

static int all_dir_components_are_dirs(char *path)
{
	struct stat sb;
	char *slash;

	slash = path;
	while ((slash = strchr(slash, '/')) != NULL) {
		int ok;
		*slash = '\0';
		ok = (lstat(path, &sb) == 0 && S_ISDIR(sb.st_mode));
		*slash++ = '/';
		if (!ok)
			return 0;
	}
	return 1;
}

void FAST_FUNC create_links_from_list(llist_t *list)
{
	// This idea sounds better (fewer file ops):
	//  Create hardlinks first, then symlinks.
	// ^^^ but it breaks the "hardlink to symlink" case:
	//  mkdir dir
	//  >dir/a
	//  ln -s ../dir/a dir/b
	//  ln dir/b dir/c
	//  mkdir new; tar cf - dir/a dir/b dir/c | tar -C new -xvf -
	// On extract, 'c' will have no 'b' to be a hardlink to.

	while (list) {
		char *target;
		char *linkname = list->data + 1;

		target = linkname + strlen(linkname) + 1;

		// Check whether the linkname has suspicious components:
		// SYMLINK_OUTSIDE_EXTRACT_DIR/etc/passwd sort of thing.
		// (Our delayed creation of symlinks is not defending against it,
		// we could create SYMLINK_OUTSIDE_EXTRACT_DIR ourself!)
		if (!all_dir_components_are_dirs(linkname)) {
			errno = ENOTDIR;
			goto err;
		}
		if ((list->data[0] ? link : symlink) (target, linkname)) {
			// shared message
 err:
			bb_perror_msg_and_die("can't create %slink '%s' to '%s'",
				list->data[0] ? "hard" : "sym",
				linkname, target
			);
			// Note: GNU tar 1.34 errors out only _after_ all links are (attempted to be) created
			// We bail out at once
		}
		list = list->link;
	}
}
