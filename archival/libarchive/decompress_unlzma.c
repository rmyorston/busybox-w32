/* vi: set sw=4 ts=4: */
/*
 * Small lzma deflate implementation.
 * Copyright (C) 2006  Aurelien Jacobs <aurel@gnuage.org>
 *
 * Based on LzmaDecode.c from the LZMA SDK 4.22 (http://www.7-zip.org/)
 * Copyright (C) 1999-2005  Igor Pavlov
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
#include "libbb.h"
#include "bb_archive.h"

#if 0
# define dbg(...) bb_error_msg(__VA_ARGS__)
#else
# define dbg(...) ((void)0)
#endif

#if ENABLE_FEATURE_LZMA_FAST
#  define speed_inline ALWAYS_INLINE
#  define size_inline
#else
#  define speed_inline
#  define size_inline ALWAYS_INLINE
#endif

typedef struct {
	int fd;
	uint8_t *ptr;

/* Was keeping rc on stack in unlzma and separately allocating buffer,
 * but with "buffer 'attached to' allocated rc" code is smaller: */
	/* uint8_t *buffer; */
#define RC_BUFFER ((uint8_t*)(rc+1))

	uint8_t *buffer_end;

/* Had provisions for variable buffer, but we don't need it here */
	/* int buffer_size; */
#define RC_BUFFER_SIZE 0x10000

	uint32_t code;
	uint32_t range;
	uint32_t bound;
} rc_t;

#define RC_TOP_BITS 24
#define RC_MOVE_BITS 5
#define RC_MODEL_TOTAL_BITS 11

/* Called once in rc_do_normalize() */
static void rc_read(rc_t *rc)
{
	int buffer_size = safe_read(rc->fd, RC_BUFFER, RC_BUFFER_SIZE);
//TODO: return -1 instead
//This will make unlzma delete broken unpacked file on unpack errors
	if (buffer_size <= 0)
		bb_simple_error_msg_and_die("unexpected EOF");
	rc->buffer_end = RC_BUFFER + buffer_size;
	rc->ptr = RC_BUFFER;
}

/* Called twice, but one callsite is in speed_inline'd rc_is_bit_1() */
static void rc_do_normalize(rc_t *rc)
{
	if (rc->ptr >= rc->buffer_end)
		rc_read(rc);
	rc->range <<= 8;
	rc->code = (rc->code << 8) | *rc->ptr++;
}
static ALWAYS_INLINE void rc_normalize(rc_t *rc)
{
	if (rc->range < (1 << RC_TOP_BITS)) {
		rc_do_normalize(rc);
	}
}

/* Called once */
static ALWAYS_INLINE rc_t* rc_init(int fd) /*, int buffer_size) */
{
	int i;
	rc_t *rc;

	rc = xzalloc(sizeof(*rc) + RC_BUFFER_SIZE);

	rc->fd = fd;
	/* rc->ptr = rc->buffer_end; */

	for (i = 0; i < 5; i++) {
		rc_do_normalize(rc);
	}
	rc->range = 0xffffffff;
	return rc;
}

/* Called once  */
static ALWAYS_INLINE void rc_free(rc_t *rc)
{
	free(rc);
}

/* rc_is_bit_1 is called 9 times */
static speed_inline int rc_is_bit_1(rc_t *rc, uint16_t *p)
{
	rc_normalize(rc);
	rc->bound = *p * (rc->range >> RC_MODEL_TOTAL_BITS);
	if (rc->code < rc->bound) {
		rc->range = rc->bound;
		*p += ((1 << RC_MODEL_TOTAL_BITS) - *p) >> RC_MOVE_BITS;
		return 0;
	}
	rc->range -= rc->bound;
	rc->code -= rc->bound;
	*p -= *p >> RC_MOVE_BITS;
	return 1;
}

