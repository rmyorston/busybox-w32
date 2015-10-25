/* vi: set sw=4 ts=4: */
/*
 * depmod - generate modules.dep
 * Copyright (c) 2008 Bernhard Reutner-Fischer
 * Copyrihgt (c) 2008-2015 Timo Teras <timo.teras@iki.fi>
 * Copyright (c) 2008 Vladimir Dronnikov
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
//config:config DEPMOD
//config:	bool "depmod (27 kb)"
//config:	default y
//config:	help
//config:	depmod generates modules.dep (and potentially modules.alias
//config:	and modules.symbols) that contain dependency information
//config:	for modprobe.

//applet:IF_DEPMOD(IF_NOT_MODPROBE_SMALL(APPLET(depmod, BB_DIR_SBIN, BB_SUID_DROP)))

//kbuild:ifneq ($(CONFIG_MODPROBE_SMALL),y)
//kbuild:lib-$(CONFIG_DEPMOD) += depmod.o modutils.o
//kbuild:endif

#include "libbb.h"
#include "modutils.h"
#include <sys/utsname.h> /* uname() */

#define INDEX_MINCHAR	32
#define INDEX_MAXCHAR	128

typedef struct index_node {
	char *prefix;
	llist_t *values;
	struct index_node *children[INDEX_MAXCHAR-INDEX_MINCHAR];
} index_node;

struct globals {
	module_db db;
	index_node *root_node;
} FIX_ALIASING;
#define G (*ptr_to_globals)
#define INIT_G() do { \
	SET_PTR_TO_GLOBALS(xzalloc(sizeof(G))); \
} while (0)

/*
 * Theory of operation:
 * - iterate over all modules and record their full path
 * - iterate over all modules looking for "depends=" entries
 *   for each depends, look through our list of full paths and emit if found
 */

static int FAST_FUNC parse_module(struct recursive_state *state,
		const char *fname,
		struct stat *sb UNUSED_PARAM)
{
	module_db *modules = state->userData;
	char *image, *ptr;
	module_entry *e;

	/* Arbitrary. Was sb->st_size, but that breaks .gz etc */
	size_t len = (64*1024*1024 - 4096);

	if (strrstr(fname, ".ko") == NULL)
		return TRUE;

	image = xmalloc_open_zipped_read_close(fname, &len);

	e = moddb_get_or_create(modules, bb_get_last_path_component_nostrip(fname));
	e->name = xstrdup(fname + 2); /* skip "./" */

	for (ptr = image; ptr < image + len - 10; ptr++) {
		if (is_prefixed_with(ptr, "depends=")) {
			ptr += 8;
			string_to_llist(replace_underscores(ptr), &e->deps, ",");
		} else if (ENABLE_FEATURE_MODUTILS_ALIAS
		 && is_prefixed_with(ptr, "alias=")
		) {
			llist_add_to(&e->aliases, replace_underscores(xstrdup(ptr + 6)));
		} else if (ENABLE_FEATURE_MODUTILS_SYMBOLS
		 && is_prefixed_with(ptr, "__ksymtab_")
		) {
			ptr += 10;
			if (is_prefixed_with(ptr, "gpl")
			 || strcmp(ptr, "strings") == 0
			) {
				continue;
			}
			llist_add_to(&e->symbols, xasprintf("symbol:%s", ptr));
		} else
			continue;
		ptr += strlen(ptr);
	}
	free(image);

	return TRUE;
}

static void order_dep_list(module_db *modules, module_entry *start, llist_t *add)
{
	module_entry *m;
	llist_t *n;

	for (n = add; n != NULL; n = n->link) {
		m = moddb_get(modules, n->data);
		if (m == NULL)
			continue;

		/* unlink current entry */
		m->dnext->dprev = m->dprev;
		m->dprev->dnext = m->dnext;

		/* and add it to tail */
		m->dnext = start;
		m->dprev = start->dprev;
		start->dprev->dnext = m;
		start->dprev = m;

		/* recurse */
		order_dep_list(modules, start, m->deps);
	}
}

//usage:#if !ENABLE_MODPROBE_SMALL
//usage:#define depmod_trivial_usage "[-n] [-b BASE] [VERSION] [MODFILES]..."
//usage:#define depmod_full_usage "\n\n"
//usage:       "Generate modules.dep, alias, and symbols files"
//usage:     "\n"
//usage:     "\n	-b BASE	Use BASE/lib/modules/VERSION"
//usage:     "\n	-n	Dry run: print files to stdout"
//usage:#endif

