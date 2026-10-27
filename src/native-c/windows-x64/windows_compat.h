#ifndef AMLANG_WINDOWS_COMPAT_H
#define AMLANG_WINDOWS_COMPAT_H

// Force-included into every windows-x64 compile (-include, see package.yml):
// the POSIX bits MinGW-w64 leaves out. windows.c implements them. Kept free
// of <windows.h> -- its macros (min, max, near, far, ...) would leak into
// all generated code.

#include <direct.h>     // _mkdir
#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 260
#endif

#ifdef __cplusplus
extern "C" {
#endif
int setenv(const char *name, const char *value, int overwrite);
int unsetenv(const char *name);
char *realpath(const char *path, char *resolved);
#ifdef __cplusplus
}
#endif

#endif
