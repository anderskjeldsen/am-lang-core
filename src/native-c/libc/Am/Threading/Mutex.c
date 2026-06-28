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
	this->object_properties.class_object_properties.object_data.value.custom_value = m;

__exit: ;
	return __result;
}

function_result Am_Threading_Mutex__native_release_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;

	pthread_mutex_t *m = (pthread_mutex_t *) this->object_properties.class_object_properties.object_data.value.custom_value;
	if (m != NULL) {
		pthread_mutex_destroy(m);
		free(m);
		this->object_properties.class_object_properties.object_data.value.custom_value = NULL;
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

	pthread_mutex_t *m = (pthread_mutex_t *) this->object_properties.class_object_properties.object_data.value.custom_value;
	pthread_mutex_lock(m);

__exit: ;
	return __result;
}

function_result Am_Threading_Mutex_unlock_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;

	pthread_mutex_t *m = (pthread_mutex_t *) this->object_properties.class_object_properties.object_data.value.custom_value;
	pthread_mutex_unlock(m);

__exit: ;
	return __result;
}

function_result Am_Threading_Mutex_tryLock_0(aobject * const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;

	pthread_mutex_t *m = (pthread_mutex_t *) this->object_properties.class_object_properties.object_data.value.custom_value;
	int rc = pthread_mutex_trylock(m);
	__result.return_value.value.bool_value = (rc == 0);

__exit: ;
	return __result;
}
