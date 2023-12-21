/* vi: set sw=4 ts=4: */
/*
 * Copyright (C) 2024 Ur4t
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
//config:config B2SUM
//config:	bool "b2sum (UNKNOWN_SIZE kb)"
//config:	default y
//config:	help
//config:	Compute and check BLAKE2b message digest
//config:
//config:config B3SUM
//config:	bool "b3sum (UNKNOWN_SIZE kb)"
//config:	default y
//config:	help
//config:	Compute and check BLAKE3 message digest
//config:
//config:config XXHSUM
//config:	bool "xxhsum (UNKNOWN_SIZE kb)"
//config:	default y
//config:	help
//config:	Compute and check xxHash message digest
//config:
//config:config XXH32SUM
//config:	bool "xxh32sum (UNKNOWN_SIZE kb)"
//config:	default y
//config:	help
//config:	Compute and check xxHash 32 bit message digest
//config:
//config:config XXH64SUM
//config:	bool "xxh64sum (UNKNOWN_SIZE kb)"
//config:	default y
//config:	help
//config:	Compute and check xxHash 64 bit message digest
//config:
//config:config XXH128SUM
//config:	bool "xxh128sum (UNKNOWN_SIZE kb)"
//config:	default y
//config:	help
//config:	Compute and check xxHash 128 bit message digest
//config:
//config:comment "Common options for b2sum, b3sum, xxhsum, xxh32sum, xxh64sum, xxh128sum"
//config:	depends on B2SUM || B3SUM || XXHSUM || XXH32SUM || XXH64SUM || XXH128SUM
//config:
//config:config FEATURE_BLAKE_XXH_SUM_CHECK
//config:	bool "Enable -c, -s and -w options"
//config:	default y
//config:	depends on B2SUM || B3SUM || XXHSUM || XXH32SUM || XXH64SUM || XXH128SUM
//config:	help
//config:	Enabling the -c options allows files to be checked
//config:	against pre-calculated hash values.
//config:	-s and -w are useful options when verifying checksums.

//applet:IF_B2SUM    (APPLET_NOEXEC(b2sum,     blake_xxh_sum, BB_DIR_USR_BIN, BB_SUID_DROP, b2sum))
//applet:IF_B3SUM    (APPLET_NOEXEC(b3sum,     blake_xxh_sum, BB_DIR_USR_BIN, BB_SUID_DROP, b3sum))
//applet:IF_XXHSUM   (APPLET_NOEXEC(xxhsum,    blake_xxh_sum, BB_DIR_USR_BIN, BB_SUID_DROP, xxhsum))
//applet:IF_XXH32SUM (APPLET_NOEXEC(xxh32sum,  blake_xxh_sum, BB_DIR_USR_BIN, BB_SUID_DROP, xxh32sum))
//applet:IF_XXH64SUM (APPLET_NOEXEC(xxh64sum,  blake_xxh_sum, BB_DIR_USR_BIN, BB_SUID_DROP, xxh64sum))
//applet:IF_XXH128SUM(APPLET_NOEXEC(xxh128sum, blake_xxh_sum, BB_DIR_USR_BIN, BB_SUID_DROP, xxh128sum))

//kbuild:lib-$(CONFIG_XXHSUM)    += blake_xxh_sum.o
//kbuild:lib-$(CONFIG_XXH32SUM)  += blake_xxh_sum.o
//kbuild:lib-$(CONFIG_XXH64SUM)  += blake_xxh_sum.o
//kbuild:lib-$(CONFIG_XXH128SUM) += blake_xxh_sum.o

//usage:#define b2sum_trivial_usage
//usage:	IF_FEATURE_BLAKE_XXH_SUM_CHECK("[-c[sw]] ")"[-[al] BITS] [FILE]..."
//usage:#define b2sum_full_usage "\n\n"
//usage:       "Print" IF_FEATURE_BLAKE_XXH_SUM_CHECK(" or check") " xxHash checksums"
//usage:	IF_FEATURE_BLAKE_XXH_SUM_CHECK( "\n"
//usage:     "\n	-c	Check sums against list in FILEs"
//usage:     "\n	-s	Don't output anything, status code shows success"
//usage:     "\n	-w	Warn about improperly formatted checksum lines"
//usage:	)
//usage:     "\n	-a 	Alias of -l"
//usage:     "\n	-l BITS	Digest length in bits, multiple of 8 with maximum 512 (default)"
//usage:
//usage:#define b3sum_trivial_usage
//usage:	IF_FEATURE_BLAKE_XXH_SUM_CHECK("[-c[sw]] ")"[-a BITS] [-l BYTES] [FILE]..."
//usage:#define b3sum_full_usage "\n\n"
//usage:       "Print" IF_FEATURE_BLAKE_XXH_SUM_CHECK(" or check") " xxHash checksums"
//usage:	IF_FEATURE_BLAKE_XXH_SUM_CHECK( "\n"
//usage:     "\n	-c	Check sums against list in FILEs"
//usage:     "\n	-s	Don't output anything, status code shows success"
//usage:     "\n	-w	Warn about improperly formatted checksum lines"
//usage:	)
//usage:     "\n	-a BITS	Digest length in bits, multiple of 8"
//usage:     "\n	-l BYTES	Digest length in bytes, default to 32"
//usage:
//usage:#define xxhsum_trivial_usage
//usage:	IF_FEATURE_BLAKE_XXH_SUM_CHECK("[-c[sw]] ")"[-[aH] [ALGO|BITS]] [FILE]..."
//usage:#define xxhsum_full_usage "\n\n"
//usage:       "Print" IF_FEATURE_BLAKE_XXH_SUM_CHECK(" or check") " xxHash checksums"
//usage:	IF_FEATURE_BLAKE_XXH_SUM_CHECK( "\n"
//usage:     "\n	-c	Check sums against list in FILEs"
//usage:     "\n	-s	Don't output anything, status code shows success"
//usage:     "\n	-w	Warn about improperly formatted checksum lines"
//usage:	)
//usage:     "\n	-a 	Alias of -H"
//usage:     "\n	-H 	0, 1 (default), 2, 3 or 32, 64 (default), 128"
//usage:
//usage:#define xxh32sum_trivial_usage
//usage:	IF_FEATURE_BLAKE_XXH_SUM_CHECK("[-c[sw]] ")"[FILE]..."
//usage:#define xxh32sum_full_usage "\n\n"
//usage:       "Print" IF_FEATURE_BLAKE_XXH_SUM_CHECK(" or check") " xxHash 32 bit checksums"
//usage:	IF_FEATURE_BLAKE_XXH_SUM_CHECK( "\n"
//usage:     "\n	-c	Check sums against list in FILEs"
//usage:     "\n	-s	Don't output anything, status code shows success"
//usage:     "\n	-w	Warn about improperly formatted checksum lines"
//usage:	)
//usage:
//usage:#define xxh64sum_trivial_usage
//usage:	IF_FEATURE_BLAKE_XXH_SUM_CHECK("[-c[sw]] ")"[FILE]..."
//usage:#define xxh64sum_full_usage "\n\n"
//usage:       "Print" IF_FEATURE_BLAKE_XXH_SUM_CHECK(" or check") " xxHash 64 bit checksums"
//usage:	IF_FEATURE_BLAKE_XXH_SUM_CHECK( "\n"
//usage:     "\n	-c	Check sums against list in FILEs"
//usage:     "\n	-s	Don't output anything, status code shows success"
//usage:     "\n	-w	Warn about improperly formatted checksum lines"
//usage:	)
//usage:
//usage:#define xxh128sum_trivial_usage
//usage:	IF_FEATURE_BLAKE_XXH_SUM_CHECK("[-c[sw]] ")"[FILE]..."
//usage:#define xxh128sum_full_usage "\n\n"
//usage:       "Print" IF_FEATURE_BLAKE_XXH_SUM_CHECK(" or check") " xxHash 128 bit checksums"
//usage:	IF_FEATURE_BLAKE_XXH_SUM_CHECK( "\n"
//usage:     "\n	-c	Check sums against list in FILEs"
//usage:     "\n	-s	Don't output anything, status code shows success"
//usage:     "\n	-w	Warn about improperly formatted checksum lines"
//usage:	)

//FIXME: b2sum, b3sum and xxhsum have no -s/-q option, but two long opts:
// --quiet   don't print OK for each successfully verified file
// --status  don't output anything, status code shows success

//NOTE: b2sum, b3sum and xxhsum have no -a option, but -H/-l

#include "libbb.h"

#if ENABLE_XXH32SUM || ENABLE_XXH64SUM || ENABLE_XXH128SUM || ENABLE_XXHSUM
#undef XXH_NAMESPACE
#undef XXH_PRIVATE_API
#undef XXH_INLINE_ALL
#define XXH_NAMESPACE HASH_
#define XXH_PRIVATE_API
#define XXH_INLINE_ALL
#include "hashes/xxhash.h"
#endif

#if ENABLE_B3SUM
#define BLAKE3_NO_AVX512
#define BLAKE3_NO_AVX2
#define BLAKE3_NO_SSE41
#define BLAKE3_NO_SSE2
#include "hashes/blake3/blake3.c"
#include "hashes/blake3/blake3_dispatch.c"
#include "hashes/blake3/blake3_portable.c"
#endif

#if ENABLE_B2SUM
#if !ENABLE_B3SUM
static ALWAYS_INLINE void store32(void *dst, uint32_t w)
{
#if defined(BB_LITTLE_ENDIAN)
	memcpy(dst, &w, sizeof w);
#else
	uint8_t *p = (uint8_t *)dst;
	p[0] = (uint8_t)(w >> 0);
	p[1] = (uint8_t)(w >> 8);
	p[2] = (uint8_t)(w >> 16);
	p[3] = (uint8_t)(w >> 24);
#endif
}
#endif
#include "hashes/blake2b.c"
#endif

#if ENABLE_XXH32SUM || ENABLE_XXHSUM
static void FAST_FUNC xxh32_end(void *ctx, void *resbuf)
{
	XXH32_canonicalFromHash(resbuf, XXH32_digest(ctx));
}
#endif

#if ENABLE_XXH64SUM || ENABLE_XXHSUM
static void FAST_FUNC xxh64_end(void *ctx, void *resbuf)
{
	XXH64_canonicalFromHash(resbuf, XXH64_digest(ctx));
}
#endif

#if ENABLE_XXH128SUM || ENABLE_XXHSUM
static void FAST_FUNC xxh128_end(void *ctx, void *resbuf)
{
	XXH128_canonicalFromHash(resbuf, XXH3_128bits_digest(ctx));
}
#endif

#if ENABLE_XXHSUM
static void FAST_FUNC xxh3_end(void *ctx, void *resbuf)
{
	XXH64_canonicalFromHash(resbuf, XXH3_64bits_digest(ctx));
}
#endif

/* This is a NOEXEC applet. Be very careful! */

