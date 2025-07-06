/*-
 * Copyright 2009 Colin Percival
 * Copyright 2013-2018 Alexander Peslyak
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file was originally written by Colin Percival as part of the Tarsnap
 * online backup system.
 */
#ifdef YESCRYPT_INTERNAL
# if 1
#  define dbg(...) ((void)0)
# else
#  define dbg(...) bb_error_msg(__VA_ARGS__)
# endif
#endif

/**
 * Type and possible values for the flags argument of yescrypt_kdf(),
 * yescrypt_encode_params_r(), yescrypt_encode_params().  Most of these may be
 * OR'ed together, except that YESCRYPT_WORM stands on its own.
 * Please refer to the description of yescrypt_kdf() below for the meaning of
 * these flags.
 */
typedef uint32_t yescrypt_flags_t;
/* Public */
#define YESCRYPT_WORM			1
#define YESCRYPT_RW			0x002
#define YESCRYPT_ROUNDS_3		0x000
#define YESCRYPT_ROUNDS_6		0x004
#define YESCRYPT_GATHER_1		0x000
#define YESCRYPT_GATHER_2		0x008
#define YESCRYPT_GATHER_4		0x010
#define YESCRYPT_GATHER_8		0x018
#define YESCRYPT_SIMPLE_1		0x000
#define YESCRYPT_SIMPLE_2		0x020
#define YESCRYPT_SIMPLE_4		0x040
#define YESCRYPT_SIMPLE_8		0x060
#define YESCRYPT_SBOX_6K		0x000
#define YESCRYPT_SBOX_12K		0x080
#define YESCRYPT_SBOX_24K		0x100
#define YESCRYPT_SBOX_48K		0x180
#define YESCRYPT_SBOX_96K		0x200
#define YESCRYPT_SBOX_192K		0x280
#define YESCRYPT_SBOX_384K		0x300
#define YESCRYPT_SBOX_768K		0x380

#ifdef YESCRYPT_INTERNAL
/* Private */
#define YESCRYPT_MODE_MASK		0x003
#define YESCRYPT_RW_FLAVOR_MASK		0x3fc
#define YESCRYPT_ALLOC_ONLY		0x08000000
#define YESCRYPT_PREHASH		0x10000000
#endif

#define YESCRYPT_RW_DEFAULTS \
	(YESCRYPT_RW | \
	YESCRYPT_ROUNDS_6 | YESCRYPT_GATHER_4 | YESCRYPT_SIMPLE_2 | \
	YESCRYPT_SBOX_12K)

#define YESCRYPT_DEFAULTS YESCRYPT_RW_DEFAULTS

#ifdef YESCRYPT_INTERNAL
#define YESCRYPT_KNOWN_FLAGS \
	(YESCRYPT_MODE_MASK | YESCRYPT_RW_FLAVOR_MASK | \
	YESCRYPT_ALLOC_ONLY | YESCRYPT_PREHASH)
#endif

/**
 * Internal type used by the memory allocator.  Please do not use it directly.
 * Use yescrypt_shared_t and yescrypt_local_t as appropriate instead, since
 * they might differ from each other in a future version.
 */
typedef struct {
//	void *base;
	void *aligned;
//	size_t base_size;
	size_t aligned_size;
} yescrypt_region_t;

/**
 * yescrypt parameters combined into one struct.  N, r, p are the same as in
 * classic scrypt, except that the meaning of p changes when YESCRYPT_RW is
 * set.  flags, t, g, NROM are special to yescrypt.
 */
typedef struct {
	yescrypt_flags_t flags;
	uint64_t N;
	uint32_t r, p, t, g;
	uint64_t NROM;
} yescrypt_params_t;

typedef struct {
	yescrypt_params_t param;

	/* salt in binary form */
	/* stored here to cut down on the amount of function paramaters */
	unsigned char salt[64];
	size_t saltlen;

	/* used by the memory allocator */
	//yescrypt_region_t shared[1];
	yescrypt_region_t local[1];
} yescrypt_ctx_t;

/* How many chars base-64 encoded bytes require? */
#define YESCRYPT_BYTES2CHARS(bytes) ((((bytes) * 8) + 5) / 6)
/* The /etc/passwd-style hash is "<prefix>$<hash><NUL>" */
/*
 * "$y$", up to 8 params of up to 6 chars each, '$', salt
 * Alternatively, but that's smaller:
 * "$7$", 3 params encoded as 1+5+5 chars, salt
 */
#define YESCRYPT_PREFIX_LEN (3 + 8 * 6 + 1 + YESCRYPT_BYTES2CHARS(32))

#define YESCRYPT_HASH_SIZE 32
#define YESCRYPT_HASH_LEN  YESCRYPT_BYTES2CHARS(YESCRYPT_HASH_SIZE)

/**
 * yescrypt_r(shared, local, passwd, passwdlen, setting, key, buf, buflen):
 * Compute and encode an scrypt or enhanced scrypt hash of passwd given the
 * parameters and salt value encoded in setting.  If shared is not NULL, a ROM
 * is used and YESCRYPT_RW is required.  Otherwise, whether to compute classic
 * scrypt, YESCRYPT_WORM (a slight deviation from classic scrypt), or
 * YESCRYPT_RW (time-memory tradeoff discouraging modification) is determined
 * by the setting string.  shared (if not NULL) and local must be initialized
 * as described above for yescrypt_kdf().  buf must be large enough (as
 * indicated by buflen) to hold the encoded hash string.
 *
 * Return the encoded hash string on success; or NULL on error.
 *
 * MT-safe as long as local and buf are local to the thread.
 */
extern uint8_t *yescrypt_r(
	    const uint8_t *passwd, size_t passwdlen,
	    const uint8_t *setting,
	    uint8_t *buf, size_t buflen
);
