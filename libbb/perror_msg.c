/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
#include "libbb.h"

#if ENABLE_PLATFORM_MINGW32
/* Add the error number to WSA errors, since strerror doesn't support them.
 * The result may be in an allocated buffer.  If the caller needs to
 * preserve this, it should take a copy. */
static char *strerror_wsa(void)
{
	char *errname = NULL;
	static char *wsa_error = NULL;

	// The range is from 995 to 11031
	// https://learn.microsoft.com/en-us/windows/win32/winsock/windows-sockets-error-codes-2
	if (errno >= WSA_OPERATION_ABORTED && errno <= WSA_QOS_RESERVED_PETYPE) {
		free(wsa_error);
		errname = wsa_error = xasprintf("%d: WSA error", errno);
	} else if (errno) {
		errname = strerror(errno);
	}
	return errname;
}
#endif

void bb_perror_msg(const char *s, ...)
{
	va_list p;

	va_start(p, s);
	/* Guard against "<error message>: Success" */
#if ENABLE_PLATFORM_MINGW32
	bb_verror_msg(s, p, strerror_wsa());
#else
	bb_verror_msg(s, p, errno ? strerror(errno) : NULL);
#endif
	va_end(p);
}

void bb_perror_msg_and_die(const char *s, ...)
{
	va_list p;

	va_start(p, s);
	/* Guard against "<error message>: Success" */
#if ENABLE_PLATFORM_MINGW32
	bb_verror_msg(s, p, strerror_wsa());
#else
	bb_verror_msg(s, p, errno ? strerror(errno) : NULL);
#endif
	va_end(p);
	xfunc_die();
}

void FAST_FUNC bb_simple_perror_msg(const char *s)
{
	bb_perror_msg("%s", s);
}

void FAST_FUNC bb_simple_perror_msg_and_die(const char *s)
{
	bb_perror_msg_and_die("%s", s);
}
