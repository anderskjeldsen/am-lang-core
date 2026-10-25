#include <libc/core.h>

// Xbox 360 (libxenon, run from XeLL on an RGH/JTAG console) platform layer:
// the pthreads shim declared in pthread.h, and the startup that gives an
// AmLang program a big stack, a filesystem and somewhere to print.

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <sys/stat.h>
#include <dirent.h>
#include <xenos/xenos.h>
#include <console/console.h>
#include <xenon_soc/xenon_power.h>
#include <usb/usbmain.h>
#include <diskio/ata.h>
#include <ppc/atomic.h>
#include <ppc/timebase.h>
#include <time/time.h>
#include <libfat/fat.h>
#include <xenon/xenon.h>

char am_xenon_device[16] = "";

// ---- hardware threads -------------------------------------------------------

int am_xenon_thread_id(void) {
    unsigned int pir;
    __asm__ volatile("mfspr %0, 1023" : "=r"(pir));
    return (int) (pir & 7);
}

static inline void am_xenon_sync(void) { __asm__ volatile("sync" ::: "memory"); }
static inline void am_xenon_pause(void) {
    // Lower this hardware thread's priority for a few cycles so a spinning
    // waiter does not starve its core sibling (the "db16cyc" hint).
    __asm__ volatile("or 31,31,31\n\tor 2,2,2" ::: "memory");
}

typedef struct {
    void *(*fn)(void *);
    void *arg;
    void *ret;
    void *stack;          // ours to free (NULL when the caller supplied one)
    volatile int state;   // 0 free, 1 starting/running, 2 finished
    volatile int detached;
} am_xthread_slot;

static am_xthread_slot am_xthreads[AM_XENON_HW_THREADS];
static unsigned int am_xthreads_lock = 0;
static void *am_xkey_values[AM_XENON_HW_THREADS][AM_XENON_MAX_KEYS];
static volatile int am_xkey_count = 0;
// Hardware thread order for new threads: the other cores first, then the
// second thread of each core. Thread 0 runs main and is never handed out.
static const int am_xthread_order[5] = { 2, 4, 3, 5, 1 };
#define AM_XENON_DEFAULT_STACK (4 * 1024 * 1024)

static void am_xthread_trampoline(void) {
    int id = am_xenon_thread_id();
    am_xthread_slot *s = &am_xthreads[id];
    am_xenon_sync();
    s->ret = s->fn(s->arg);
    for (int k = 0; k < AM_XENON_MAX_KEYS; k++) am_xkey_values[id][k] = NULL;
    lock(&am_xthreads_lock);
    if (s->detached) {
        // Nobody will join: the slot is free as soon as this task returns.
        // The stack is still in use until then, so it is leaked on purpose
        // (detached threads are rare here; the runtime's are joined).
        s->state = 0;
    } else {
        s->state = 2;
    }
    am_xenon_sync();
    unlock(&am_xthreads_lock);
}

int pthread_attr_init(pthread_attr_t *a) { memset(a, 0, sizeof(*a)); return 0; }
int pthread_attr_destroy(pthread_attr_t *a) { (void) a; return 0; }
int pthread_attr_setstack(pthread_attr_t *a, void *stack, size_t size) { a->stack = stack; a->stacksize = size; return 0; }
int pthread_attr_setstacksize(pthread_attr_t *a, size_t size) { a->stacksize = size; return 0; }
int pthread_attr_setdetachstate(pthread_attr_t *a, int st) { a->detached = st == PTHREAD_CREATE_DETACHED; return 0; }

int pthread_create(pthread_t *t, const pthread_attr_t *a, void *(*fn)(void *), void *arg) {
    int chosen = -1;
    lock(&am_xthreads_lock);
    for (int i = 0; i < 5; i++) {
        int id = am_xthread_order[i];
        if (am_xthreads[id].state == 0 && !xenon_is_thread_task_running(id)) {
            chosen = id;
            am_xthreads[id].state = 1;
            break;
        }
    }
    unlock(&am_xthreads_lock);
    if (chosen < 0) return 11; // EAGAIN: all five hardware threads are busy

    size_t size = (a != NULL && a->stacksize != 0) ? a->stacksize : AM_XENON_DEFAULT_STACK;
    void *stack = (a != NULL) ? a->stack : NULL;
    void *owned = NULL;
    if (stack == NULL) {
        owned = memalign(128, size);
        if (owned == NULL) { am_xthreads[chosen].state = 0; return 12; }
        stack = owned;
    }
    am_xthread_slot *s = &am_xthreads[chosen];
    s->fn = fn;
    s->arg = arg;
    s->ret = NULL;
    s->stack = owned;
    s->detached = (a != NULL) ? a->detached : 0;
    am_xenon_sync();
    // The stack grows down: hand over its top, below a small red zone.
    char *top = (char *) stack + size - 256;
    top = (char *) ((unsigned long) top & ~15UL);
    xenon_run_thread_task(chosen, top, (void *) am_xthread_trampoline);
    *t = chosen;
    return 0;
}