enum {
	/* 4th letter of applet_name is... */
	HASH_XXH = 's', /* "xxh>s<um" */
	HASH_XXH32 = '3',
	HASH_XXH64 = '6',
	HASH_XXH128 = '1',
	/* 2nd letter of applet_name is... */
	HASH_BLAKE2 = '2',
	HASH_BLAKE3 = '3',
};

#define FLAG_SILENT 1
#define FLAG_CHECK  2
#define FLAG_WARN   4

#define BUFSZ (CONFIG_FEATURE_COPYBUF_KB < 4 ? 4096 : CONFIG_FEATURE_COPYBUF_KB * 1024)

static uint8_t *hash_file(unsigned char *in_buf, const char *filename, unsigned digest_bits)
{
	int src_fd, hash_len, count;
	union {
		XXH32_state_t xxh32;
		XXH64_state_t xxh64;
		XXH3_state_t xxh3;
		blake2b_state blake2;
		blake3_hasher blake3;
	} context;
	uint8_t *out_buf;
	uint8_t *hash_value;
	void FAST_FUNC (*update)(void *, const void *, size_t);
	void FAST_FUNC (*final)(void *, void *, size_t);

	src_fd = open_or_warn_stdin(filename);
	if (src_fd < 0) {
		return NULL;
	}

	switch (applet_name[3]) {
#if ENABLE_XXH32SUM
	case HASH_XXH32:
		hash_len = 4;
		XXH32_reset(&context.xxh32, 0);
		update = (void *)XXH32_update;
		final = (void *)xxh32_end;
		break;
#endif
#if ENABLE_XXH64SUM
	case HASH_XXH64:
		hash_len = 8;
		XXH64_reset(&context.xxh64, 0);
		update = (void *)XXH64_update;
		final = (void *)xxh64_end;
		break;
#endif
#if ENABLE_XXH128SUM
	case HASH_XXH128:
		hash_len = 16;
		XXH3_128bits_reset(&context.xxh3);
		update = (void *)XXH3_128bits_update;
		final = (void *)xxh128_end;
		break;
#endif
#if ENABLE_XXHSUM
	case HASH_XXH:
		switch (digest_bits) {
		case 0:
			hash_len = 8;
			XXH64_reset(&context.xxh64, 0);
			update = (void *)XXH64_update;
			final = (void *)xxh64_end;
			break;
		case 32:
			hash_len = 4;
			XXH32_reset(&context.xxh32, 0);
			update = (void *)XXH32_update;
			final = (void *)xxh32_end;
			break;
		case 64:
			hash_len = 8;
			XXH3_64bits_reset(&context.xxh3);
			update = (void *)XXH3_64bits_update;
			final = (void *)xxh3_end;
			break;
		case 128:
			hash_len = 16;
			XXH3_128bits_reset(&context.xxh3);
			update = (void *)XXH3_128bits_update;
			final = (void *)xxh128_end;
			break;
		default:
			bb_error_msg_and_die("bad digest length: %u", digest_bits);
		}
		break;
#endif
	default:
		switch (applet_name[1]) {
#if ENABLE_B2SUM
		case HASH_BLAKE2:
			hash_len = digest_bits == 0 ? BLAKE2B_OUTBYTES : digest_bits / 8;
			blake2b_init(&context.blake2, hash_len);
			update = (void *)blake2b_update;
			final = (void *)blake2b_final;
			break;
#endif
#if ENABLE_B3SUM
		case HASH_BLAKE3:
			hash_len = digest_bits == 0 ? 32 : digest_bits / 8;
			blake3_hasher_init(&context.blake3);
			update = (void *)blake3_hasher_update;
			final = (void *)blake3_hasher_finalize;
			break;
#endif
		default:
			xfunc_die(); /* can't reach this */
		}
	}

	hash_value = xzalloc((hash_len * 2) + 1);

	while ((count = safe_read(src_fd, in_buf, BUFSZ)) > 0) {
		update(&context, in_buf, count);
	}
	if (count < 0)
		bb_perror_msg("can't read '%s'", filename);
	else /* count == 0 */ {
		out_buf = xmalloc(hash_len);
		final(&context, out_buf, hash_len);
		bin2hex((char *)hash_value, (char *)out_buf, hash_len);
		free(out_buf);
	}

	if (src_fd != STDIN_FILENO) {
		close(src_fd);
	}

	return hash_value;
}