/* Upstream usage:
 * [-aAenv] [-C FILE or DIR] [-b BASE] [-F System.map] [VERSION] [MODFILES]...
 *	-a --all
 *		Probe all modules. Default if no MODFILES.
 *	-A --quick
 *		Check modules.dep's mtime against module files' mtimes.
 *	-b --basedir BASE
 *		Use $BASE/lib/modules/VERSION
 *	-C --config FILE or DIR
 *		Path to /etc/depmod.conf or /etc/depmod.d/
 *	-e --errsyms
 *		When combined with the -F option, this reports any symbols
 *		which are not supplied by other modules or kernel.
 *	-F --filesyms System.map
 *	-n --dry-run
 *		Print modules.dep etc to standard output
 *	-v --verbose
 *		Print to stdout all the symbols each module depends on
 *		and the module's file name which provides that symbol.
 *	-r	No-op
 *	-u	No-op
 *	-q	No-op
 *
 * So far we only support: [-n] [-b BASE] [VERSION] [MODFILES]...
 * Accepted but ignored:
 * -aAe
 * -F System.map
 * -C FILE/DIR
 *
 * Not accepted: -v
 */
enum {
	//OPT_a = (1 << 0), /* All modules, ignore mods in argv */
	//OPT_A = (1 << 1), /* Only emit .ko that are newer than modules.dep file */
	OPT_b = (1 << 2), /* base directory when modules are in staging area */
	//OPT_e = (1 << 3), /* with -F, print unresolved symbols */
	//OPT_F = (1 << 4), /* System.map that contains the symbols */
	OPT_n = (1 << 5), /* dry-run, print to stdout only */
	OPT_r = (1 << 6), /* Compat dummy. Linux Makefile uses it */
	OPT_u = (1 << 7), /* -u,--unresolved-error: ignored */
	OPT_q = (1 << 8), /* -q,--quiet: ignored */
	OPT_C = (1 << 9), /* -C,--config etc_modules_conf: ignored */
};

/* Support for the mod binary index generation */

static void index_init(const char *filename)
{
	if (ENABLE_FEATURE_MODUTILS_BIN) {
		index_node *n;

		n = xzalloc(sizeof(index_node));
		n->prefix = xstrdup("");
		G.root_node = n;
	}

	if (filename && !(option_mask32 & OPT_n)) {
		if (freopen(filename, "w", stdout) == NULL)
			bb_perror_msg_and_die("can't open '%s'", filename);
	}
}

static void index_add(const char *key, char *value, const char *prefix)
{
	if (prefix && *prefix)
		printf("%s%s %s\n", prefix, key, value);
	else if (prefix)
		printf("%s\n", value);

	if (ENABLE_FEATURE_MODUTILS_BIN) {
		index_node *cur = G.root_node, *n;
		unsigned i = 0, j, ch;

		while (1) {
			/* Ensure node->prefix is a prefix of &str[i].
			 * If it is not already, then we must split node. */
			for (j = 0; cur->prefix[j]; j++) {
				ch = cur->prefix[j];
				if (ch != key[i+j]) {
					/* New child is copy of node with prefix[j+1..N] */
					n = xzalloc(sizeof(index_node));
					n->prefix = xstrdup(&cur->prefix[j+1]);
					n->values = cur->values;
					memcpy(n->children, cur->children, sizeof(n->children));

					/* Parent has prefix[0..j], child at prefix[j] */
					cur->prefix[j] = '\0';
					cur->values = NULL;
					memset(cur->children, 0, sizeof(cur->children));
					cur->children[ch-INDEX_MINCHAR] = n;
					break;
				}
			}
			i += j;

			ch = key[i];
			if (ch == 0)
				break;

			if (ch < INDEX_MINCHAR || ch >= INDEX_MAXCHAR)
				bb_simple_error_msg_and_die("bad module name");

			ch -= INDEX_MINCHAR;
			if (!cur->children[ch]) {
				n = xzalloc(sizeof(index_node));
				cur->children[ch] = n;
				n->prefix = xstrdup(&key[i+1]);
				cur = n;
				break;
			}

			/* Descend into child node and continue */
			cur = cur->children[ch];
			i++;
		}

		llist_add_to(&cur->values, value);
	}
}

static uint32_t index_write_node(FILE *out, index_node *n, void (*freeit)(void *data))
{
	uint32_t child_offs[INDEX_MAXCHAR-INDEX_MINCHAR];
	uint32_t offset;
	uint8_t first = 255, last = 0;
	unsigned i;

	for (i = 0; i < INDEX_MAXCHAR-INDEX_MINCHAR; i++) {
		child_offs[i] = 0;
		if (!n->children[i])
			continue;
		child_offs[i] = index_write_node(out, n->children[i], freeit);
		if (first > INDEX_MAXCHAR)
			first = i;
		last = i;
	}

	offset = ftell(out);

	if (n->prefix[0]) {
		fputs(n->prefix, out);
		fputc('\0', out);
		offset |= INDEX_NODE_PREFIX;
	}

	if (first < INDEX_MAXCHAR) {
		fputc(first + INDEX_MINCHAR, out);
		fputc(last + INDEX_MINCHAR, out);
		fwrite(child_offs + first, sizeof(uint32_t), last - first + 1, out);
		offset |= INDEX_NODE_CHILDS;
	}

	if (n->values) {
		const llist_t *v;
		unsigned int cnt;
		uint32_t u;

		n->values = llist_rev(n->values);
		for (v = n->values, cnt = 0; v != NULL; v = v->link, cnt++);
		u = htonl(cnt);
		fwrite(&u, sizeof(u), 1, out);
		for (v = n->values, cnt = 0; v != NULL; v = v->link, cnt++) {
			u = htonl(cnt);
			fwrite(&u, sizeof(u), 1, out);
			fputs(v->data, out);
			fputc('\0', out);
		}
		offset |= INDEX_NODE_VALUES;
	}

	llist_free(n->values, freeit);
	free(n->prefix);
	free(n);

	return htonl(offset);
}

