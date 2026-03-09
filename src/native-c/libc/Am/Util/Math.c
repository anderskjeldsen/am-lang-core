#include <libc/core.h>
#include <Am/Util/Math.h>
#include <libc/Am/Util/Math.h>
#include <Am/Lang/ClassRef.h>
#include <Am/Lang/Double.h>
#include <Am/Lang/Object.h>
#include <libc/core_inline_functions.h>

#include <math.h>

function_result Am_Util_Math__native_init_0(aobject * const this)
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

function_result Am_Util_Math__native_release_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
__exit: ;
	return __result;
}

function_result Am_Util_Math__native_mark_children_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
__exit: ;
	return __result;
}

function_result Am_Util_Math_sin_0(double angle)
{
	function_result __result = { .has_return_value = true };
	bool __returning = true;
	__result.return_value.value.double_value = sin(angle);
__exit: ;
	return __result;
}

function_result Am_Util_Math_cos_0(double angle)
{
	function_result __result = { .has_return_value = true };
	bool __returning = true;
	__result.return_value.value.double_value = cos(angle);
__exit: ;
	return __result;
}

function_result Am_Util_Math_tan_0(double angle)
{
	function_result __result = { .has_return_value = true };
	bool __returning = true;
	__result.return_value.value.double_value = tan(angle);
__exit: ;
	return __result;
}

function_result Am_Util_Math_asin_0(double value)
{
	function_result __result = { .has_return_value = true };
	bool __returning = true;
	__result.return_value.value.double_value = asin(value);
__exit: ;
	return __result;
}

function_result Am_Util_Math_acos_0(double value)
{
	function_result __result = { .has_return_value = true };
	bool __returning = true;
	__result.return_value.value.double_value = acos(value);
__exit: ;
	return __result;
}

function_result Am_Util_Math_atan_0(double value)
{
	function_result __result = { .has_return_value = true };
	bool __returning = true;
	__result.return_value.value.double_value = atan(value);
__exit: ;
	return __result;
}

function_result Am_Util_Math_atan2_0(double y, double x)
{
	function_result __result = { .has_return_value = true };
	bool __returning = true;
	__result.return_value.value.double_value = atan2(y, x);
__exit: ;
	return __result;
}
