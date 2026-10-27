#ifndef AMLANG_MACOS9_PTHREAD_H
#define AMLANG_MACOS9_PTHREAD_H

// Classic Mac OS (Retro68, PowerPC): a minimal pthreads on the Thread
// Manager, enough for the AmLang runtime.
//
// Thread Manager threads are COOPERATIVE: a thread runs until it yields. So
// a mutex only has to remember its owner (another thread finding it locked
// yields until it is free), a condition variable waits by yielding until it
// is signalled, and a thread blocked in join yields until the thread ends.
// Thread-specific data is a table indexed by our own small thread slot.
//
// This header shadows the toolchain's (absent) <pthread.h>: the
// am-lang-core include directory is searched before the system one. No Mac
// headers here -- their types (Point, Size, ...) would leak into everything.

#include <stddef.h>
#include <sys/types.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AM_MACOS9_MAX_THREADS 32
#define AM_MACOS9_MAX_KEYS    16

typedef int am_m9thread_t;                  // our slot (0 = the main thread)
typedef struct { int owner; int count; int type; } am_m9mutex_t;
typedef struct { int type; } am_m9mutexattr_t;
typedef struct { volatile unsigned int seq; } am_m9cond_t;
typedef struct { int unused; } am_m9condattr_t;
typedef struct { size_t stacksize; int detached; } am_m9attr_t;
typedef int am_m9key_t;
typedef struct { volatile int done; } am_m9once_t;

#define pthread_t           am_m9thread_t
#define pthread_mutex_t     am_m9mutex_t
#define pthread_mutexattr_t am_m9mutexattr_t
#define pthread_cond_t      am_m9cond_t
#define pthread_condattr_t  am_m9condattr_t
#define pthread_attr_t      am_m9attr_t
#define pthread_key_t       am_m9key_t
#define pthread_once_t      am_m9once_t

#define PTHREAD_MUTEX_NORMAL    0
#define PTHREAD_MUTEX_RECURSIVE 1
#define PTHREAD_MUTEX_DEFAULT   0
#define PTHREAD_MUTEX_INITIALIZER { -1, 0, 0 }
#define PTHREAD_COND_INITIALIZER { 0 }
#define PTHREAD_ONCE_INIT { 0 }
#define PTHREAD_CREATE_JOINABLE 0
#define PTHREAD_CREATE_DETACHED 1

// Our slot for the running thread.
int  am_macos9_thread_slot(void);
// Give the other threads a turn.
void am_macos9_yield(void);

int  pthread_mutexattr_init(pthread_mutexattr_t *a);
int  pthread_mutexattr_destroy(pthread_mutexattr_t *a);
int  pthread_mutexattr_settype(pthread_mutexattr_t *a, int type);
int  pthread_mutex_init(pthread_mutex_t *m, const pthread_mutexattr_t *a);
int  pthread_mutex_destroy(pthread_mutex_t *m);
int  pthread_mutex_lock(pthread_mutex_t *m);
int  pthread_mutex_trylock(pthread_mutex_t *m);
int  pthread_mutex_unlock(pthread_mutex_t *m);

int  pthread_cond_init(pthread_cond_t *c, const pthread_condattr_t *a);
int  pthread_cond_destroy(pthread_cond_t *c);
int  pthread_cond_wait(pthread_cond_t *c, pthread_mutex_t *m);
int  pthread_cond_timedwait(pthread_cond_t *c, pthread_mutex_t *m, const struct timespec *abstime);
int  pthread_cond_signal(pthread_cond_t *c);
int  pthread_cond_broadcast(pthread_cond_t *c);

int  pthread_attr_init(pthread_attr_t *a);
int  pthread_attr_destroy(pthread_attr_t *a);
int  pthread_attr_setstacksize(pthread_attr_t *a, size_t size);
int  pthread_attr_setdetachstate(pthread_attr_t *a, int state);

int  pthread_create(pthread_t *t, const pthread_attr_t *a, void *(*fn)(void *), void *arg);
int  pthread_join(pthread_t t, void **ret);
int  pthread_detach(pthread_t t);
pthread_t pthread_self(void);
int  pthread_equal(pthread_t a, pthread_t b);

int  pthread_key_create(pthread_key_t *k, void (*destructor)(void *));
int  pthread_key_delete(pthread_key_t k);
void *pthread_getspecific(pthread_key_t k);
int  pthread_setspecific(pthread_key_t k, const void *v);

int  pthread_once(pthread_once_t *o, void (*fn)(void));
int  sched_yield(void);

#ifdef __cplusplus
}
#endif

#endif
