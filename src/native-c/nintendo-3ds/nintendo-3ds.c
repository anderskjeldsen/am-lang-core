#include <libc/core.h>

// Nintendo 3DS platform layer (libctru, run as a .3dsx from the Homebrew
// Launcher or loaded straight into Azahar).

#include <3ds.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

// libctru gives main a 32 KB stack unless the program says otherwise; it
// reads this symbol at startup. AmLang's call depth (recursive parsing, deep
// ARC teardown) needs a desktop-sized one, and an overrun here is silent.
u32 __stacksize__ = 1024 * 1024;

// libctru gives the GPU's linear heap a fixed 32 MB and malloc the rest. On
// an original 3DS that leaves malloc 28 MB of the 64 while most of the linear
// heap sits idle (framebuffers, vertex buffers and textures, a few MB).
// 20 MB of linear memory leaves malloc about 40 MB there, and New 3DS
// models far more.
u32 __ctru_linear_heap_size = 20 * 1024 * 1024;

extern int __real_main(int argc, char **argv);

// Give println() somewhere to go.
//
// The 3DS has no terminal: stdout is discarded unless a console is drawn on
// one of the screens, and the app owns the screens. So stdout and stderr go
// to log.txt in the current directory -- the .3dsx's own folder, which
// libctru moves into at startup -- line buffered, so what was printed before
// a crash is already on the card when the crash happens.
static void am_3ds_log_to_card(void) {
    if (freopen("log.txt", "w", stdout) != NULL) {
        setvbuf(stdout, NULL, _IOLBF, 0);
    }
    freopen("log.txt", "a", stderr);
}

// A console has no command line. An args.txt beside the program supplies
// one: its whitespace-separated words follow argv[0], as if typed after the
// program name. Absent, the program gets the arguments it was launched with.
#define AM_3DS_MAX_ARGS 32
static char *am_3ds_argv[AM_3DS_MAX_ARGS + 1];
static char am_3ds_argbuf[1024];

static int am_3ds_args_from_file(int argc, char **argv, char ***out) {
    FILE *f = fopen("args.txt", "r");
    if (f == NULL) return argc;
    size_t n = fread(am_3ds_argbuf, 1, sizeof(am_3ds_argbuf) - 1, f);
    fclose(f);
    am_3ds_argbuf[n] = '\0';
    int c = 0;
    am_3ds_argv[c++] = (argc > 0 && argv != NULL && argv[0] != NULL) ? argv[0] : "app";
    for (char *t = strtok(am_3ds_argbuf, " \t\r\n"); t != NULL && c < AM_3DS_MAX_ARGS; t = strtok(NULL, " \t\r\n"))
        am_3ds_argv[c++] = t;
    am_3ds_argv[c] = NULL;
    *out = am_3ds_argv;
    return c;
}

// Where threads run.
//
// libctru's pthread_create (__syscall_thread_create) puts every thread on
// core 0 -- the app core, where main runs -- at the lowest priority, so a
// worker only gets the CPU while main is waiting. The platform links with
// -Wl,--wrap=__syscall_thread_create and places them itself:
//   core 2  New 3DS only, a whole spare core (if the launcher allows it);
//   core 1  the system core, which lends an app part of its time once
//           APT_SetAppCpuTimeLimit is set -- room for ONE app thread;
//   core 0  as before, for any thread that fits nowhere else.
// Same contract as the original: *thread gets the libctru Thread handle,
// which pthread_join/detach hand back to threadJoin/threadDetach.
#define AM_3DS_CORE1_PERCENT 70

struct __pthread_t;
static int am_3ds_core1_taken = 0;

int __wrap___syscall_thread_create(struct __pthread_t **thread, void *(*func)(void *), void *arg,
                                   void *stack_addr, size_t stack_size) {
    if (stack_addr != NULL) return EINVAL;
    if (stack_size == 0) stack_size = 32 * 1024;
    s32 prio = 0x30;
    svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
    int core = 2;
    Thread t = threadCreate((ThreadFunc) func, arg, stack_size, prio, 2, false);
    if (t == NULL && !am_3ds_core1_taken) {
        core = 1;
        t = threadCreate((ThreadFunc) func, arg, stack_size, prio, 1, false);
        if (t != NULL) am_3ds_core1_taken = 1;
    }
    if (t == NULL) {
        core = 0;
        t = threadCreate((ThreadFunc) func, arg, stack_size, 0x3F, 0, false);
    }
    if (t == NULL) return ENOMEM;
    printf("[3ds] thread on core %d\n", core);
    *thread = (struct __pthread_t *) t;
    return 0;
}

// Started with `3dslink -s`: stdout and stderr go back over Wi-Fi to the
// 3dslink on the computer instead of to log.txt.
#define AM_3DS_SOC_BUFFER (256 * 1024)
static int am_3ds_log_to_3dslink(void) {
    if (__3dslink_host.s_addr == 0) return 0;
    u32 *buf = (u32 *) memalign(0x1000, AM_3DS_SOC_BUFFER);
    if (buf == NULL || R_FAILED(socInit(buf, AM_3DS_SOC_BUFFER))) return 0;
    if (link3dsStdio() < 0) {
        socExit();
        return 0;
    }
    setvbuf(stdout, NULL, _IOLBF, 0);
    printf("[3ds] stdout over 3dslink\n");
    return 1;
}

int __wrap_main(int argc, char **argv) {
    // New 3DS: 804 MHz and the L2 cache instead of the old model's 268 MHz.
    // A no-op on an original 3DS.
    osSetSpeedupEnable(true);
    // Let one app thread run on the system core (see above).
    APT_SetAppCpuTimeLimit(AM_3DS_CORE1_PERCENT);
    if (!am_3ds_log_to_3dslink()) am_3ds_log_to_card();
    {
        // What this launch got to work with: an applet launch (Rosalina, a
        // Download Play takeover) gets far less than a title takeover.
        extern u32 __ctru_heap_size, __ctru_linear_heap_size;
        bool n3ds = false;
        APT_CheckNew3DS(&n3ds);
        printf("[3ds] %s 3DS, app region %lu KB: heap %lu KB, linear %lu KB\n",
            n3ds ? "New" : "Old",
            (unsigned long) (osGetMemRegionSize(MEMREGION_APPLICATION) / 1024),
            (unsigned long) (__ctru_heap_size / 1024), (unsigned long) (__ctru_linear_heap_size / 1024));
    }
    argc = am_3ds_args_from_file(argc, argv, &argv);
    return __real_main(argc, argv);
}