int pthread_join(pthread_t t, void **ret) {
    if (t <= 0 || t >= AM_XENON_HW_THREADS) return 22;
    am_xthread_slot *s = &am_xthreads[t];
    while (s->state != 2) am_xenon_pause();
    while (xenon_is_thread_task_running(t)) am_xenon_pause();
    if (ret != NULL) *ret = s->ret;
    if (s->stack != NULL) free(s->stack);
    s->stack = NULL;
    am_xenon_sync();
    s->state = 0;
    return 0;
}

int pthread_detach(pthread_t t) {
    if (t <= 0 || t >= AM_XENON_HW_THREADS) return 22;
    am_xthread_slot *s = &am_xthreads[t];
    lock(&am_xthreads_lock);
    if (s->state == 2) {
        if (s->stack != NULL) free(s->stack);
        s->stack = NULL;
        s->state = 0;
    } else {
        s->detached = 1;
    }
    unlock(&am_xthreads_lock);
    return 0;
}

pthread_t pthread_self(void) { return am_xenon_thread_id(); }
int pthread_equal(pthread_t a, pthread_t b) { return a == b; }
int sched_yield(void) { am_xenon_pause(); return 0; }

// ---- thread-specific data ---------------------------------------------------

int pthread_key_create(pthread_key_t *k, void (*dtor)(void *)) {
    (void) dtor;
    lock(&am_xthreads_lock);
    int id = am_xkey_count;
    if (id < AM_XENON_MAX_KEYS) am_xkey_count = id + 1;
    unlock(&am_xthreads_lock);
    if (id >= AM_XENON_MAX_KEYS) return 11;
    *k = id;
    return 0;
}
int pthread_key_delete(pthread_key_t k) { (void) k; return 0; }
void *pthread_getspecific(pthread_key_t k) {
    if (k < 0 || k >= AM_XENON_MAX_KEYS) return NULL;
    return am_xkey_values[am_xenon_thread_id()][k];
}
int pthread_setspecific(pthread_key_t k, const void *v) {
    if (k < 0 || k >= AM_XENON_MAX_KEYS) return 22;
    am_xkey_values[am_xenon_thread_id()][k] = (void *) v;
    return 0;
}

int pthread_once(pthread_once_t *o, void (*fn)(void)) {
    if (o->done) return 0;
    lock((unsigned int *) &o->lk);
    if (!o->done) { fn(); am_xenon_sync(); o->done = 1; }
    unlock((unsigned int *) &o->lk);
    return 0;
}

// ---- mutexes ------------------------------------------------------------------

int pthread_mutexattr_init(pthread_mutexattr_t *a) { a->type = PTHREAD_MUTEX_NORMAL; return 0; }
int pthread_mutexattr_destroy(pthread_mutexattr_t *a) { (void) a; return 0; }
int pthread_mutexattr_settype(pthread_mutexattr_t *a, int type) { a->type = type; return 0; }
int pthread_mutex_init(pthread_mutex_t *m, const pthread_mutexattr_t *a) {
    m->lk = 0; m->owner = -1; m->count = 0;
    m->type = (a != NULL) ? a->type : PTHREAD_MUTEX_NORMAL;
    return 0;
}
int pthread_mutex_destroy(pthread_mutex_t *m) { (void) m; return 0; }

int pthread_mutex_lock(pthread_mutex_t *m) {
    int self = am_xenon_thread_id();
    if (m->type == PTHREAD_MUTEX_RECURSIVE && m->owner == self) { m->count++; return 0; }
    lock((unsigned int *) &m->lk);
    am_xenon_sync();
    m->owner = self;
    m->count = 1;
    return 0;
}

