#include <libc/core.h>
#include <Am/Threading/Thread.h>
#include <Am/Lang/Object.h>
#include <Am/Lang/Runnable.h>
#include <Am/Lang/Long.h>
#include <Am/Lang/String.h>
#include <libc/core_inline_functions.h>

#include <pthread.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>

// pthread-backed implementation of Am.Threading.Thread for libc-derived
// targets (linux-*, macos-*, aros-x86-64, …). Mirrors the structure of
// the AmigaOS m68k version (../amigaos/Am/Threading/Thread.c):
//
//   * Per-instance `data` malloc'd in `_native_init_0`, freed in
//     `_native_release_0`. Holds the pthread handle plus a `done` flag
//     equivalent to the AmigaOS one.
//   * `start_0` spawns a worker that calls runnable.run() then sets
//     `done = true`. Same Runnable-dispatch pattern as AmigaOS:
//     properties[0] is the runnable, vtable index 3 on its class is
//     `run()`.
//   * `join_0` blocks via `pthread_join` (no busy-wait — pthread
//     supports a real join, unlike the AmigaOS Delay() poll).
//   * `getCurrent_0` reads the AmLang Thread aobject out of pthread
//     thread-local storage. Returns NULL on the main thread (it never
//     went through `start_0`), matching what the AmigaOS version does
//     when `tc_UserData` is unset.
//   * `sleep_0` uses `nanosleep` for ms-resolution sleep.
//
// `start_0` takes an extra reference on `this` for the worker thread's
// lifetime; the worker entry drops that reference right before it
// returns. This bracket-pair guarantees the Thread aobject stays alive
// even if the AmLang caller drops their last reference before the
// runnable finishes — without it, a worker accessing `data->done` (or
// any property on `thread`) after the caller's release is a use-after-
// free. The refcount helpers themselves aren't atomic right now, so
// concurrent ref-changes from two threads are still racy in principle;
// once the runtime gains atomic refcounts, this pattern stays correct.

typedef struct _Am_Threading_Thread_data Am_Threading_Thread_data;
struct _Am_Threading_Thread_data {
    pthread_t thread_id;
    bool started;
    bool done;
};

// Thread-local key used to expose the AmLang Thread aobject from
// `getCurrent_0`. Lazily initialised on first use; pthread_once makes
// the initialisation safe across concurrent first-callers.
static pthread_key_t  current_thread_key;
static pthread_once_t current_thread_key_once = PTHREAD_ONCE_INIT;

static void make_current_thread_key(void)
{
    pthread_key_create(&current_thread_key, NULL);
}

function_result Am_Threading_Thread__native_init_0(aobject * const this)
{
    function_result __result = { .has_return_value = false };
    bool __returning = false;

    Am_Threading_Thread_data *data = malloc(sizeof(Am_Threading_Thread_data));
    data->started = false;
    data->done    = false;
    // Unwrap before writing object_data — if `this` was passed in as a
    // cross-thread wrapper, direct writes would land on the wrapper's
    // union variant instead of the real object's object_data slot.
    __unwrap(this)->object_properties.class_object_properties.object_data.value.custom_value = data;

__exit: ;
    return __result;
}

function_result Am_Threading_Thread__native_release_0(aobject * const this)
{
    function_result __result = { .has_return_value = false };
    bool __returning = false;

    aobject * const real = __unwrap(this);
    Am_Threading_Thread_data *data =
        (Am_Threading_Thread_data *) real->object_properties.class_object_properties.object_data.value.custom_value;
    if (data != NULL) {
        // If the thread was started but never joined, detach so the
        // worker's pthread resources get reclaimed when it eventually
        // exits. (If `done` is already true, detach is still safe — it
        // just lets the kernel reap immediately.)
        if (data->started) {
            pthread_detach(data->thread_id);
        }
        free(data);
        real->object_properties.class_object_properties.object_data.value.custom_value = NULL;
    }

__exit: ;
    return __result;
}

function_result Am_Threading_Thread__native_mark_children_0(aobject * const this)
{
    function_result __result = { .has_return_value = false };
    bool __returning = false;
__exit: ;
    return __result;
}

