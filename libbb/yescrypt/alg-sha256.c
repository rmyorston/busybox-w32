/*-
 * Copyright 2005-2016 Colin Percival
 * Copyright 2016-2018,2021 Alexander Peslyak
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
 */

/**
 * HMAC_SHA256_Init(ctx, K, Klen):
 * Initialize the HMAC-SHA256 context ${ctx} with ${Klen} bytes of key from
 * ${K}.
 */
static void
HMAC_SHA256_Init(HMAC_SHA256_CTX *ctx, const void *_K, size_t Klen)
{
	uint8_t pad[64];
	uint8_t khash[32];
	const uint8_t *K = _K;
	size_t i;

	/* If Klen > 64, the key is really SHA256(K). */
	if (Klen > 64) {
		sha256_block(K, Klen, khash);
		K = khash;
		Klen = 32;
	}

	/* Inner SHA256 operation is SHA256(K xor [block of 0x36] || data). */
	sha256_begin(&ctx->ictx);
	memset(pad, 0x36, 64);
	for (i = 0; i < Klen; i++)
		pad[i] ^= K[i];
	sha256_hash(&ctx->ictx, pad, 64);

	/* Outer SHA256 operation is SHA256(K xor [block of 0x5c] || hash). */
	sha256_begin(&ctx->octx);
	memset(pad, 0x5c, 64);
	for (i = 0; i < Klen; i++)
		pad[i] ^= K[i];
	sha256_hash(&ctx->octx, pad, 64);
}

/**
 * HMAC_SHA256_Update(ctx, in, len):
 * Input ${len} bytes from ${in} into the HMAC-SHA256 context ${ctx}.
 */
static void
HMAC_SHA256_Update(HMAC_SHA256_CTX *ctx, const void *in, size_t len)
{
	/* Feed data to the inner SHA256 operation. */
	sha256_hash(&ctx->ictx, in, len);
}

/**
 * HMAC_SHA256_Final(ctx, digest):
 * Output the HMAC-SHA256 of the data input to the context ${ctx} into the
 * buffer ${digest}.
 */
static void
HMAC_SHA256_Final(HMAC_SHA256_CTX *ctx, uint8_t digest[32])
{
	/* Finish the inner SHA256 operation. */
	sha256_end(&ctx->ictx, digest); /* using digest[] as scratch space */
	/* Feed the inner hash to the outer SHA256 operation. */
	sha256_hash(&ctx->octx, digest, 32); /* using digest[] as scratch space */
	/* Finish the outer SHA256 operation. */
	sha256_end(&ctx->octx, digest);
}

/**
 * HMAC_SHA256_Buf(K, Klen, in, len, digest):
 * Compute the HMAC-SHA256 of ${len} bytes from ${in} using the key ${K} of
 * length ${Klen}, and write the result to ${digest}.
 */
static void
HMAC_SHA256_Buf(const void *K, size_t Klen, const void *in, size_t len,
		uint8_t digest[32])
{
	HMAC_SHA256_CTX ctx;
	HMAC_SHA256_Init(&ctx, K, Klen);
	HMAC_SHA256_Update(&ctx, in, len);
	HMAC_SHA256_Final(&ctx, digest);
}

/**
 * PBKDF2_SHA256(passwd, passwdlen, salt, saltlen, c, buf, dkLen):
 * Compute PBKDF2(passwd, salt, c, dkLen) using HMAC-SHA256 as the PRF, and
 * write the output to buf.  The value dkLen must be at most 32 * (2^32 - 1).
 */
static void
PBKDF2_SHA256(const uint8_t *passwd, size_t passwdlen,
		const uint8_t *salt, size_t saltlen,
		uint64_t c, uint8_t *buf, size_t dkLen)
{
	HMAC_SHA256_CTX Phctx, PShctx, hctx;
	size_t i;
	uint8_t ivec[4];
	uint8_t U[32];
	uint8_t T[32];
	uint64_t j;
	int k;
	size_t clen;

	/* Sanity-check. */
	assert(dkLen <= 32 * (size_t)(UINT32_MAX));

	/* Compute HMAC state after processing P. */
	HMAC_SHA256_Init(&Phctx, passwd, passwdlen);

	/* Compute HMAC state after processing P and S. */
	memcpy(&PShctx, &Phctx, sizeof(HMAC_SHA256_CTX));
	HMAC_SHA256_Update(&PShctx, salt, saltlen);

	/* Iterate through the blocks. */
	for (i = 0; i * 32 < dkLen; i++) {
		/* Generate INT(i + 1). */
		be32enc(ivec, (uint32_t)(i + 1));

		/* Compute U_1 = PRF(P, S || INT(i)). */
		memcpy(&hctx, &PShctx, sizeof(HMAC_SHA256_CTX));
		HMAC_SHA256_Update(&hctx, ivec, 4);
		HMAC_SHA256_Final(&hctx, T);

		if (c > 1) {
			/* T_i = U_1 ... */
			memcpy(U, T, 32);
			for (j = 2; j <= c; j++) {
				/* Compute U_j. */
				memcpy(&hctx, &Phctx, sizeof(HMAC_SHA256_CTX));
				HMAC_SHA256_Update(&hctx, U, 32);
				HMAC_SHA256_Final(&hctx, U);
				/* ... xor U_j ... */
				for (k = 0; k < 32; k++)
					T[k] ^= U[k];
			}
		}

		/* Copy as many bytes as necessary into buf. */
		clen = dkLen - i * 32;
		if (clen > 32)
			clen = 32;
		memcpy(&buf[i * 32], T, clen);
	}
}
