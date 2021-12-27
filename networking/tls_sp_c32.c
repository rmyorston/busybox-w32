/*
 * Copyright (C) 2021 Denys Vlasenko
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
#include "tls.h"

#define SP_DEBUG          0
#define FIXED_SECRET      0
#define FIXED_PEER_PUBKEY 0

#define ALLOW_ASM         1

#if SP_DEBUG
# define dbg(...) fprintf(stderr, __VA_ARGS__)
static void dump_hex(const char *fmt, const void *vp, int len)
{
	char hexbuf[32 * 1024 + 4];
	const uint8_t *p = vp;

	bin2hex(hexbuf, (void*)p, len)[0] = '\0';
	dbg(fmt, hexbuf);
}
#else
# define dbg(...) ((void)0)
# define dump_hex(...) ((void)0)
#endif

typedef uint32_t sp_digit;
typedef int32_t signed_sp_digit;

/* The code below is taken from parts of
 *  wolfssl-3.15.3/wolfcrypt/src/sp_c32.c
 * and heavily modified.
 */

typedef struct sp_point {
	sp_digit x[2 * 8];
	sp_digit y[2 * 8];
	sp_digit z[2 * 8];
	int infinity;
} sp_point;

/* The modulus (prime) of the curve P256. */
static const sp_digit p256_mod[8] = {
	0xffffffff,0xffffffff,0xffffffff,0x00000000,
	0x00000000,0x00000000,0x00000001,0xffffffff,
};

#define p256_mp_mod ((sp_digit)0x000001)

/* Normalize the values in each word to 32 bits - NOP */
#define sp_256_norm_8(a) ((void)0)

/* Write r as big endian to byte array.
 * Fixed length number of bytes written: 32
 *
 * r  A single precision integer.
 * a  Byte array.
 */
static void sp_256_to_bin_8(const sp_digit* r, uint8_t* a)
{
	int i;

	sp_256_norm_8(r);

	r += 8;
	for (i = 0; i < 8; i++) {
		r--;
		move_to_unaligned32(a, SWAP_BE32(*r));
		a += 4;
	}
}

/* Read big endian unsigned byte array into r.
 *
 * r  A single precision integer.
 * a  Byte array.
 * n  Number of bytes in array to read.
 */
static void sp_256_from_bin_8(sp_digit* r, const uint8_t* a)
{
	int i;

	r += 8;
	for (i = 0; i < 8; i++) {
		sp_digit v;
		move_from_unaligned32(v, a);
		*--r = SWAP_BE32(v);
		a += 4;
	}
}

#if SP_DEBUG
static void dump_256(const char *fmt, const sp_digit* r)
{
	uint8_t b32[32];
	sp_256_to_bin_8(r, b32);
	dump_hex(fmt, b32, 32);
}
static void dump_512(const char *fmt, const sp_digit* r)
{
	uint8_t b64[64];
	sp_256_to_bin_8(r, b64 + 32);
	sp_256_to_bin_8(r+8, b64);
	dump_hex(fmt, b64, 64);
}
#else
# define dump_256(...) ((void)0)
# define dump_512(...) ((void)0)
#endif

/* Convert a point of big-endian 32-byte x,y pair to type sp_point. */
static void sp_256_point_from_bin2x32(sp_point* p, const uint8_t *bin2x32)
{
	memset(p, 0, sizeof(*p));
	/*p->infinity = 0;*/
	sp_256_from_bin_8(p->x, bin2x32);
	sp_256_from_bin_8(p->y, bin2x32 + 32);
	p->z[0] = 1; /* p->z = 1 */
}

/* Compare a with b.
 *
 * return -ve, 0 or +ve if a is less than, equal to or greater than b
 * respectively.
 */
static signed_sp_digit sp_256_cmp_8(const sp_digit* a, const sp_digit* b)
{
	int i;
	for (i = 7; i >= 0; i--) {
/*		signed_sp_digit r = a[i] - b[i];
 *		if (r != 0)
 *			return r;
 * does not work: think about a[i]=0, b[i]=0xffffffff
 */
		if (a[i] == b[i])
			continue;
		return (a[i] > b[i]) * 2 - 1;
	}
	return 0;
}

/* Compare two numbers to determine if they are equal.
 *
 * return 1 when equal and 0 otherwise.
 */
static int sp_256_cmp_equal_8(const sp_digit* a, const sp_digit* b)
{
	return sp_256_cmp_8(a, b) == 0;
}

/* Add b to a into r. (r = a + b). Return !0 on overflow */
static int sp_256_add_8(sp_digit* r, const sp_digit* a, const sp_digit* b)
{
#if ALLOW_ASM && defined(__GNUC__) && defined(__i386__)
	sp_digit reg;
	asm volatile (
"\n		movl	(%0), %3"
"\n		addl	(%1), %3"
"\n		movl	%3, (%2)"
"\n"
"\n		movl	1*4(%0), %3"
"\n		adcl	1*4(%1), %3"
"\n		movl	%3, 1*4(%2)"
"\n"
"\n		movl	2*4(%0), %3"
"\n		adcl	2*4(%1), %3"
"\n		movl	%3, 2*4(%2)"
"\n"
"\n		movl	3*4(%0), %3"
"\n		adcl	3*4(%1), %3"
"\n		movl	%3, 3*4(%2)"
"\n"
"\n		movl	4*4(%0), %3"
"\n		adcl	4*4(%1), %3"
"\n		movl	%3, 4*4(%2)"
"\n"
"\n		movl	5*4(%0), %3"
"\n		adcl	5*4(%1), %3"
"\n		movl	%3, 5*4(%2)"
"\n"
"\n		movl	6*4(%0), %3"
"\n		adcl	6*4(%1), %3"
"\n		movl	%3, 6*4(%2)"
"\n"
"\n		movl	7*4(%0), %3"
"\n		adcl	7*4(%1), %3"
"\n		movl	%3, 7*4(%2)"
"\n"
"\n		sbbl	%3, %3"
"\n"
		: "=r" (a), "=r" (b), "=r" (r), "=r" (reg)
		: "0" (a), "1" (b), "2" (r)
		: "memory"
	);
	return reg;
#elif ALLOW_ASM && defined(__GNUC__) && defined(__x86_64__)
	/* x86_64 has no alignment restrictions, and is little-endian,
	 * so 64-bit and 32-bit representations are identical */
	uint64_t reg;
	asm volatile (
"\n		movq	(%0), %3"
"\n		addq	(%1), %3"
"\n		movq	%3, (%2)"
"\n"
"\n		movq	1*8(%0), %3"
"\n		adcq	1*8(%1), %3"
"\n		movq	%3, 1*8(%2)"
"\n"
"\n		movq	2*8(%0), %3"
"\n		adcq	2*8(%1), %3"
"\n		movq	%3, 2*8(%2)"
"\n"
"\n		movq	3*8(%0), %3"
"\n		adcq	3*8(%1), %3"
"\n		movq	%3, 3*8(%2)"
"\n"
"\n		sbbq	%3, %3"
"\n"
		: "=r" (a), "=r" (b), "=r" (r), "=r" (reg)
		: "0" (a), "1" (b), "2" (r)
		: "memory"
	);
	return reg;
#else
	int i;
	sp_digit carry;

	carry = 0;
	for (i = 0; i < 8; i++) {
		sp_digit w, v;
		w = b[i] + carry;
		v = a[i];
		if (w != 0) {
			v = a[i] + w;
			carry = (v < a[i]);
			/* hope compiler detects above as "carry flag set" */
		}
		/* else: b + carry == 0, two cases:
		 * b:ffffffff, carry:1
		 * b:00000000, carry:0
		 * in either case, r[i] = a[i] and carry remains unchanged
		 */
		r[i] = v;
	}
	return carry;
#endif
}

