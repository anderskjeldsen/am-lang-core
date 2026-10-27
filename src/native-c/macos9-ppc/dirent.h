#ifndef AMLANG_MACOS9_DIRENT_H
#define AMLANG_MACOS9_DIRENT_H

// Classic Mac OS (Retro68): newlib has no directory API here (its
// <dirent.h> is an #error). This shadows it with the declarations File.c
// needs; macos9.c implements them. No d_type: callers fall back to stat().
// Listing is not implemented yet -- opendir() fails with ENOTSUP.

#ifdef __cplusplus
extern "C" {
#endif

struct dirent {
    char d_name[256];
};

typedef struct am_m9dir DIR;

DIR *opendir(const char *path);
struct dirent *readdir(DIR *d);
int closedir(DIR *d);

#ifdef __cplusplus
}
#endif

#endif
