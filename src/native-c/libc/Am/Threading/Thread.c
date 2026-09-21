#include <libc/core.h>
#include <Am/Threading/Thread.h>
#include <Am/Lang/Object.h>
#include <Am/Lang/Runnable.h>
#include <Am/Lang/Long.h>
#include <Am/Lang/String.h>
#include <Am/Lang/Exception.h>
#include <libc/core_inline_functions.h>

#include <pthread.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// pthread backend of Am.Threading.Thread under the ISOLATE model (see
// core.h). A Thread aobject is a HANDLE: object_data.custom_value points at
// the aisolate it names — NULL until start() creates one or getCurrent()/
// getParent() binds an existing one — and holds one count on it. The isolate
// owns the OS thread, the mailbox and the thread's private heap and statics.
//
// start() deep-copies the Runnable into a message the worker opens on entry,
// so the worker never touches the creator's objects; results flow back as
// messages. The worker thread holds its own handle on the isolate for as
// long as it runs, so handles held by other threads may die first.

typedef struct {
    pthread_t thread_id;
    bool launched;
    bool joined;    // a joiner has claimed pthread_join (global lock)
    bool detached;  // the worker detached itself at exit (global lock)
} thread_native;

static aisolate * handle_isolate(aobject * const this)
{
    return (aisolate *) this->object_properties.class_object_properties.object_data.value.custom_value;
}

static void bind_handle(aobject * const this, aisolate * const iso)
{
    this->object_properties.class_object_properties.object_data.value.custom_value = iso;
}

// A fresh Thread handle object for `iso` in the calling isolate (defaults
// initialised, one count taken on the isolate). Returns an owned reference.
static aobject * make_handle(aisolate * const iso)
{
    aobject * h = __allocate_object(&Am_Threading_Thread);
    if (h == NULL) return NULL;
    function_result r = Am_Threading_Thread___init_instance((nullable_value) { .flags = 0, .value.object_value = h });
    if (r.exception != NULL) __decrease_reference_count(r.exception);
    __amlc_isolate_retain(iso);
    bind_handle(h, iso);
    if (iso->name != NULL) {
        aobject * name = __create_string(iso->name, &Am_Lang_String);
        if (name != NULL) {
            __set_property(h, Am_Threading_Thread_P_name, (nullable_value) { .flags = 0, .value.object_value = name });
            __decrease_reference_count(name);
        }
    }
    return h;
}

function_result Am_Threading_Thread__native_init_0(aobject * const this)
{
    function_result __result = { .has_return_value = false };
    bind_handle(this, NULL);
    return __result;
}

function_result Am_Threading_Thread__native_release_0(aobject * const this)
{
    function_result __result = { .has_return_value = false };
    aisolate * iso = handle_isolate(this);
    if (iso != NULL) {
        bind_handle(this, NULL);
        __amlc_isolate_release(iso);
    }
    return __result;
}

function_result Am_Threading_Thread__native_mark_children_0(aobject * const this)
{
    function_result __result = { .has_return_value = false };
    (void) this;
    return __result;
}

// #sendable: a copied handle names the same isolate. Only the name travels;
// the runnable and finalizers belong to the creating thread.
aobject * Am_Threading_Thread__native_copy_0(aobject * const src, void * const copy_ctx)
{
    aobject * dst = __allocate_object(src->class_ptr);
    if (dst == NULL) return NULL;
    function_result r = Am_Threading_Thread___init_instance((nullable_value) { .flags = 0, .value.object_value = dst });
    if (r.exception != NULL) __decrease_reference_count(r.exception);
    aobject * name = src->object_properties.class_object_properties.properties[Am_Threading_Thread_P_name].nullable_value.value.object_value;
    if (name != NULL) {
        aobject * c = __amlc_copy_child(copy_ctx, name);
        if (c == NULL) {
            __decrease_reference_count(dst);
            return NULL;
        }
        __set_property(dst, Am_Threading_Thread_P_name, (nullable_value) { .flags = 0, .value.object_value = c });
        __decrease_reference_count(c);
    }
    aisolate * iso = handle_isolate(src);
    if (iso != NULL) {
        __amlc_isolate_retain(iso);
        bind_handle(dst, iso);
    }
    return dst;
}

// Worker entry: become the isolate, initialise its statics, open the copied
// Runnable, run it, run the finalizers registered on this thread's own
// handle, tear the heap down, and drop the running thread's handle.
static void * Am_Threading_Thread__pthread_entry(void *arg)
{
    aisolate * const iso = (aisolate *) arg;
    __amlc_isolate_enter(iso);
    __amlc_isolate_run_static_inits();

    amessage * boot = iso->boot;
    iso->boot = NULL;
    aobject * runnable = boot != NULL ? __amlc_message_open(boot) : NULL;
    if (runnable != NULL) {
        // Runnable is an interface: dispatch through its implementation
        // table. run() is at index 3, after the three inherited
        // AnyInterface methods.
        Am_Lang_Runnable_f_run_0_T rFunc =
            (Am_Lang_Runnable_f_run_0_T) runnable->object_properties.iface_reference.iface_implementation->functions[3];
        function_result r = rFunc(runnable->object_properties.iface_reference.implementation_object);
        if (r.exception != NULL) {
            Am_Lang_Exception_f_printWithStackTrace_0(r.exception);
            __decrease_reference_count(r.exception);
        }
        __decrease_reference_count(runnable);
    }

    if (iso->self_handle != NULL) {
#ifdef Am_Threading_Thread_f_runFinalizers_0__DIRECT_NOTHROW_ABI
        Am_Threading_Thread_f_runFinalizers_0__direct(iso->self_handle);
#else
        function_result fr = Am_Threading_Thread_f_runFinalizers_0(iso->self_handle);
        if (fr.exception != NULL) __decrease_reference_count(fr.exception);
#endif
    }

    __amlc_isolate_teardown(iso);

    thread_native * n = (thread_native *) iso->native;
    __amlc_global_lock();
    bool detach = !n->joined;
    if (detach) n->detached = true;
    __amlc_global_unlock();
    if (detach) pthread_detach(pthread_self());

    __amlc_isolate_release(iso);
    __amlc_isolate_count_dec();   // last runtime action of this thread
    return NULL;
}

