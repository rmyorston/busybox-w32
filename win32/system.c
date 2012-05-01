#include "libbb.h"

int mingw_system(const char *cmd)
{
	STARTUPINFO siStartInfo;
	PROCESS_INFORMATION piProcInfo;
	int success;
	int len, count;
	char *cmd_buff = NULL;
	const char *s;
	char *t;
	DWORD ret;

	if ( cmd == NULL ) {
		return 1;
	}

	/* count double quotes */
	count = 0;
	for ( s=cmd; *s; ++s ) {
		if ( *s == '"' ) {
			++count;
		}
	}

	len = strlen(cmd) + 10 + count;
	if ( (cmd_buff=malloc(len)) == NULL ) {
		return -1;
	}

	/* escape double quotes */
	strcpy(cmd_buff, "sh -c \"");
	for ( s=cmd,t=cmd_buff+strlen(cmd_buff); *s; ++s ) {
		if ( *s == '"' ) {
			*t++ = '\\';
		}
		*t++ = *s;
	}
	*t++ = '"';
	*t = '\0';

	/* Now create the child process */
	ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
	siStartInfo.cb           = sizeof(STARTUPINFO);
	siStartInfo.hStdInput    = GetStdHandle(STD_INPUT_HANDLE);
	siStartInfo.hStdOutput   = GetStdHandle(STD_OUTPUT_HANDLE);
	siStartInfo.hStdError    = GetStdHandle(STD_ERROR_HANDLE);
	siStartInfo.dwFlags      = STARTF_USESTDHANDLES;

	success = CreateProcess(NULL,
				(LPTSTR)cmd_buff,  /* command line */
				NULL,              /* process security attributes */
				NULL,              /* primary thread security attributes */
				TRUE,              /* handles are inherited */
				0,                 /* creation flags */
				NULL,              /* use parent's environment */
				NULL,              /* use parent's current directory */
				&siStartInfo,      /* STARTUPINFO pointer */
				&piProcInfo);      /* receives PROCESS_INFORMATION */

	if ( !success ) {
		free(cmd_buff);
		return 127;
	}

	free(cmd_buff);

	WaitForSingleObject(piProcInfo.hProcess, INFINITE);

	ret = 0;
	GetExitCodeProcess(piProcInfo.hProcess, &ret);

	CloseHandle(piProcInfo.hProcess);
	CloseHandle(piProcInfo.hThread);

	return WEXITSTATUS(ret);
}
