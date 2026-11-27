/* vi: set sw=4 ts=4: */
/*
 * Match fsopts for use in mount unmount -O.
 *
 * Returns 1 for a match, otherwise 0.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

#include "libbb.h"

// This function implements the mnt_match_options function from libmount"
//
// -O support
//    -O interprets a list of filter options which select whether a mount
// point will be mounted: only mounts with options matching *all* filtering
// options will be selected.
//    By default each -O filter option must be present in the list of mount
// options, but if it is prefixed by "no" then it must be absent.
// For example,
//  -O a,nob,c  matches  -o a,c  but fails to match  -o a,b,c
//              (and also fails to match  -o a  because  -o c  is absent).
//
// It is different from -t in that each option is matched exactly; a leading
// "no" at the beginning of one option does not negate the rest.
int FAST_FUNC fsopts_matches(const char *fsopts, const char *O_opt)
{
	if (!O_opt)
		return 1;

	while (1) {
		const char *fs_opt = fsopts;
		int O_len;
		int inverted;

		// If option begins with "no" then treat as an inverted match:
		// matching is a failure
		inverted = 0;
		if (O_opt[0] == 'n' && O_opt[1] == 'o') {
			inverted = 1;
			O_opt += 2;
		}
		// Isolate current -O OPT
		O_len = strchrnul(O_opt, ',') - O_opt;
		// The lone "no" or "" is an error, matching ends with False
		if (O_len == 0)
			return 0;
		// Try to match current -O OPT against fsopts
		while (1) {
			if (strncmp(fs_opt, O_opt, O_len) == 0
			 && (fs_opt[O_len] == '\0' || fs_opt[O_len] == ',')
			) {
				if (inverted)
					return 0; // "no" prefix, but option found
				break; // current -O OPT found, go check next -O OPT
			}
			fs_opt = strchr(fs_opt, ',');
			if (!fs_opt) {
				if (!inverted)
					return 0; // match wanted but not found
				break; // "no" prefix, and option not found, go check next -O OPT
			}
			fs_opt++;
		}
		if (O_opt[O_len] == '\0') // end?
			break;
		// Step to next -O OPT
		O_opt += O_len + 1;
	}
	// If we get here then everything matched
	return 1;
}