function_result Am_Threading_Thread_start_0(aobject * const this)
{
    function_result __result = { .has_return_value = false };

    if (handle_isolate(this) != NULL) {
        __throw_simple_exception("Thread.start called on a thread that was already started", "in Am_Threading_Thread_start_0", &__result);
        return __result;
    }

    aisolate * iso = __amlc_isolate_create();
    bind_handle(this, iso);

    aobject * name = this->object_properties.class_object_properties.properties[Am_Threading_Thread_P_name].nullable_value.value.object_value;
    if (name != NULL) {
        string_holder * nh = (string_holder *) name->object_properties.class_object_properties.object_data.value.custom_value;
        if (nh != NULL && nh->string_value != NULL) iso->name = strdup(nh->string_value);
    }
    iso->priority = this->object_properties.class_object_properties.properties[Am_Threading_Thread_P_priority].nullable_value.value.int_value;

    // The runnable travels to the worker as a message: a deep copy, or an
    // exception here if it references something that cannot be copied.
    aobject * runnable = this->object_properties.class_object_properties.properties[Am_Threading_Thread_P_runnable].nullable_value.value.object_value;
    iso->boot = __amlc_message_create(runnable, &__result);
    if (iso->boot == NULL) {
        return __result;
    }

    thread_native * n = (thread_native *) calloc(1, sizeof(thread_native));
    iso->native = n;
    __amlc_isolate_retain(iso);   // the running thread's own handle
    __amlc_isolate_count_inc();
    int rc = pthread_create(&n->thread_id, NULL, Am_Threading_Thread__pthread_entry, (void *) iso);
    if (rc != 0) {
        __amlc_isolate_count_dec();
        __amlc_isolate_release(iso);
        char msg[96];
        snprintf(msg, sizeof(msg), "Thread.start: pthread_create failed (rc=%d)", rc);
        __throw_simple_exception_copy(msg, "in Am_Threading_Thread_start_0", &__result);
        return __result;
    }
    n->launched = true;
    return __result;
}

function_result Am_Threading_Thread_join_0(aobject * const this)
{
    function_result __result = { .has_return_value = false };
    aisolate * iso = handle_isolate(this);
    if (iso == NULL || iso->native == NULL) {
        return __result;
    }
    thread_native * n = (thread_native *) iso->native;
    __amlc_global_lock();
    bool do_join = n->launched && !n->joined && !n->detached;
    if (do_join) n->joined = true;
    __amlc_global_unlock();
    if (do_join) {
        pthread_join(n->thread_id, NULL);
    } else {
        // Someone else joins, or the worker already detached: wait for done.
        while (!iso->done) {
            struct timespec ts = { 0, 1000000L };
            nanosleep(&ts, NULL);
        }
    }
    return __result;
}

function_result Am_Threading_Thread_send_0(aobject * const this, aobject * message)
{
    function_result __result = { .has_return_value = false };
    aisolate * iso = handle_isolate(this);
    if (iso == NULL) {
        __throw_simple_exception("Thread.send: the thread has not been started", "in Am_Threading_Thread_send_0", &__result);
        return __result;
    }
    __amlc_send(iso, message, &__result);
    return __result;
}

function_result Am_Threading_Thread_receive_0()
{
    function_result __result = { .has_return_value = true };
    aobject * m = __amlc_receive(true);
    if (m == NULL) {
        __throw_simple_exception("Thread.receive: the mailbox is closed", "in Am_Threading_Thread_receive_0", &__result);
        return __result;
    }
    __result.return_value.value.object_value = m;
    return __result;
}

function_result Am_Threading_Thread_tryReceive_0()
{
    function_result __result = { .has_return_value = true };
    __result.return_value.value.object_value = __amlc_receive(false);
    return __result;
}

function_result Am_Threading_Thread_getCurrent_0()
{
    function_result __result = { .has_return_value = true };
    aisolate * iso = __amlc_isolate();
    if (iso->self_handle == NULL) {
        iso->self_handle = make_handle(iso);   // owned by the isolate
    }
    if (iso->self_handle != NULL) {
        __increase_reference_count(iso->self_handle);
    }
    __result.return_value.value.object_value = iso->self_handle;
    return __result;
}

function_result Am_Threading_Thread_getParent_0()
{
    function_result __result = { .has_return_value = true };
    aisolate * parent = __amlc_isolate()->parent;
    if (parent == NULL) {
        __throw_simple_exception("Thread.getParent: the main thread has no parent", "in Am_Threading_Thread_getParent_0", &__result);
        return __result;
    }
    __result.return_value.value.object_value = make_handle(parent);
    return __result;
}

function_result Am_Threading_Thread_sleep_0(long long milliseconds)
{
    function_result __result = { .has_return_value = false };

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
    return __result;
}

function_result Am_Threading_Thread_setCurrentPriority_0(int priority)
{
    // Scheduling priorities are an Amiga-family concept; no-op here.
    function_result __result = { .has_return_value = false };
    (void) priority;
    return __result;
}
