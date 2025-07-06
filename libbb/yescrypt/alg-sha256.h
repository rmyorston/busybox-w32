/*-
 * Copyright 2005-2016 Colin Percival
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

/*
 * Use #defines in order to avoid namespace collisions with anyone else's
 * SHA256 code (e.g., the code in OpenSSL).
 */
#define HMAC_SHA256_Init libcperciva_HMAC_SHA256_Init
#define HMAC_SHA256_Update libcperciva_HMAC_SHA256_Update
#define HMAC_SHA256_Final libcperciva_HMAC_SHA256_Final
#define HMAC_SHA256_Buf libcperciva_HMAC_SHA256_Buf
#define HMAC_SHA256_CTX libcperciva_HMAC_SHA256_CTX

/**
 * SHA256_Buf(in, len, digest):
 * Compute the SHA256 hash of ${len} bytes from ${in} and write it to ${digest}.
 */
extern void SHA256_Buf(const void *, size_t, uint8_t[32]);

/* Context structure for HMAC-SHA256 operations. */
typedef struct {
	sha256_ctx_t ictx;
	sha256_ctx_t octx;
} HMAC_SHA256_CTX;

/**
 * PBKDF2_SHA256(passwd, passwdlen, salt, saltlen, c, buf, dkLen):
 * Compute PBKDF2(passwd, salt, c, dkLen) using HMAC-SHA256 as the PRF, and
 * write the output to buf.  The value dkLen must be at most 32 * (2^32 - 1).
 */
extern void PBKDF2_SHA256(const uint8_t *, size_t, const uint8_t *, size_t,
    uint64_t, uint8_t *, size_t);
