/*
   BLAKE2 reference source code package - reference C implementations

   Copyright 2012, Samuel Neves <sneves@dei.uc.pt>.  You may use this under the
   terms of the CC0, the OpenSSL Licence, or the Apache Public License 2.0, at
   your option.  The terms of these licenses can be found at:

   - CC0 1.0 Universal : https://creativecommons.org/publicdomain/zero/1.0
   - OpenSSL license   : https://www.openssl.org/source/license.html
   - Apache 2.0        : https://www.apache.org/licenses/LICENSE-2.0

   More information about the BLAKE2 hash function can be found at
   https://blake2.net.
*/

#include "libbb.h"
#include "platform.h"

static ALWAYS_INLINE uint64_t load64(const void *src)
{
#if BB_LITTLE_ENDIAN
	uint64_t w;
	memcpy(&w, src, sizeof w);
	return w;
#else
	const uint8_t *p = (const uint8_t *)src;
	return ((uint64_t)(p[0]) << 0) |
	       ((uint64_t)(p[1]) << 8) |
	       ((uint64_t)(p[2]) << 16) |
	       ((uint64_t)(p[3]) << 24) |
	       ((uint64_t)(p[4]) << 32) |
	       ((uint64_t)(p[5]) << 40) |
	       ((uint64_t)(p[6]) << 48) |
	       ((uint64_t)(p[7]) << 56);
#endif
}

static ALWAYS_INLINE void store64(void *dst, uint64_t w)
{
#if BB_LITTLE_ENDIAN
	memcpy(dst, &w, sizeof w);
#else
	uint8_t *p = (uint8_t *)dst;
	p[0] = (uint8_t)(w >> 0);
	p[1] = (uint8_t)(w >> 8);
	p[2] = (uint8_t)(w >> 16);
	p[3] = (uint8_t)(w >> 24);
	p[4] = (uint8_t)(w >> 32);
	p[5] = (uint8_t)(w >> 40);
	p[6] = (uint8_t)(w >> 48);
	p[7] = (uint8_t)(w >> 56);
#endif
}

static ALWAYS_INLINE uint64_t rotr64(const uint64_t w, const unsigned c)
{
	return (w >> c) | (w << (64 - c));
}

/* prevents compiler optimizing out memset() */
static ALWAYS_INLINE void secure_zero_memory(void *v, size_t n)
{
	static void *(*const volatile memset_v)(void *, int, size_t) = &memset;
	memset_v(v, 0, n);
}

/* BLAKE2b */
enum {
	BLAKE2B_BLOCKBYTES = 128,
	BLAKE2B_OUTBYTES = 64,
};

typedef struct
{
	uint64_t h[8];
	uint64_t t[2];
	uint64_t f[2];
	uint8_t buf[BLAKE2B_BLOCKBYTES];
	size_t buflen;
	size_t outlen;
	uint8_t last_node;
} blake2b_state;

typedef struct
{
	uint8_t digest_length; /* 1 */
	uint8_t key_length;    /* 2 */
	uint8_t fanout;        /* 3 */
	uint8_t depth;         /* 4 */
	uint32_t leaf_length;  /* 8 */
	uint32_t node_offset;  /* 12 */
	uint32_t xof_length;   /* 16 */
	uint8_t node_depth;    /* 17 */
	uint8_t inner_length;  /* 18 */
	uint8_t reserved[14];  /* 32 */
	uint8_t salt[16];      /* 48 */
	uint8_t personal[16];  /* 64 */
} PACKED blake2b_param;

static const uint64_t BLAKE2B_IV[8] = {
	0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL,
	0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL,
	0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL,
	0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL
};

static const uint8_t BLAKE2B_SIGMA[12][16] = {
	/* clang-format off */
	{  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 },
	{ 14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3 },
	{ 11,  8, 12,  0,  5,  2, 15, 13, 10, 14,  3,  6,  7,  1,  9,  4 },
	{  7,  9,  3,  1, 13, 12, 11, 14,  2,  6,  5, 10,  4,  0, 15,  8 },
	{  9,  0,  5,  7,  2,  4, 10, 15, 14,  1, 11, 12,  6,  8,  3, 13 },
	{  2, 12,  6, 10,  0, 11,  8,  3,  4, 13,  7,  5, 15, 14,  1,  9 },
	{ 12,  5,  1, 15, 14, 13,  4, 10,  0,  7,  6,  3,  9,  2,  8, 11 },
	{ 13, 11,  7, 14, 12,  1,  3,  9,  5,  0, 15,  4,  8,  6,  2, 10 },
	{  6, 15, 14,  9, 11,  3,  0,  8, 12,  2, 13,  7,  1,  4, 10,  5 },
	{ 10,  2,  8,  4,  7,  6,  1,  5, 15, 11,  9, 14,  3, 12, 13,  0 },
	{  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 },
	{ 14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3 }
	/* clang-format on */
};

static void blake2b_increment_counter(blake2b_state *S, const uint64_t inc)
{
	S->t[0] += inc;
	S->t[1] += (S->t[0] < inc);
}

/* init xors IV with input parameter block */
static int blake2b_init_param(blake2b_state *S, const blake2b_param *P)
{
	const uint8_t *p = (const uint8_t *)(P);
	size_t i;

	memset(S, 0, sizeof(blake2b_state));
	for (i = 0; i < 8; ++i)
		S->h[i] = BLAKE2B_IV[i];

	/* IV XOR ParamBlock */
	for (i = 0; i < 8; ++i)
		S->h[i] ^= load64(p + sizeof(S->h[i]) * i);

	S->outlen = P->digest_length;
	return 0;
}

