#include "libbb.h"
#include "bb_archive.h"

IF_DESKTOP(long long) int FAST_FUNC
unpack_zstd_stream(transformer_state_t *xstate)
{
	// FIXME Provide an internal implementation of zstd decompression.
	char *argv[] = {(char *)"unzstd", (char *)"-cf", (char *)"-", NULL};

	xmove_fd(xstate->src_fd, STDIN_FILENO);
	xmove_fd(xstate->dst_fd, STDOUT_FILENO);
	BB_EXECVP_or_die(argv);
}