int pthread_mutex_trylock(pthread_mutex_t *m) {
    int self = am_xenon_thread_id();
    if (m->type == PTHREAD_MUTEX_RECURSIVE && m->owner == self) { m->count++; return 0; }
    // lock() only spins; probe first so a held lock reports EBUSY.
    if (m->lk != 0) return 16;
    lock((unsigned int *) &m->lk);
    am_xenon_sync();
    m->owner = self;
    m->count = 1;
    return 0;
}

int pthread_mutex_unlock(pthread_mutex_t *m) {
    if (m->type == PTHREAD_MUTEX_RECURSIVE && --m->count > 0) return 0;
    m->owner = -1;
    m->count = 0;
    am_xenon_sync();
    unlock((unsigned int *) &m->lk);
    return 0;
}

// ---- condition variables -------------------------------------------------------
// A sequence number: a waiter notes it, drops the mutex, spins until it moves.
// Signal and broadcast both bump it, so signal may wake more than one waiter --
// allowed by POSIX (spurious wakeups), and every caller re-checks its predicate.

int pthread_cond_init(pthread_cond_t *c, const pthread_condattr_t *a) { (void) a; c->seq = 0; return 0; }
int pthread_cond_destroy(pthread_cond_t *c) { (void) c; return 0; }
int pthread_cond_wait(pthread_cond_t *c, pthread_mutex_t *m) {
    unsigned int seen = c->seq;
    int count = m->count;
    m->count = 1;
    pthread_mutex_unlock(m);
    while (c->seq == seen) am_xenon_pause();
    pthread_mutex_lock(m);
    m->count = count;
    return 0;
}
int pthread_cond_signal(pthread_cond_t *c) { atomic_inc((unsigned int *) &c->seq); am_xenon_sync(); return 0; }
int pthread_cond_broadcast(pthread_cond_t *c) { return pthread_cond_signal(c); }

// ---- sleeping -------------------------------------------------------------------------

int usleep(useconds_t us) { udelay(us); return 0; }
unsigned int sleep(unsigned int s) { mdelay(s * 1000); return 0; }
int clock_gettime(int clk, struct timespec *ts) {
    if (ts == NULL) return -1;
    if (clk == CLOCK_MONOTONIC) {
        unsigned long long tb = mftb();
        ts->tv_sec = (time_t) (tb / PPC_TIMEBASE_FREQ);
        ts->tv_nsec = (long) ((tb % PPC_TIMEBASE_FREQ) * 1000000000ULL / PPC_TIMEBASE_FREQ);
        return 0;
    }
    struct timeval tv;
    gettimeofday(&tv, NULL);
    ts->tv_sec = tv.tv_sec;
    ts->tv_nsec = tv.tv_usec * 1000;
    return 0;
}
int nanosleep(const struct timespec *req, struct timespec *rem) {
    if (rem != NULL) { rem->tv_sec = 0; rem->tv_nsec = 0; }
    if (req == NULL) return -1;
    if (req->tv_sec > 0) mdelay((int) req->tv_sec * 1000);
    if (req->tv_nsec > 0) udelay((int) (req->tv_nsec / 1000));
    return 0;
}

// ---- missing POSIX ---------------------------------------------------------------

long sysconf(int name) {
    (void) name;
    // Only asked for the page size (Diagnostics.Arc).
    return 4096;
}

int rmdir(const char *path) {
    // libfat's unlink removes an EMPTY directory too.
    return unlink(path);
}

char *realpath(const char *path, char *resolved) {
    if (path == NULL) return NULL;
    char *out = resolved != NULL ? resolved : (char *) malloc(1024);
    if (out == NULL) return NULL;
    if (strchr(path, ':') != NULL || getcwd(out, 1024) == NULL) {
        snprintf(out, 1024, "%s", path);
    } else {
        size_t n = strlen(out);
        snprintf(out + n, 1024 - n, "%s%s", (n > 0 && out[n - 1] == '/') ? "" : "/", path);
    }
    return out;
}

// ---- startup --------------------------------------------------------------------------

#define AM_XENON_MAIN_STACK (8 * 1024 * 1024)

// The folder (on the root of the USB stick / HDD) the app lives in. An app
// passes its own with -DAM_XENON_APP_DIR=... ; XeLL gives no argv to find it.
#ifndef AM_XENON_APP_DIR
#define AM_XENON_APP_DIR "amlang"
#endif

extern int __real_main(int argc, char **argv);
int bdev_enum(int handle, const char **name);

