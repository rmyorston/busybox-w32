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
		const uint8_t *src, uint32_t min)
{
	uint32_t start = 0, end = 47, chars = 1, bits = 0;
	uint32_t c;

	c = atoi64(*src++);
	if (c > 63)
		goto fail;

	*dst = min;
	while (c > end) {
		*dst += (end + 1 - start) << bits;
		start = end + 1;
		end = start + (62 - end) / 2;
		chars++;
		bits += 6;
	}

	*dst += (c - start) << bits;

	while (--chars) {
		c = atoi64(*src++);
		if (c > 63)
			goto fail;
		bits -= 6;
		*dst += c << bits;
	}

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
		yescrypt_local_t *local,
		const uint8_t *passwd, size_t passwdlen,
		const uint8_t *setting,
		uint8_t *buf, size_t buflen)
{
	unsigned char saltbin[64], hashbin[32];
	const uint8_t *src, *saltstr, *saltend;
	uint8_t *dst;
	size_t need, prefixlen, saltstrlen, saltlen;
	uint32_t flavor, N_log2;
	yescrypt_params_t params = { .p = 1 };

	/* we assume setting starts with "$y$" (caller must ensure this) */
	src = setting + 3;

	src = decode64_uint32(&flavor, src, 0);
	if (!src)
		return NULL;

	if (flavor < YESCRYPT_RW) {
		params.flags = flavor;
	} else if (flavor <= YESCRYPT_RW + (YESCRYPT_RW_FLAVOR_MASK >> 2)) {
		params.flags = YESCRYPT_RW + ((flavor - YESCRYPT_RW) << 2);
	} else {
		return NULL;
	}

	src = decode64_uint32(&N_log2, src, 1);
	if (!src || N_log2 > 63)
		return NULL;
	params.N = (uint64_t)1 << N_log2;

	src = decode64_uint32(&params.r, src, 1);
	if (!src)
		return NULL;

	if (*src != '$') {
		uint32_t have;

		src = decode64_uint32(&have, src, 1);
		if (!src)
			return NULL;

		if (have & 1) {
			src = decode64_uint32(&params.p, src, 2);
			if (!src)
				return NULL;
		}

		if (have & 2) {
			src = decode64_uint32(&params.t, src, 1);
			if (!src)
				return NULL;
		}

		if (have & 4) {
			src = decode64_uint32(&params.g, src, 1);
			if (!src)
				return NULL;
		}

		if (have & 8) {
			uint32_t NROM_log2;
			src = decode64_uint32(&NROM_log2, src, 1);
			if (!src || NROM_log2 > 63)
				return NULL;
			params.NROM = (uint64_t)1 << NROM_log2;
		}
	}

	if (*src++ != '$')
		return NULL;

	prefixlen = src - setting;

	saltstr = src;
	src = (uint8_t *)strrchr((char *)saltstr, '$');
	if (src)
		saltstrlen = src - saltstr;
	else
		saltstrlen = strlen((char *)saltstr);

	saltlen = sizeof(saltbin);
	saltend = decode64(saltbin, &saltlen, saltstr, saltstrlen);

	if (!saltend || (size_t)(saltend - saltstr) != saltstrlen)
		goto fail;

	need = prefixlen + saltstrlen + 1 + HASH_LEN + 1;
	if (need > buflen || need < saltstrlen)
		goto fail;

	if (yescrypt_kdf(local, passwd, passwdlen, saltbin, saltlen,
	    &params, hashbin, sizeof(hashbin)))
		goto fail;

	dst = mempcpy(buf, setting, prefixlen + saltstrlen);
	*dst++ = '$';

	dst = encode64(dst, buflen - (dst - buf), hashbin, sizeof(hashbin));
	explicit_bzero(hashbin, sizeof(hashbin));
	if (!dst || dst >= buf + buflen)
		return NULL;

	*dst = 0; /* NUL termination */

	return buf;

fail:
	explicit_bzero(saltbin, sizeof(saltbin));
	explicit_bzero(hashbin, sizeof(hashbin));
	return NULL;
}

int yescrypt_init_local(yescrypt_local_t *local)
{
	init_region(local);
	return 0;
}

int yescrypt_free_local(yescrypt_local_t *local)
{
	return free_region(local);
}
