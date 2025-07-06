/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
#if !ENABLE_USE_BB_CRYPT
# if !defined(__FreeBSD__)
#  include <crypt.h>
# endif
#endif
#include "libbb.h"

/* 0..63 ->
 * "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
 */
int FAST_FUNC i2a64(int i)
{
	i &= 0x3f;
	if (i == 0)
		return '.';
	if (i == 1)
		return '/';
	if (i < 12)
		return ('0' - 2 + i);
	if (i < 38)
		return ('A' - 12 + i);
	return ('a' - 38 + i);
}

/* Returns >=64 for invalid chars */
int FAST_FUNC a2i64(char c)
{
	unsigned char ch = c;
	if (ch >= 'a')
		/* "a..z" to 38..63 */
		/* anything after "z": positive int >= 64 */
		return (ch - 'a' + 38);

	if (ch > 'Z')
		/* after "Z" but before "a": positive byte >= 64 */
		return ch;

	if (ch >= 'A')
		/* "A..Z" to 12..37 */
		return (ch - 'A' + 12);

	if (ch > '9')
		return 64;

	/* "./0123456789" to 0,1,2..11 */
	/* anything before "." becomes positive byte >= 64 */
	return (unsigned char)(ch - '.');
}

int FAST_FUNC crypt_make_rand64encoded(char *p, int cnt /*, int x */)
{
	/* was: x += ... */
	unsigned x = getpid() + monotonic_us();
	do {
		/* x = (x*1664525 + 1013904223) % 2^32 generator is lame
		 * (low-order bit is not "random", etc...),
		 * but for our purposes it is good enough */
		x = x*1664525 + 1013904223;
		/* BTW, Park and Miller's "minimal standard generator" is
		 * x = x*16807 % ((2^31)-1)
		 * It has no problem with visibly alternating lowest bit
		 * but is also weak in cryptographic sense + needs div,
		 * which needs more code (and slower) on many CPUs */
		*p++ = i2a64(x >> 16);
		*p++ = i2a64(x >> 22);
	} while (--cnt);
	*p = '\0';
	return x;
}

char* FAST_FUNC crypt_make_pw_salt(char salt[MAX_PW_SALT_LEN], const char *algo)
{
	int len = 2/2;
	char *salt_ptr = salt;

	/* Standard chpasswd uses uppercase algos ("MD5", not "md5").
	 * Need to be case-insensitive in the code below.
	 */
	if ((algo[0]|0x20) != 'd') { /* not des */
		len = 8/2; /* so far assuming md5 */
		*salt_ptr++ = '$';
		*salt_ptr++ = '1';
		*salt_ptr++ = '$';
#if !ENABLE_USE_BB_CRYPT || ENABLE_USE_BB_CRYPT_SHA
		if ((algo[0]|0x20) == 's') { /* sha */
			salt[1] = '5' + (strcasecmp(algo, "sha512") == 0);
			len = 16 / 2;
		}
#endif
#if !ENABLE_USE_BB_CRYPT || ENABLE_USE_BB_CRYPT_YES
		if ((algo[0]|0x20) == 'y') { /* yescrypt */
			salt[1] = 'y';
			len = 24 / 2;
// The "j9T$" below is the default "yescrypt parameters" encoded by yescrypt_encode_params_r():
//
//shadow-4.17.4/src/passwd.c
//	salt = crypt_make_salt(NULL, NULL);
//shadow-4.17.4/lib/salt.c
//const char *crypt_make_salt(const char *meth, void *arg)
//      if (streq(method, "YESCRYPT")) {
//              MAGNUM(result, 'y');
//              salt_len = YESCRYPT_SALT_SIZE; // 24
//              rounds = YESCRYPT_get_salt_cost(arg);  // always Y_COST_DEFAULT == 5 for NULL arg
//              YESCRYPT_salt_cost_to_buf(result, rounds); // always "j9T$"
//      char *retval = crypt_gensalt(result, rounds, NULL, 0);
//libxcrypt-4.4.38/lib/crypt-yescrypt.c
//void gensalt_yescrypt_rn (unsigned long count,
//                     const uint8_t *rbytes, size_t nrbytes,
//                     uint8_t *output, size_t o_size)
//  yescrypt_params_t params = {
//    .flags = YESCRYPT_DEFAULTS,
//    .p = 1,
//  };
//  if (count < 3) ... else
//      params.r = 32;                  // N in 4KiB
//      params.N = 1ULL << (count + 7); // 3 -> 1024, 4 -> 2048, ... 11 -> 262144
//  yescrypt_encode_params_r(&params, rbytes, nrbytes, outbuf, o_size) // always "$y$j9T$<random>"
			salt_ptr = stpcpy(salt_ptr, "j9T$");
			crypt_make_rand64encoded(salt_ptr, len); /* appends 2*len random chars */
			/* For "mkpasswd -m yescrypt PASS j9T$<salt>" use case,
			 * "j9T$" is considered part of salt,
			 * need to return pointer to 'j'. Without -4,
			 * we'd end up using "j9T$j9T$<salt>" as salt.
			 */
			return salt_ptr - 4;
		}
#endif
	}
	crypt_make_rand64encoded(salt_ptr, len); /* appends 2*len random chars */
	return salt_ptr;
}