// Mount every disk libfat finds, then move into the game's folder: XeLL
// passes no argv, so the folder is FOUND -- AM_XENON_APP_DIR if a device has
// it, else the first top-level directory holding an "assets" folder (the game's
// folder can have any name, at the top level of the stick or HDD).
// "assets/..." paths are then relative, as on every other platform.
static int am_xenon_has_assets(const char *dir) {
    char path[160];
    struct stat st;
    snprintf(path, sizeof(path), "%s/assets", dir);
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static int am_xenon_enter(const char *dev, const char *dir) {
    snprintf(am_xenon_device, sizeof(am_xenon_device), "%s", dev);
    if (chdir(dir) != 0) return 0;
    printf("app folder: %s\n", dir);
    return 1;
}

static void am_xenon_mount_storage(void) {
    usb_init();
    usb_do_poll();
    xenon_ata_init();
    fatInitDefault();

    const char *names[8];
    int n = 0;
    int handle = -1;
    const char *s;
    while (n < 8 && (handle = bdev_enum(handle, &s)) >= 0) names[n++] = s;

    char path[160];
    for (int i = 0; i < n; i++) {
        snprintf(path, sizeof(path), "%s:/%s", names[i], AM_XENON_APP_DIR);
        if (am_xenon_has_assets(path) && am_xenon_enter(names[i], path)) return;
    }
    for (int i = 0; i < n; i++) {
        char root[24];
        snprintf(root, sizeof(root), "%s:/", names[i]);
        DIR *d = opendir(root);
        if (d == NULL) continue;
        struct dirent *de;
        while ((de = readdir(d)) != NULL) {
            if (de->d_name[0] == '.') continue;
            snprintf(path, sizeof(path), "%s%s", root, de->d_name);
            if (am_xenon_has_assets(path)) {
                closedir(d);
                if (am_xenon_enter(names[i], path)) return;
                d = NULL;
                break;
            }
        }
        if (d != NULL) closedir(d);
    }
    if (n > 0) {
        snprintf(path, sizeof(path), "%s:/", names[0]);
        am_xenon_enter(names[0], path);
    }
}

typedef struct { int argc; char **argv; int rc; } am_xenon_main_args;
static am_xenon_main_args am_xenon_args;

static void am_xenon_call_main(void) {
    am_xenon_args.rc = __real_main(am_xenon_args.argc, am_xenon_args.argv);
}

// Call fn() with r1 moved to `top`, then put the old stack back. Plain
// 32-bit SysV: the new frame's back chain points at the old one, so a
// debugger (or an unwinder) can still walk out.
static void am_xenon_run_on_stack(void (*fn)(void), void *top) {
    __asm__ volatile(
        "mr    %%r14, %%r1      \n\t"
        "mr    %%r1, %0         \n\t"
        "stwu  %%r14, -32(%%r1) \n\t"
        "mtctr %1               \n\t"
        "bctrl                  \n\t"
        "mr    %%r1, %%r14      \n\t"
        :
        : "r"(top), "r"(fn)
        : "r0", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "r11", "r12",
          "r14", "ctr", "lr", "cr0", "cr1", "cr5", "cr6", "cr7", "memory");
}

int __wrap_main(int argc, char **argv) {
    xenos_init(VIDEO_MODE_AUTO);
    console_init();
    xenon_make_it_faster(XENON_SPEED_FULL);
    // Bring the five secondary hardware threads up so pthread_create has
    // somewhere to run (a no-op if make_it_faster already woke them).
    xenon_thread_startup();
    printf("AmLang on Xenon: mounting storage...\n");
    am_xenon_mount_storage();
    if (am_xenon_device[0] != '\0') {
        printf("running from %s\n", am_xenon_device);
        // Diagnostics go to log.txt beside the game: once the GPU owns the
        // screen there is nowhere else to read them.
        if (freopen("log.txt", "w", stdout) != NULL) setvbuf(stdout, NULL, _IOLBF, 0);
        freopen("log.txt", "a", stderr);
    } else {
        printf("no storage device found: assets will not load\n");
    }

    am_xenon_args.argc = argc;
    am_xenon_args.argv = argv;
    am_xenon_args.rc = 0;
    char *stack = (char *) memalign(128, AM_XENON_MAIN_STACK);
    if (stack == NULL) return __real_main(argc, argv);
    char *top = (char *) (((unsigned long) (stack + AM_XENON_MAIN_STACK - 256)) & ~15UL);
    am_xenon_run_on_stack(am_xenon_call_main, top);
    fflush(stdout);
    return am_xenon_args.rc;
}
