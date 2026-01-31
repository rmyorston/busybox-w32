/* vi: set sw=4 ts=4: */
/*
 * Mini uuidgen implementation for busybox
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
//config:config UUIDGEN
//config:	bool "uuidgen (1.1 kb)"
//config:	default y
//config:	help
//config:	Generate a UUID (Universally Unique Identifier) in RFC 4122 format.

//applet:IF_UUIDGEN(APPLET_NOFORK(uuidgen, uuidgen, BB_DIR_USR_BIN, BB_SUID_DROP, uuidgen))

//kbuild:lib-$(CONFIG_UUIDGEN) += uuidgen.o

//usage:#define uuidgen_trivial_usage
//usage:       ""
//usage:#define uuidgen_full_usage "\n\n"
//usage:       "Generate a random UUID"

/* util-linux 2.41.1:
 * -r, --random          generate random-based uuid
 * -t, --time            generate time-based uuid
 * -n, --namespace <ns>  generate hash-based uuid in this namespace
 *                       available namespaces: @dns @url @oid @x500
 * -N, --name <name>     generate hash-based uuid from this name
 * -m, --md5             generate md5 hash
 * -C, --count <num>     generate more uuids in loop
 * -s, --sha1            generate sha1 hash
 * -6, --time-v6         generate time-based v6 uuid
 * -7, --time-v7         generate time-based v7 uuid
 * -x, --hex             interpret name as hex string
 * manpage:
 * "By default uuidgen will generate a random-based UUID if a high-quality random number generator is present.
 * Otherwise, it will choose a time-based UUID."
 */
#include "libbb.h"
#include "volume_id/volume_id_internal.h"

/* This is a NOFORK applet. Be very careful! */

int uuidgen_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int uuidgen_main(int argc UNUSED_PARAM, char **argv UNUSED_PARAM)
{
	struct volume_id id;
	uint8_t uuid[16];

	/* support/ignore -r (--random) */
	getopt32(argv, "^" "r" "\0" "=0"/* no args!*/);

	generate_uuid(uuid);
	volume_id_set_uuid(&id, uuid, UUID_DCE);
	puts(id.uuid);

	return 0;
}
