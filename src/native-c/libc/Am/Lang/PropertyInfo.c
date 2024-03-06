#include <libc/core.h>
#include <Am/Lang/PropertyInfo.h>
#include <libc/Am/Lang/PropertyInfo.h>
#include <Am/Lang/Object.h>
#include <Am/Lang/Any.h>
#include <Am/Lang/String.h>
#include <libc/core_inline_functions.h>

function_result Am_Lang_PropertyInfo__native_init_0(aobject * const this)
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

function_result Am_Lang_PropertyInfo__native_release_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
__exit: ;
	return __result;
};

function_result Am_Lang_PropertyInfo_getValue_0(aobject * const this, aobject * target)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	if (this != NULL) {
		__increase_reference_count(this);
	}
	if (target != NULL) {
		__increase_reference_count(target);
	}

	int index = this->object_properties.class_object_properties.properties[Am_Lang_PropertyInfo_P_index].nullable_value.value.uchar_value;

	__result.return_value = target->object_properties.class_object_properties.properties[index].nullable_value;

__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	return __result;
};

function_result Am_Lang_PropertyInfo_setValue_0(aobject * const this, aobject * target, nullable_value value)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
	if (this != NULL) {
		__increase_reference_count(this);
	}
	if (target != NULL) {
		__increase_reference_count(target);
	}
	__increase_reference_count_nullable_value(value);

	int index = this->object_properties.class_object_properties.properties[Am_Lang_PropertyInfo_P_index].nullable_value.value.uchar_value;
	target->object_properties.class_object_properties.properties[index].nullable_value = value;


__exit: ;
	if (target != NULL) {
		__decrease_reference_count(target);
	}
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	__decrease_reference_count_nullable_value(value);
	return __result;
};