// Worker thread entry point. `arg` is the AmLang Thread aobject we
// were handed by `start_0`. We stash it in TLS for `getCurrent_0`,
// dispatch into the runnable, then set `done` so `join` can observe
// completion (mirrors the AmigaOS `_InitTask` flow).
static void *Am_Threading_Thread__pthread_entry(void *arg)
{
    // `arg` is the wrapper pointer that `start_0` handed us. Keep the
    // raw pointer for TLS + refcount (matches the wrapper we took a ref
    // on) and use an unwrapped copy for every DATA read — cross-thread
    // wrappers store the real object in a different union variant, so
    // dereferences on the wrapper directly would read the wrapper's own
    // memory and silently no-op the thread body.
    aobject *thread_ref = (aobject *) arg;
    aobject *thread     = __unwrap(thread_ref);

    pthread_once(&current_thread_key_once, make_current_thread_key);
    // TLS carries the wrapper pointer — `getCurrent_0` on this thread
    // then hands the same wrapper back to AmLang code, so its lifetime
    // is tied to the ref taken in `start_0`.
    pthread_setspecific(current_thread_key, thread_ref);

    aobject *runnable_ref =
        thread->object_properties.class_object_properties.properties[0].nullable_value.value.object_value;
    // Runnable slot itself may be a wrapper — unwrap for dispatch reads.
    aobject *runnable = __unwrap(runnable_ref);
    // Runnable is an interface, so dispatch through the iface_implementation
    // the wrapper carries. The interface function table is laid out as the
    // inherited AnyInterface methods (indices 0..2) followed by Runnable's own
    // methods, so run() is at index 3 — matching the compiler-generated call
    // sites (e.g. Am/Ui/Window.c). Using index 0 calls an AnyInterface method
    // instead, which silently no-ops the thread body.
    Am_Lang_Runnable_f_run_0_T rFunc =
        (Am_Lang_Runnable_f_run_0_T) runnable->object_properties.iface_reference.iface_implementation->functions[3];
    rFunc(runnable->object_properties.iface_reference.implementation_object);

    Am_Threading_Thread_data *data =
        (Am_Threading_Thread_data *) thread->object_properties.class_object_properties.object_data.value.custom_value;
    data->done = true;

    // Drop the worker's reference on the wrapper we were handed (matches
    // the `__increase_reference_count(this)` in `start_0`). Every
    // property access above this line happens while the wrapper — and
    // therefore the real — is still alive.
    __decrease_reference_count(thread_ref);

    return NULL;
}

function_result Am_Threading_Thread_start_0(aobject * const this)
{
    function_result __result = { .has_return_value = false };
    bool __returning = false;

    pthread_once(&current_thread_key_once, make_current_thread_key);

    Am_Threading_Thread_data *data =
        (Am_Threading_Thread_data *) __unwrap(this)->object_properties.class_object_properties.object_data.value.custom_value;

    // Take an extra ref for the worker thread; it'll drop it as the
    // very last thing in the entry function. See file-level comment.
    //
    // DO NOT STRIP — this is a lifetime extension for the WORKER thread,
    // not the redundant per-call inc that the native-arg-strip sweep
    // targeted. The strip script accidentally took it; the pthread entry
    // still calls __decrease_reference_count(thread) on exit, so removing
    // this dec leaves a -1 imbalance → premature free → heap corruption
    // observed as SIGBUS in TaskScheduler.stopAll's println() at shutdown.
    __increase_reference_count(this);

    int rc = pthread_create(&data->thread_id, NULL,
                            Am_Threading_Thread__pthread_entry, (void *) this);
    if (rc != 0) {
        // Worker won't run, so undo the ref we took above.
        __decrease_reference_count(this);
        // TODO: throw a proper exception once the runtime exposes a
        // helper. For now, log and leave `started` false so `join`
        // becomes a no-op.
        printf("Am_Threading_Thread_start_0: pthread_create failed (rc=%d)\n", rc);
    } else {
        data->started = true;
    }

__exit: ;
    return __result;
}

function_result Am_Threading_Thread_join_0(aobject * const this)
{
    function_result __result = { .has_return_value = false };
    bool __returning = false;

    Am_Threading_Thread_data *data =
        (Am_Threading_Thread_data *) __unwrap(this)->object_properties.class_object_properties.object_data.value.custom_value;
    if (data != NULL && data->started) {
        pthread_join(data->thread_id, NULL);
        // pthread_join joined-and-reaped — no further detach needed.
        data->started = false;
    }

__exit: ;
    return __result;
}

function_result Am_Threading_Thread_getCurrent_0()
{
    function_result __result = { .has_return_value = true };
    bool __returning = false;

    pthread_once(&current_thread_key_once, make_current_thread_key);
    aobject *thread = (aobject *) pthread_getspecific(current_thread_key);

    // NULL on the main thread (or any other thread not started via
    // `Am.Threading.Thread.start`). Matches AmigaOS behaviour when
    // `tc_UserData` is unset.
    __result.return_value.value.object_value = thread;

__exit: ;
    return __result;
}

function_result Am_Threading_Thread_sleep_0(long long milliseconds)
{
    function_result __result = { .has_return_value = false };
    bool __returning = false;

    if (milliseconds > 0) {
        struct timespec ts;
        ts.tv_sec  = (time_t) (milliseconds / 1000LL);
        ts.tv_nsec = (long)   ((milliseconds % 1000LL) * 1000000L);
        // EINTR-safe spin: nanosleep returns -1/EINTR with `rem` set
        // to the remaining duration if interrupted by a signal.
        struct timespec rem;
        while (nanosleep(&ts, &rem) == -1) {
            ts = rem;
        }
    }

__exit: ;
    return __result;
}
