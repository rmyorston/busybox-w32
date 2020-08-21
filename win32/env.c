#include "libbb.h"

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

/*
 * Removing an environment variable with WIN32 _putenv requires an argument
 * like "NAME="; glibc omits the '='.  The implementations of unsetenv and
 * clearenv allow for this.
 *
 * It isn't possible to create an environment variable with an empty value
 * using WIN32 _putenv.
 */
int unsetenv(const char *name)
{
	char *envstr;
	int ret;

	if (!name || !*name || strchr(name, '=') ) {
		return -1;
	}

	envstr = xasprintf("%s=", name);
	ret = _putenv(envstr);
	free(envstr);

	return ret;
}

int clearenv(void)
{
	char *envp, *name, *s;

	while ( environ && (envp=*environ) ) {
		if ( (s=strchr(envp, '=')) != NULL ) {
			name = xstrndup(envp, s-envp+1);
			if (_putenv(name) == -1) {
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
	char *s, **envp;
	int ret = 0;

	if ( (s=strchr(env, '=')) == NULL ) {
		return unsetenv(env);
	}

	if (s[1] != '\0') {
		/* setting non-empty value is fine */
		return _putenv(env);
	}
	else {
		/* set empty value by setting a non-empty one then truncating */
		char *envstr = xasprintf("%s0", env);
		ret = _putenv(envstr);

		for (envp = environ; *envp; ++envp) {
			if (strcmp(*envp, envstr) == 0) {
				(*envp)[s - env + 1] = '\0';
				break;
			}
		}
		free(envstr);
	}

	return ret;
}
