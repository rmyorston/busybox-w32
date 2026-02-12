/* vi: set sw=4 ts=4: */
/*
 * Mini lsblk implementation for busybox
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
//config:config LSBLK
//config:	bool "lsblk (2.5 kb)"
//config:	default y
//config:	help
//config:	List information about all available or specified block devices.

//applet:IF_LSBLK(APPLET(lsblk, BB_DIR_USR_BIN, BB_SUID_DROP))

//kbuild:lib-$(CONFIG_LSBLK) += lsblk.o

//usage:#define lsblk_trivial_usage
//usage:       "[BLOCKDEV]..."
//usage:#define lsblk_full_usage "\n\n"
//usage:       "List block devices"

/* from util-linux 2.41.1
 -A, --noempty        don't print empty devices
 -D, --discard        print discard capabilities
 -E, --dedup <column> de-duplicate output by <column>
 -I, --include <list> show only devices with specified major numbers
 -J, --json           use JSON output format
 -M, --merge          group parents of sub-trees (RAIDs, Multi-path)
 -O, --output-all     output all columns
 -P, --pairs          use key="value" output format
 -Q, --filter <expr>  print only lines matching the expression
     --highlight <expr> colorize lines matching the expression
     --ct-filter <expr> restrict the next counter
     --ct <name>[:<param>[:<func>]] define a custom counter
 -S, --scsi           output info about SCSI devices
 -N, --nvme           output info about NVMe devices
 -v, --virtio         output info about virtio devices
 -T, --tree[=<column>] use tree format output
 -a, --all            print all devices
 -b, --bytes          print SIZE in bytes instead of a human-readable format
 -d, --nodeps         don't print slaves or holders
 -e, --exclude <list> exclude devices by major number (default: RAM disks)
 -f, --fs             output info about filesystems
 -i, --ascii          use ascii characters only
 -l, --list           use list format output
 -m, --perms          output info about permissions
 -n, --noheadings     don't print headings
 -o, --output <list>  output columns (see --list-columns)
 -p, --paths          print complete device path
 -r, --raw            use raw output format
 -s, --inverse        inverse dependencies
 -t, --topology       output info about topology
 -w, --width <num>    specifies output width as number of characters
 -x, --sort <column>  sort output by <column>
 -y, --shell          use column names that can be used as shell variables
 -z, --zoned          print zone related information
     --sysroot <dir>  use specified directory as system root
     --properties-by <list>
                      methods used to gather data (default: file,udev,blkid)
 -H, --list-columns   list the available columns
*/
#include "libbb.h"
#include <mntent.h>

struct blockdev_info {
	char *name;
	unsigned long long size;
	const char *type;
	const char *majmin;
	//const char *mountpoint;
};

struct globals {
	struct blockdev_info *list;
	char *mountinfo;
	unsigned count;
	unsigned exitcode;
};
#define G (*ptr_to_globals)
#define INIT_G() do { \
	SET_PTR_TO_GLOBALS(xzalloc(sizeof(G))); \
} while (0)

static struct blockdev_info *get_or_create_info(const char *devname)
{
	unsigned i;

	for (i = 0; i < G.count; i++) {
		if (strcmp(G.list[i].name, devname) == 0)
			return &G.list[i];
	}
	G.list = xrealloc_vector(G.list, 4, G.count);
	G.list[G.count].name = xstrdup(devname);
	return &G.list[G.count++];
}

static char *get_mountpoints(const char *majmin)
{
	unsigned len;
	char *p, *mountpoints;

	mountpoints = NULL;
	len = strlen(majmin);
	p = G.mountinfo; // contents of "/proc/self/mountinfo"
	/* lines a-la "63 1 259:3 / /MNTPOINT per-mount_options - ext4 /dev/NAME per-superblock_options" */
	while (*p) {
		char *e;

		p = skip_non_whitespace(p);
		if (*p != ' ') break;
		// at " 1 259:3"
		p = skip_non_whitespace(p + 1);
		if (*p != ' ') break;
		p++;
		// at "259:3 / /MNTPOINT"
		if (strncmp(p, majmin, len) != 0 || (p+=len)[0] != ' ') {
			p = strchr(p, '\n');
			if (!p)
				break;
			p++;
			continue;
		}
		// at " / /MNTPOINT"
		p = skip_non_whitespace(p + 1);
		if (*p != ' ') break;

		// at " /MNTPOINT"
		e = skip_non_whitespace(p + 1);
		// e is at the end of " /MNTPOINT"
// NO. We return " /MNT1 /MNT2 /MNT3" _with_ leading space!
//		if (!mountpoints)
//			p++;
		xasprintf_inplace(mountpoints, "%s%.*s",
			mountpoints ? mountpoints : "", (int)(e - p), p);
	}
	return mountpoints;
}

