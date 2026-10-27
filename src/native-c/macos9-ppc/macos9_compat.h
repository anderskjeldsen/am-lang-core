#ifndef AMLANG_MACOS9_COMPAT_H
#define AMLANG_MACOS9_COMPAT_H

// Force-included into every macos9-ppc compile (-include, see package.yml):
// the POSIX bits Retro68's newlib leaves out on classic Mac OS. macos9.c
// implements them (timers on Microseconds(), sleeping by yielding).

#include <time.h>
#include <sys/time.h>

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME 0
#endif
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif

#ifdef __cplusplus
extern "C" {
#endif
int clock_gettime(int clk, struct timespec *ts);
int nanosleep(const struct timespec *req, struct timespec *rem);
#ifdef __cplusplus
}
#endif

#endif
