#include <libc/core.h>

// Nintendo PPC (GameCube/Wii) platform layer.

#include <gccore.h>
#include <malloc.h>
#include <pthread.h>
#include <stdio.h>

// libogc2 runs main() on a fixed 128 KB stack (lwp.o's static __stack).
// AmLang's call depth (recursive JSON / script parsing, deep ARC teardown)
// overruns that, and on these consoles an overrun is silent: it tramples
// whatever lies below and surfaces much later as a crash in free(). So the
// platform links with -Wl,--wrap=main and the real main runs on a thread
// with a desktop-sized stack, taken from the heap (MEM2 on the Wii).
// It must be a pthread, not a bare LWP_CreateThread one: the runtime finds
// the current isolate through a pthread key (core.c), and libogc2's keys
// only work on threads pthread_create made.
#define AM_NINTENDO_MAIN_STACK (4 * 1024 * 1024)

extern int __real_main(int argc, char **argv);

typedef struct {
    int argc;
    char **argv;
    int rc;
} am_main_args;

static void *am_main_entry(void *p) {
    am_main_args *a = (am_main_args *) p;
    // A new thread gets its own stdio state: keep the console line-buffered
    // so a log reaches its device line by line, not at exit.
    setvbuf(stdout, NULL, _IOLBF, 0);
    a->rc = __real_main(a->argc, a->argv);
    return NULL;
}

int __wrap_main(int argc, char **argv) {
    static am_main_args args;
    args.argc = argc;
    args.argv = argv;
    args.rc = 0;
    void *stack = memalign(32, AM_NINTENDO_MAIN_STACK);
    if (stack == NULL) {
        return __real_main(argc, argv);   // no memory for a big stack: run as before
    }
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstack(&attr, stack, AM_NINTENDO_MAIN_STACK);
    pthread_t thread;
    int rc = pthread_create(&thread, &attr, am_main_entry, &args);
    pthread_attr_destroy(&attr);
    if (rc != 0) {
        free(stack);
        return __real_main(argc, argv);
    }
    pthread_join(thread, NULL);
    return args.rc;
}
