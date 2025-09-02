#include <libc/core.h>
#include <Am/Util/Random.h>
#include <libc/Am/Util/Random.h>
#include <Am/Lang/ClassRef.h>
#include <Am/Lang/Int.h>
#include <Am/Lang/Object.h>
#include <libc/core_inline_functions.h>

#include <stdlib.h>

function_result Am_Util_Random__native_init_0(aobject * const this)
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

function_result Am_Util_Random__native_release_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
__exit: ;
	return __result;
}

function_result Am_Util_Random__native_mark_children_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
__exit: ;
	return __result;
}

function_result Am_Util_Random_randomInt_0(int max)
{
	function_result __result = { .has_return_value = true };
	bool __returning = true;

	__result.return_value.value.int_value = rand() % max;

__exit: ;
	return __result;
}

