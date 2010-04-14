#include "libbb.h"

int waitpid(pid_t pid, int *status, unsigned options)
{
	/* Windows does not understand parent-child */
	if (options == 0 && pid != -1)
		return _cwait(status, pid, 0);
	errno = EINVAL;
	return -1;
}

/*
 * Splits the PATH into parts.
 */
static char **
get_path_split(void)
{
	char *p, **path, *envpath = getenv("PATH");
	int i, n = 0;

	if (!envpath || !*envpath)
		return NULL;

	envpath = xstrdup(envpath);
	p = envpath;
	while (p) {
		char *dir = p;
		p = strchr(p, ';');
		if (p) *p++ = '\0';
		if (*dir) {	/* not earlier, catches series of ; */
			++n;
		}
	}
	if (!n)
		return NULL;

	path = xmalloc((n+1)*sizeof(char*));
	p = envpath;
	i = 0;
	do {
		if (*p)
			path[i++] = xstrdup(p);
		p = p+strlen(p)+1;
	} while (i < n);
	path[i] = NULL;

	free(envpath);

	return path;
}

static void
free_path_split(char **path)
{
	if (path) {
		char **p = path;

		while (*p)
			free(*p++);
		free(path);
	}
}

/*
 * exe_only means that we only want to detect .exe files, but not scripts
 * (which do not have an extension)
 */
static char *
lookup_prog(const char *dir, const char *cmd, int isexe, int exe_only)
{
	char path[MAX_PATH];
	snprintf(path, sizeof(path), "%s/%s.exe", dir, cmd);

	if (!isexe && access(path, F_OK) == 0)
		return xstrdup(path);
	path[strlen(path)-4] = '\0';
	if ((!exe_only || isexe) && access(path, F_OK) == 0)
		if (!(GetFileAttributes(path) & FILE_ATTRIBUTE_DIRECTORY))
			return xstrdup(path);
	return NULL;
}

/*
 * Determines the absolute path of cmd using the the split path in path.
 * If cmd contains a slash or backslash, no lookup is performed.
 */
static char *
path_lookup(const char *cmd, char **path, int exe_only)
{
	char *prog = NULL;
	int len = strlen(cmd);
	int isexe = len >= 4 && !strcasecmp(cmd+len-4, ".exe");

	if (strchr(cmd, '/') || strchr(cmd, '\\')) {
		if (!isexe) {
			char path_exe[MAX_PATH];
			sprintf(path_exe, "%s.exe", cmd);
			if (!access(path_exe, F_OK))
				return xstrdup(path_exe);
		}
		prog = xstrdup(cmd);
	}

	while (!prog && *path)
		prog = lookup_prog(*path++, cmd, isexe, exe_only);

	return prog;
}

static const char *
parse_interpreter(const char *cmd)
{
	static char buf[100];
	char *p, *opt;
	int n, fd;

	/* don't even try a .exe */
	n = strlen(cmd);
	if (n >= 4 && !strcasecmp(cmd+n-4, ".exe"))
		return NULL;

	fd = open(cmd, O_RDONLY);
	if (fd < 0)
		return NULL;
	n = read(fd, buf, sizeof(buf)-1);
	close(fd);
	if (n < 4)	/* at least '#!/x' and not error */
		return NULL;

	if (buf[0] != '#' || buf[1] != '!')
		return NULL;
	buf[n] = '\0';
	p = strchr(buf, '\n');
	if (!p)
		return NULL;

	*p = '\0';
	if (!(p = strrchr(buf+2, '/')) && !(p = strrchr(buf+2, '\\')))
		return NULL;
	/* strip options */
	if ((opt = strchr(p+1, ' ')))
		*opt = '\0';
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
			if (*arg == '"') {
				while (count-- > 0)
					*d++ = '\\';
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
		   const char *const *envp,
		   int transfer_fd)
{
	char **env = copy_environ(envp);
	char path[MAX_PATH+20];
	int ret;

	sprintf(path, "BUSYBOX_APPLET_NAME=%s", applet);
	env = env_setenv(env, path);
	if (transfer_fd) {
		sprintf(path, "BUSYBOX_ASH_TRANSFER=%x", (int)_get_osfhandle(transfer_fd));
		env = env_setenv(env, path);
	}
	ret = spawnveq(mode, get_busybox_exec_path(), argv, (const char *const *)env);
	free_environ(env);
	return ret;
}

static pid_t
mingw_spawn_interpreter(int mode, const char *prog, const char *const *argv, char **path, const char *const *envp)
{
	const char *interpr = parse_interpreter(prog);

	if (!interpr)
		return spawnveq(mode, prog, argv, envp);

	if (ENABLE_FEATURE_PREFER_APPLETS && !strcmp(interpr, "sh")) {
		int ret;
		const char **new_argv;
		int argc = 0;

		while (argv[argc])
			argc++;
		new_argv = malloc(sizeof(*argv)*(argc+2));
		memcpy(new_argv+1, argv, sizeof(*argv)*(argc+1));
		new_argv[0] = prog; /* pass absolute path */
		ret = mingw_spawn_applet(mode, "sh", new_argv, envp, 0);
		free(new_argv);
		return ret;
	}
	else {
		int ret;
		char *iprog = path_lookup(interpr, path, 1);
		if (!iprog) {
			errno = ENOENT;
			return -1;
		}
		ret = spawnveq(mode, iprog, argv, envp);
		free(iprog);
		return ret;
	}
}

pid_t
mingw_spawn_1(int mode, const char *cmd, const char *const *argv, const char *const *envp)
{
	int ret;

	if (ENABLE_FEATURE_PREFER_APPLETS &&
	    find_applet_by_name(cmd) >= 0)
		return mingw_spawn_applet(mode, cmd, argv++, envp, 0);
	else {
		char **path = get_path_split();
		char *prog = path_lookup(cmd, path, 0);

		if (prog) {
			ret = mingw_spawn_interpreter(mode, prog, argv, path, envp);
			free(prog);
			free_path_split(path);
			return ret;
		}
		else {
			errno = ENOENT;
			free_path_split(path);
			return -1;
		}
	}
	return ret;
}

pid_t
mingw_spawn(char **argv)
{
	return mingw_spawn_1(P_NOWAIT, argv[0], (const char *const *)(argv+1), (const char *const *)environ);
}

int
mingw_execvp(const char *cmd, const char *const *argv)
{
	int ret = (int)mingw_spawn_1(P_WAIT, cmd, argv,
				     (const char *const *)environ);
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
		ret = mingw_spawn_applet(mode, cmd, argv++, envp, 0);
	else {
		char **path = get_path_split();
		ret = mingw_spawn_interpreter(mode, cmd, argv, path, envp);
		free_path_split(path);
	}
	if (ret != -1)
		exit(ret);
	return ret;
}

int
mingw_execv(const char *cmd, const char *const *argv)
{
	return mingw_execve(cmd, argv, (const char *const *)environ);
}
