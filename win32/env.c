#include "libbb.h"

char **copy_environ(char *const *envp)
{
	char **env;
	int i = 0;
	while (envp[i])
		i++;
	env = xmalloc((i+1)*sizeof(*env));
	for (i = 0; envp[i]; i++)
		env[i] = xstrdup(envp[i]);
	env[i] = NULL;
	return env;
}

void free_environ(char **env)
{
	int i;
	for (i = 0; env[i]; i++)
		free(env[i]);
	free(env);
}

static int lookup_env(char **env, const char *name, size_t nmln)
{
	int i;

	for (i = 0; env[i]; i++) {
		if (0 == strncmp(env[i], name, nmln)
		    && '=' == env[i][nmln])
			/* matches */
			return i;
	}
	return -1;
}

#undef getenv
char *mingw_getenv(const char *name)
{
	char *result = getenv(name);
	if (!result && !strcmp(name, "TMPDIR")) {
		/* on Windows it is TMP and TEMP */
		result = getenv("TMP");
		if (!result)
			result = getenv("TEMP");
	}
	return result;
}

int setenv(const char *name, const char *value, int replace)
{
	int out;
	size_t namelen, valuelen;
	char *envstr;

	if (!name || !value) return -1;
	if (!replace) {
		char *oldval = NULL;
		oldval = getenv(name);
		if (oldval) return 0;
	}

	namelen = strlen(name);
	valuelen = strlen(value);
	envstr = malloc((namelen + valuelen + 2));
	if (!envstr) return -1;

	memcpy(envstr, name, namelen);
	envstr[namelen] = '=';
	memcpy(envstr + namelen + 1, value, valuelen);
	envstr[namelen + valuelen + 1] = 0;

	out = putenv(envstr);
	/* putenv(3) makes the argument string part of the environment,
	 * and changing that string modifies the environment --- which
	 * means we do not own that storage anymore.  Do not free
	 * envstr.
	 */

	return out;
}

/*
 * If name contains '=', then sets the variable, otherwise it unsets it
 */
char **env_setenv(char **env, const char *name)
{
	char *eq = strchrnul(name, '=');
	int i = lookup_env(env, name, eq-name);

	if (i < 0) {
		if (*eq) {
			for (i = 0; env[i]; i++)
				;
			env = xrealloc(env, (i+2)*sizeof(*env));
			env[i] = xstrdup(name);
			env[i+1] = NULL;
		}
	}
	else {
		free(env[i]);
		if (*eq)
			env[i] = xstrdup(name);
		else {
			for (; env[i]; i++)
				env[i] = env[i+1];
#if !ENABLE_SAFE_ENV
			SetEnvironmentVariable(name, NULL);
#endif
		}
	}
	return env;
}

#if ENABLE_SAFE_ENV
/*
 * Removing an environment variable with WIN32 putenv requires an argument
 * like "NAME="; glibc omits the '='.  The implementations of unsetenv and
 * clearenv allow for this.
 *
 * It isn't possible to create an environment variable with an empty value
 * using WIN32 putenv.
 */
#undef putenv
int unsetenv(const char *env)
{
	char *name;
	int ret;

	name = xmalloc(strlen(env)+2);
	strcat(strcpy(name, env), "=");
	ret = putenv(name);
	free(name);

	return ret;
}

int clearenv(void)
{
	char *name, *s;

	while ( environ && *environ ) {
		if ( (s=strchr(*environ, '=')) != NULL ) {
			name = xstrndup(*environ, s-*environ+1);
			putenv(name);
			free(name);
		}
		else {
			return -1;
		}
	}
	return 0;
}

int mingw_putenv(const char *env)
{
	char *s;

	if ( (s=strchr(env, '=')) == NULL ) {
		return unsetenv(env);
	}

	if ( s[1] != '\0' ) {
		return putenv(env);
	}

	/* can't set empty value */
	return 0;
}
#else
void unsetenv(const char *env)
{
	env_setenv(environ, env);
}

int clearenv(void)
{
	char **env = environ;
	if (!env)
		return 0;
	while (*env) {
		free(*env);
		env++;
	}
	free(env);
	environ = NULL;
	return 0;
}
#endif
