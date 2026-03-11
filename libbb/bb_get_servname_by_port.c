/*
 * Utility routines.
 *
 * Copyright (C) 2026 Denys Vlasenko
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
//kbuild:lib-$(CONFIG_NETSTAT) += bb_get_servname_by_port.o
//kbuild:lib-$(CONFIG_PSCAN) += bb_get_servname_by_port.o
#include "libbb.h"

//Rationale for existing:
//#define getservbyport dont_use_getservbyport_uses_global_buffer
//(for example: -280 bytes on musl, x86-32)

char* FAST_FUNC bb_get_servname_by_port(char **p_etc_services, int port, const char *type)
{
	char *sp;

	sp = *p_etc_services;
	if (!sp) {
//TODO: we need mmap_entire_file() for this use case!
		sp = xmalloc_open_read_close("/etc/services", NULL);
		if (!sp)
			return NULL;
		*p_etc_services = sp;
	}
	while (*sp) {
		const char *pnstr, *sp_end;
		char *end;
		unsigned n;

		sp = skip_whitespace(sp);
		if (*sp == '#')
			goto next;
		sp_end = skip_non_whitespace(sp);
		pnstr = skip_whitespace(sp_end);
		n = bb_strtou(pnstr, &end, 10);
		if (n != port || *end != '/')
			goto next;
		if (type) {
			end++;
			end = is_prefixed_with(end, type);
			if (!end
			 || !(isspace(*end) || *end == '\0'
			    || *end == '#') // glibc treats "http 80/tcp#COMMENT" (no space!) as valid
			) {
				goto next;
			}
		}
		return auto_string(xstrndup(sp, sp_end - sp));
 next:
		sp = strchrnul(sp, '\n');
	}
	return NULL;
}
