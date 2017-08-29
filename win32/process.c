#include "libbb.h"
#include <tlhelp32.h>

int waitpid(pid_t pid, int *status, int options)
{
	HANDLE proc;
	intptr_t ret;

	/* Windows does not understand parent-child */
	if (pid > 0 && options == 0) {
		if ( (proc=OpenProcess(SYNCHRONIZE|PROCESS_QUERY_INFORMATION,
						FALSE, pid)) != NULL ) {
			ret = _cwait(status, (intptr_t)proc, 0);
			CloseHandle(proc);
			return ret == -1 ? -1 : pid;
		}
	}
	errno = EINVAL;
	return -1;
}

const char *
next_path_sep(const char *path)
{
	static const char *from = NULL, *to;
	static int has_semicolon;
	int len = strlen(path);

	if (!from || !(path >= from && path+len <= to)) {
		from = path;
		to = from+len;
		has_semicolon = strchr(path, ';') != NULL;
	}

	/* Semicolons take precedence, it's Windows PATH */
	if (has_semicolon)
		return strchr(path, ';');
	/* PATH=C:, not really a separator */
	return strchr(has_dos_drive_prefix(path) ? path+2 : path, ':');
}

static char *
parse_interpreter(const char *cmd, char **name, char **opts)
{
	static char buf[100];
	char *path, *t;
	int n, fd;

	fd = open(cmd, O_RDONLY);
	if (fd < 0)
		return NULL;
	n = read(fd, buf, sizeof(buf)-1);
	close(fd);
	if (n < 4)	/* at least '#!/x' and not error */
		return NULL;

	/*
	 * See http://www.in-ulm.de/~mascheck/various/shebang/ for trivia
	 * relating to '#!'.
	 */
	if (buf[0] != '#' || buf[1] != '!')
		return NULL;
	buf[n] = '\0';
	if ((t=strchr(buf, '\n')) == NULL)
		return NULL;
	t[1] = '\0';

	if ((path=strtok(buf+2, " \t\r\n")) == NULL)
		return NULL;

	t = (char *)bb_basename(path);
	if (*t == '\0')
		return NULL;

	*name = t;
	*opts = strtok(NULL, "\r\n");

	return path;
}

/*
 * See http://msdn2.microsoft.com/en-us/library/17w5ykft(vs.71).aspx
 * (Parsing C++ Command-Line Arguments)
 */
static char *
quote_arg(const char *arg)
{
	int len = 0, n = 0;
	int force_quotes = 0;
	char *q, *d;
	const char *p = arg;

	/* empty arguments must be quoted */
	if (!*p) {
		force_quotes = 1;
	}

	while (*p) {
		if (isspace(*p)) {
			/* arguments containing whitespace must be quoted */
			force_quotes = 1;
		}
		else if (*p == '"') {
			/* double quotes in arguments need to be escaped */
			n++;
		}
		else if (*p == '\\') {
			/* count contiguous backslashes */
			int count = 0;
			while (*p == '\\') {
				count++;
				p++;
				len++;
			}

			/*
			 * Only escape backslashes before explicit double quotes or
			 * or where the backslashes are at the end of an argument
			 * that is scheduled to be quoted.
			 */
			if (*p == '"' || (force_quotes && *p == '\0')) {
				n += count*2 + 1;
			}

			if (*p == '\0') {
				break;
			}
			continue;
		}
		len++;
		p++;
	}

	if (!force_quotes && n == 0) {
		return (char*)arg;
	}

	/* insert double quotes and backslashes where necessary */
	d = q = xmalloc(len+n+3);
	if (force_quotes) {
		*d++ = '"';
	}

	while (*arg) {
		if (*arg == '"') {
			*d++ = '\\';
		}
		else if (*arg == '\\') {
			int count = 0;
			while (*arg == '\\') {
				count++;
				*d++ = *arg++;
			}

			if (*arg == '"' || (force_quotes && *arg == '\0')) {
				while (count-- > 0) {
					*d++ = '\\';
				}
				if (*arg == '"') {
					*d++ = '\\';
				}
			}
		}
		if (*arg != '\0') {
			*d++ = *arg++;
		}
	}
	if (force_quotes) {
		*d++ = '"';
	}
	*d = '\0';

	return q;
}

static char *
find_first_executable(const char *name)
{
	char *tmp, *path = getenv("PATH");
	char *exe_path = NULL;

	if (path) {
		tmp = path = xstrdup(path);
		exe_path = find_executable(name, &tmp);
		free(path);
	}

	return exe_path;
}

static intptr_t
spawnveq(int mode, const char *path, char *const *argv, char *const *env)
{
	char **new_argv;
	int i, argc = 0;
	intptr_t ret;

	if (!argv) {
		char *const empty_argv[] = { (char *)path, NULL };
		return spawnve(mode, path, empty_argv, env);
	}


	while (argv[argc])
		argc++;

	new_argv = malloc(sizeof(*argv)*(argc+1));
	for (i = 0;i < argc;i++)
		new_argv[i] = quote_arg(argv[i]);
	new_argv[argc] = NULL;
	ret = spawnve(mode, path, new_argv, env);
	for (i = 0;i < argc;i++)
		if (new_argv[i] != argv[i])
			free(new_argv[i]);
	free(new_argv);
	return ret;
}