int blake_xxh_sum_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int blake_xxh_sum_main(int argc UNUSED_PARAM, char **argv)
{
	unsigned char *in_buf;
	int return_value = EXIT_SUCCESS;
	unsigned flags;
	unsigned digest_bits = 0; /* 0 bits means default */
	unsigned individual = -1;

	if (ENABLE_FEATURE_BLAKE_XXH_SUM_CHECK) {
		/* -s and -w require -c */
		switch (applet_name[3]) {
#if ENABLE_XXHSUM
		case HASH_XXH:
			/* -b -i -q are ignored, -a treated as -H (xxhsum compat) */
			flags = getopt32(argv, "^" "scwbiqH:+a:+" "\0"  "s?c:w?c", &individual, &digest_bits);
			break;
#endif
#if ENABLE_XXH32SUM || ENABLE_XXH64SUM || ENABLE_XXH128SUM
		case HASH_XXH32:
		case HASH_XXH64:
		case HASH_XXH128:
			/* -b -i -q are ignored */
			flags = getopt32(argv, "^" "scwbiq" "\0" "s?c:w?c");
			break;
#endif
		default:
#if ENABLE_B2SUM || ENABLE_B3SUM
			/* -b "binary", -t "text" are ignored, -l is added (b2sum, b3sum compat) */
			flags = getopt32(argv, "^" "scwbtl:+a:+" "\0" "s?c:w?c", &individual, &digest_bits);
#endif
		}
	} else {
		switch (applet_name[3]) {
#if ENABLE_XXHSUM
		case HASH_XXH:
			/* -b -i -q are ignored, -a treated as -H (xxhsum compat) */
			getopt32(argv, "H:+a:+", &individual, &digest_bits);
			break;
#endif
#if ENABLE_XXH32SUM || ENABLE_XXH64SUM || ENABLE_XXH128SUM
		case HASH_XXH32:
		case HASH_XXH64:
		case HASH_XXH128:
			/* -b -i -q are ignored */
			getopt32(argv, "");
			break;
#endif
		default:
#if ENABLE_B2SUM || ENABLE_B3SUM
			/* -b "binary", -t "text" are ignored, -l is added (b2sum, b3sum compat) */
			getopt32(argv, "l:+a:+", &individual, &digest_bits);
#endif
		}
	}

#if ENABLE_B2SUM || ENABLE_B3SUM || ENABLE_XXHSUM
	/* -H/-l will override -a */
	if (individual != -1) {
		if (applet_name[3] == HASH_XXH) {
			switch (individual) {
			case 0:
				digest_bits = 32;
				break;
			case 1:
				digest_bits = 0;
				break;
			case 2:
				digest_bits = 128;
				break;
			case 3:
				digest_bits = 64;
				break;
			default:
				bb_error_msg_and_die("bad algorithm: %u", individual);
			}
		} else {
			digest_bits = individual * (applet_name[1] == HASH_BLAKE2 ? 1 : 8);
		}
	}
	if (digest_bits % 8 != 0) {
		bb_error_msg_and_die("bad digest length: %u", digest_bits);
	}
#endif

	argv += optind;
	//argc -= optind;
	if (!*argv)
		*--argv = (char *)"-";

	/* The buffer is not alloc/freed for each input file:
	 * for big values of COPYBUF_KB, this helps to keep its pages
	 * pre-faulted and possibly even fully cached on local CPU.
	 */
	in_buf = xmalloc(BUFSZ);

	do {
		if (ENABLE_FEATURE_BLAKE_XXH_SUM_CHECK && (flags & FLAG_CHECK)) {
			FILE *pre_computed_stream;
			char *line;
			int count_total = 0;
			int count_failed = 0;

			pre_computed_stream = xfopen_stdin(*argv);

			while ((line = xmalloc_fgetline(pre_computed_stream)) != NULL) {
				uint8_t *hash_value;
				char *filename_ptr;

				count_total++;
				filename_ptr = strchr(line, ' ');
				if (!filename_ptr) {
					if (flags & FLAG_WARN) {
						bb_simple_error_msg("invalid format");
					}
					count_failed++;
					return_value = EXIT_FAILURE;
					free(line);
					continue;
				}
				*filename_ptr++ = '\0';
				/* coreutils 9.1 allows "HASH FILENAME" format,
				 * with only one space. Skip the 'correct'
				 * "  " or " *" delimiter if it is there:
				 */
				if (*filename_ptr == ' ' || *filename_ptr == '*')
					filename_ptr++;

				hash_value = hash_file(in_buf, filename_ptr, digest_bits);

#if ENABLE_PLATFORM_MINGW32
				if (hash_value && (strcasecmp((char*)hash_value, line) == 0)) {
#else
				if (hash_value && (strcmp((char*)hash_value, line) == 0)) {
#endif
					if (!(flags & FLAG_SILENT))
						printf("%s: OK\n", filename_ptr);
				} else {
					if (!(flags & FLAG_SILENT))
						printf("%s: FAILED\n", filename_ptr);
					count_failed++;
					return_value = EXIT_FAILURE;
				}
				/* possible free(NULL) */
				free(hash_value);
				free(line);
			}
			if (count_failed && !(flags & FLAG_SILENT)) {
				bb_error_msg("WARNING: %d of %d computed checksums did NOT match", count_failed, count_total);
			}
			if (count_total == 0) {
				return_value = EXIT_FAILURE;
				/*
				 * md5sum from GNU coreutils 8.25 says:
				 * md5sum: <FILE>: no properly formatted MD5 checksum lines found
				 */
				bb_error_msg("%s: no checksum lines found", *argv);
			}
			fclose_if_not_stdin(pre_computed_stream);
		} else {
			uint8_t *hash_value = hash_file(in_buf, *argv, digest_bits);
			if (hash_value == NULL) {
				return_value = EXIT_FAILURE;
			} else {
				printf("%s  %s\n", hash_value, *argv);
				free(hash_value);
			}
		}
	} while (*++argv);

	return return_value;
}
