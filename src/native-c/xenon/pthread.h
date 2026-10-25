#ifndef AMLANG_XENON_PTHREAD_H
#define AMLANG_XENON_PTHREAD_H

// Xbox 360 (libxenon): a minimal pthreads, enough for the AmLang runtime.
//
// libxenon has no scheduler. What it does have is the Xenon's six HARDWARE
// threads (three cores x two), each of which can be handed one task with
// xenon_run_thread_task(). So a "thread" here IS a hardware thread: at most
// five besides main, never preempted, never sharing a core's thread with a
// second task. Locks spin (lwarx/stwcx. via libxenon's lock()), condition
// variables spin on a sequence number, and thread-specific data is a table
// indexed by the hardware thread id (PIR) -- which is what the runtime's
// isolate binding needs.
//
// This header deliberately shadows newlib's <pthread.h> (which is gated off
// without _POSIX_THREADS): the am-lang-core include directory is searched
// before the system one.

#include <stddef.h>
#include <sys/types.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AM_XENON_HW_THREADS 6
#define AM_XENON_MAX_KEYS   16

typedef int am_xpthread_t;
typedef struct { volatile unsigned int lk; volatile int owner; int count; int type; } am_xmutex_t;
typedef struct { int type; } am_xmutexattr_t;
typedef struct { volatile unsigned int seq; } am_xcond_t;
typedef struct { int unused; } am_xcondattr_t;
typedef struct { void *stack; size_t stacksize; int detached; } am_xattr_t;
typedef int am_xkey_t;
typedef struct { volatile unsigned int lk; volatile int done; } am_xonce_t;

#define pthread_t           am_xpthread_t
#define pthread_mutex_t     am_xmutex_t
#define pthread_mutexattr_t am_xmutexattr_t
#define pthread_cond_t      am_xcond_t
#define pthread_condattr_t  am_xcondattr_t
#define pthread_attr_t      am_xattr_t
#define pthread_key_t       am_xkey_t
#define pthread_once_t      am_xonce_t

#define PTHREAD_MUTEX_NORMAL    0
#define PTHREAD_MUTEX_RECURSIVE 1
#define PTHREAD_MUTEX_DEFAULT   0
#define PTHREAD_MUTEX_INITIALIZER { 0, -1, 0, 0 }
#define PTHREAD_ONCE_INIT { 0, 0 }
#define PTHREAD_CREATE_JOINABLE 0
#define PTHREAD_CREATE_DETACHED 1

int  am_xenon_thread_id(void);

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
int  pthread_cond_signal(pthread_cond_t *c);
int  pthread_cond_broadcast(pthread_cond_t *c);

int  pthread_attr_init(pthread_attr_t *a);
int  pthread_attr_destroy(pthread_attr_t *a);
int  pthread_attr_setstack(pthread_attr_t *a, void *stack, size_t size);
int  pthread_attr_setstacksize(pthread_attr_t *a, size_t size);
int  pthread_attr_setdetachstate(pthread_attr_t *a, int state);

int  pthread_create(pthread_t *t, const pthread_attr_t *a, void *(*fn)(void *), void *arg);
int  pthread_join(pthread_t t, void **ret);
int  pthread_detach(pthread_t t);
pthread_t pthread_self(void);
int  pthread_equal(pthread_t a, pthread_t b);

int  pthread_key_create(pthread_key_t *k, void (*dtor)(void *));
int  pthread_key_delete(pthread_key_t k);
void *pthread_getspecific(pthread_key_t k);
int  pthread_setspecific(pthread_key_t k, const void *v);
int  pthread_once(pthread_once_t *o, void (*fn)(void));

int  sched_yield(void);

#ifdef __cplusplus
}
#endif

#endif
