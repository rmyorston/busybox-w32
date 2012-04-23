#include <fcntl.h>
#include "libbb.h"

typedef struct {
	PROCESS_INFORMATION piProcInfo;
	HANDLE in[2], out[2], err[2];
	char mode;
	FILE *fd;
} pipe_data;

static pipe_data *pipes = NULL;
static int num_pipes = 0;

static int mingw_pipe(HANDLE *readwrite)
{
	SECURITY_ATTRIBUTES sa;

	sa.nLength = sizeof(sa);          /* Length in bytes */
	sa.bInheritHandle = 1;            /* the child must inherit these handles */
	sa.lpSecurityDescriptor = NULL;

	if ( !CreatePipe (&readwrite[0], &readwrite[1], &sa, 1 << 13) ) {
		return -1;
	}

	return 0;
}

FILE *mingw_popen(const char *cmd, const char *mode)
{
	int i;
	pipe_data *p = NULL;
	FILE *fptr = NULL;
	STARTUPINFO siStartInfo;
	int success;
	int fd;
	int len, count;
	char *cmd_buff = NULL;
	const char *s;
	char *t;

	if ( cmd == NULL || *cmd == '\0' || mode == NULL ||
			(*mode != 'r' && *mode != 'w') ) {
		return NULL;
	}

	/* find an unused pipe structure */
	for ( i=0; i<num_pipes; ++i ) {
		if ( pipes[i].mode == '\0' ) {
			p = pipes+i;
			break;
		}
	}

	if ( p == NULL ) {
		/* need to extend array */
		if ( (p=realloc(pipes, sizeof(pipe_data)*(num_pipes+10))) == NULL ) {
			return NULL;
		}

		pipes = p;
		for ( i=0; i<10; ++i ) {
			memset(pipes+num_pipes+i, 0, sizeof(pipe_data));
		}
		p = pipes + num_pipes;
		num_pipes += 10;
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
		return NULL;
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

	p->in[0]   = INVALID_HANDLE_VALUE;
	p->in[1]   = INVALID_HANDLE_VALUE;
	p->out[0]  = INVALID_HANDLE_VALUE;
	p->out[1]  = INVALID_HANDLE_VALUE;
	p->err[0]  = INVALID_HANDLE_VALUE;
	p->err[1]  = INVALID_HANDLE_VALUE;

	/* Create the Pipes... */
	if ( mingw_pipe(p->in)  == -1 ||
			mingw_pipe(p->out) == -1 ||
			mingw_pipe(p->err) == -1) {
		goto finito;
	}

	/* Make the parent ends of the pipes non-inheritable */
	SetHandleInformation(p->in[1], HANDLE_FLAG_INHERIT, 0);
	SetHandleInformation(p->out[0], HANDLE_FLAG_INHERIT, 0);
	SetHandleInformation(p->err[0], HANDLE_FLAG_INHERIT, 0);

	/* Now create the child process */
	ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
	siStartInfo.cb           = sizeof(STARTUPINFO);
	siStartInfo.hStdInput    = p->in[0];
	siStartInfo.hStdOutput   = p->out[1];
	siStartInfo.hStdError    = p->err[1];
	siStartInfo.dwFlags      = STARTF_USESTDHANDLES;

	success = CreateProcess(NULL,
				(LPTSTR)cmd_buff,  /* command line */
				NULL,              /* process security attributes */
				NULL,              /* primary thread security attributes */
				TRUE,              /* handles are inherited */
				CREATE_NO_WINDOW,  /* creation flags */
				NULL,              /* use parent's environment */
				NULL,              /* use parent's current directory */
				&siStartInfo,      /* STARTUPINFO pointer */
				&p->piProcInfo);   /* receives PROCESS_INFORMATION */

	if ( !success ) {
		goto finito;
	}

	/* Close the child ends of the pipes */
	CloseHandle(p->in[0]);  p->in[0]  = INVALID_HANDLE_VALUE;
	CloseHandle(p->out[1]); p->out[1] = INVALID_HANDLE_VALUE;
	CloseHandle(p->err[1]); p->err[1] = INVALID_HANDLE_VALUE;

	if ( *mode == 'r' ) {
		fd = _open_osfhandle((long)p->out[0], _O_RDONLY|_O_BINARY);
		fptr = _fdopen(fd, "rb");
	}
	else {
		fd = _open_osfhandle((long)p->in[1], _O_WRONLY|_O_BINARY);
		fptr = _fdopen(fd, "wb");
	}

finito:
  if ( !fptr ) {
		if ( p->in[0]  != INVALID_HANDLE_VALUE ) {
			CloseHandle(p->in[0]);
		}
		if ( p->in[1]  != INVALID_HANDLE_VALUE ) {
			CloseHandle(p->in[1]);
		}
		if ( p->out[0] != INVALID_HANDLE_VALUE ) {
			CloseHandle(p->out[0]);
		}
		if ( p->out[1] != INVALID_HANDLE_VALUE ) {
			CloseHandle(p->out[1]);
		}
		if ( p->err[0] != INVALID_HANDLE_VALUE ) {
			CloseHandle(p->err[0]);
		}
		if ( p->err[1] != INVALID_HANDLE_VALUE ) {
			CloseHandle(p->err[1]);
		}
	}
	else {
		p->mode = *mode;
	}
	free(cmd_buff);

	p->fd = fptr;
	return fptr;
}

int mingw_pclose(FILE *fd)
{
	int i;
	pipe_data *p = NULL;
	DWORD ret;

	if ( fd == NULL ) {
		return -1;
	}

	/* find struct containing fd */
	for ( i=0; i<num_pipes; ++i ) {
		if ( pipes[i].fd == fd ) {
			p = pipes+i;
			break;
		}
	}

	if ( p == NULL ) {
		/* no pipe data, maybe fd isn't a pipe? */
		return -1;
	}

	fclose(fd);

	CloseHandle(p->err[0]);
	if ( p->mode == 'r' ) {
		CloseHandle(p->in[1]);
	}
	else {
		CloseHandle(p->out[0]);
	}

	ret = WaitForSingleObject(p->piProcInfo.hProcess, INFINITE);

	CloseHandle(p->piProcInfo.hProcess);
	CloseHandle(p->piProcInfo.hThread);

	p->mode = '\0';

	return (ret == WAIT_OBJECT_0) ? 0 : -1;
}
