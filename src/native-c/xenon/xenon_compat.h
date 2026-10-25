#ifndef AMLANG_XENON_COMPAT_H
#define AMLANG_XENON_COMPAT_H

// Force-included into every xenon compile (-include, see package.yml): the
// POSIX bits newlib leaves undeclared for this bare-metal target. xenon.c
// implements them on libxenon's timers.

#include <time.h>
#include <sys/time.h>

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
