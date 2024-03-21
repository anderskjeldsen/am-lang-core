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

function_result Am_Lang_PropertyInfo__native_mark_children_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
__exit: ;
	return __result;
}

function_result Am_Lang_PropertyInfo_getPropertyClassRef_0(aobject * const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	if (this != NULL) {
		__increase_reference_count(this);
	}
	property * properties = (property *) &this[1];
	aclass ** class_holder_ptr = (aclass **) &properties[3];
	aclass * cls = class_holder_ptr[0];
	aobject * class_ref = cls->class_ref_singleton;
	__increase_reference_count(class_ref);

	__result.return_value = (nullable_value) { .flags = 0, .value.object_value = class_ref };
__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	return __result;
}

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

	printf("set property 1\n");
	int index = this->object_properties.class_object_properties.properties[Am_Lang_PropertyInfo_P_index].nullable_value.value.uchar_value;
	printf("set property 2\n");

	if (!__set_property_safe(target, index, value)) {
		// 		__throw_simple_exception("Failed to open directory", "in Am_IO_File_listNative_0", &__result);

		__throw_simple_exception("Failed to set property", "in Am_Lang_PropertyInfo_setValue_0", &__result);
		goto __exit;
	}
	printf("set property 3\n");
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