#if ENABLE_USE_BB_CRYPT

static char*
to64(char *s, unsigned v, int n)
{
	while (--n >= 0) {
		*s++ = i2a64(v);
		v >>= 6;
	}
	return s;
}

/*
 * DES and MD5 crypt implementations are taken from uclibc.
 * They were modified to not use static buffers.
 */

#include "pw_encrypt_des.c"
#include "pw_encrypt_md5.c"
#if ENABLE_USE_BB_CRYPT_SHA
#include "pw_encrypt_sha.c"
#endif
#if ENABLE_USE_BB_CRYPT_YES
#include "pw_encrypt_yes.c"
#endif

/* Other advanced crypt ids (TODO?): */
/* $2$ or $2a$: Blowfish */

static struct const_des_ctx *des_cctx;
static struct des_ctx *des_ctx;

/* my_crypt returns malloc'ed data */
static char *my_crypt(const char *key, const char *salt)
{
	/* "$x$...." string? */
	if (salt[0] == '$' && salt[1] && salt[2] == '$') {
		if (salt[1] == '1')
			return md5_crypt(xzalloc(MD5_OUT_BUFSIZE), (unsigned char*)key, (unsigned char*)salt);
#if ENABLE_USE_BB_CRYPT_YES
		if (salt[1] == 'y')
			return yes_crypt(key, salt);
#endif
#if ENABLE_USE_BB_CRYPT_SHA
		if (salt[1] == '5' || salt[1] == '6')
			return sha_crypt((char*)key, (char*)salt);
#endif
	}

	if (!des_cctx)
		des_cctx = const_des_init();
	des_ctx = des_init(des_ctx, des_cctx);
	/* Can return NULL if salt is bad ("" or "<one_char>") */
	return des_crypt(des_ctx, xzalloc(DES_OUT_BUFSIZE), (unsigned char*)key, (unsigned char*)salt);
}

/* So far nobody wants to have it public */
static void my_crypt_cleanup(void)
{
	free(des_cctx);
	free(des_ctx);
	des_cctx = NULL;
	des_ctx = NULL;
}

char* FAST_FUNC pw_encrypt(const char *clear, const char *salt, int cleanup)
{
	char *encrypted;

	encrypted = my_crypt(clear, salt);
	if (!encrypted)
		bb_simple_error_msg_and_die("bad salt");

	if (cleanup)
		my_crypt_cleanup();

	return encrypted;
}

#else /* if !ENABLE_USE_BB_CRYPT */

char* FAST_FUNC pw_encrypt(const char *clear, const char *salt, int cleanup)
{
	char *encrypted;

	encrypted = crypt(clear, salt);
	/*
	 * glibc used to return "" on malformed salts (for example, ""),
	 * but since 2.17 it returns NULL.
	 */
	if (!encrypted || !encrypted[0])
		bb_simple_error_msg_and_die("bad salt");
	return xstrdup(encrypted);
}

#endif
