/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
#include "libbb.h"

void bb_perror_msg(const char *s, ...)
{
#if ENABLE_PLATFORM_MINGW32
	va_list p;
	char *errname = NULL;
	char *freeme = NULL;
	/* We add the error number to WSA errors, since strerror doesn't support them.
	   This has to be done here, since we need the resulting buffer to be freed,
	   and there is no way in mingw_strerror to do that without needing to
	   involve thread-locals */
	/* we have the range from 995 to 11031 */
	/* see https://learn.microsoft.com/en-us/windows/win32/winsock/windows-sockets-error-codes-2 */
	if (errno >= WSA_OPERATION_ABORTED && errno <= WSA_QOS_RESERVED_PETYPE)
		errname = freeme = xasprintf("%d: WSA error", errno);
	else if (errno)
		errname = strerror(errno);

	va_start(p, s);
	/* Guard against "<error message>: Success" */
	bb_verror_msg(s, p, errname);
	va_end(p);
	free(freeme);
#else
	va_list p;

	va_start(p, s);
	/* Guard against "<error message>: Success" */
	bb_verror_msg(s, p, errno ? strerror(errno) : NULL);
	va_end(p);
#endif
}

void bb_perror_msg_and_die(const char *s, ...)
{
#if ENABLE_PLATFORM_MINGW32
	va_list p;
	char *errname = NULL;
	char *freeme = NULL;
	/* see above */
	if (errno >= WSA_OPERATION_ABORTED && errno <= WSA_QOS_RESERVED_PETYPE)
		errname = freeme = xasprintf("%u: WSA error", errno);
	else if (errno)
		errname = strerror(errno);

	va_start(p, s);
	/* Guard against "<error message>: Success" */
	bb_verror_msg(s, p, errname);
	va_end(p);
	free(freeme);
#else
	va_list p;

	va_start(p, s);
	/* Guard against "<error message>: Success" */
	bb_verror_msg(s, p, errno ? strerror(errno) : NULL);
	va_end(p);
#endif
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
