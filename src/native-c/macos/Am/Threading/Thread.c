#include <libc/core.h>
#include <Am/Threading/Thread.h>
#include <macos/Am/Threading/Thread.h>
#include <Am/Lang/Object.h>
#include <Am/Lang/Runnable.h>
#include <Am/Lang/Long.h>
#include <Am/Lang/String.h>
#include <libc/core_inline_functions.h>

function_result Am_Threading_Thread__native_init_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
	// Add reference count for this in Thread._native_init
	if (this != NULL) {
		__increase_reference_count(this);
	}
	// TODO: implement native function Am_Threading_Thread__native_init_0
__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	return __result;
};

function_result Am_Threading_Thread__native_release_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
	// TODO: implement native function Am_Threading_Thread__native_release_0
__exit: ;
	return __result;
};

function_result Am_Threading_Thread_start_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
	// Add reference count for this in Thread.start
	if (this != NULL) {
		__increase_reference_count(this);
	}
	// TODO: implement native function Am_Threading_Thread_start_0
__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	return __result;
};

function_result Am_Threading_Thread_join_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
	// Add reference count for this in Thread.join
	if (this != NULL) {
		__increase_reference_count(this);
	}
	// TODO: implement native function Am_Threading_Thread_join_0
__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	return __result;
};

function_result Am_Threading_Thread_getCurrent_0()
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
	// TODO: implement native function Am_Threading_Thread_getCurrent_0
__exit: ;
	return __result;
};

function_result Am_Threading_Thread_sleep_0(long long milliseconds)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
	// TODO: implement native function Am_Threading_Thread_sleep_0
__exit: ;
	return __result;
};
