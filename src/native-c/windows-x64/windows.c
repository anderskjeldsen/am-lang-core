// Windows (MinGW-w64): the POSIX calls MinGW lacks (see windows_compat.h).

#include <stdlib.h>
#include <string.h>
#include <errno.h>

// _putenv("NAME=value"), not _putenv_s: the old msvcrt.dll (Windows XP,
// ReactOS -- the windows-x86 platform) has no _putenv_s.
static int put_env(const char *name, const char *value) {
    size_t n = strlen(name), v = strlen(value);
    char *s = (char *) malloc(n + v + 2);
    if (s == NULL) { errno = ENOMEM; return -1; }
    memcpy(s, name, n);
    s[n] = '=';
    memcpy(s + n + 1, value, v + 1);
    int r = _putenv(s);  // copies the string
    free(s);
    return r == 0 ? 0 : -1;
}

int setenv(const char *name, const char *value, int overwrite) {
    if (name == NULL || *name == 0 || strchr(name, '=') != NULL) {
        errno = EINVAL;
        return -1;
    }
    if (!overwrite && getenv(name) != NULL) {
        return 0;
    }
    return put_env(name, value != NULL ? value : "");
}

int unsetenv(const char *name) {
    if (name == NULL || *name == 0 || strchr(name, '=') != NULL) {
        errno = EINVAL;
        return -1;
    }
    // An empty value removes the variable on Windows.
    return put_env(name, "");
}

// Absolute path; Windows has _fullpath (no symlinks to resolve under
// AM_NO_SYMLINKS).
char *realpath(const char *path, char *resolved) {
    return _fullpath(resolved, path, PATH_MAX);
}

#if defined(_WIN32) && !defined(_WIN64)
// 32-bit Windows (windows-x86): Debian's static winpthreads imports
// GetTickCount64, which Windows XP / Server 2003 and ReactOS (NT 5.2) lack --
// the exe then refuses to start ("entry point not found"). Supplying the
// import slot ourselves makes the linker take this one instead of KERNEL32's:
// GetTickCount, widened by counting its 49.7-day wraps.
#include <windows.h>

static ULONGLONG WINAPI am_get_tick_count64(void) {
    static volatile LONG high = 0;
    static volatile DWORD last = 0;
    DWORD now = GetTickCount();
    if (now < last) {
        InterlockedIncrement(&high);
    }
    last = now;
    return ((ULONGLONG) (DWORD) high << 32) | now;
}

__attribute__((used)) void *am_imp_get_tick_count64 __asm__("__imp__GetTickCount64@0") = (void *) am_get_tick_count64;
#endif