/* Sub b from a into r. (r = a - b). Return !0 on underflow */
static int sp_256_sub_8(sp_digit* r, const sp_digit* a, const sp_digit* b)
{
#if ALLOW_ASM && defined(__GNUC__) && defined(__i386__)
	sp_digit reg;
	asm volatile (
"\n		movl	(%0), %3"
"\n		subl	(%1), %3"
"\n		movl	%3, (%2)"
"\n"
"\n		movl	1*4(%0), %3"
"\n		sbbl	1*4(%1), %3"
"\n		movl	%3, 1*4(%2)"
"\n"
"\n		movl	2*4(%0), %3"
"\n		sbbl	2*4(%1), %3"
"\n		movl	%3, 2*4(%2)"
"\n"
"\n		movl	3*4(%0), %3"
"\n		sbbl	3*4(%1), %3"
"\n		movl	%3, 3*4(%2)"
"\n"
"\n		movl	4*4(%0), %3"
"\n		sbbl	4*4(%1), %3"
"\n		movl	%3, 4*4(%2)"
"\n"
"\n		movl	5*4(%0), %3"
"\n		sbbl	5*4(%1), %3"
"\n		movl	%3, 5*4(%2)"
"\n"
"\n		movl	6*4(%0), %3"
"\n		sbbl	6*4(%1), %3"
"\n		movl	%3, 6*4(%2)"
"\n"
"\n		movl	7*4(%0), %3"
"\n		sbbl	7*4(%1), %3"
"\n		movl	%3, 7*4(%2)"
"\n"
"\n		sbbl	%3, %3"
"\n"
		: "=r" (a), "=r" (b), "=r" (r), "=r" (reg)
		: "0" (a), "1" (b), "2" (r)
		: "memory"
	);
	return reg;
#elif ALLOW_ASM && defined(__GNUC__) && defined(__x86_64__)
	/* x86_64 has no alignment restrictions, and is little-endian,
	 * so 64-bit and 32-bit representations are identical */
	uint64_t reg;
	asm volatile (
"\n		movq	(%0), %3"
"\n		subq	(%1), %3"
"\n		movq	%3, (%2)"
"\n"
"\n		movq	1*8(%0), %3"
"\n		sbbq	1*8(%1), %3"
"\n		movq	%3, 1*8(%2)"
"\n"
"\n		movq	2*8(%0), %3"
"\n		sbbq	2*8(%1), %3"
"\n		movq	%3, 2*8(%2)"
"\n"
"\n		movq	3*8(%0), %3"
"\n		sbbq	3*8(%1), %3"
"\n		movq	%3, 3*8(%2)"
"\n"
"\n		sbbq	%3, %3"
"\n"
		: "=r" (a), "=r" (b), "=r" (r), "=r" (reg)
		: "0" (a), "1" (b), "2" (r)
		: "memory"
	);
	return reg;
#else
	int i;
	sp_digit borrow;

	borrow = 0;
	for (i = 0; i < 8; i++) {
		sp_digit w, v;
		w = b[i] + borrow;
		v = a[i];
		if (w != 0) {
			v = a[i] - w;
			borrow = (v > a[i]);
			/* hope compiler detects above as "carry flag set" */
		}
		/* else: b + borrow == 0, two cases:
		 * b:ffffffff, borrow:1
		 * b:00000000, borrow:0
		 * in either case, r[i] = a[i] and borrow remains unchanged
		 */
		r[i] = v;
	}
	return borrow;
#endif
}

/* Sub p256_mod from r. (r = r - p256_mod). */
#if ALLOW_ASM && defined(__GNUC__) && defined(__i386__)
static void sp_256_sub_8_p256_mod(sp_digit* r)
{
//p256_mod[7..0] = ffffffff 00000001 00000000 00000000 00000000 ffffffff ffffffff ffffffff
	asm volatile (
"\n		subl	$0xffffffff, (%0)"
"\n		sbbl	$0xffffffff, 1*4(%0)"
"\n		sbbl	$0xffffffff, 2*4(%0)"
"\n		sbbl	$0, 3*4(%0)"
"\n		sbbl	$0, 4*4(%0)"
"\n		sbbl	$0, 5*4(%0)"
"\n		sbbl	$1, 6*4(%0)"
"\n		sbbl	$0xffffffff, 7*4(%0)"
"\n"
		: "=r" (r)
		: "0" (r)
		: "memory"
	);
}
#elif ALLOW_ASM && defined(__GNUC__) && defined(__x86_64__)
static void sp_256_sub_8_p256_mod(sp_digit* r)
{
	uint64_t reg;
	uint64_t ooff;
//p256_mod[3..0] = ffffffff00000001 0000000000000000 00000000ffffffff ffffffffffffffff
	asm volatile (
"\n		addq	$1, (%0)"	// adding 1 is the same as subtracting ffffffffffffffff
"\n		cmc"			// only carry bit needs inverting
"\n"
"\n		sbbq	%1, 1*8(%0)"	// %1 holds 00000000ffffffff
"\n"
"\n		sbbq	$0, 2*8(%0)"
"\n"
"\n		movq	3*8(%0), %2"
"\n		sbbq	$0, %2"		// adding 00000000ffffffff (in %1)
"\n		addq	%1, %2"		// is the same as subtracting ffffffff00000001
"\n		movq	%2, 3*8(%0)"
"\n"
		: "=r" (r), "=r" (ooff), "=r" (reg)
		: "0" (r), "1" (0x00000000ffffffff)
		: "memory"
	);
}
#else
static void sp_256_sub_8_p256_mod(sp_digit* r)
{
	sp_256_sub_8(r, r, p256_mod);
}
#endif

