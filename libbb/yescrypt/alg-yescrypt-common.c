/*-
 * Copyright 2013-2018 Alexander Peslyak
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted.
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

/* Not inlining:
 * de/encode64 functions are only used to read
 * yescrypt_params_t field, and convert salt to binary -
 * both of these are negligible compared to main hashing operation
 */
static NOINLINE const uint8_t *decode64_uint32(
		uint32_t *dst,
		const uint8_t *src, uint32_t val)
{
	uint32_t start = 0, end = 47, chars = 1, bits = 0;
	uint32_t c;

	if (!src) /* prevous decode failed already? */
		goto fail;

	c = a2i64(*src++);
	if (c > 63)
		goto fail;

	while (c > end) {
		val += (end + 1 - start) << bits;
		start = end + 1;
		end = start + (62 - end) / 2;
		chars++;
		bits += 6;
	}

	val += (c - start) << bits;

	while (--chars) {
		c = a2i64(*src++);
		if (c > 63)
			goto fail;
		bits -= 6;
		val += c << bits;
	}
	*dst = val;

	return src;

fail:
	*dst = 0;
	return NULL;
}

#if 1
static const uint8_t *decode64(
		uint8_t *dst, size_t *dstlen,
		const uint8_t *src)
{
	size_t dstpos = 0;

	dbg_dec64("src:'%s' len:%d", src, (int)srclen);
	for (;;) {
		uint32_t c, value = 0;
		int bits = 0;
		while (*src && *src != '$') {
			c = a2i64(*src);
			if (c > 63) { /* bad ascii64 char, stop decoding at it */
				break;
			}
			src++;
			value |= c << bits;
			bits += 6;
			if (bits == 24) /* got 4 chars */
				goto store;
		}
		/* we read entire src, or met a non-ascii64 char (such as "$") */
		if (bits == 0)
			break;
		/* else: we got last, partial bit block - store it */
 store:
		dbg_dec64(" storing bits:%d v:%08x", bits, (int)SWAP_BE32(value)); //BE to see lsb first
		while (dstpos < *dstlen) {
			if ((!*src || *src == '$') && value == 0 && bits < 8) {
				/* Example: mkpasswd PWD '$y$j9T$123':
				 * the "123" is bits:18 value:03,51,00
				 * is considered to be 2 bytes, not 3!
				 *
				 * '$y$j9T$zzz' in upstream fails outright (3rd byte isn't zero).
				 * IOW: for upstream, validity of salt depends on VALUE,
				 * not just size of salt. Which is a bug.
				 * The '$y$j9T$zzz.' salt is the same
				 * (it adds 6 zero msbits) but upstream works with it,
				 * thus '$y$j9T$zzz' should work too and give the same result.
				 */
				goto end;
			}
			dstpos++;
			*dst++ = value;
			value >>= 8;
			bits -= 8;
			if (bits <= 0) /* can get negative, if we e.g. had 6 bits */
				goto next;
		}
		dbg_dec64(" ERR: bits:%d dst[] is too small", bits);
		goto fail;
 next:
		if (!*src || *src == '$')
			break;
	}
 end:
	*dstlen = dstpos;
	dbg_dec64("dec64: OK, dst[%d]", (int)dstpos);
	return src;
 fail:
	/* *dstlen = 0; - not needed, caller detects error by seeing NULL */
	return NULL;
}
#else
/* Buggy (and larger) original code */
static const uint8_t *decode64(
		uint8_t *dst, size_t *dstlen,
		const uint8_t *src, size_t srclen)
{
	size_t dstpos = 0;

	while (dstpos <= *dstlen && srclen) {
		uint32_t value = 0, bits = 0;
		while (srclen--) {
			uint32_t c = a2i64(*src);
			if (c > 63) {
				srclen = 0;
				break;
			}
			src++;
			value |= c << bits;
			bits += 6;
			if (bits >= 24)
				break;
		}
		if (!bits)
			break;
		if (bits < 12) /* must have at least one full byte */
			goto fail;
		dbg_dec64(" storing bits:%d v:%08x", (int)bits, (int)SWAP_BE32(value)); //BE to see lsb first
		while (dstpos++ < *dstlen) {
			*dst++ = value;
			value >>= 8;
			bits -= 8;
			if (bits < 8) { /* 2 or 4 */
				if (value) /* must be 0 */
					goto fail;
				bits = 0;
				break;
			}
		}
		if (bits)
			goto fail;
	}

	if (!srclen && dstpos <= *dstlen) {
		*dstlen = dstpos;
		dbg_dec64("dec64: OK, dst[%d]", (int)dstpos);
		return src;
	}
 fail:
	/* *dstlen = 0; - not needed, caller detects error by seeing NULL */
	return NULL;
}
#endif

