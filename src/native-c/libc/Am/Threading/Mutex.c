#include <libc/core.h>
#include <Am/Threading/Mutex.h>
#include <Am/Lang/Object.h>
#include <Am/Lang/Bool.h>
#include <libc/core_inline_functions.h>

#include <pthread.h>
#include <stdlib.h>

/*
 * Am.Threading.Mutex — recursive mutex backed by a pthread_mutex_t
 * with PTHREAD_MUTEX_RECURSIVE. Mirrors the AmigaOS SignalSemaphore
 * implementation (../amigaos/Am/Threading/Mutex.c): recursive (same
 * thread may lock n times, must unlock n times), blocks on
 * contention, tryLock returns false on a busy mutex.
 *
 * Per-instance pthread_mutex_t hangs off the aobject's custom_value
 * slot. __native_init_0 initialises with PTHREAD_MUTEX_RECURSIVE;
 * __native_release_0 destroys + frees. Destroying a still-locked
 * mutex is undefined per POSIX; caller's job to balance.
 */

function_result Am_Threading_Mutex__native_init_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;

	pthread_mutex_t *m = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(m, &attr);
	pthread_mutexattr_destroy(&attr);
	__unwrap(this)->object_properties.class_object_properties.object_data.value.custom_value = m;

__exit: ;
	return __result;
}

function_result Am_Threading_Mutex__native_release_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;

	aobject * const real = __unwrap(this);
	pthread_mutex_t *m = (pthread_mutex_t *) real->object_properties.class_object_properties.object_data.value.custom_value;
	if (m != NULL) {
		pthread_mutex_destroy(m);
		free(m);
		real->object_properties.class_object_properties.object_data.value.custom_value = NULL;
	}

__exit: ;
	return __result;
}

function_result Am_Threading_Mutex__native_mark_children_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
__exit: ;
	return __result;
}

function_result Am_Threading_Mutex_lock_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;

	// Unwrap first: a cross-thread wrapper overlays object_data with
	// object_wrapper.wrapped_object, so reading custom_value off the raw
	// wrapper yields a garbage pointer — pthread_mutex_lock would then
	// operate on the wrong memory and provide NO mutual exclusion (races,
	// corruption). Mirrors Thread.c. `this` may legitimately be a wrapper
	// here: a mutex owned by one thread is locked from another.
	pthread_mutex_t *m = (pthread_mutex_t *) __unwrap(this)->object_properties.class_object_properties.object_data.value.custom_value;
	pthread_mutex_lock(m);

__exit: ;
	return __result;
}

function_result Am_Threading_Mutex_unlock_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;

	// See lock_0: unwrap so a cross-thread wrapper resolves to the real mutex.
	pthread_mutex_t *m = (pthread_mutex_t *) __unwrap(this)->object_properties.class_object_properties.object_data.value.custom_value;
	pthread_mutex_unlock(m);

__exit: ;
	return __result;
}

function_result Am_Threading_Mutex_tryLock_0(aobject * const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;

	// See lock_0: unwrap so a cross-thread wrapper resolves to the real mutex.
	pthread_mutex_t *m = (pthread_mutex_t *) __unwrap(this)->object_properties.class_object_properties.object_data.value.custom_value;
	int rc = pthread_mutex_trylock(m);
	__result.return_value.value.bool_value = (rc == 0);

__exit: ;
	return __result;
}
