// Windows (MinGW-w64): the POSIX calls MinGW lacks (see windows_compat.h).

#include <stdlib.h>
#include <string.h>
#include <errno.h>

int setenv(const char *name, const char *value, int overwrite) {
    if (name == NULL || *name == 0 || strchr(name, '=') != NULL) {
        errno = EINVAL;
        return -1;
    }
    if (!overwrite && getenv(name) != NULL) {
        return 0;
    }
    return _putenv_s(name, value != NULL ? value : "") == 0 ? 0 : -1;
}

int unsetenv(const char *name) {
    if (name == NULL || *name == 0 || strchr(name, '=') != NULL) {
        errno = EINVAL;
        return -1;
    }
    // An empty value removes the variable on Windows.
    return _putenv_s(name, "") == 0 ? 0 : -1;
}

// Absolute path; Windows has _fullpath (no symlinks to resolve under
// AM_NO_SYMLINKS).
char *realpath(const char *path, char *resolved) {
    return _fullpath(resolved, path, PATH_MAX);
}
