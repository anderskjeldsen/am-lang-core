// Classic Mac OS (Retro68, PowerPC): pthreads on the Thread Manager (see
// pthread.h) and the POSIX timing calls newlib lacks (see macos9_compat.h).

#include <macos9-ppc/pthread.h>
#include <macos9-ppc/macos9_compat.h>
#include <errno.h>
#include <string.h>

#include <Multiverse.h>

// Retro68's interfaces (Multiversal) have no Threads.h and don't declare
// Microseconds; the import libraries (ThreadsLib, InterfaceLib) export them.
// Declared here from Apple's Universal Interfaces.
typedef unsigned long ThreadID;
enum { kCooperativeThread = 1L << 0 };                 // ThreadStyle
enum { kCreateIfNeeded = 1L << 2 };                    // ThreadOptions
extern OSErr NewThread(unsigned long threadStyle, UniversalProcPtr threadEntry, void *threadParam,
                       long stackSize, unsigned long options, void **threadResult, ThreadID *threadMade);
extern OSErr YieldToAnyThread(void);
extern OSErr GetCurrentThread(ThreadID *currentThreadID);
typedef struct { unsigned long hi, lo; } am_m9wide;
extern void Microseconds(am_m9wide *microTickCount);

// The thread entry as a routine descriptor: the Thread Manager calls it
// through the Mixed Mode Manager, which would run a bare PowerPC function
// pointer as 68k code. C calling convention, one 4-byte argument, a 4-byte
// result: kCStackBased | RESULT_SIZE(kFourByteCode)
// | STACK_ROUTINE_PARAMETER(1, kFourByteCode).
#define AM_M9_THREAD_ENTRY_PROCINFO 0x000000F1UL

// ---- threads -----------------------------------------------------------------

typedef struct {
    int       used;
    ThreadID  tid;
    void   *(*fn)(void *);
    void     *arg;
    void     *result;
    volatile int done;
    int       detached;
    const void *specific[AM_MACOS9_MAX_KEYS];
} am_m9slot;

static am_m9slot am_m9slots[AM_MACOS9_MAX_THREADS];
static int am_m9keys_used;
static UniversalProcPtr am_m9_entry_upp;

void am_macos9_yield(void) {
    YieldToAnyThread();
}

int am_macos9_thread_slot(void) {
    ThreadID tid = 0;
    GetCurrentThread(&tid);
    for (int i = 0; i < AM_MACOS9_MAX_THREADS; i++) {
        if (am_m9slots[i].used && am_m9slots[i].tid == tid) return i;
    }
    // First call on the main thread: it becomes slot 0.
    if (!am_m9slots[0].used) {
        am_m9slots[0].used = 1;
        am_m9slots[0].tid = tid;
        return 0;
    }
    return 0;
}

static void *am_m9_entry(void *param) {
    am_m9slot *s = (am_m9slot *) param;
    s->result = s->fn(s->arg);
    s->done = 1;
    return s->result;
}

int pthread_attr_init(pthread_attr_t *a) { a->stacksize = 0; a->detached = 0; return 0; }
int pthread_attr_destroy(pthread_attr_t *a) { (void) a; return 0; }
int pthread_attr_setstacksize(pthread_attr_t *a, size_t size) { a->stacksize = size; return 0; }
int pthread_attr_setdetachstate(pthread_attr_t *a, int state) { a->detached = state; return 0; }

int pthread_create(pthread_t *t, const pthread_attr_t *a, void *(*fn)(void *), void *arg) {
    (void) am_macos9_thread_slot();     // make sure main is slot 0
    int slot = -1;
    for (int i = 1; i < AM_MACOS9_MAX_THREADS; i++) {
        if (!am_m9slots[i].used) { slot = i; break; }
    }
    if (slot < 0) return EAGAIN;
    am_m9slot *s = &am_m9slots[slot];
    memset(s, 0, sizeof(*s));
    s->used = 1;
    s->fn = fn;
    s->arg = arg;
    s->detached = a != NULL && a->detached;
    Size stack = (a != NULL && a->stacksize > 0) ? (Size) a->stacksize : 256 * 1024;
    if (am_m9_entry_upp == NULL) {
        am_m9_entry_upp = NewRoutineDescriptor((ProcPtr) am_m9_entry, AM_M9_THREAD_ENTRY_PROCINFO, GetCurrentArchitecture());
    }
    ThreadID tid = 0;
    OSErr err = NewThread(kCooperativeThread, am_m9_entry_upp, s, (long) stack,
                          kCreateIfNeeded, NULL, &tid);
    if (err != noErr) {
        s->used = 0;
        return EAGAIN;
    }
    s->tid = tid;
    *t = slot;
    return 0;
}

