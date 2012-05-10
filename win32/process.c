#include "libbb.h"
#include <tlhelp32.h>

int waitpid(pid_t pid, int *status, unsigned options)
{
	/* Windows does not understand parent-child */
	if (options == 0 && pid != -1)
		return _cwait(status, pid, 0);
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

#define MAX_OPT 10

static const char *
parse_interpreter(const char *cmd, char ***opts, int *nopts)
{
	static char buf[100], *opt[MAX_OPT];
	char *p, *s, *t;
	int n, fd;

	*nopts = 0;
	*opts = opt;

	/* don't even try a .exe */
	n = strlen(cmd);
	if (n >= 4 &&
	    (!strcasecmp(cmd+n-4, ".exe") ||
	     !strcasecmp(cmd+n-4, ".com")))
		return NULL;

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
	p = strchr(buf, '\n');
	if (!p)
		return NULL;
	*p = '\0';

	/* remove trailing whitespace */
	while ( isspace(*--p) ) {
		*p = '\0';
	}

	/* skip whitespace after '#!' */
	for ( s=buf+2; *s && isspace(*s); ++s ) {
	}

	/* move to end of interpreter path (which may not contain spaces) */
	for ( ; *s && !isspace(*s); ++s ) {
	}

	n = 0;
	if ( *s != '\0' ) {
		/* there are options */
		*s++ = '\0';

		while ( (t=strtok(s, " \t")) && n < MAX_OPT ) {
			s = NULL;
			opt[n++] = t;
		}
	}

	/* find interpreter name */
	if (!(p = strrchr(buf+2, '/')))
		return NULL;

	*nopts = n;
	*opts = opt;

	return p+1;
}

/*
 * See http://msdn2.microsoft.com/en-us/library/17w5ykft(vs.71).aspx
 * (Parsing C++ Command-Line Arguments)
 */
static char *
quote_arg(const char *arg)
{
	/* count chars to quote */
	int len = 0, n = 0;
	int force_quotes = 0;
	char *q, *d;
	const char *p = arg;
	if (!*p) force_quotes = 1;
	while (*p) {
		if (isspace(*p) || *p == '*' || *p == '?' || *p == '{' || *p == '\'')
			force_quotes = 1;
		else if (*p == '"')
			n++;
		else if (*p == '\\') {
			int count = 0;
			while (*p == '\\') {
				count++;
				p++;
				len++;
			}
			if (*p == '"')
				n += count*2 + 1;
			else
				n += count;
			continue;
		}
		len++;
		p++;
	}
	if (!force_quotes && n == 0)
		return (char*)arg;

	/* insert \ where necessary */
	d = q = xmalloc(len+n+3);
	*d++ = '"';
	while (*arg) {
		if (*arg == '"')
			*d++ = '\\';
		else if (*arg == '\\') {
			int count = 0;
			while (*arg == '\\') {
				count++;
				*d++ = *arg++;
			}
			while (count-- > 0)
				*d++ = '\\';
			if (*arg == '"') {
				*d++ = '\\';
			}
		}
		*d++ = *arg++;
	}
	*d++ = '"';
	*d++ = 0;
	return q;
}

static pid_t
spawnveq(int mode, const char *path, const char *const *argv, const char *const *env)
{
	char **new_argv;
	int i, argc = 0;
	pid_t ret;

	if (!argv) {
		const char *empty_argv[] = { path, NULL };
		return spawnve(mode, path, empty_argv, env);
	}


	while (argv[argc])
		argc++;

	new_argv = malloc(sizeof(*argv)*(argc+1));
	for (i = 0;i < argc;i++)
		new_argv[i] = quote_arg(argv[i]);
	new_argv[argc] = NULL;
	ret = spawnve(mode, path, (const char *const *)new_argv, env);
	for (i = 0;i < argc;i++)
		if (new_argv[i] != argv[i])
			free(new_argv[i]);
	free(new_argv);
	return ret;
}

pid_t
mingw_spawn_applet(int mode,
		   const char *applet,
		   const char *const *argv,
		   const char *const *envp)
{
	char **env = copy_environ(envp);
	char path[MAX_PATH+20];
	int ret;

	sprintf(path, "BUSYBOX_APPLET_NAME=%s", applet);
	env = env_setenv(env, path);
	ret = spawnveq(mode, get_busybox_exec_path(), argv, (const char *const *)env);
	free_environ(env);
	return ret;
}

static pid_t
mingw_spawn_interpreter(int mode, const char *prog, const char *const *argv, const char *const *envp)
{
	int ret;
	char **opts;
	int nopts;
	const char *interpr = parse_interpreter(prog, &opts, &nopts);
	const char **new_argv;
	int argc = 0;

	if (!interpr)
		return spawnveq(mode, prog, argv, envp);


	while (argv[argc])
		argc++;
	new_argv = malloc(sizeof(*argv)*(argc+nopts+2));
	memcpy(new_argv+1, opts, sizeof(*opts)*nopts);
	memcpy(new_argv+nopts+2, argv+1, sizeof(*argv)*argc);
	new_argv[nopts+1] = prog; /* pass absolute path */

	if (ENABLE_FEATURE_PREFER_APPLETS && find_applet_by_name(interpr) >= 0) {
		new_argv[0] = interpr;
		ret = mingw_spawn_applet(mode, interpr, new_argv, envp);
	}
	else {
		char *path = xstrdup(getenv("PATH"));
		char *tmp = path;
		char *iprog = find_execable(interpr, &tmp);
		free(path);
		if (!iprog) {
			free(new_argv);
			errno = ENOENT;
			return -1;
		}
		new_argv[0] = iprog;
		ret = spawnveq(mode, iprog, new_argv, envp);
		free(iprog);
	}

	free(new_argv);
	return ret;
}

pid_t
mingw_spawn_1(int mode, const char *cmd, const char *const *argv, const char *const *envp)
{
	int ret;

	if (ENABLE_FEATURE_PREFER_APPLETS &&
	    find_applet_by_name(cmd) >= 0)
		return mingw_spawn_applet(mode, cmd, argv, envp);
	else if (is_absolute_path(cmd))
		return mingw_spawn_interpreter(mode, cmd, argv, envp);
	else {
		char *tmp, *path = getenv("PATH");
		char *prog;

		if (!path) {
			errno = ENOENT;
			return -1;
		}

		/* exists_execable() does not return new file name */
		tmp = path = xstrdup(path);
		prog = find_execable(cmd, &tmp);
		free(path);
		if (!prog) {
			errno = ENOENT;
			return -1;
		}
		ret = mingw_spawn_interpreter(mode, prog, argv, envp);
		free(prog);
	}
	return ret;
}

pid_t
mingw_spawn(char **argv)
{
	return mingw_spawn_1(P_NOWAIT, argv[0], (const char *const *)argv, (const char *const *)environ);
}

int
mingw_execvp(const char *cmd, const char *const *argv)
{
	int ret = (int)mingw_spawn_1(P_WAIT, cmd, argv, (const char *const *)environ);
	if (ret != -1)
		exit(ret);
	return ret;
}

int
mingw_execve(const char *cmd, const char *const *argv, const char *const *envp)
{
	int ret;
	int mode = P_WAIT;

	if (ENABLE_FEATURE_PREFER_APPLETS &&
	    find_applet_by_name(cmd) >= 0)
		ret = mingw_spawn_applet(mode, cmd, argv, envp);
	/*
	 * execve(bb_busybox_exec_path, argv, envp) won't work
	 * because argv[0] will be replaced to bb_busybox_exec_path
	 * by MSVC runtime
	 */
	else if (argv && cmd != argv[0] && cmd == bb_busybox_exec_path)
		ret = mingw_spawn_applet(mode, argv[0], argv, envp);
	else
		ret = mingw_spawn_interpreter(mode, cmd, argv, envp);
	if (ret != -1)
		exit(ret);
	return ret;
}

int
mingw_execv(const char *cmd, const char *const *argv)
{
	return mingw_execve(cmd, argv, (const char *const *)environ);
}

/* POSIX version in libbb/procps.c */
procps_status_t* FAST_FUNC procps_scan(procps_status_t* sp, int flags)
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
	strncpy(sp->comm, pe.szExeFile, COMM_LEN);
	return sp;
}

int kill(pid_t pid, int sig)
{
	HANDLE h;

	if (sig != SIGTERM) {
		bb_error_msg("kill only supports SIGTERM");
		errno = EINVAL;
		return -1;
	}
	h = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
	if (h == NULL)
		return -1;
	if (TerminateProcess(h, 0) == 0)
		return -1;
	return 0;
}
