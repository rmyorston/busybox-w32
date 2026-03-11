/*
 * Utility routines.
 *
 * Copyright (C) 2026 Denys Vlasenko
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
//kbuild:lib-y += bb_get_servport_by_name.o
//kbuild:lib-y += bb_get_servport_by_name.o
#include "libbb.h"

//Rationale for existing:
//#define getservbyname dont_use_getservbyname_uses_global_buffer
//(for example: -32 BSS bytes on musl, x86-32)

int FAST_FUNC bb_get_servport_by_name(char **p_etc_services, const char *name, const char *proto)
{
	unsigned namelen;
	char *sp;

	if (!name[0]) // This can hang the search (strstr advances by 0 bytes)
		return -1; // bad service name form: ""
	// Any other bogosity to reject?
	// Or just enforce isalnum_or_dash_or_slash(name[i])?
	// Yes, service name like "cl/1" is allowed and _exists_.
	//
	// Protect against finding service "www#c" in "http 10/tcp www#c" line
	//if (strchr(name, '#'))
	//	return -1; // bad service name form
	//^^^^ the main code already catches this possibility.
	//
	// Current code allows e.g. service names like "http 80/tcp"
	// to map to port 80. Probably harmless?
	//if (skip_non_whitespace(name)[0])
	//	return -1; // bad service name form (has whitespace)

	sp = *p_etc_services;
	if (!sp) {
//TODO: we need mmap_entire_file() for this use case!
		sp = xmalloc_open_read_close("/etc/services", NULL);
		if (!sp)
			return -1;
		*p_etc_services = sp;
	}
	namelen = strlen(name);
	while (*sp) {
		const char *pnstr;
		char *start, *end;
		unsigned n;

		// First, find a possible service name without regard for line separators (!)
		start = strstr(sp, name);
		if (!start)
			return -1;
		sp = start + namelen;
		if (start != *p_etc_services && !isspace(start[-1])) {
			// There is a char before it, and it's not whitespace
			continue;
		}
		if (!(isspace(*sp) || *sp == '\0' || *sp == '#'))
			// After it: not whitespace/EOF/comment
			continue;
		// The found substring _is_ correctly delimited before and after

		// Find beginning of the line we are on
		while (start != *p_etc_services && start[-1] != '\n')
			start--;
		start = skip_whitespace(start);
		// Is there a comment char between start of line and "service name"?
		if (memchr(start, '#', sp - start)) {
			// The "service name" we found is actually in comment / has comment in it.
			// Also rejects names with #:
			// service "www#c" won't be found even on "http 80/tcp www#c" line
			continue;
		}
		// Is the [start...sp) of the form "SERVNAME NUM/PROTO[ ALIAS[ ALIAS...][ ]]"?
		pnstr = skip_whitespace(skip_non_whitespace(start)); // go to NUM...

		// I've seen this line in /etc/services of a real-world machine:
		//914c/g 211/tcp 914c-g
		// Now consider just a bit more pathological example:
		//914c/tcp 914/tcp 914/tcp
		// If we search for service name "914/tcp",
		// we'll find it at second word first, yet we must not reject this line
		// because the THIRD word also matches, and it's a valid match (tested on glibc!).
		// But in this case we must not match:
		//914c/tcp 914/tcp something-else
		if (sp - namelen == pnstr)
			continue; // the match is at NUM..., try matching further

		n = bb_strtou(pnstr, &end, 10);
		if (n > 0xffff || *end != '/')
			continue; // NUM... part is not a valid port#, or has no slash

		if (proto) {
			end++; // points to PROTO
			end = is_prefixed_with(end, proto);
			if (!end
			 || !(isspace(*end) || *end == '\0'
			    || *end == '#') // glibc treats "http 80/tcp#COMMENT" (no space!) as valid
			) {
				continue; // PROTO does not match
			}
		}
		// By now, either WORD or one of the ALIASes has to be the part
		// which was found by strstr()!
		return n;
	}
	return -1;
}

// Return port number for a service.
// If "port" is a number use it as the port.
// If "port" is a name it is looked up in /etc/services.
// if NULL, return default_port
unsigned FAST_FUNC bb_lookup_port(const char *port, const char *protocol, unsigned port_nr)
{
	if (port) {
		port_nr = bb_strtou(port, NULL, 10);
		if (errno || port_nr > 65535) {
			char *p_etc_services = NULL;
			port_nr = bb_get_servport_by_name(&p_etc_services, port, protocol);
			if (port_nr > 0xffff) // -1?
				bb_error_msg_and_die("bad port '%s'", port);
			free(p_etc_services);
		}
	}
	return (uint16_t)port_nr;
}
