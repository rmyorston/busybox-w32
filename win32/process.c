#include "libbb.h"

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
	const char *interpr = parse_interpreter(prog);

	if (!interpr)
		return spawnveq(mode, prog, argv, envp);

	if (ENABLE_FEATURE_PREFER_APPLETS && !strcmp(interpr, "sh")) {
		const char **new_argv;
		int argc = 0;

		while (argv[argc])
			argc++;
		new_argv = malloc(sizeof(*argv)*(argc+2));
		memcpy(new_argv+1, argv, sizeof(*argv)*(argc+1));
		new_argv[0] = prog; /* pass absolute path */
		ret = mingw_spawn_applet(mode, "sh", new_argv, envp);
		free(new_argv);
	}
	else {
		char *path = xstrdup(getenv("PATH"));
		char *tmp = path;
		char *iprog = find_execable(interpr, &tmp);
		free(path);
		if (!prog) {
			errno = ENOENT;
			return -1;
		}
		ret = spawnveq(mode, iprog, argv, envp);
		free(iprog);
	}
	return ret;
}

pid_t
mingw_spawn_1(int mode, const char *cmd, const char *const *argv, const char *const *envp)
{
	int ret;

	if (ENABLE_FEATURE_PREFER_APPLETS &&
	    find_applet_by_name(cmd) >= 0)
		return mingw_spawn_applet(mode, cmd, argv++, envp);
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
	return mingw_spawn_1(P_NOWAIT, argv[0], (const char *const *)(argv+1), (const char *const *)environ);
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