/* Multiply a and b into r. (r = a * b) */
static void sp_256_mul_8(sp_digit* r, const sp_digit* a, const sp_digit* b)
{
#if ALLOW_ASM && defined(__GNUC__) && defined(__i386__)
	sp_digit rr[15]; /* in case r coincides with a or b */
	int k;
	uint32_t accl;
	uint32_t acch;

	acch = accl = 0;
	for (k = 0; k < 15; k++) {
		int i, j;
		uint32_t acc_hi;
		i = k - 7;
		if (i < 0)
			i = 0;
		j = k - i;
		acc_hi = 0;
		do {
////////////////////////
//			uint64_t m = ((uint64_t)a[i]) * b[j];
//			acc_hi:acch:accl += m;
			asm volatile (
			// a[i] is already loaded in %%eax
"\n			mull	%7"
"\n			addl	%%eax, %0"
"\n			adcl	%%edx, %1"
"\n			adcl	$0, %2"
			: "=rm" (accl), "=rm" (acch), "=rm" (acc_hi)
			: "0" (accl), "1" (acch), "2" (acc_hi), "a" (a[i]), "m" (b[j])
			: "cc", "dx"
			);
////////////////////////
		        j--;
			i++;
		} while (i != 8 && i <= k);
		rr[k] = accl;
		accl = acch;
		acch = acc_hi;
	}
	r[15] = accl;
	memcpy(r, rr, sizeof(rr));
#elif ALLOW_ASM && defined(__GNUC__) && defined(__x86_64__)
	/* x86_64 has no alignment restrictions, and is little-endian,
	 * so 64-bit and 32-bit representations are identical */
	const uint64_t* aa = (const void*)a;
	const uint64_t* bb = (const void*)b;
	uint64_t rr[8];
	int k;
	uint64_t accl;
	uint64_t acch;

	acch = accl = 0;
	for (k = 0; k < 7; k++) {
		int i, j;
		uint64_t acc_hi;
		i = k - 3;
		if (i < 0)
			i = 0;
		j = k - i;
		acc_hi = 0;
		do {
////////////////////////
//			uint128_t m = ((uint128_t)a[i]) * b[j];
//			acc_hi:acch:accl += m;
			asm volatile (
			// aa[i] is already loaded in %%rax
"\n			mulq	%7"
"\n			addq	%%rax, %0"
"\n			adcq	%%rdx, %1"
"\n			adcq	$0, %2"
			: "=rm" (accl), "=rm" (acch), "=rm" (acc_hi)
			: "0" (accl), "1" (acch), "2" (acc_hi), "a" (aa[i]), "m" (bb[j])
			: "cc", "dx"
			);
////////////////////////
			j--;
			i++;
		} while (i != 4 && i <= k);
		rr[k] = accl;
		accl = acch;
		acch = acc_hi;
	}
	rr[7] = accl;
	memcpy(r, rr, sizeof(rr));
#elif 0
	//TODO: arm assembly (untested)
	sp_digit tmp[16];

	asm volatile (
"\n		mov	r5, #0"
"\n		mov	r6, #0"
"\n		mov	r7, #0"
"\n		mov	r8, #0"
"\n	1:"
"\n		subs	r3, r5, #28"
"\n		movcc	r3, #0"
"\n		sub	r4, r5, r3"
"\n	2:"
"\n		ldr	r14, [%[a], r3]"
"\n		ldr	r12, [%[b], r4]"
"\n		umull	r9, r10, r14, r12"
"\n		adds	r6, r6, r9"
"\n		adcs	r7, r7, r10"
"\n		adc	r8, r8, #0"
"\n		add	r3, r3, #4"
"\n		sub	r4, r4, #4"
"\n		cmp	r3, #32"
"\n		beq	3f"
"\n		cmp	r3, r5"
"\n		ble	2b"
"\n	3:"
"\n		str	r6, [%[r], r5]"
"\n		mov	r6, r7"
"\n		mov	r7, r8"
"\n		mov	r8, #0"
"\n		add	r5, r5, #4"
"\n		cmp	r5, #56"
"\n		ble	1b"
"\n		str	r6, [%[r], r5]"
		: [r] "r" (tmp), [a] "r" (a), [b] "r" (b)
		: "memory", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "r12", "r14"
	);
	memcpy(r, tmp, sizeof(tmp));
#else
	sp_digit rr[15]; /* in case r coincides with a or b */
	int i, j, k;
	uint64_t acc;

	acc = 0;
	for (k = 0; k < 15; k++) {
		uint32_t acc_hi;
		i = k - 7;
		if (i < 0)
			i = 0;
		j = k - i;
		acc_hi = 0;
		do {
			uint64_t m = ((uint64_t)a[i]) * b[j];
			acc += m;
			if (acc < m)
				acc_hi++;
		        j--;
			i++;
		} while (i != 8 && i <= k);
		rr[k] = acc;
		acc = (acc >> 32) | ((uint64_t)acc_hi << 32);
	}
	r[15] = acc;
	memcpy(r, rr, sizeof(rr));
#endif
}

/* Shift number right one bit. Bottom bit is lost. */
static void sp_256_rshift1_8(sp_digit* r, sp_digit* a, sp_digit carry)
{
	int i;

	carry = (!!carry << 31);
	for (i = 7; i >= 0; i--) {
		sp_digit c = a[i] << 31;
		r[i] = (a[i] >> 1) | carry;
		carry = c;
	}
}

/* Divide the number by 2 mod the modulus (prime). (r = a / 2 % m) */
static void sp_256_div2_8(sp_digit* r, const sp_digit* a, const sp_digit* m)
{
	int carry = 0;
	if (a[0] & 1)
		carry = sp_256_add_8(r, a, m);
	sp_256_norm_8(r);
	sp_256_rshift1_8(r, r, carry);
}

/* Add two Montgomery form numbers (r = a + b % m) */
static void sp_256_mont_add_8(sp_digit* r, const sp_digit* a, const sp_digit* b
		/*, const sp_digit* m*/)
{
//	const sp_digit* m = p256_mod;

	int carry = sp_256_add_8(r, a, b);
	sp_256_norm_8(r);
	if (carry) {
		sp_256_sub_8_p256_mod(r);
		sp_256_norm_8(r);
	}
}

/* Subtract two Montgomery form numbers (r = a - b % m) */
static void sp_256_mont_sub_8(sp_digit* r, const sp_digit* a, const sp_digit* b
		/*, const sp_digit* m*/)
{
	const sp_digit* m = p256_mod;

	int borrow;
	borrow = sp_256_sub_8(r, a, b);
	sp_256_norm_8(r);
	if (borrow) {
		sp_256_add_8(r, r, m);
		sp_256_norm_8(r);
	}
}

