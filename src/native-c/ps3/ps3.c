#include <libc/core.h>

// PlayStation 3 (PSL1GHT homebrew, PPU) platform layer: a main stack big
// enough for AmLang, and a working directory that is the app's own folder.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/process.h>
#include <sys/systime.h>
#include <lv2/systime.h>
#include <time.h>
#include <errno.h>
#include <ps3/ps3.h>

// lv2 gives main() the stack this asks for (priority 1001 = normal). The
// default is tiny; AmLang call chains (JSON/script parsing, ARC teardown) are
// deep, and an overrun on a console is silent.
SYS_PROCESS_PARAM(1001, 0x100000);

// ---- POSIX bits PSL1GHT's newlib declares but does not provide ----------

int clock_gettime(clockid_t clk, struct timespec *ts) {
    if (ts == NULL) return -1;
#ifdef CLOCK_MONOTONIC
    if (clk == CLOCK_MONOTONIC) {
        s64 us = sysGetSystemTime();   // microseconds since boot
        ts->tv_sec = (time_t) (us / 1000000);
        ts->tv_nsec = (long) ((us % 1000000) * 1000);
        return 0;
    }
#endif
    u64 sec = 0, nsec = 0;
    sysGetCurrentTime(&sec, &nsec);
    ts->tv_sec = (time_t) sec;
    ts->tv_nsec = (long) nsec;
    return 0;
}

int nanosleep(const struct timespec *req, struct timespec *rem) {
    if (rem != NULL) { rem->tv_sec = 0; rem->tv_nsec = 0; }
    if (req == NULL) return -1;
    if (req->tv_sec > 0) sysSleep((u32) req->tv_sec);
    if (req->tv_nsec >= 1000) sysUsleep((u32) (req->tv_nsec / 1000));
    return 0;
}

// No shell and no child processes on lv2.
FILE *popen(const char *cmd, const char *mode) { (void) cmd; (void) mode; errno = ENOSYS; return NULL; }
int pclose(FILE *f) { (void) f; return -1; }

extern int __real_main(int argc, char **argv);

// argv[0] is where EBOOT.BIN/the ELF was started from
// (/dev_hdd0/game/<TITLE>/USRDIR/EBOOT.BIN, /dev_usb000/..., or the host
// folder RPCS3 booted). "assets/..." paths are relative, as everywhere else.
int __wrap_main(int argc, char **argv) {
    if (argc > 0 && argv != NULL && argv[0] != NULL) {
        char path[512];
        snprintf(path, sizeof(path), "%s", argv[0]);
        char *slash = strrchr(path, '/');
        if (slash != NULL) {
            *slash = '\0';
            if (chdir(path) == 0) printf("app folder: %s\n", path);
        }
    }
    setvbuf(stdout, NULL, _IOLBF, 0);
    return __real_main(argc, argv);
}