static int blake2b_init(blake2b_state *S, size_t outlen)
{
	blake2b_param P[1];

	if ((!outlen) || (outlen > BLAKE2B_OUTBYTES))
		return -1;

	P->digest_length = (uint8_t)outlen;
	P->key_length = 0;
	P->fanout = 1;
	P->depth = 1;
	store32(&P->leaf_length, 0);
	store32(&P->node_offset, 0);
	store32(&P->xof_length, 0);
	P->node_depth = 0;
	P->inner_length = 0;
	memset(P->reserved, 0, sizeof(P->reserved));
	memset(P->salt, 0, sizeof(P->salt));
	memset(P->personal, 0, sizeof(P->personal));
	return blake2b_init_param(S, P);
}

#define BLAKE2B_G(r, i, a, b, c, d) \
	do { \
		a = a + b + m[BLAKE2B_SIGMA[r][2 * i + 0]]; \
		d = rotr64(d ^ a, 32); \
		c = c + d; \
		b = rotr64(b ^ c, 24); \
		a = a + b + m[BLAKE2B_SIGMA[r][2 * i + 1]]; \
		d = rotr64(d ^ a, 16); \
		c = c + d; \
		b = rotr64(b ^ c, 63); \
	} while (0)

#define BLAKE2B_ROUND(r) \
	do { \
		BLAKE2B_G(r, 0, v[0], v[4], v[8], v[12]); \
		BLAKE2B_G(r, 1, v[1], v[5], v[9], v[13]); \
		BLAKE2B_G(r, 2, v[2], v[6], v[10], v[14]); \
		BLAKE2B_G(r, 3, v[3], v[7], v[11], v[15]); \
		BLAKE2B_G(r, 4, v[0], v[5], v[10], v[15]); \
		BLAKE2B_G(r, 5, v[1], v[6], v[11], v[12]); \
		BLAKE2B_G(r, 6, v[2], v[7], v[8], v[13]); \
		BLAKE2B_G(r, 7, v[3], v[4], v[9], v[14]); \
	} while (0)

static void blake2b_compress(blake2b_state *S, const uint8_t block[BLAKE2B_BLOCKBYTES])
{
	uint64_t m[16];
	uint64_t v[16];
	size_t i;

	for (i = 0; i < 16; ++i) {
		m[i] = load64(block + i * sizeof(m[i]));
	}

	for (i = 0; i < 8; ++i) {
		v[i] = S->h[i];
	}

	v[8] = BLAKE2B_IV[0];
	v[9] = BLAKE2B_IV[1];
	v[10] = BLAKE2B_IV[2];
	v[11] = BLAKE2B_IV[3];
	v[12] = BLAKE2B_IV[4] ^ S->t[0];
	v[13] = BLAKE2B_IV[5] ^ S->t[1];
	v[14] = BLAKE2B_IV[6] ^ S->f[0];
	v[15] = BLAKE2B_IV[7] ^ S->f[1];

	BLAKE2B_ROUND(0);
	BLAKE2B_ROUND(1);
	BLAKE2B_ROUND(2);
	BLAKE2B_ROUND(3);
	BLAKE2B_ROUND(4);
	BLAKE2B_ROUND(5);
	BLAKE2B_ROUND(6);
	BLAKE2B_ROUND(7);
	BLAKE2B_ROUND(8);
	BLAKE2B_ROUND(9);
	BLAKE2B_ROUND(10);
	BLAKE2B_ROUND(11);

	for (i = 0; i < 8; ++i) {
		S->h[i] = S->h[i] ^ v[i] ^ v[i + 8];
	}
}

#undef BLAKE2B_G
#undef BLAKE2B_ROUND

static int blake2b_update(blake2b_state *S, const void *pin, size_t inlen)
{
	const unsigned char *in = (const unsigned char *)pin;
	if (inlen > 0) {
		size_t left = S->buflen;
		size_t fill = BLAKE2B_BLOCKBYTES - left;
		if (inlen > fill) {
			S->buflen = 0;
			memcpy(S->buf + left, in, fill); /* Fill buffer */
			blake2b_increment_counter(S, BLAKE2B_BLOCKBYTES);
			blake2b_compress(S, S->buf); /* Compress */
			in += fill;
			inlen -= fill;
			while (inlen > BLAKE2B_BLOCKBYTES) {
				blake2b_increment_counter(S, BLAKE2B_BLOCKBYTES);
				blake2b_compress(S, in);
				in += BLAKE2B_BLOCKBYTES;
				inlen -= BLAKE2B_BLOCKBYTES;
			}
		}
		memcpy(S->buf + S->buflen, in, inlen);
		S->buflen += inlen;
	}
	return 0;
}

static int blake2b_final(blake2b_state *S, void *out, size_t outlen)
{
	uint8_t buffer[BLAKE2B_OUTBYTES] = { 0 };
	size_t i;

	if (out == NULL || outlen < S->outlen)
		return -1;

	if (S->f[0] != 0) /* Last block */
		return -1;

	blake2b_increment_counter(S, S->buflen);
	{ /* Set last block */
		if (S->last_node)
			S->f[1] = (uint64_t)-1;
		S->f[0] = (uint64_t)-1;
	}
	memset(S->buf + S->buflen, 0, BLAKE2B_BLOCKBYTES - S->buflen); /* Padding */
	blake2b_compress(S, S->buf);

	for (i = 0; i < 8; ++i) /* Output full hash to temp buffer */
		store64(buffer + sizeof(S->h[i]) * i, S->h[i]);

	memcpy(out, buffer, S->outlen);
	secure_zero_memory(buffer, sizeof(buffer));
	return 0;
}
