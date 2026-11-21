#include <libc/core.h>
#include <ThreadExitRace/Probe.h>
#include <libc/core_inline_functions.h>

function_result ThreadExitRace_Probe__native_init_0(aobject * const this)
{ function_result r = { .has_return_value = false }; (void) this; return r; }
function_result ThreadExitRace_Probe__native_release_0(aobject * const this)
{ function_result r = { .has_return_value = false }; (void) this; return r; }
function_result ThreadExitRace_Probe__native_mark_children_0(aobject * const this)
{ function_result r = { .has_return_value = false }; (void) this; return r; }

// POSIX threads leave no trace to count once joined (pthread_join waits for
// the thread itself to end).
function_result ThreadExitRace_Probe_tasks_0(aobject * const prefix)
{
	function_result __result = { .has_return_value = true };
	(void) prefix;
	__result.return_value.value.int_value = 0;
	return __result;
}

function_result ThreadExitRace_Probe_freeMemory_0(void)
{
	function_result __result = { .has_return_value = true };
	__result.return_value.value.long_value = 0;
	return __result;
}
