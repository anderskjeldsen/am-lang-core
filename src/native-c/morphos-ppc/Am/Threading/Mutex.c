#include <libc/core.h>
#include <Am/Threading/Mutex.h>
#include <Am/Lang/Object.h>
#include <Am/Lang/Bool.h>
#include <libc/core_inline_functions.h>

#include <amigaos/amiga.h>

#include <exec/types.h>
#include <exec/semaphores.h>
#include <proto/exec.h>

/*
 * Am.Threading.Mutex — MorphOS port. Same exec.library SignalSemaphore
 * primitives as the AmigaOS build (recursive, OS-queued, no spin); see
 * the amigaos/ sibling for the full rationale.
 */

function_result Am_Threading_Mutex__native_init_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;

	struct SignalSemaphore *sem = (struct SignalSemaphore *) malloc(sizeof(struct SignalSemaphore));
	InitSemaphore(sem);
	this->object_properties.class_object_properties.object_data.value.custom_value = sem;

__exit: ;
	return __result;
}

function_result Am_Threading_Mutex__native_release_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;

	struct SignalSemaphore *sem = (struct SignalSemaphore *) this->object_properties.class_object_properties.object_data.value.custom_value;
	if (sem != NULL) {
		free(sem);
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

	struct SignalSemaphore *sem = (struct SignalSemaphore *) this->object_properties.class_object_properties.object_data.value.custom_value;
	ObtainSemaphore(sem);

__exit: ;
	return __result;
}

function_result Am_Threading_Mutex_unlock_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;

	struct SignalSemaphore *sem = (struct SignalSemaphore *) this->object_properties.class_object_properties.object_data.value.custom_value;
	ReleaseSemaphore(sem);

__exit: ;
	return __result;
}

function_result Am_Threading_Mutex_tryLock_0(aobject * const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;

	struct SignalSemaphore *sem = (struct SignalSemaphore *) this->object_properties.class_object_properties.object_data.value.custom_value;
	ULONG got = AttemptSemaphore(sem);
	__result.return_value.value.bool_value = (got != 0);

__exit: ;
	return __result;
}
