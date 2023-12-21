/*
 * This file uses zstd library code which is written
 * by Meta Platforms, Inc. and affiliates.
 *
 * See README file in zstd/ directory for more information.
 *
 * This file is:
 * Copyright (C) 2024 Ur4t
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */

#include "libbb.h"
#include "bb_archive.h"

#if ENABLE_DEBUG
#define DEBUGLEVEL 2
#endif
#undef XXH_NAMESPACE
#define XXH_NAMESPACE ZSTD_
#undef XXH_PRIVATE_API
#define XXH_PRIVATE_API
#undef XXH_INLINE_ALL
#define XXH_INLINE_ALL
#undef XXH_NO_XXH3
#define XXH_NO_XXH3
#define ZSTD_STRIP_ERROR_STRINGS
#define ZSTD_NO_TRACE
#define ZSTD_DISABLE_ASM 1

/* Use custom malloc/calloc/free */
#define ZSTD_DEPS_MALLOC
#define ZSTD_malloc(s)    xmalloc(s)
#define ZSTD_calloc(n, s) calloc((n), (s))
#define ZSTD_free(p)      free((p))

#include "zstd/common/zstd_deps.h"

#include "zstd/common/debug.c"
#include "zstd/common/entropy_common.c"
#include "zstd/common/error_private.c"
#include "zstd/common/fse_decompress.c"
#include "zstd/common/zstd_common.c"

#include "zstd/decompress/huf_decompress.c"
#include "zstd/decompress/zstd_ddict.c"
#include "zstd/decompress/zstd_decompress.c"
#include "zstd/decompress/zstd_decompress_block.c"

IF_DESKTOP(long long) int FAST_FUNC
unpack_zstd_stream(transformer_state_t *xstate)
{
	size_t read;
	IF_DESKTOP(long long) int total = 0;
	const size_t inbuf_size = ZSTD_DStreamInSize();
	const size_t outbuf_size = ZSTD_DStreamOutSize();
	uint8_t *const inbuf = xmalloc(ZSTD_DStreamInSize());
	uint8_t *const outbuf = xmalloc(ZSTD_DStreamOutSize());
	ZSTD_inBuffer input = { inbuf, 0, 0 };
	ZSTD_outBuffer output = { outbuf, outbuf_size, 0 };
	ZSTD_DCtx *const dctx = ZSTD_createDCtx();
	if (dctx == NULL) {
		bb_simple_error_msg_and_die("failed to create zstd decompress context");
	}
	if (!xstate || xstate->signature_skipped) {
		uint16_t *magicp = (uint16_t *)inbuf;
		magicp[0] = ZSTD_MAGIC1;
		magicp[1] = ZSTD_MAGIC2;
		input.size += 2 * sizeof(uint16_t);
	}
	while ((read = safe_read(xstate->src_fd, inbuf + input.size, inbuf_size - input.size)) > 0) {
		input.size += read;
		input.pos = 0;
		/* Given a valid frame, zstd won't consume the last byte of the frame
         * until it has flushed all of the decompressed data of the frame.
         * Therefore, instead of checking if the return code is 0, we can
         * decompress just check if input.pos < input.size.
         */
		while (input.pos < input.size) {
			output.size = outbuf_size;
			output.pos = 0;
			if (ZSTD_isError(ZSTD_decompressStream(dctx, &output, &input))) {
				goto end;
			}
			xtransformer_write(xstate, output.dst, output.pos);
			IF_DESKTOP(total += output.pos;)
		}
		input.size = 0;
	}
end:
	ZSTD_freeDCtx(dctx);
	free(inbuf);
	free(outbuf);
	return total;
}