/* Double a Montgomery form number (r = a + a % m) */
static void sp_256_mont_dbl_8(sp_digit* r, const sp_digit* a /*, const sp_digit* m*/)
{
//	const sp_digit* m = p256_mod;

	int carry = sp_256_add_8(r, a, a);
	sp_256_norm_8(r);
	if (carry)
		sp_256_sub_8_p256_mod(r);
	sp_256_norm_8(r);
}

/* Triple a Montgomery form number (r = a + a + a % m) */
static void sp_256_mont_tpl_8(sp_digit* r, const sp_digit* a /*, const sp_digit* m*/)
{
//	const sp_digit* m = p256_mod;

	int carry = sp_256_add_8(r, a, a);
	sp_256_norm_8(r);
	if (carry) {
		sp_256_sub_8_p256_mod(r);
		sp_256_norm_8(r);
	}
	carry = sp_256_add_8(r, r, a);
	sp_256_norm_8(r);
	if (carry) {
		sp_256_sub_8_p256_mod(r);
		sp_256_norm_8(r);
	}
}

/* Shift the result in the high 256 bits down to the bottom. */
static void sp_256_mont_shift_8(sp_digit* r, const sp_digit* a)
{
	int i;

	for (i = 0; i < 8; i++) {
		r[i] = a[i+8];
		r[i+8] = 0;
	}
}

/* Mul a by scalar b and add into r. (r += a * b) */
static int sp_256_mul_add_8(sp_digit* r /*, const sp_digit* a, sp_digit b*/)
{
//	const sp_digit* a = p256_mod;
//a[7..0] = ffffffff 00000001 00000000 00000000 00000000 ffffffff ffffffff ffffffff
	sp_digit b = r[0];

	uint64_t t;

//	t = 0;
//	for (i = 0; i < 8; i++) {
//		uint32_t t_hi;
//		uint64_t m = ((uint64_t)b * a[i]) + r[i];
//		t += m;
//		t_hi = (t < m);
//		r[i] = (sp_digit)t;
//		t = (t >> 32) | ((uint64_t)t_hi << 32);
//	}
//	r[8] += (sp_digit)t;

	// Unroll, then optimize the above loop:
		//uint32_t t_hi;
		uint64_t m;
		uint32_t t32;

		//m = ((uint64_t)b * a[0]) + r[0];
		//  Since b is r[0] and a[0] is ffffffff, the above optimizes to:
		//  m = r[0] * ffffffff + r[0] = (r[0] * 100000000 - r[0]) + r[0] = r[0] << 32;
		//t += m;
		//  t = r[0] << 32 = b << 32;
		//t_hi = (t < m);
		//  t_hi = 0;
		//r[0] = (sp_digit)t;
		r[0] = 0;
		//t = (t >> 32) | ((uint64_t)t_hi << 32);
		//  t = b;

		//m = ((uint64_t)b * a[1]) + r[1];
		//  Since a[1] is ffffffff, the above optimizes to:
		//  m = b * ffffffff + r[1] = (b * 100000000 - b) + r[1] = (b << 32) - b + r[1];
		//t += m;
		//  t = b + (b << 32) - b + r[1] = (b << 32) + r[1];
		//t_hi = (t < m);
		//  t_hi = 0;
		//r[1] = (sp_digit)t;
		//  r[1] = r[1];
		//t = (t >> 32) | ((uint64_t)t_hi << 32);
		//  t = b;

		//m = ((uint64_t)b * a[2]) + r[2];
		//  Since a[2] is ffffffff, the above optimizes to:
		//  m = b * ffffffff + r[2] = (b * 100000000 - b) + r[2] = (b << 32) - b + r[2];
		//t += m;
		//  t = b + (b << 32) - b + r[2] = (b << 32) + r[2]
		//t_hi = (t < m);
		//  t_hi = 0;
		//r[2] = (sp_digit)t;
		//  r[2] = r[2];
		//t = (t >> 32) | ((uint64_t)t_hi << 32);
		//  t = b;

		//m = ((uint64_t)b * a[3]) + r[3];
		//  Since a[3] is 00000000, the above optimizes to:
		//  m = b * 0 + r[3] = r[3];
		//t += m;
		//  t = b + r[3];
		//t_hi = (t < m);
		//  t_hi = 0;
		//r[3] = (sp_digit)t;
		r[3] = r[3] + b;
		//t = (t >> 32) | ((uint64_t)t_hi << 32);
		t32 = (r[3] < b); // 0 or 1

		//m = ((uint64_t)b * a[4]) + r[4];
		//  Since a[4] is 00000000, the above optimizes to:
		//  m = b * 0 + r[4] = r[4];
		//t += m;
		//  t = t32 + r[4];
		//t_hi = (t < m);
		//  t_hi = 0;
		//r[4] = (sp_digit)t;
		//t = (t >> 32) | ((uint64_t)t_hi << 32);
		if (t32 != 0) {
			r[4]++;
			t32 = (r[4] == 0); // 0 or 1

		//m = ((uint64_t)b * a[5]) + r[5];
		//  Since a[5] is 00000000, the above optimizes to:
		//  m = b * 0 + r[5] = r[5];
		//t += m;
		//  t = t32 + r[5]; (t32 is 0 or 1)
		//t_hi = (t < m);
		//  t_hi = 0;
		//r[5] = (sp_digit)t;
		//t = (t >> 32) | ((uint64_t)t_hi << 32);
			if (t32 != 0) {
				r[5]++;
				t32 = (r[5] == 0); // 0 or 1
			}
		}

		//m = ((uint64_t)b * a[6]) + r[6];
		//  Since a[6] is 00000001, the above optimizes to:
		//  m = (uint64_t)b + r[6]; // 33 bits at most
		//t += m;
		t = t32 + (uint64_t)b + r[6];
		//t_hi = (t < m);
		//  t_hi = 0;
		r[6] = (sp_digit)t;
		//t = (t >> 32) | ((uint64_t)t_hi << 32);
		t = (t >> 32);

		//m = ((uint64_t)b * a[7]) + r[7];
		//  Since a[7] is ffffffff, the above optimizes to:
		//  m = b * ffffffff + r[7] = (b * 100000000 - b) + r[7]
		m = ((uint64_t)b << 32) - b + r[7];
		t += m;
		//t_hi = (t < m);
		//  t_hi in fact is always 0 here (256bit * 32bit can't have more than 32 bits of overflow)
		r[7] = (sp_digit)t;
		//t = (t >> 32) | ((uint64_t)t_hi << 32);
		t = (t >> 32);

	r[8] += (sp_digit)t;
	return (r[8] < (sp_digit)t); /* 1 if addition overflowed */
}

/* Reduce the number back to 256 bits using Montgomery reduction.
 *
 * a   A single precision number to reduce in place.
 * m   The single precision number representing the modulus.
 * mp  The digit representing the negative inverse of m mod 2^n.
 */