int pthread_join(pthread_t t, void **ret) {
    if (t <= 0 || t >= AM_MACOS9_MAX_THREADS || !am_m9slots[t].used) return EINVAL;
    while (!am_m9slots[t].done) YieldToAnyThread();
    if (ret != NULL) *ret = am_m9slots[t].result;
    am_m9slots[t].used = 0;
    return 0;
}

int pthread_detach(pthread_t t) {
    if (t > 0 && t < AM_MACOS9_MAX_THREADS) am_m9slots[t].detached = 1;
    return 0;
}

pthread_t pthread_self(void) { return am_macos9_thread_slot(); }
int pthread_equal(pthread_t a, pthread_t b) { return a == b; }
int sched_yield(void) { YieldToAnyThread(); return 0; }

// ---- mutexes and condition variables (cooperative: no atomics needed) ---------

int pthread_mutexattr_init(pthread_mutexattr_t *a) { a->type = PTHREAD_MUTEX_NORMAL; return 0; }
int pthread_mutexattr_destroy(pthread_mutexattr_t *a) { (void) a; return 0; }
int pthread_mutexattr_settype(pthread_mutexattr_t *a, int type) { a->type = type; return 0; }

int pthread_mutex_init(pthread_mutex_t *m, const pthread_mutexattr_t *a) {
    m->owner = -1;
    m->count = 0;
    m->type = a != NULL ? a->type : PTHREAD_MUTEX_NORMAL;
    return 0;
}
int pthread_mutex_destroy(pthread_mutex_t *m) { (void) m; return 0; }

int pthread_mutex_lock(pthread_mutex_t *m) {
    int self = am_macos9_thread_slot();
    if (m->count > 0 && m->owner == self) {
        m->count++;                     // recursive (and harmless if not)
        return 0;
    }
    while (m->count > 0) YieldToAnyThread();
    m->owner = self;
    m->count = 1;
    return 0;
}

int pthread_mutex_trylock(pthread_mutex_t *m) {
    int self = am_macos9_thread_slot();
    if (m->count > 0 && m->owner != self) return EBUSY;
    m->owner = self;
    m->count++;
    return 0;
}

int pthread_mutex_unlock(pthread_mutex_t *m) {
    if (m->count > 0) m->count--;
    if (m->count == 0) m->owner = -1;
    return 0;
}

int pthread_cond_init(pthread_cond_t *c, const pthread_condattr_t *a) { (void) a; c->seq = 0; return 0; }
int pthread_cond_destroy(pthread_cond_t *c) { (void) c; return 0; }

int pthread_cond_wait(pthread_cond_t *c, pthread_mutex_t *m) {
    unsigned int seq = c->seq;
    int count = m->count;
    m->count = 0;
    m->owner = -1;
    while (c->seq == seq) YieldToAnyThread();
    while (m->count > 0) YieldToAnyThread();
    m->owner = am_macos9_thread_slot();
    m->count = count;
    return 0;
}

int pthread_cond_timedwait(pthread_cond_t *c, pthread_mutex_t *m, const struct timespec *abstime) {
    unsigned int seq = c->seq;
    int count = m->count;
    int rc = 0;
    m->count = 0;
    m->owner = -1;
    while (c->seq == seq) {
        struct timespec now;
        clock_gettime(CLOCK_REALTIME, &now);
        if (now.tv_sec > abstime->tv_sec || (now.tv_sec == abstime->tv_sec && now.tv_nsec >= abstime->tv_nsec)) {
            rc = ETIMEDOUT;
            break;
        }
        YieldToAnyThread();
    }
    while (m->count > 0) YieldToAnyThread();
    m->owner = am_macos9_thread_slot();
    m->count = count;
    return rc;
}

