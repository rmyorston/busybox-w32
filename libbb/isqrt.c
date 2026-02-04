/*
 * Copyright (C) 2017 Denys Vlasenko <vda.linux@googlemail.com>
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */

//kbuild:lib-y += isqrt.o

#ifndef ISQRT_TEST
# include "libbb.h"
#else
// gcc -DISQRT_TEST -Wall -Os -ffunction-sections isqrt.c -S -fverbose-asm
// gcc -DISQRT_TEST -Wall -Os -ffunction-sections isqrt.c -c
// gcc -DISQRT_TEST -Wall -Os -ffunction-sections isqrt.c -oisqrt && ./isqrt $((RANDOM*RANDOM))
# include <stdlib.h>
# include <stdio.h>
# include <time.h>
# include <limits.h>
# define FAST_FUNC /* nothing */
#endif

/* Returns such x that x+1 > sqrt(N) */

/* Stolen from kernel code */
unsigned FAST_FUNC isqrt(unsigned long x)
{
	unsigned long y = 0;
	unsigned long m = 1UL << (8*sizeof(x) - 2);
	// ^^^^^ should be m = 1UL << ((fls(x) - 1) & ~1UL);
	// but calculating fls() would take time and code

	do {
		unsigned long b = y + m;
		y >>= 1;
		if (x >= b) {
			x -= b;
			y += m;
		}
		m >>= 2;
	} while (m);
        return y;
}

#if LLONG_MAX > LONG_MAX
# if defined(i386)
/* Smaller code on this register-starved arch */
unsigned long FAST_FUNC isqrt_ull(unsigned long long N)
{
	unsigned long x;
	unsigned shift;
#define LL_WIDTH_BITS (unsigned)(sizeof(N)*8)

	shift = LL_WIDTH_BITS - 2;
	x = 0;
	do {
		x = (x << 1) + 1;
		if ((unsigned long long)x * x > (N >> shift))
			x--; /* whoops, that +1 was too much */
		shift -= 2;
	} while ((int)shift >= 0);
	return x;
}
# else
/* Stolen from kernel code */
unsigned long FAST_FUNC isqrt_ull(unsigned long long x)
{
	unsigned long long y = 0;
	unsigned long long m = 1ULL << (8*sizeof(x) - 2);
	// ^^^^^ should be m = 1ULL << ((fls(x) - 1) & ~1ULL);
	// but calculating fls() would take time and code

	do {
		unsigned long long b = y + m;
		y >>= 1;
		if (x >= b) {
			x -= b;
			y += m;
		}
		m >>= 2;
	} while (m);
        return y;
}
# endif
#endif /* wide version needed */

#ifdef ISQRT_TEST
# if LLONG_MAX == LONG_MAX
#  define isqrt_ull(N) isqrt(N)
# endif
int main(int argc, char **argv)
{
	unsigned long long n = argv[1] ? strtoull(argv[1], NULL, 0) : time(NULL);
	for (;;) {
		unsigned long h;
		n--;
		h = isqrt_ull(n);
		if (!(n & 0xffff))
			printf("isqrt(%llx)=%lx\n", n, h);
		if ((unsigned long long)h * h > n) {
			printf("BAD1: isqrt(%llx)=%lx\n", n, h);
			return 1;
		}
		h++;
		if ((unsigned long long)h * h != 0 /* this can overflow to 0 - not a bug */
		 && (unsigned long long)h * h <= n)
		{
			printf("BAD2: isqrt(%llx)=%lx\n", n, h);
			return 1;
		}
	}
}
#endif