#if ENABLE_FEATURE_PREFER_APPLETS || ENABLE_FEATURE_SH_STANDALONE
static intptr_t
mingw_spawn_applet(int mode,
		   char *const *argv,
		   char *const *envp)
{
	return spawnveq(mode, bb_busybox_exec_path, argv, envp);
}
#endif

static intptr_t
mingw_spawn_interpreter(int mode, const char *prog, char *const *argv, char *const *envp)
{
	intptr_t ret;
	int nopts;
	char *int_name, *opts;
	char *int_path = parse_interpreter(prog, &int_name, &opts);
	char **new_argv;
	int argc = 0;
	char *fullpath = NULL;

	if (!int_path)
		return spawnveq(mode, prog, argv, envp);

	nopts = opts != NULL;
	while (argv[argc])
		argc++;

	new_argv = xmalloc(sizeof(*argv)*(argc+nopts+2));
	new_argv[1] = opts;
	new_argv[nopts+1] = (char *)prog; /* pass absolute path */
	memcpy(new_argv+nopts+2, argv+1, sizeof(*argv)*argc);

	if (file_is_executable(int_path) ||
			(fullpath=file_is_win32_executable(int_path)) != NULL) {
		new_argv[0] = fullpath ? fullpath : int_path;
		ret = spawnveq(mode, new_argv[0], new_argv, envp);
	} else
#if ENABLE_FEATURE_PREFER_APPLETS || ENABLE_FEATURE_SH_STANDALONE
	if (find_applet_by_name(int_name) >= 0) {
		new_argv[0] = int_name;
		ret = mingw_spawn_applet(mode, new_argv, envp);
	} else
#endif
	if ((fullpath=find_first_executable(int_name)) != NULL) {
		new_argv[0] = fullpath;
		ret = spawnveq(mode, fullpath, new_argv, envp);
	}
	else {
		errno = ENOENT;
		ret = -1;
	}

	free(fullpath);
	free(new_argv);
	return ret;
}

static intptr_t
mingw_spawn_1(int mode, const char *cmd, char *const *argv, char *const *envp)
{
	char *prog;

#if ENABLE_FEATURE_PREFER_APPLETS || ENABLE_FEATURE_SH_STANDALONE
	if (find_applet_by_name(cmd) >= 0)
		return mingw_spawn_applet(mode, argv, envp);
	else
#endif
	if (strchr(cmd, '/') || strchr(cmd, '\\')) {
		return mingw_spawn_interpreter(mode, cmd, argv, envp);
	}
	else if ((prog=find_first_executable(cmd)) != NULL) {
		intptr_t ret = mingw_spawn_interpreter(mode, prog, argv, envp);
		free(prog);
		return ret;
	}

	errno = ENOENT;
	return -1;
}

pid_t FAST_FUNC
mingw_spawn(char **argv)
{
	intptr_t ret;

	ret = mingw_spawn_1(P_NOWAIT, argv[0], (char *const *)argv, environ);

	return ret == -1 ? -1 : GetProcessId((HANDLE)ret);
}

intptr_t FAST_FUNC
mingw_spawn_proc(const char **argv)
{
	return mingw_spawn_1(P_NOWAIT, argv[0], (char *const *)argv, environ);
}

int
mingw_execvp(const char *cmd, char *const *argv)
{
	int ret = (int)mingw_spawn_1(P_WAIT, cmd, argv, environ);
	if (ret != -1)
		exit(ret);
	return ret;
}

int
mingw_execve(const char *cmd, char *const *argv, char *const *envp)
{
	int ret;
	int mode = P_WAIT;

	ret = (int)mingw_spawn_interpreter(mode, cmd, argv, envp);
	if (ret != -1)
		exit(ret);
	return ret;
}

int
mingw_execv(const char *cmd, char *const *argv)
{
	return mingw_execve(cmd, argv, environ);
}

/* POSIX version in libbb/procps.c */
procps_status_t* FAST_FUNC procps_scan(procps_status_t* sp, int flags UNUSED_PARAM)
{
	PROCESSENTRY32 pe;

	pe.dwSize = sizeof(pe);
	if (!sp) {
		sp = xzalloc(sizeof(struct procps_status_t));
		sp->snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (sp->snapshot == INVALID_HANDLE_VALUE) {
			free(sp);
			return NULL;
		}
		if (!Process32First(sp->snapshot, &pe)) {
			CloseHandle(sp->snapshot);
			free(sp);
			return NULL;
		}
	}
	else {
		if (!Process32Next(sp->snapshot, &pe)) {
			CloseHandle(sp->snapshot);
			free(sp);
			return NULL;
		}
	}

	sp->pid = pe.th32ProcessID;
	safe_strncpy(sp->comm, pe.szExeFile, COMM_LEN);
	return sp;
}

int kill(pid_t pid, int sig)
{
	HANDLE h;

	if (pid > 0 && sig == SIGTERM) {
		if ((h=OpenProcess(PROCESS_TERMINATE, FALSE, pid)) != NULL &&
				TerminateProcess(h, 0)) {
			CloseHandle(h);
			return 0;
		}

		errno = err_win_to_posix(GetLastError());
		if (h != NULL)
			CloseHandle(h);
		return -1;
	}

	errno = EINVAL;
	return -1;
}