int pthread_cond_signal(pthread_cond_t *c) { c->seq++; return 0; }
int pthread_cond_broadcast(pthread_cond_t *c) { c->seq++; return 0; }

// ---- thread-specific data ---------------------------------------------------

int pthread_key_create(pthread_key_t *k, void (*destructor)(void *)) {
    (void) destructor;
    if (am_m9keys_used >= AM_MACOS9_MAX_KEYS) return EAGAIN;
    *k = am_m9keys_used++;
    return 0;
}
int pthread_key_delete(pthread_key_t k) { (void) k; return 0; }

void *pthread_getspecific(pthread_key_t k) {
    if (k < 0 || k >= AM_MACOS9_MAX_KEYS) return NULL;
    return (void *) am_m9slots[am_macos9_thread_slot()].specific[k];
}

int pthread_setspecific(pthread_key_t k, const void *v) {
    if (k < 0 || k >= AM_MACOS9_MAX_KEYS) return EINVAL;
    am_m9slots[am_macos9_thread_slot()].specific[k] = v;
    return 0;
}

int pthread_once(pthread_once_t *o, void (*fn)(void)) {
    if (!o->done) {
        o->done = 1;
        fn();
    }
    return 0;
}

// ---- time ---------------------------------------------------------------------

// Microseconds() counts from boot; REALTIME adds the clock's seconds at the
// first call, so both clocks advance smoothly.
static unsigned long long am_m9_micros(void) {
    am_m9wide w;
    Microseconds(&w);
    return ((unsigned long long) w.hi << 32) | w.lo;
}

int clock_gettime(int clk, struct timespec *ts) {
    static unsigned long long boot_us = 0;
    static time_t wall_at_boot = 0;
    unsigned long long us = am_m9_micros();
    if (clk == CLOCK_REALTIME) {
        if (wall_at_boot == 0) {
            wall_at_boot = time(NULL) - (time_t) (us / 1000000ULL);
            boot_us = 0;
        }
        us += (unsigned long long) wall_at_boot * 1000000ULL - boot_us;
    }
    ts->tv_sec = (time_t) (us / 1000000ULL);
    ts->tv_nsec = (long) (us % 1000000ULL) * 1000L;
    return 0;
}

int nanosleep(const struct timespec *req, struct timespec *rem) {
    unsigned long long until = am_m9_micros() + (unsigned long long) req->tv_sec * 1000000ULL + (unsigned long long) (req->tv_nsec / 1000);
    while (am_m9_micros() < until) YieldToAnyThread();
    if (rem != NULL) { rem->tv_sec = 0; rem->tv_nsec = 0; }
    return 0;
}

// ---- directories (not implemented yet) ------------------------------------------

#include <macos9-ppc/dirent.h>

DIR *opendir(const char *path) {
    (void) path;
    errno = ENOTSUP;
    return NULL;
}

struct dirent *readdir(DIR *d) {
    (void) d;
    return NULL;
}

int closedir(DIR *d) {
    (void) d;
    return 0;
}

// ---- the rest of POSIX the runtime links against ------------------------------
// (Working directory and process control don't exist here; directories are
// left for when the File Manager calls are wired up.)

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

char *getcwd(char *buf, size_t size) {
    if (buf == NULL || size < 2) { errno = ERANGE; return NULL; }
    strcpy(buf, ":");                   // Mac OS: the current (application) folder
    return buf;
}

int chdir(const char *path) { (void) path; errno = ENOTSUP; return -1; }

int access(const char *path, int mode) {
    (void) mode;
    struct stat st;
    return stat(path, &st);             // exists (no permissions on HFS)
}

int mkdir(const char *path, mode_t mode) { (void) path; (void) mode; errno = ENOTSUP; return -1; }
int rmdir(const char *path) { (void) path; errno = ENOTSUP; return -1; }

FILE *popen(const char *command, const char *type) { (void) command; (void) type; errno = ENOTSUP; return NULL; }
int pclose(FILE *f) { (void) f; return -1; }

char *realpath(const char *path, char *resolved) {
    if (resolved == NULL) {
        resolved = (char *) malloc(PATH_MAX);
        if (resolved == NULL) return NULL;
    }
    strncpy(resolved, path, PATH_MAX - 1);
    resolved[PATH_MAX - 1] = 0;
    return resolved;
}
