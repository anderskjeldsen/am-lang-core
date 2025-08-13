#include <libc/core.h>
#include <Am/Lang/Diagnostics/Debug.h>
#include <libc/Am/Lang/Diagnostics/Debug.h>
#include <Am/Lang/ClassRef.h>
#include <Am/Lang/Object.h>
#include <Am/Lang/Bool.h>
#include <libc/core_inline_functions.h>

function_result Am_Lang_Diagnostics_Debug__native_init_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
	if (this != NULL) {
		__increase_reference_count(this);
	}
__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	return __result;
}

function_result Am_Lang_Diagnostics_Debug__native_release_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
__exit: ;
	return __result;
}

function_result Am_Lang_Diagnostics_Debug__native_mark_children_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
__exit: ;
	return __result;
}

function_result Am_Lang_Diagnostics_Debug_setConditionalLogging_0(bool on)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
	__conditional_logging_on = on;
__exit: ;
	return __result;
}