static char *encode64(
		char *dst, size_t dstlen,
		const uint8_t *src, size_t srclen)
{
	while (srclen) {
		uint32_t value = 0, b = 0;
		do {
			value |= (uint32_t)(*src++ << b);
			b += 8;
			srclen--;
		} while (srclen && b < 24);

		b >>= 3; /* number of bits to number of bytes */
		b++; /* 1, 2 or 3 bytes will become 2, 3 or 4 ascii64 chars */
		dstlen -= b;
		if ((ssize_t)dstlen <= 0)
			return NULL;
		dst = num2str64_lsb_first(dst, value, b);
	}
	*dst = '\0';
	return dst;
}

char *yescrypt_r(
		const uint8_t *passwd, size_t passwdlen,
		const uint8_t *setting,
		char *buf, size_t buflen)
{
	yescrypt_ctx_t yctx[1];
	unsigned char hashbin32[32];
	char *dst;
	const uint8_t *src, *saltend;
	size_t need, prefixlen;
	uint32_t u32;

	memset(yctx, 0, sizeof(yctx));
	yctx->param.p = 1;

	/* we assume setting starts with "$y$" (caller must ensure this) */
	src = setting + 3;

	src = decode64_uint32(&yctx->param.flags, src, 0);
	/* "j9T" returns: 0x2f */
	//if (!src)
	//	goto fail;

	if (yctx->param.flags < YESCRYPT_RW) {
		dbg("yctx->param.flags=0x%x", (unsigned)yctx->param.flags);
		goto fail; // bbox: we don't support scrypt - only yescrypt
	} else if (yctx->param.flags <= YESCRYPT_RW + (YESCRYPT_RW_FLAVOR_MASK >> 2)) {
		/* "j9T" sets flags to 0xb6 */
		yctx->param.flags = YESCRYPT_RW + ((yctx->param.flags - YESCRYPT_RW) << 2);
		dbg("yctx->param.flags=0x%x", (unsigned)yctx->param.flags);
		dbg(" YESCRYPT_RW:%u", !!(yctx->param.flags & YESCRYPT_RW));
                dbg((yctx->param.flags & YESCRYPT_RW_FLAVOR_MASK) ==
                       (YESCRYPT_ROUNDS_6 | YESCRYPT_GATHER_4 | YESCRYPT_SIMPLE_2 | YESCRYPT_SBOX_12K)
		    ? " YESCRYPT_ROUNDS_6 | YESCRYPT_GATHER_4 | YESCRYPT_SIMPLE_2 | YESCRYPT_SBOX_12K"
		    : " flags are not standard"
		);
	} else {
		goto fail;
	}

	src = decode64_uint32(&u32, src, 1);
	if (/*!src ||*/ u32 > 63)
		goto fail;
	yctx->param.N = (uint64_t)1 << u32;
	/* "j9T" sets to 4096 (1<<12) */
	dbg("yctx->param.N=%llu (1<<%u)", (unsigned long long)yctx->param.N, (unsigned)u32);

	src = decode64_uint32(&yctx->param.r, src, 1);
	/* "j9T" sets to 32 */
	dbg("yctx->param.r=%u", yctx->param.r);

	if (!src)
		goto fail;
	if (*src != '$') {
		src = decode64_uint32(&u32, src, 1);
		dbg("yescrypt has extended params:0x%x", (unsigned)u32);
		if (u32 & 1)
			src = decode64_uint32(&yctx->param.p, src, 2);
		if (u32 & 2)
			src = decode64_uint32(&yctx->param.t, src, 1);
		if (u32 & 4)
			src = decode64_uint32(&yctx->param.g, src, 1);
		if (u32 & 8) {
			src = decode64_uint32(&u32, src, 1);
			if (/*!src ||*/ u32 > 63)
				goto fail;
			yctx->param.NROM = (uint64_t)1 << u32;
		}
		if (!src)
			goto fail;
		if (*src != '$')
			goto fail;
	}

	yctx->saltlen = sizeof(yctx->salt);
	src++; /* now points to salt */
	saltend = decode64(yctx->salt, &yctx->saltlen, src);
	if (!saltend || (*saltend != '\0' && *saltend != '$'))
		goto fail; /* salt[] is too small, or bad char during decode */

	prefixlen = saltend - setting;
	need = prefixlen + 1 + YESCRYPT_HASH_LEN + 1;
	if (need > buflen || need < prefixlen)
		goto fail;

	if (yescrypt_kdf32(yctx, passwd, passwdlen, hashbin32))
		goto fail;

	dst = mempcpy(buf, setting, prefixlen);
	*dst++ = '$';
	dst = encode64(dst, buflen - (dst - buf), hashbin32, sizeof(hashbin32));
	if (!dst)
		goto fail;
 ret:
	free_region(yctx->local);
	explicit_bzero(yctx, sizeof(yctx));
	explicit_bzero(hashbin32, sizeof(hashbin32));
	return buf;
 fail:
	buf = NULL;
	goto ret;
}