static char *get_majmin_from_stat(const char *filename)
{
	struct stat st;
	if (stat(filename, &st) == 0 && S_ISBLK(st.st_mode)) {
		return xasprintf("%u:%u",
			(unsigned)major(st.st_rdev),
			(unsigned)minor(st.st_rdev)
		);
	}
	bb_error_msg("%s: not a block device", filename);
	G.exitcode |= 64; /* util-linux compat */
	return NULL;
}

static unsigned long long read_ull(const char *path, const char *name)
{
	char *filename;
	ssize_t len;
	unsigned long long size = ~0ULL; /* error return is all-ones */
	char buf[sizeof(size)*3];

	filename = concat_path_file(path, name);
	len = open_read_close(filename, buf, sizeof(buf) - 1);
#if 0
bb_error_msg("open_read_close('%s'):'%.*s'",
    filename,
    len < 0 ? 5 : (int)len,
    len < 0 ? "ERROR" : buf
);
#endif
	free(filename);
	if (len > 0 && len < sizeof(buf) - 1) {
		buf[len] = '\0';
		size = bb_strtoull(buf, NULL, 10);
	}
	return size;
}

/* For reading one-line /sys files. Truncates at \n. NULL on error */
static char *read_str(const char *path, const char *name)
{
	char *filename;
	char *res;

	filename = concat_path_file(path, name);
	res = xmalloc_open_read_close(filename, NULL);
//bb_error_msg("open_read_close('%s'):'%s'", filename, res);
	free(filename);

	if (res)
		strchrnul(res, '\n')[0] = '\0';
	return res;
}

/* To see what util-linux does:
 * strace -eopen,openat,getdents,getdents64,read,fcntl -s99 -v lsblk 2>&1 | less
 * What I observed:
 * open("/sys/block")+getdents()
 * For each found entry DEV:
 *    open("/sys/block/DEV/hidden")+read, if 1, skip (probably)
 *    MAJMIN=open("/sys/block/DEV/dev")+read
 *    open("/sys/dev/block/MAJMIN/size")+read
 *    open("/sys/dev/block/MAJMIN")+getdents()
 *    For each found entry PART:
 *       open("/sys/block/PART/hidden") - if found then what?
 *       else
 *       MAJMIN1=open("/sys/block/DEV/PART/dev")+read
 *       recurse into handling MAJMIN1 for this partition
 */
static void process__sys_block_NAME(const char *path, const char *devname);

/* Note: consumes malloc'ed majmin */
static void process__sys_dev_block_MAJMIN(const char *path, char *majmin, const char *devname)
{
	DIR *dir;
	struct dirent *entry;
	struct blockdev_info *info;

	info = get_or_create_info(devname);
	if (info->type) /* we already saw this one! */
		return;

	info->size = read_ull(path, "size");
//see util-linux's lsblk.c::static char *get_type()
	info->type = (
		(long long)read_ull(path, "partition") >= 0 ? "part" /* PATH/partition exists and has a number */
		: is_prefixed_with(devname, "loop") ? "loop"
		: "disk"
	);
//TODO: ^^^^ read from /sys/XYZ/uevent, DEVTYPE=xyz line? Not what util-linux does, though...
	info->majmin = majmin;
	//info->rm = ...;
	//info->ro = ...;
	//info->mountpoint = get_mountpoint(majmin);

	/* Scan for partititons */
	dir = xopendir(path);
	while ((entry = readdir(dir)) != NULL) {
		if (is_prefixed_with(entry->d_name, devname)) {
			char *part = xasprintf("/sys/block/%s/%s", devname, entry->d_name);
			process__sys_block_NAME(part, entry->d_name);
			free(part);
		}
	}
	closedir(dir);
}
static void process__sys_block_NAME(const char *path, const char *devname)
{
	char *majmin = read_str(path, "dev");
//bb_error_msg("%s/dev:'%s'", path, majmin);
	if (majmin /*&& majmin[0]*/) {
		char *sys_dev_block_MAJMIN = concat_path_file("/sys/dev/block", majmin);
		process__sys_dev_block_MAJMIN(sys_dev_block_MAJMIN, majmin, devname);
		/* ^^^ consumes malloc'ed majmin */
		free(sys_dev_block_MAJMIN);
	}
	/* WRONG: free(majmin); */
//	return !majmin; /* 1 if no PATH/dev was seen */
}