static void index_dump(const char *filename, int deps_file)
{
	if (ENABLE_FEATURE_MODUTILS_BIN) {
		FILE *out;
		uint32_t header[3] = {
			htonl(INDEX_MAGIC),
			htonl(INDEX_VERSION),
		};

		if (option_mask32 & OPT_n)
			filename = "/dev/null";
		else
			filename = xasprintf("tmp.%s.bin", filename);

		out = xfopen_for_write(filename);
		fwrite(header, sizeof(uint32_t), 3, out);
		header[2] = index_write_node(out, G.root_node, deps_file ? free : 0);
		rewind(out);
		G.root_node = NULL;
		fwrite(header, sizeof(uint32_t), 3, out);
		if (fclose(out)) {
			remove(filename);
			bb_simple_error_msg_and_die(bb_msg_write_error);
		}
		/* .bin files are mmap'ed; not renaming it may crash
		 * long standing daemon using libkmod */
		rename_or_warn(filename, filename + 4);
	}
}

int depmod_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int depmod_main(int argc UNUSED_PARAM, char **argv)
{
	module_db modules;
	module_entry *m, *dep;
	const char *moddir_base = "/";
	char *moddir, *version;
	struct utsname uts;
	unsigned i;
	int tmp;

	INIT_G();

	getopt32(argv, "aAb:eF:nruqC:", &moddir_base, NULL, NULL);
	argv += optind;

	/* goto modules location */
	xchdir(moddir_base);

	/* If a version is provided, then that kernel version's module directory
	 * is used, rather than the current kernel version (as returned by
	 * "uname -r").  */
	if (*argv && sscanf(*argv, "%u.%u.%u", &tmp, &tmp, &tmp) == 3) {
		version = *argv++;
	} else {
		uname(&uts);
		version = uts.release;
	}
	moddir = concat_path_file(&CONFIG_DEFAULT_MODULES_DIR[1], version);
	xchdir(moddir);
	if (ENABLE_FEATURE_CLEAN_UP)
		free(moddir);

	/* Scan modules */
	memset(&modules, 0, sizeof(modules));
	if (*argv) {
		do {
			recursive_action(*argv, 0 /* no ACTION_RECURSE! */,
				parse_module, NULL, &modules);
		} while (*++argv);
	} else {
		recursive_action(".", ACTION_RECURSE,
				parse_module, NULL, &modules);
	}

	/* Generate dependency and alias files */
	index_init(CONFIG_DEFAULT_DEPMOD_FILE);
	moddb_foreach_module(&modules, m, i) {
		char *buf = xasprintf("%s:", m->name);
		order_dep_list(&modules, m, m->deps);

		while (m->dnext != m) {
			dep = m->dnext;
			buf = gather_options_str(buf, dep->name);
			/* unlink current entry */
			dep->dnext->dprev = dep->dprev;
			dep->dprev->dnext = dep->dnext;
			dep->dnext = dep->dprev = dep;
		}
		index_add(m->modname, buf, "");
	}
	index_dump(CONFIG_DEFAULT_DEPMOD_FILE, 1);

	if (ENABLE_FEATURE_MODUTILS_ALIAS) {
		index_init("modules.alias");
		moddb_foreach_module(&modules, m, i) {
			while (m->aliases) {
				/*
				 * Last word used to be a basename
				 * (filename with path and .ko.* stripped)
				 * at the time of module-init-tools 3.4.
				 * kmod v.12 uses module name, i.e., s/-/_/g.
				 */
				index_add((char*)llist_pop(&m->aliases), m->modname, "alias ");
			}
		}
		index_dump("modules.alias", 0);
	}
	if (ENABLE_FEATURE_MODUTILS_SYMBOLS) {
		index_init("modules.symbols");
		moddb_foreach_module(&modules, m, i) {
			while (m->symbols) {
				index_add((char*)llist_pop(&m->symbols), m->modname, "alias ");
			}
		}
		index_dump("modules.symbols", 0);
	}
	if (ENABLE_FEATURE_MODUTILS_BIN) {
		char line[PATH_MAX], modname[MODULE_NAME_LEN];
		FILE *in;

		index_init(NULL);
		in = xfopen_for_read("modules.builtin");
		while (fgets(line, sizeof(line), in) != NULL) {
			filename2modname(line, modname);
			index_add(modname, (char *) "", 0);
		}
		fclose(in);
		index_dump("modules.builtin", 0);
	}

	if (ENABLE_FEATURE_CLEAN_UP)
		moddb_free(&modules);

	return EXIT_SUCCESS;
}