/* Called 4 times in unlzma loop */
// I tried to expose the value of inserted bit via
//rc_insert_bit(int *bit, rc_t *rc, uint16_t *p, int symbol)
// prototype, provided dedicated variables at callsites
// to be set by rc_insert_bit(&bit, ...) calls.
// This allows to eliminate redundant "... & 1" ops we have now.
// But gcc does not optimize this well - the code size increases.
static ALWAYS_INLINE int rc_insert_bit(rc_t *rc, uint16_t *p, int symbol)
{
	int bit = rc_is_bit_1(rc, p);
	symbol = symbol * 2 + bit;
	return symbol;
}

/* Called once */
static ALWAYS_INLINE int rc_direct_bit(rc_t *rc)
{
	rc_normalize(rc);
	rc->range >>= 1;
	if (rc->code >= rc->range) {
		rc->code -= rc->range;
		return 1;
	}
	return 0;
}

/* Called twice */
static speed_inline int
rc_bit_tree_decode(rc_t *rc, uint16_t *p, int num_levels)
{
	int symbol;
	int i = num_levels;

	symbol = 1;
	while (i--)
		symbol = rc_insert_bit(rc, p + symbol, symbol);
	symbol -= 1 << num_levels;
	return symbol;
}

/* #defines will force compiler to compute/optimize each one with each usage.
 * Have heart and use enum instead. */
enum {
	LZMA_BASE_SIZE = 1846,
	LZMA_LIT_SIZE  = 768,

	LZMA_NUM_POS_BITS_MAX = 4,

	LZMA_LEN_NUM_LOW_BITS  = 3,
	LZMA_LEN_NUM_MID_BITS  = 3,
	LZMA_LEN_NUM_HIGH_BITS = 8,

	LZMA_LEN_CHOICE     = 0,
	LZMA_LEN_CHOICE_2   = (LZMA_LEN_CHOICE + 1),
	LZMA_LEN_LOW        = (LZMA_LEN_CHOICE_2 + 1),
	LZMA_LEN_MID        = (LZMA_LEN_LOW \
	                      + (1 << (LZMA_NUM_POS_BITS_MAX + LZMA_LEN_NUM_LOW_BITS))),
	LZMA_LEN_HIGH       = (LZMA_LEN_MID \
	                      + (1 << (LZMA_NUM_POS_BITS_MAX + LZMA_LEN_NUM_MID_BITS))),
	LZMA_NUM_LEN_PROBS  = (LZMA_LEN_HIGH + (1 << LZMA_LEN_NUM_HIGH_BITS)),

	LZMA_NUM_STATES     = 12,
	LZMA_NUM_LIT_STATES = 7,

	LZMA_START_POS_MODEL_INDEX = 4,
	LZMA_END_POS_MODEL_INDEX   = 14,
	LZMA_NUM_FULL_DISTANCES    = (1 << (LZMA_END_POS_MODEL_INDEX >> 1)),

	LZMA_NUM_POS_SLOT_BITS = 6,
	LZMA_NUM_LEN_TO_POS_STATES = 4,

	LZMA_NUM_ALIGN_BITS = 4,

	LZMA_MATCH_MIN_LEN  = 2,

	LZMA_IS_MATCH       = 0,
	LZMA_IS_REP         = (LZMA_IS_MATCH + (LZMA_NUM_STATES << LZMA_NUM_POS_BITS_MAX)),
	LZMA_IS_REP_G0      = (LZMA_IS_REP + LZMA_NUM_STATES),
	LZMA_IS_REP_G1      = (LZMA_IS_REP_G0 + LZMA_NUM_STATES),
	LZMA_IS_REP_G2      = (LZMA_IS_REP_G1 + LZMA_NUM_STATES),
	LZMA_IS_REP_0_LONG  = (LZMA_IS_REP_G2 + LZMA_NUM_STATES),
	LZMA_POS_SLOT       = (LZMA_IS_REP_0_LONG \
	                      + (LZMA_NUM_STATES << LZMA_NUM_POS_BITS_MAX)),
	LZMA_SPEC_POS       = (LZMA_POS_SLOT \
	                      + (LZMA_NUM_LEN_TO_POS_STATES << LZMA_NUM_POS_SLOT_BITS)),
	LZMA_ALIGN          = (LZMA_SPEC_POS \
	                      + LZMA_NUM_FULL_DISTANCES - LZMA_END_POS_MODEL_INDEX),
	LZMA_LEN_CODER      = (LZMA_ALIGN + (1 << LZMA_NUM_ALIGN_BITS)),
	LZMA_REP_LEN_CODER  = (LZMA_LEN_CODER + LZMA_NUM_LEN_PROBS),
	LZMA_LITERAL        = (LZMA_REP_LEN_CODER + LZMA_NUM_LEN_PROBS),
};

