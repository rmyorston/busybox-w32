#include "libbb.h"


#ifndef __WATCOMC__
#undef getenv
#undef putenv

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
	char *envstr;

	if (!name || !*name || strchr(name, '=') || !value) return -1;
	if (!replace) {
		if (getenv(name)) return 0;
	}

	envstr = xasprintf("%s=%s", name, value);
	out = mingw_putenv(envstr);
	free(envstr);

	return out;
}
#endif /* getenv setenv already present in watcom */

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

#ifndef __WATCOMC__
#if ENABLE_SAFE_ENV
/*
 * Removing an environment variable with WIN32 putenv requires an argument
 * like "NAME="; glibc omits the '='.  The implementations of unsetenv and
 * clearenv allow for this.
 *
 * It isn't possible to create an environment variable with an empty value
 * using WIN32 putenv.
 */
int unsetenv(const char *name)
{
	char *envstr;
	int ret;

	if (!name || !*name || strchr(name, '=') ) {
		return -1;
	}

	envstr = xmalloc(strlen(name)+2);
	strcat(strcpy(envstr, name), "=");
	ret = putenv(envstr);
	free(envstr);

	return ret;
}

int clearenv(void)
{
	char *envp, *name, *s;

	while ( environ && (envp=*environ) ) {
		if ( (s=strchr(envp, '=')) != NULL ) {
			name = xstrndup(envp, s-envp+1);
			if ( putenv(name) == -1 ) {
				free(name);
				return -1;
			}
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
#endif /* safe env */
#endif /* unsetenv clearenv already present in watcom */




