#include <libc/core.h>

// Nintendo PPC (GameCube/Wii) platform layer.

#include <gccore.h>
#include <malloc.h>

// libogc2 runs main() on a fixed 128 KB stack (lwp.o's static __stack).
// AmLang's call depth (recursive JSON / script parsing, deep ARC teardown)
// overruns that, and on these consoles an overrun is silent: it tramples
// whatever lies below and surfaces much later as a crash in free(). So the
// platform links with -Wl,--wrap=main and the real main runs on a thread
// with a desktop-sized stack, taken from the heap (MEM2 on the Wii).
#define AM_NINTENDO_MAIN_STACK (4 * 1024 * 1024)

extern int __real_main(int argc, char **argv);

typedef struct {
    int argc;
    char **argv;
    int rc;
} am_main_args;

static void *am_main_entry(void *p) {
    am_main_args *a = (am_main_args *) p;
    a->rc = __real_main(a->argc, a->argv);
    return NULL;
}

int __wrap_main(int argc, char **argv) {
    static am_main_args args;
    args.argc = argc;
    args.argv = argv;
    args.rc = 0;
    void *stack = memalign(32, AM_NINTENDO_MAIN_STACK);
    lwp_t thread;
    if (stack == NULL ||
        LWP_CreateThread(&thread, am_main_entry, &args, stack, AM_NINTENDO_MAIN_STACK, 64) != 0) {
        return __real_main(argc, argv);   // no memory for a big stack: run as before
    }
    LWP_JoinThread(thread, NULL);
    return args.rc;
}
