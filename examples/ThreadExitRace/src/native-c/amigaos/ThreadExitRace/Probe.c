#include <libc/core.h>
#include <ThreadExitRace/Probe.h>
#include <libc/core_inline_functions.h>

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <exec/tasks.h>
#include <proto/exec.h>
#include <string.h>

function_result ThreadExitRace_Probe__native_init_0(aobject * const this)
{ function_result r = { .has_return_value = false }; (void) this; return r; }
function_result ThreadExitRace_Probe__native_release_0(aobject * const this)
{ function_result r = { .has_return_value = false }; (void) this; return r; }
function_result ThreadExitRace_Probe__native_mark_children_0(aobject * const this)
{ function_result r = { .has_return_value = false }; (void) this; return r; }

// Walks the ready and wait lists (the caller, running, is in neither) for
// tasks whose name starts with prefix.
function_result ThreadExitRace_Probe_tasks_0(aobject * const prefix)
{
	function_result __result = { .has_return_value = true };
	string_holder * h = (string_holder *) prefix->object_properties.class_object_properties.object_data.value.custom_value;
	const char * p = h != NULL && h->string_value != NULL ? h->string_value : "";
	size_t n = strlen(p);
	int alive = 0, bound = 0;
	struct List * const lists[2] = { &SysBase->TaskReady, &SysBase->TaskWait };
	Disable();
	for (int l = 0; l < 2; l++) {
		for (struct Node * nd = lists[l]->lh_Head; nd->ln_Succ != NULL; nd = nd->ln_Succ) {
			if (nd->ln_Name != NULL && strncmp(nd->ln_Name, p, n) == 0) {
				alive++;
				if (((struct Task *) nd)->tc_UserData != NULL) bound++;
			}
		}
	}
	Enable();
	__result.return_value.value.int_value = alive * 1000 + bound;
	return __result;
}

function_result ThreadExitRace_Probe_freeMemory_0(void)
{
	function_result __result = { .has_return_value = true };
	__result.return_value.value.long_value = (long long) AvailMem(MEMF_ANY);
	return __result;
}
