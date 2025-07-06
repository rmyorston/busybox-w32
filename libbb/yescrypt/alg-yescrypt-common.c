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
 * decode64 fuinctions are only used to read
 * yescrypt_params_t field, and convert salt ti binary -
 * both of these are negligible compared to main hashing operation
 */
static NOINLINE uint32_t atoi64(uint8_t src)
{
	static const uint8_t atoi64_partial[77] = {
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
		64, 64, 64, 64, 64, 64, 64,
		12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
		25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37,
		64, 64, 64, 64, 64, 64,
		38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50,
		51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63
	};
	if (src >= '.' && src <= 'z')
		return atoi64_partial[src - '.'];
	return 64;
}

static NOINLINE const uint8_t *decode64_uint32(
		uint32_t *dst,
		const uint8_t *src, uint32_t val)
{
	uint32_t start = 0, end = 47, chars = 1, bits = 0;
	uint32_t c;

	if (!src) /* prevous decode failed already? */
		goto fail;

	c = atoi64(*src++);
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
		c = atoi64(*src++);
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

static uint8_t *encode64_uint32_fixed(
		uint8_t *dst, size_t dstlen,
		uint32_t src, uint32_t srcbits)
{
	uint32_t bits;

	for (bits = 0; bits < srcbits; bits += 6) {
		if (dstlen < 2)
			return NULL;
		*dst++ = itoa64[src & 0x3f];
		dstlen--;
		src >>= 6;
	}

	if (src || dstlen < 1)
		return NULL;

	*dst = 0; /* NUL terminate just in case */

	return dst;
}

static uint8_t *encode64(
		uint8_t *dst, size_t dstlen,
		const uint8_t *src, size_t srclen)
{
	size_t i;

	for (i = 0; i < srclen; ) {
		uint8_t *dnext;
		uint32_t value = 0, bits = 0;
		do {
			value |= (uint32_t)src[i++] << bits;
			bits += 8;
		} while (bits < 24 && i < srclen);
		dnext = encode64_uint32_fixed(dst, dstlen, value, bits);
		if (!dnext)
			return NULL;
		dstlen -= dnext - dst;
		dst = dnext;
	}

	if (dstlen < 1)
		return NULL;

	*dst = 0; /* NUL terminate just in case */

	return dst;
}

static const uint8_t *decode64(
		uint8_t *dst, size_t *dstlen,
		const uint8_t *src, size_t srclen)
{
	size_t dstpos = 0;

	while (dstpos <= *dstlen && srclen) {
		uint32_t value = 0, bits = 0;
		while (srclen--) {
			uint32_t c = atoi64(*src);
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
		return src;
	}

fail:
	*dstlen = 0;
	return NULL;
}

uint8_t *yescrypt_r(
		const uint8_t *passwd, size_t passwdlen,
		const uint8_t *setting,
		uint8_t *buf, size_t buflen)
{
	yescrypt_ctx_t yctx[1];
	unsigned char hashbin32[32];
	const uint8_t *src, *saltstr, *saltend;
	uint8_t *dst;
	size_t need, prefixlen, saltstrlen;
	uint32_t flavor, N_log2;

	memset(yctx, 0, sizeof(yctx));
	yctx->param.p = 1;

	/* we assume setting starts with "$y$" (caller must ensure this) */
	src = setting + 3;

	src = decode64_uint32(&flavor, src, 0);
	/* "j9T" returns: 0x2f */
	dbg("yescrypt flavor=0x%x YESCRYPT_RW:%u", (unsigned)flavor, !!(flavor & YESCRYPT_RW));
	//if (!src)
	//	goto fail;

	if (flavor < YESCRYPT_RW) {
		yctx->param.flags = flavor;
	} else if (flavor <= YESCRYPT_RW + (YESCRYPT_RW_FLAVOR_MASK >> 2)) {
		/* "j9T" sets flags to 0xb6 */
		yctx->param.flags = YESCRYPT_RW + ((flavor - YESCRYPT_RW) << 2);
		dbg("yctx->param.flags=0x%x", (unsigned)yctx->param.flags);
		dbg(" YESCRYPT_RW:%u"       , !!(yctx->param.flags & YESCRYPT_RW       ));
		dbg(" YESCRYPT_ROUNDS_6:%u" , !!(yctx->param.flags & YESCRYPT_ROUNDS_6 ));
		dbg(" YESCRYPT_GATHER_2:%u" , !!(yctx->param.flags & YESCRYPT_GATHER_2 ));
		dbg(" YESCRYPT_GATHER_4:%u" , !!(yctx->param.flags & YESCRYPT_GATHER_4 ));
		dbg(" YESCRYPT_GATHER_8:%u" , !!(yctx->param.flags & YESCRYPT_GATHER_8 ));
		dbg(" YESCRYPT_SIMPLE_2:%u" , !!(yctx->param.flags & YESCRYPT_SIMPLE_2 ));
		dbg(" YESCRYPT_SIMPLE_4:%u" , !!(yctx->param.flags & YESCRYPT_SIMPLE_4 ));
		dbg(" YESCRYPT_SIMPLE_8:%u" , !!(yctx->param.flags & YESCRYPT_SIMPLE_8 ));
		dbg(" YESCRYPT_SBOX_12K:%u" , !!(yctx->param.flags & YESCRYPT_SBOX_12K ));
		dbg(" YESCRYPT_SBOX_24K:%u" , !!(yctx->param.flags & YESCRYPT_SBOX_24K ));
		dbg(" YESCRYPT_SBOX_48K:%u" , !!(yctx->param.flags & YESCRYPT_SBOX_48K ));
		dbg(" YESCRYPT_SBOX_96K:%u" , !!(yctx->param.flags & YESCRYPT_SBOX_96K ));
		dbg(" YESCRYPT_SBOX_192K:%u", !!(yctx->param.flags & YESCRYPT_SBOX_192K));
		dbg(" YESCRYPT_SBOX_384K:%u", !!(yctx->param.flags & YESCRYPT_SBOX_384K));
		dbg(" YESCRYPT_SBOX_768K:%u", !!(yctx->param.flags & YESCRYPT_SBOX_768K));
	} else {
		goto fail;
	}

	src = decode64_uint32(&N_log2, src, 1);
	if (/*!src ||*/ N_log2 > 63)
		goto fail;
	yctx->param.N = (uint64_t)1 << N_log2;
	/* "j9T" sets to 4096 (1<<12) */
	dbg("yctx->param.N=%llu (1<<%u)", (unsigned long long)yctx->param.N, (unsigned)N_log2);

	src = decode64_uint32(&yctx->param.r, src, 1);
	/* "j9T" sets to 32 */
	dbg("yctx->param.r=%u", yctx->param.r);

	if (!src)
		goto fail;
	if (*src != '$') {
		uint32_t have;
		src = decode64_uint32(&have, src, 1);
		dbg("yescrypt has extended params:0x%x", (unsigned)have);
		if (have & 1)
			src = decode64_uint32(&yctx->param.p, src, 2);
		if (have & 2)
			src = decode64_uint32(&yctx->param.t, src, 1);
		if (have & 4)
			src = decode64_uint32(&yctx->param.g, src, 1);
		if (have & 8) {
			uint32_t NROM_log2;
			src = decode64_uint32(&NROM_log2, src, 1);
			if (/*!src ||*/ NROM_log2 > 63)
				goto fail;
			yctx->param.NROM = (uint64_t)1 << NROM_log2;
		}
	}
	if (!src)
		goto fail;
	if (*src != '$')
		goto fail;

	saltstr = src + 1;
	src = (uint8_t *)strchrnul((char *)saltstr, '$');
	prefixlen = src - setting;  /* len("$y$<params>$<salt>") */
	saltstrlen = src - saltstr; /* len("<salt>") */
	/* src points to end of salt ('$' or NUL byte), won't be used past this point */

	yctx->saltlen = sizeof(yctx->salt);
	saltend = decode64(yctx->salt, &yctx->saltlen, saltstr, saltstrlen);
	if (saltend != saltstr + saltstrlen)
		goto fail; /* salt[] is too small, or bad char during decode */

	need = prefixlen + 1 + HASH_LEN + 1;
	if (need > buflen || need < prefixlen)
		goto fail;

	if (yescrypt_kdf32(yctx, passwd, passwdlen, hashbin32))
		goto fail;

	dst = mempcpy(buf, setting, prefixlen);
	*dst++ = '$';
	dst = encode64(dst, buflen - (dst - buf), hashbin32, sizeof(hashbin32));
	if (!dst || dst >= buf + buflen)
		goto fail;

	*dst = 0; /* NUL termination */
 ret:
	free_region(yctx->local);
	explicit_bzero(yctx, sizeof(yctx));
	explicit_bzero(hashbin32, sizeof(hashbin32));
	return buf;
fail:
	buf = NULL;
	goto ret;
}
