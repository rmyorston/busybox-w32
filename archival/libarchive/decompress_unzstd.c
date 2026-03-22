#include "libbb.h"
#include "bb_archive.h"

IF_DESKTOP(long long) int FAST_FUNC
unpack_zstd_stream(transformer_state_t *xstate)
{
	char *argv[] = {(char *)"unzstd", NULL};

	/* Exec external unzstd. */
#if BB_MMU
	if (xstate->signature_skipped) {
		/* If called from fork_transformer_with_no_sig() signature_skipped
		 * is just a flag, not the actual offset.  But we know it's 4. */
		xlseek(xstate->src_fd, -4, SEEK_CUR);
	}
#endif
	xmove_fd(xstate->src_fd, STDIN_FILENO);
	xmove_fd(xstate->dst_fd, STDOUT_FILENO);
	BB_EXECVP_or_die(argv);
}