static void sp_256_mont_reduce_8(sp_digit* a/*, const sp_digit* m, sp_digit mp*/)
{
//	const sp_digit* m = p256_mod;
	sp_digit mp = p256_mp_mod;

	int i;
//	sp_digit mu;

	if (mp != 1) {
		sp_digit word16th = 0;
		for (i = 0; i < 8; i++) {
//			mu = (sp_digit)(a[i] * mp);
			if (sp_256_mul_add_8(a+i /*, m, mu*/)) {
				int j = i + 8;
 inc_next_word0:
				if (++j > 15) { /* a[16] array has no more words? */
					word16th++;
					continue;
				}
				if (++a[j] == 0) /* did this overflow too? */
					goto inc_next_word0;
			}
		}
		sp_256_mont_shift_8(a, a);
		if (word16th != 0)
			sp_256_sub_8_p256_mod(a);
		sp_256_norm_8(a);
	}
	else { /* Same code for explicit mp == 1 (which is always the case for P256) */
		sp_digit word16th = 0;
		for (i = 0; i < 8; i++) {
			/*mu = a[i];*/
			if (sp_256_mul_add_8(a+i /*, m, mu*/)) {
				int j = i + 8;
 inc_next_word:
				if (++j > 15) { /* a[16] array has no more words? */
					word16th++;
					continue;
				}
				if (++a[j] == 0) /* did this overflow too? */
					goto inc_next_word;
			}
		}
		sp_256_mont_shift_8(a, a);
		if (word16th != 0)
			sp_256_sub_8_p256_mod(a);
		sp_256_norm_8(a);
	}
}
#if 0
//TODO: arm32 asm (also adapt for x86?)
static void sp_256_mont_reduce_8(sp_digit* a, sp_digit* m, sp_digit mp)
{
	sp_digit ca = 0;

	asm volatile (
	# i = 0
	mov	r12, #0
	ldr	r10, [%[a], #0]
	ldr	r14, [%[a], #4]
1:
	# mu = a[i] * mp
	mul	r8, %[mp], r10
	# a[i+0] += m[0] * mu
	ldr	r7, [%[m], #0]
	ldr	r9, [%[a], #0]
	umull	r6, r7, r8, r7
	adds	r10, r10, r6
	adc	r5, r7, #0
	# a[i+1] += m[1] * mu
	ldr	r7, [%[m], #4]
	ldr	r9, [%[a], #4]
	umull	r6, r7, r8, r7
	adds	r10, r14, r6
	adc	r4, r7, #0
	adds	r10, r10, r5
	adc	r4, r4, #0
	# a[i+2] += m[2] * mu
	ldr	r7, [%[m], #8]
	ldr	r14, [%[a], #8]
	umull	r6, r7, r8, r7
	adds	r14, r14, r6
	adc	r5, r7, #0
	adds	r14, r14, r4
	adc	r5, r5, #0
	# a[i+3] += m[3] * mu
	ldr	r7, [%[m], #12]
	ldr	r9, [%[a], #12]
	umull	r6, r7, r8, r7
	adds	r9, r9, r6
	adc	r4, r7, #0
	adds	r9, r9, r5
	str	r9, [%[a], #12]
	adc	r4, r4, #0
	# a[i+4] += m[4] * mu
	ldr	r7, [%[m], #16]
	ldr	r9, [%[a], #16]
	umull	r6, r7, r8, r7
	adds	r9, r9, r6
	adc	r5, r7, #0
	adds	r9, r9, r4
	str	r9, [%[a], #16]
	adc	r5, r5, #0
	# a[i+5] += m[5] * mu
	ldr	r7, [%[m], #20]
	ldr	r9, [%[a], #20]
	umull	r6, r7, r8, r7
	adds	r9, r9, r6
	adc	r4, r7, #0
	adds	r9, r9, r5
	str	r9, [%[a], #20]
	adc	r4, r4, #0
	# a[i+6] += m[6] * mu
	ldr	r7, [%[m], #24]
	ldr	r9, [%[a], #24]
	umull	r6, r7, r8, r7
	adds	r9, r9, r6
	adc	r5, r7, #0
	adds	r9, r9, r4
	str	r9, [%[a], #24]
	adc	r5, r5, #0
	# a[i+7] += m[7] * mu
	ldr	r7, [%[m], #28]
	ldr	r9, [%[a], #28]
	umull	r6, r7, r8, r7
	adds	r5, r5, r6
	adcs	r7, r7, %[ca]
	mov	%[ca], #0
	adc	%[ca], %[ca], %[ca]
	adds	r9, r9, r5
	str	r9, [%[a], #28]
	ldr	r9, [%[a], #32]
	adcs	r9, r9, r7
	str	r9, [%[a], #32]
	adc	%[ca], %[ca], #0
	# i += 1
	add	%[a], %[a], #4
	add	r12, r12, #4
	cmp	r12, #32
	blt	1b

	str	r10, [%[a], #0]
	str	r14, [%[a], #4]
	: [ca] "+r" (ca), [a] "+r" (a)
	: [m] "r" (m), [mp] "r" (mp)
	: "memory", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "r12", "r14"
	);

	memcpy(a, a + 8, 32);
	if (ca)
		a -= m;
}
#endif

/* Multiply two Montogmery form numbers mod the modulus (prime).
 * (r = a * b mod m)
 *
 * r   Result of multiplication.
 * a   First number to multiply in Montogmery form.
 * b   Second number to multiply in Montogmery form.
 * m   Modulus (prime).
 * mp  Montogmery mulitplier.
 */
static void sp_256_mont_mul_8(sp_digit* r, const sp_digit* a, const sp_digit* b
		/*, const sp_digit* m, sp_digit mp*/)
{
	//const sp_digit* m = p256_mod;
	//sp_digit mp = p256_mp_mod;
	sp_256_mul_8(r, a, b);
	sp_256_mont_reduce_8(r /*, m, mp*/);
}

/* Square the Montgomery form number. (r = a * a mod m)
 *
 * r   Result of squaring.
 * a   Number to square in Montogmery form.
 * m   Modulus (prime).
 * mp  Montogmery mulitplier.
 */
static void sp_256_mont_sqr_8(sp_digit* r, const sp_digit* a
		/*, const sp_digit* m, sp_digit mp*/)
{
	//const sp_digit* m = p256_mod;
	//sp_digit mp = p256_mp_mod;
	sp_256_mont_mul_8(r, a, a /*, m, mp*/);
}

/* Invert the number, in Montgomery form, modulo the modulus (prime) of the
 * P256 curve. (r = 1 / a mod m)
 *
 * r   Inverse result.
 * a   Number to invert.
 */
#if 0
/* Mod-2 for the P256 curve. */
static const uint32_t p256_mod_2[8] = {
	0xfffffffd,0xffffffff,0xffffffff,0x00000000,
	0x00000000,0x00000000,0x00000001,0xffffffff,
};
//Bit pattern:
//2    2         2         2         2         2         2         1...1
//5    5         4         3         2         1         0         9...0         9...1
//543210987654321098765432109876543210987654321098765432109876543210...09876543210...09876543210
//111111111111111111111111111111110000000000000000000000000000000100...00000111111...11111111101
#endif
static void sp_256_mont_inv_8(sp_digit* r, sp_digit* a)
{
	sp_digit t[2*8]; //can be just [8]?
	int i;

	memcpy(t, a, sizeof(sp_digit) * 8);
	for (i = 254; i >= 0; i--) {
		sp_256_mont_sqr_8(t, t /*, p256_mod, p256_mp_mod*/);
		/*if (p256_mod_2[i / 32] & ((sp_digit)1 << (i % 32)))*/
		if (i >= 224 || i == 192 || (i <= 95 && i != 1))
			sp_256_mont_mul_8(t, t, a /*, p256_mod, p256_mp_mod*/);
	}
	memcpy(r, t, sizeof(sp_digit) * 8);
}

/* Multiply a number by Montogmery normalizer mod modulus (prime).
 *
 * r  The resulting Montgomery form number.
 * a  The number to convert.
 */
static void sp_256_mod_mul_norm_8(sp_digit* r, const sp_digit* a)
{
	int64_t t[8];
	int32_t o;

#define A(n) ((uint64_t)a[n])
	/*  1  1  0 -1 -1 -1 -1  0 */
	t[0] = 0 + A(0) + A(1) - A(3) - A(4) - A(5) - A(6);
	/*  0  1  1  0 -1 -1 -1 -1 */
	t[1] = 0 + A(1) + A(2) - A(4) - A(5) - A(6) - A(7);
	/*  0  0  1  1  0 -1 -1 -1 */
	t[2] = 0 + A(2) + A(3) - A(5) - A(6) - A(7);
	/* -1 -1  0  2  2  1  0 -1 */
	t[3] = 0 - A(0) - A(1) + 2 * A(3) + 2 * A(4) + A(5) - A(7);
	/*  0 -1 -1  0  2  2  1  0 */
	t[4] = 0 - A(1) - A(2) + 2 * A(4) + 2 * A(5) + A(6);
	/*  0  0 -1 -1  0  2  2  1 */
	t[5] = 0 - A(2) - A(3) + 2 * A(5) + 2 * A(6) + A(7);
	/* -1 -1  0  0  0  1  3  2 */
	t[6] = 0 - A(0) - A(1) + A(5) + 3 * A(6) + 2 * A(7);
	/*  1  0 -1 -1 -1 -1  0  3 */
	t[7] = 0 + A(0) - A(2) - A(3) - A(4) - A(5) + 3 * A(7);
#undef A

	t[1] += t[0] >> 32; t[0] &= 0xffffffff;
	t[2] += t[1] >> 32; t[1] &= 0xffffffff;
	t[3] += t[2] >> 32; t[2] &= 0xffffffff;
	t[4] += t[3] >> 32; t[3] &= 0xffffffff;
	t[5] += t[4] >> 32; t[4] &= 0xffffffff;
	t[6] += t[5] >> 32; t[5] &= 0xffffffff;
	t[7] += t[6] >> 32; t[6] &= 0xffffffff;
	o     = t[7] >> 32; //t[7] &= 0xffffffff;
	t[0] += o;
	t[3] -= o;
	t[6] -= o;
	t[7] += o;
	r[0] = (sp_digit)t[0];
	t[1] += t[0] >> 32;
	r[1] = (sp_digit)t[1];
	t[2] += t[1] >> 32;
	r[2] = (sp_digit)t[2];
	t[3] += t[2] >> 32;
	r[3] = (sp_digit)t[3];
	t[4] += t[3] >> 32;
	r[4] = (sp_digit)t[4];
	t[5] += t[4] >> 32;
	r[5] = (sp_digit)t[5];
	t[6] += t[5] >> 32;
	r[6] = (sp_digit)t[6];
//	t[7] += t[6] >> 32;
//	r[7] = (sp_digit)t[7];
	r[7] = (sp_digit)t[7] + (sp_digit)(t[6] >> 32);
}

/* Map the Montgomery form projective co-ordinate point to an affine point.
 *
 * r  Resulting affine co-ordinate point.
 * p  Montgomery form projective co-ordinate point.
 */
static void sp_256_map_8(sp_point* r, sp_point* p)
{
	sp_digit t1[2*8];
	sp_digit t2[2*8];

	sp_256_mont_inv_8(t1, p->z);

	sp_256_mont_sqr_8(t2, t1 /*, p256_mod, p256_mp_mod*/);
	sp_256_mont_mul_8(t1, t2, t1 /*, p256_mod, p256_mp_mod*/);

	/* x /= z^2 */
	sp_256_mont_mul_8(r->x, p->x, t2 /*, p256_mod, p256_mp_mod*/);
	memset(r->x + 8, 0, sizeof(r->x) / 2);
	sp_256_mont_reduce_8(r->x /*, p256_mod, p256_mp_mod*/);
	/* Reduce x to less than modulus */
	if (sp_256_cmp_8(r->x, p256_mod) >= 0)
		sp_256_sub_8_p256_mod(r->x);
	sp_256_norm_8(r->x);

	/* y /= z^3 */
	sp_256_mont_mul_8(r->y, p->y, t1 /*, p256_mod, p256_mp_mod*/);
	memset(r->y + 8, 0, sizeof(r->y) / 2);
	sp_256_mont_reduce_8(r->y /*, p256_mod, p256_mp_mod*/);
	/* Reduce y to less than modulus */
	if (sp_256_cmp_8(r->y, p256_mod) >= 0)
		sp_256_sub_8_p256_mod(r->y);
	sp_256_norm_8(r->y);

	memset(r->z, 0, sizeof(r->z));
	r->z[0] = 1;
}

/* Double the Montgomery form projective point p.
 *
 * r  Result of doubling point.
 * p  Point to double.
 */
static void sp_256_proj_point_dbl_8(sp_point* r, sp_point* p)
{
	sp_digit t1[2*8];
	sp_digit t2[2*8];

	/* Put point to double into result */
	if (r != p)
		*r = *p; /* struct copy */

	if (r->infinity)
		return;

	if (SP_DEBUG) {
		/* unused part of t2, may result in spurios
		 * differences in debug output. Clear it.
		 */
		memset(t2, 0, sizeof(t2));
	}

	/* T1 = Z * Z */
	sp_256_mont_sqr_8(t1, r->z /*, p256_mod, p256_mp_mod*/);
	/* Z = Y * Z */
	sp_256_mont_mul_8(r->z, r->y, r->z /*, p256_mod, p256_mp_mod*/);
	/* Z = 2Z */
	sp_256_mont_dbl_8(r->z, r->z /*, p256_mod*/);
	/* T2 = X - T1 */
	sp_256_mont_sub_8(t2, r->x, t1 /*, p256_mod*/);
	/* T1 = X + T1 */
	sp_256_mont_add_8(t1, r->x, t1 /*, p256_mod*/);
	/* T2 = T1 * T2 */
	sp_256_mont_mul_8(t2, t1, t2 /*, p256_mod, p256_mp_mod*/);
	/* T1 = 3T2 */
	sp_256_mont_tpl_8(t1, t2 /*, p256_mod*/);
	/* Y = 2Y */
	sp_256_mont_dbl_8(r->y, r->y /*, p256_mod*/);
	/* Y = Y * Y */
	sp_256_mont_sqr_8(r->y, r->y /*, p256_mod, p256_mp_mod*/);
	/* T2 = Y * Y */
	sp_256_mont_sqr_8(t2, r->y /*, p256_mod, p256_mp_mod*/);
	/* T2 = T2/2 */
	sp_256_div2_8(t2, t2, p256_mod);
	/* Y = Y * X */
	sp_256_mont_mul_8(r->y, r->y, r->x /*, p256_mod, p256_mp_mod*/);
	/* X = T1 * T1 */
	sp_256_mont_mul_8(r->x, t1, t1 /*, p256_mod, p256_mp_mod*/);
	/* X = X - Y */
	sp_256_mont_sub_8(r->x, r->x, r->y /*, p256_mod*/);
	/* X = X - Y */
	sp_256_mont_sub_8(r->x, r->x, r->y /*, p256_mod*/);
	/* Y = Y - X */
	sp_256_mont_sub_8(r->y, r->y, r->x /*, p256_mod*/);
	/* Y = Y * T1 */
	sp_256_mont_mul_8(r->y, r->y, t1 /*, p256_mod, p256_mp_mod*/);
	/* Y = Y - T2 */
	sp_256_mont_sub_8(r->y, r->y, t2 /*, p256_mod*/);
	dump_512("y2 %s\n", r->y);
}

/* Add two Montgomery form projective points.
 *
 * r  Result of addition.
 * p  Frist point to add.
 * q  Second point to add.
 */
static NOINLINE void sp_256_proj_point_add_8(sp_point* r, sp_point* p, sp_point* q)
{
	sp_digit t1[2*8];
	sp_digit t2[2*8];
	sp_digit t3[2*8];
	sp_digit t4[2*8];
	sp_digit t5[2*8];

	/* Ensure only the first point is the same as the result. */
	if (q == r) {
		sp_point* a = p;
		p = q;
		q = a;
	}

	/* Check double */
	sp_256_sub_8(t1, p256_mod, q->y);
	sp_256_norm_8(t1);
	if (sp_256_cmp_equal_8(p->x, q->x)
	 && sp_256_cmp_equal_8(p->z, q->z)
	 && (sp_256_cmp_equal_8(p->y, q->y) || sp_256_cmp_equal_8(p->y, t1))
	) {
		sp_256_proj_point_dbl_8(r, p);
	}
	else {
		sp_point tp;
		sp_point *v;

		v = r;
		if (p->infinity | q->infinity) {
			memset(&tp, 0, sizeof(tp));
			v = &tp;
		}

		*r = p->infinity ? *q : *p; /* struct copy */

		/* U1 = X1*Z2^2 */
		sp_256_mont_sqr_8(t1, q->z /*, p256_mod, p256_mp_mod*/);
		sp_256_mont_mul_8(t3, t1, q->z /*, p256_mod, p256_mp_mod*/);
		sp_256_mont_mul_8(t1, t1, v->x /*, p256_mod, p256_mp_mod*/);
		/* U2 = X2*Z1^2 */
		sp_256_mont_sqr_8(t2, v->z /*, p256_mod, p256_mp_mod*/);
		sp_256_mont_mul_8(t4, t2, v->z /*, p256_mod, p256_mp_mod*/);
		sp_256_mont_mul_8(t2, t2, q->x /*, p256_mod, p256_mp_mod*/);
		/* S1 = Y1*Z2^3 */
		sp_256_mont_mul_8(t3, t3, v->y /*, p256_mod, p256_mp_mod*/);
		/* S2 = Y2*Z1^3 */
		sp_256_mont_mul_8(t4, t4, q->y /*, p256_mod, p256_mp_mod*/);
		/* H = U2 - U1 */
		sp_256_mont_sub_8(t2, t2, t1 /*, p256_mod*/);
		/* R = S2 - S1 */
		sp_256_mont_sub_8(t4, t4, t3 /*, p256_mod*/);
		/* Z3 = H*Z1*Z2 */
		sp_256_mont_mul_8(v->z, v->z, q->z /*, p256_mod, p256_mp_mod*/);
		sp_256_mont_mul_8(v->z, v->z, t2 /*, p256_mod, p256_mp_mod*/);
		/* X3 = R^2 - H^3 - 2*U1*H^2 */
		sp_256_mont_sqr_8(v->x, t4 /*, p256_mod, p256_mp_mod*/);
		sp_256_mont_sqr_8(t5, t2 /*, p256_mod, p256_mp_mod*/);
		sp_256_mont_mul_8(v->y, t1, t5 /*, p256_mod, p256_mp_mod*/);
		sp_256_mont_mul_8(t5, t5, t2 /*, p256_mod, p256_mp_mod*/);
		sp_256_mont_sub_8(v->x, v->x, t5 /*, p256_mod*/);
		sp_256_mont_dbl_8(t1, v->y /*, p256_mod*/);
		sp_256_mont_sub_8(v->x, v->x, t1 /*, p256_mod*/);
		/* Y3 = R*(U1*H^2 - X3) - S1*H^3 */
		sp_256_mont_sub_8(v->y, v->y, v->x /*, p256_mod*/);
		sp_256_mont_mul_8(v->y, v->y, t4 /*, p256_mod, p256_mp_mod*/);
		sp_256_mont_mul_8(t5, t5, t3 /*, p256_mod, p256_mp_mod*/);
		sp_256_mont_sub_8(v->y, v->y, t5 /*, p256_mod*/);
	}
}

/* Multiply the point by the scalar and return the result.
 * If map is true then convert result to affine co-ordinates.
 *
 * r     Resulting point.
 * g     Point to multiply.
 * k     Scalar to multiply by.
 * map   Indicates whether to convert result to affine.
 */
static void sp_256_ecc_mulmod_8(sp_point* r, const sp_point* g, const sp_digit* k /*, int map*/)
{
	enum { map = 1 }; /* we always convert result to affine coordinates */
	sp_point t[3];
	sp_digit n = n; /* for compiler */
	int c, y;

	memset(t, 0, sizeof(t));

	/* t[0] = {0, 0, 1} * norm */
	t[0].infinity = 1;
	/* t[1] = {g->x, g->y, g->z} * norm */
	sp_256_mod_mul_norm_8(t[1].x, g->x);
	sp_256_mod_mul_norm_8(t[1].y, g->y);
	sp_256_mod_mul_norm_8(t[1].z, g->z);

	/* For every bit, starting from most significant... */
	k += 7;
	c = 256;
	for (;;) {
		if ((c & 0x1f) == 0) {
			if (c == 0)
				break;
			n = *k--;
		}

		y = (n >> 31);
		dbg("y:%d t[%d] = t[0]+t[1]\n", y, y^1);
		sp_256_proj_point_add_8(&t[y^1], &t[0], &t[1]);
		dump_512("t[0].x %s\n", t[0].x);
		dump_512("t[0].y %s\n", t[0].y);
		dump_512("t[0].z %s\n", t[0].z);
		dump_512("t[1].x %s\n", t[1].x);
		dump_512("t[1].y %s\n", t[1].y);
		dump_512("t[1].z %s\n", t[1].z);
		dbg("t[2] = t[%d]\n", y);
		memcpy(&t[2], &t[y], sizeof(sp_point));
		dbg("t[2] *= 2\n");
		sp_256_proj_point_dbl_8(&t[2], &t[2]);
		dump_512("t[2].x %s\n", t[2].x);
		dump_512("t[2].y %s\n", t[2].y);
		dump_512("t[2].z %s\n", t[2].z);
		memcpy(&t[y], &t[2], sizeof(sp_point));

		n <<= 1;
		c--;
	}

	if (map)
		sp_256_map_8(r, &t[0]);
	else
		memcpy(r, &t[0], sizeof(sp_point));

	memset(t, 0, sizeof(t)); //paranoia
}

/* Multiply the base point of P256 by the scalar and return the result.
 * If map is true then convert result to affine co-ordinates.
 *
 * r     Resulting point.
 * k     Scalar to multiply by.
 * map   Indicates whether to convert result to affine.
 */
static void sp_256_ecc_mulmod_base_8(sp_point* r, sp_digit* k /*, int map*/)
{
	/* Since this function is called only once, save space:
	 * don't have "static const sp_point p256_base = {...}",
	 * it would have more zeros than data.
	 */
	static const uint8_t p256_base_bin[] = {
		/* x (big-endian) */
		0x6b,0x17,0xd1,0xf2,0xe1,0x2c,0x42,0x47,0xf8,0xbc,0xe6,0xe5,0x63,0xa4,0x40,0xf2,0x77,0x03,0x7d,0x81,0x2d,0xeb,0x33,0xa0,0xf4,0xa1,0x39,0x45,0xd8,0x98,0xc2,0x96,
		/* y */
		0x4f,0xe3,0x42,0xe2,0xfe,0x1a,0x7f,0x9b,0x8e,0xe7,0xeb,0x4a,0x7c,0x0f,0x9e,0x16,0x2b,0xce,0x33,0x57,0x6b,0x31,0x5e,0xce,0xcb,0xb6,0x40,0x68,0x37,0xbf,0x51,0xf5,
		/* z will be set to 1, infinity flag to "false" */
	};
	sp_point p256_base;

	sp_256_point_from_bin2x32(&p256_base, p256_base_bin);

	sp_256_ecc_mulmod_8(r, &p256_base, k /*, map*/);
}

/* Multiply the point by the scalar and serialize the X ordinate.
 * The number is 0 padded to maximum size on output.
 *
 * priv    Scalar to multiply the point by.
 * pub2x32 Point to multiply.
 * out32   Buffer to hold X ordinate.
 */
static void sp_ecc_secret_gen_256(const sp_digit priv[8], const uint8_t *pub2x32, uint8_t* out32)
{
	sp_point point[1];

#if FIXED_PEER_PUBKEY
	memset((void*)pub2x32, 0x55, 64);
#endif
	dump_hex("peerkey %s\n", pub2x32, 32); /* in TLS, this is peer's public key */
	dump_hex("        %s\n", pub2x32 + 32, 32);

	sp_256_point_from_bin2x32(point, pub2x32);
	dump_512("point->x %s\n", point->x);
	dump_512("point->y %s\n", point->y);

	sp_256_ecc_mulmod_8(point, point, priv);

	sp_256_to_bin_8(point->x, out32);
	dump_hex("out32: %s\n", out32, 32);
}

/* Generates a random scalar in [1..order-1] range. */
static void sp_256_ecc_gen_k_8(sp_digit k[8])
{
	/* Since 32-bit words are "dense", no need to use
	 * sp_256_from_bin_8(k, buf) to convert random stream
	 * to sp_digit array - just store random bits there directly.
	 */
	tls_get_random(k, 8 * sizeof(k[0]));
#if FIXED_SECRET
	memset(k, 0x77, 8 * sizeof(k[0]));
#endif

// If scalar is too large, try again (pseudo-code)
//	if (k >= 0xffffffff00000000ffffffffffffffffbce6faada7179e84f3b9cac2fc632551 - 1) // order of P256
//		goto pick_another_random;
//	k++; // ensure non-zero
	/* Simpler alternative, at the cost of not choosing some valid
	 * random values, and slightly non-uniform distribution */
	if (k[0] == 0)
		k[0] = 1;
	if (k[7] >= 0xffffffff)
		k[7] = 0xfffffffe;
}

/* Makes a random EC key pair. */
static void sp_ecc_make_key_256(sp_digit privkey[8], uint8_t *pubkey)
{
	sp_point point[1];

	sp_256_ecc_gen_k_8(privkey);
	dump_256("privkey %s\n", privkey);
	sp_256_ecc_mulmod_base_8(point, privkey);
	dump_512("point->x %s\n", point->x);
	dump_512("point->y %s\n", point->y);
	sp_256_to_bin_8(point->x, pubkey);
	sp_256_to_bin_8(point->y, pubkey + 32);

	memset(point, 0, sizeof(point)); //paranoia
}

void FAST_FUNC curve_P256_compute_pubkey_and_premaster(
		uint8_t *pubkey2x32, uint8_t *premaster32,
		const uint8_t *peerkey2x32)
{
	sp_digit privkey[8];

	dump_hex("peerkey2x32: %s\n", peerkey2x32, 64);
	sp_ecc_make_key_256(privkey, pubkey2x32);
	dump_hex("pubkey: %s\n", pubkey2x32, 32);
	dump_hex("        %s\n", pubkey2x32 + 32, 32);

	/* Combine our privkey and peer's public key to generate premaster */
	sp_ecc_secret_gen_256(privkey, /*x,y:*/peerkey2x32, premaster32);
	dump_hex("premaster: %s\n", premaster32, 32);
}