static int compare_devices(const void *a, const void *b)
{
	const struct blockdev_info *da = (const struct blockdev_info *)a;
	const struct blockdev_info *db = (const struct blockdev_info *)b;
	return strcmp(da->name, db->name);
}

int lsblk_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int lsblk_main(int argc UNUSED_PARAM, char **argv)
{
	unsigned i;

	INIT_G();

	/* support/ignore -a ("all") */
	getopt32(argv, "a");
	argv += optind;

	G.mountinfo = xmalloc_xopen_read_close("/proc/self/mountinfo", NULL);

	/* If specific devices are requested, process them */
	if (*argv) {
		while (*argv) {
			char *majmin;
			char *sys_dev_block_MAJMIN;
			char *target;
			char *name;

			/* Try:
			 * cp -a /dev/DISK /tmp/bogusname; lsblk /tmp/bogusname
			 * ^^^^ should still show "DISK" as the name of blockdev, and show its partitions if any
			 */
			majmin = get_majmin_from_stat(*argv++);
//bb_error_msg("get_majmin_from_stat('%s'):'%s'", argv[-1], majmin);
			if (!majmin)
				continue;

			sys_dev_block_MAJMIN = concat_path_file("/sys/dev/block", majmin);
			/* util-linux 2.41.1 gets the "real name" from the symlink's last component */
			target = xmalloc_readlink(sys_dev_block_MAJMIN);
//bb_error_msg("target:'%s'", target);
			if (target) {
				name = strrchr(target, '/');
				if (name && *++name) {
					char *sys_block_NAME;
// Maybe there's a reason why util-linux tries /sys/block/NAME first.
// In which case uncomment this, and explain.
//					int err;
//
//					sys_block_NAME = concat_path_file("/sys/block", name);
//					err = process__sys_block_NAME(sys_block_NAME, name);
//					free(sys_block_NAME);
//					/* "/sys/block/NAME/dev" wasn't found? (Happens for "lsblk /dev/PARTITION") */
//					if (err) {
						/* util-linux seems to test for existence of /sys/dev/block/MAJMIN/partition,
						 * if that exists, it *guesses* parent disk name (!!!).
						 * We just try /sys/class/block/NAME, which exists for partitions too.
						 */
						sys_block_NAME = concat_path_file("/sys/class/block", name);
						process__sys_block_NAME(sys_block_NAME, name);
						free(sys_block_NAME);
//					}
				}
				free(target);
			}
			free(majmin);
			free(sys_dev_block_MAJMIN);
		}
	} else {
		DIR *dir;
		struct dirent *entry;

		/* Read all devices from /sys/block */
		dir = xopendir("/sys/block");
		while ((entry = readdir(dir)) != NULL) {
			char *sys_block_NAME;
			if (DOT_OR_DOTDOT(entry->d_name))
				continue;
			sys_block_NAME = concat_path_file("/sys/block", entry->d_name);
			process__sys_block_NAME(sys_block_NAME, entry->d_name);
			free(sys_block_NAME);
		}
		closedir(dir);
	}

	if (G.count == 0)
		return 32; /* try "lsblk /dev/null DOES_NOT_EXIST" */

	/* Sort devices by name */
	qsort(G.list, G.count, sizeof(G.list[0]), compare_devices);

	/* Print header */
	printf("%-15s MAJ:MIN  SIZE TYPE MOUNTPOINTS\n", "NAME");
	//util-linux 2.41.1 default set of fields:
	//"NAME        MAJ:MIN RM   SIZE RO TYPE MOUNTPOINTS"

	/* Print devices */
	for (i = 0; i < G.count; i++) {
		char buf6[6];
		char *mnt, *e;

		mnt = (get_mountpoints(G.list[i].majmin) ? : (char*)"");
		e = strchrnul(mnt[0] ? mnt + 1 : "", ' ');
		smart_ulltoa5(G.list[i].size * 512, buf6, " KMGTPEZY");
		printf("%-15s %-7s %.5s %4s%.*s\n",
			G.list[i].name,
			G.list[i].majmin,
			buf6,
			G.list[i].type,
			(int)(e - mnt), mnt
		);
		while (*mnt == ' ') {
//util-linux prints multiple mountpoints on separate lines:
//DEVNAME     259:3    0 475.4G  0 part /MNT1
//                                      /MNT2
			mnt = skip_non_whitespace(mnt + 1);
			if (!mnt[0]) break;
			e = strchrnul(mnt + 1, ' ');
			printf("%34s%.*s\n", "", (int)(e - mnt), mnt);
		}
	}

	fflush_stdout_and_exit(G.exitcode);
}
