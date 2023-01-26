#include <libc/core.h>
#include <Am/Lang/ObjectHelper.h>
#include <libc/Am/Lang/ObjectHelper.h>
#include <Am/Lang/Object.h>
#include <Am/Lang/Bool.h>

function_result Am_Lang_ObjectHelper__native_init_0(aobject * const this)
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
};

function_result Am_Lang_ObjectHelper__native_release_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
__exit: ;
	return __result;
};

function_result Am_Lang_ObjectHelper_equals_0(aobject * a, aobject * b)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value.bool_value = (a == b), .flags = 1 };
	return __result;
};