IF_DESKTOP(long long) int FAST_FUNC
unpack_lzma_stream(transformer_state_t *xstate)
{
	typedef struct {
		uint8_t  properties;
		uint32_t dict_size;
		uint64_t dst_size;
	} PACKED lzma_header_t;

	IF_DESKTOP(long long total_written = 0;)
	lzma_header_t header;
	unsigned lc; // number of literal context bits, 0..8
	unsigned pb; // number of position bits, 0..4
	unsigned lp; // number of literal position bits, 0..4
	uint32_t pos_state_mask;
	uint32_t literal_pos_mask;
	uint16_t *p;
	rc_t *rc;
	int i;
	uint8_t *buffer;
	uint32_t buffer_size;
	uint8_t previous_byte = 0;
	size_t buffer_pos;
	size_t global_pos;
	int len = 0;
	int state = 0;
	uint32_t rep0 = 1, rep1 = 1, rep2 = 1, rep3 = 1;

	if (full_read(xstate->src_fd, &header, sizeof(header)) != sizeof(header)
	 || header.properties > (4 * 5 + 4) * 9 + 8
	) {
 bad_header:
		bb_simple_error_msg("bad lzma header");
		return -1;
	}
	// properties = (pb * 5 + lp) * 9 + lc, pb<=4, lp<=4, lc<=8
	i = header.properties / 9;
	lc = header.properties % 9;
	pb = (unsigned)i / 5;
	lp = (unsigned)i % 5;
	pos_state_mask = (1 << pb) - 1;   // max 00001111
	literal_pos_mask = (1 << lp) - 1; // max 00001111

	// Example values from linux-3.3.4.tar.lzma:
	// dict_size: 64M, dst_size: 2^64-1
	header.dict_size = SWAP_LE32(header.dict_size);
	if (header.dict_size == 0
	 || header.dict_size > 1024*1024*1024 / 2 * 3 // 1.5GB upper limit mentioned in "man xz"
	 || (header.dict_size & 0xff) // code expects dict_size to be reasonably round, let's require 256 bytes
	// xz --format=lzma --lzma1=dict=SIZE requires 4K minimum, rounds dict_size up to be 2^N or 2^N+2^(N-1)
	) {
		//bb_error_msg("header.dict_size:0x%08x", header.dict_size);
		goto bad_header;
	}
	//UNUSED:header.dst_size = SWAP_LE64(header.dst_size);
	// Note that these values within header are misaligned!
	// I discovered that gcc _intentionally shifts_ "header" variable on stack to fix this (!)

	// Used to do this:
	//buffer_size = MIN(header.dst_size, header.dict_size);
//#define BUFSIZE_CAN_BE_LESS_THAN_DICTSIZE 1
	// but this requires extra "pos > buffer_size" checks
	// after each "if (pos < 0) pos += header.dict_size".
	// Considering that normally dict_size <= dst_size (uncompressed size)
	// and therefore we _have to_ allocate full buffer in most cases,
	// and that large malloc is mmap()ed and allocated _on demand_,
	// let's just unconditionally use a larger dict_size allocation:
	buffer_size = header.dict_size;
	buffer = xmalloc(buffer_size);
#define BUFSIZE_CAN_BE_LESS_THAN_DICTSIZE 0

	// Does not show any speedup in my testing (be sure to convert to mmap if you try this):
	//madvise(buffer, buffer_size, MADV_HUGEPAGE);

	{
		int num_probs;

		num_probs = LZMA_BASE_SIZE + (LZMA_LIT_SIZE << (lc + lp));
		p = xmalloc(num_probs * sizeof(*p));
		num_probs += LZMA_LITERAL - LZMA_BASE_SIZE;
		for (i = 0; i < num_probs; i++)
			p[i] = (1 << RC_MODEL_TOTAL_BITS) >> 1;
	}

	rc = rc_init(xstate->src_fd); /*, RC_BUFFER_SIZE); */

	buffer_pos = 0;
	// Since global_pos is ever only used for masking off pos_state_mask / literal_pos_mask bits,
	// it is only advanced by += header.dict_size, and since dict_size is round to at least 256 bytes,
	// the variable is actually not necessary:
	global_pos = 0;
	// Optimize its updates out (compiler will eliminate variable entirely):
#define ADVANCE_global_pos_by_dict_size() ((void)0) /*global_pos += header.dict_size*/

	while (1) {
		int pos_state = (buffer_pos + global_pos) & pos_state_mask;
		uint16_t *prob = p + LZMA_IS_MATCH + (state << LZMA_NUM_POS_BITS_MAX) + pos_state;

		if (!rc_is_bit_1(rc, prob)) {
			static const char next_state[LZMA_NUM_STATES] =
				{ 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 4, 5 };
			int mi = 1;

			prob = (p + LZMA_LITERAL
			        + (LZMA_LIT_SIZE * ((((buffer_pos + global_pos) & literal_pos_mask) << lc)
			                            + (previous_byte >> (8 - lc))
			                           )
			          )
			);

			if (state >= LZMA_NUM_LIT_STATES) {
				int match_byte;
				uint32_t pos;

				pos = buffer_pos - rep0;
				if ((int32_t)pos < 0) {
					pos += header.dict_size;
					if ((int32_t)pos < 0)
						goto bad;
# if BUFSIZE_CAN_BE_LESS_THAN_DICTSIZE
					//instead of the above: if (pos >= buffer_size) goto bad;
#endif
				}
				match_byte = buffer[pos];
				do {
					int bit;

					match_byte <<= 1;
					bit = match_byte & 0x100;
					mi = rc_insert_bit(rc, prob + 0x100 + bit + mi, mi);
					bit ^= ((mi & 1) << 8); /* 0x100 or 0 */
					if (bit)
						break;
				} while (mi < 0x100);
			}
			while (mi < 0x100) {
				mi = rc_insert_bit(rc, prob + mi, mi);
			}

			state = next_state[state];

			previous_byte = (uint8_t) mi;
#if ENABLE_FEATURE_LZMA_FAST
 one_byte1:
			buffer[buffer_pos++] = previous_byte;
			if (buffer_pos == header.dict_size) {
				buffer_pos = 0;
				ADVANCE_global_pos_by_dict_size();
				if (transformer_write(xstate, buffer, header.dict_size) != (ssize_t)header.dict_size)
					goto bad;
				IF_DESKTOP(total_written += header.dict_size;)
			}
#else
			len = 1;
			goto one_byte2;
#endif
		} else {
			int num_bits;
			int offset;
			uint16_t *prob2;
#define prob_len prob2

			prob2 = p + LZMA_IS_REP + state;
			if (!rc_is_bit_1(rc, prob2)) {
				rep3 = rep2;
				rep2 = rep1;
				rep1 = rep0;
				state = state < LZMA_NUM_LIT_STATES ? 0 : 3;
				prob2 = p + LZMA_LEN_CODER;
			} else {
				prob2 += LZMA_IS_REP_G0 - LZMA_IS_REP;
				if (!rc_is_bit_1(rc, prob2)) {
					prob2 = (p + LZMA_IS_REP_0_LONG
					        + (state << LZMA_NUM_POS_BITS_MAX)
					        + pos_state
					);
					if (!rc_is_bit_1(rc, prob2)) {
#if ENABLE_FEATURE_LZMA_FAST
						uint32_t pos;
						state = state < LZMA_NUM_LIT_STATES ? 9 : 11;

						pos = buffer_pos - rep0;
						if ((int32_t)pos < 0) {
							pos += header.dict_size;
							/* see unzip_bad_lzma_2.zip: */
# if BUFSIZE_CAN_BE_LESS_THAN_DICTSIZE
							if (pos >= buffer_size) {
								dbg("%d pos:%d buffer_size:%d", __LINE__, pos, buffer_size);
								goto bad;
							}
# else
							if ((int32_t)pos < 0)
								goto bad;
# endif
						}
						previous_byte = buffer[pos];
						goto one_byte1;
#else
						state = state < LZMA_NUM_LIT_STATES ? 9 : 11;
						len = 1;
						goto string;
#endif
					}
				} else {
					uint32_t distance;

					prob2 += LZMA_IS_REP_G1 - LZMA_IS_REP_G0;
					distance = rep1;
					if (rc_is_bit_1(rc, prob2)) {
						prob2 += LZMA_IS_REP_G2 - LZMA_IS_REP_G1;
						distance = rep2;
						if (rc_is_bit_1(rc, prob2)) {
							distance = rep3;
							rep3 = rep2;
						}
						rep2 = rep1;
					}
					rep1 = rep0;
					rep0 = distance;
				}
				state = state < LZMA_NUM_LIT_STATES ? 8 : 11;
				prob2 = p + LZMA_REP_LEN_CODER;
			}

			prob_len = prob2 + LZMA_LEN_CHOICE;
			num_bits = LZMA_LEN_NUM_LOW_BITS;
			if (!rc_is_bit_1(rc, prob_len)) {
				prob_len += LZMA_LEN_LOW - LZMA_LEN_CHOICE
				            + (pos_state << LZMA_LEN_NUM_LOW_BITS);
				offset = 0;
			} else {
				prob_len += LZMA_LEN_CHOICE_2 - LZMA_LEN_CHOICE;
				if (!rc_is_bit_1(rc, prob_len)) {
					prob_len += LZMA_LEN_MID - LZMA_LEN_CHOICE_2
					            + (pos_state << LZMA_LEN_NUM_MID_BITS);
					offset = 1 << LZMA_LEN_NUM_LOW_BITS;
					num_bits += LZMA_LEN_NUM_MID_BITS - LZMA_LEN_NUM_LOW_BITS;
				} else {
					prob_len += LZMA_LEN_HIGH - LZMA_LEN_CHOICE_2;
					offset = ((1 << LZMA_LEN_NUM_LOW_BITS)
					          + (1 << LZMA_LEN_NUM_MID_BITS));
					num_bits += LZMA_LEN_NUM_HIGH_BITS - LZMA_LEN_NUM_LOW_BITS;
				}
			}
			len = rc_bit_tree_decode(rc, prob_len, num_bits);
			len += offset;

			if (state < 4) {
				int pos_slot;
				uint16_t *prob3;

				state += LZMA_NUM_LIT_STATES;
				prob3 = p + LZMA_POS_SLOT +
				       ((len < LZMA_NUM_LEN_TO_POS_STATES ? len :
				         LZMA_NUM_LEN_TO_POS_STATES - 1)
				         << LZMA_NUM_POS_SLOT_BITS);
				pos_slot = rc_bit_tree_decode(rc, prob3, LZMA_NUM_POS_SLOT_BITS);
				rep0 = pos_slot;
				if (pos_slot >= LZMA_START_POS_MODEL_INDEX) {
					int i2, mi2, num_bits2 = (pos_slot >> 1) - 1;
					rep0 = 2 | (pos_slot & 1);
					if (pos_slot < LZMA_END_POS_MODEL_INDEX) {
						rep0 <<= num_bits2;
						prob3 = p + LZMA_SPEC_POS + rep0 - pos_slot - 1;
					} else {
						for (; num_bits2 != LZMA_NUM_ALIGN_BITS; num_bits2--)
							rep0 = (rep0 << 1) | rc_direct_bit(rc);
						rep0 <<= LZMA_NUM_ALIGN_BITS;
						// Note: (int32_t)rep0 may be < 0 here
						// (I have linux-3.3.4.tar.lzma which has it).
						// I moved the check after "++rep0 == 0" check below.
						prob3 = p + LZMA_ALIGN;
					}
					i2 = 1;
					mi2 = 1;
					while (num_bits2--) {
						mi2 = rc_insert_bit(rc, prob3 + mi2, mi2);
						if (mi2 & 1)
							rep0 |= i2;
						i2 <<= 1;
					}
				}
				rep0++;
				if ((int32_t)rep0 <= 0) {
					if (rep0 == 0)
// this is where main decompression loop exits:
						break;
					dbg("%d rep0:%d", __LINE__, rep0);
					goto bad;
				}
			}

			len += LZMA_MATCH_MIN_LEN;
			/*
			 * LZMA SDK has this optimized:
			 * it precalculates size and copies many bytes
			 * in a loop with simpler checks, a-la:
			 *	do
			 *		*(dest) = *(dest + ofs);
			 *	while (++dest != lim);
			 * and
			 *	do {
			 *		buffer[buffer_pos++] = buffer[pos];
			 *		if (++pos == header.dict_size)
			 *			pos = 0;
			 *	} while (--cur_len != 0);
			 * Our code is slower (more checks per byte copy):
			 */
 IF_NOT_FEATURE_LZMA_FAST(string:)
			do {
				uint32_t pos = buffer_pos - rep0;
				if ((int32_t)pos < 0) {
					pos += header.dict_size;
# if BUFSIZE_CAN_BE_LESS_THAN_DICTSIZE
					/* more stringent test (see unzip_bad_lzma_1.zip): */
					if (pos >= buffer_size)
						goto bad;
# else
					/* bug 10436 has an example file where this triggers: */
					if ((int32_t)pos < 0)
						goto bad;
# endif
				}
				previous_byte = buffer[pos];
 IF_NOT_FEATURE_LZMA_FAST(one_byte2:)
				buffer[buffer_pos++] = previous_byte;
				if (buffer_pos == header.dict_size) {
					buffer_pos = 0;
					ADVANCE_global_pos_by_dict_size();
					if (transformer_write(xstate, buffer, header.dict_size) != (ssize_t)header.dict_size)
						goto bad;
					IF_DESKTOP(total_written += header.dict_size;)
				}
				len--;
			} while (len != 0 /*&& buffer_pos < header.dst_size*/);
			/* FIXME: ...........^^^^^
			 * shouldn't it be "global_pos + buffer_pos < header.dst_size"?
			 * It probably should, but it is a "do we accidentally
			 * unpack more bytes than expected?" check - which
			 * never happens for well-formed compression data...
			 */
		} /* else */
	} /* while */

	{
		IF_NOT_DESKTOP(int total_written = 0; /* success */)
		IF_DESKTOP(total_written += buffer_pos;)
		if (transformer_write(xstate, buffer, buffer_pos) != (ssize_t)buffer_pos) {
 bad:
			/* One of our users, bbunpack(), expects _us_ to emit
			 * the error message (since it's the best place to give
			 * potentially more detailed information).
			 * Do not fail silently.
			 */
			bb_simple_error_msg("corrupted data");
			total_written = -1; /* failure */
		}
		rc_free(rc);
		free(p);
		free(buffer);
		return total_written;
	}
}
