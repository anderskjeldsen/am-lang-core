#include <libc/core.h>
#include <Am/Lang/ClassRef.h>
#include <libc/Am/Lang/ClassRef.h>
#include <Am/Lang/Object.h>
#include <Am/Lang/Any.h>
#include <Am/Lang/String.h>
#include <Am/Lang/Array.h>
#include <Am/Lang/PropertyInfo.h>
#include <libc/core_inline_functions.h>

function_result Am_Lang_ClassRef__native_init_0(aobject * const this)
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

function_result Am_Lang_ClassRef__native_release_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
__exit: ;
	return __result;
}

function_result Am_Lang_ClassRef__native_mark_children_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
__exit: ;
	return __result;
}

/*
function_result Am_Lang_ClassRef_initFromAny_0(aobject * const this, nullable_value any)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
	if (this != NULL) {
		__increase_reference_count(this);
	}
	__increase_reference_count_nullable_value(any);

	aclass * class_ptr = NULL;

	if (__is_primitive(any)) {
// #define PRIMITIVE_BOOL 128 | PRIMITIVE // is it a bool type?
// #define PRIMITIVE_BOOL_TRUE 16 // if it's a bool type, is it true (1) ?

// #define PRIMITIVE_LONG PRIMITIVE_BYTE_SIZE_EXP_1 | PRIMITIVE_BYTE_SIZE_EXP_2 | PRIMITIVE
// #define PRIMITIVE_INT PRIMITIVE_BYTE_SIZE_EXP_2 | PRIMITIVE
// #define PRIMITIVE_SHORT PRIMITIVE_BYTE_SIZE_EXP_1 | PRIMITIVE
// #define PRIMITIVE_CHAR PRIMITIVE

// #define PRIMITIVE_ULONG PRIMITIVE_LONG | PRIMITIVE_UNSIGNED
// #define PRIMITIVE_UINT PRIMITIVE_INT | PRIMITIVE_UNSIGNED
// #define PRIMITIVE_USHORT PRIMITIVE_SHORT | PRIMITIVE_UNSIGNED
// #define PRIMITIVE_UCHAR PRIMITIVE_CHAR | PRIMITIVE_UNSIGNED

// #define PRIMITIVE_DOUBLE PRIMITIVE_LONG | PRIMITIVE_FLOATING_POINT_NUMBER
// #define PRIMITIVE_FLOAT PRIMITIVE_INT | PRIMITIVE_FLOATING_POINT_NUMBER

		nullable_value *any_ref = &any;

		if (__any_has_flags(any_ref, PRIMITIVE_BOOL)) {
			class_ptr = &Am_Lang_Bool;
		} else if (__any_has_flags(any_ref, PRIMITIVE_LONG)) {
			class_ptr = &Am_Lang_Long;
		} else if (__any_has_flags(any_ref, PRIMITIVE_INT)) {
			class_ptr = &Am_Lang_Int;
		} else if (__any_has_flags(any_ref, PRIMITIVE_SHORT)) {
			class_ptr = &Am_Lang_Short;
		} else if (__any_has_flags(any_ref, PRIMITIVE_CHAR)) {
			class_ptr = &Am_Lang_Byte;
		} else if (__any_has_flags(any_ref, PRIMITIVE_ULONG)) {
			class_ptr = &Am_Lang_ULong;
		} else if (__any_has_flags(any_ref, PRIMITIVE_UINT)) {
			class_ptr = &Am_Lang_UInt;
		} else if (__any_has_flags(any_ref, PRIMITIVE_USHORT)) {
			class_ptr = &Am_Lang_UShort;
		} else if (__any_has_flags(any_ref, PRIMITIVE_UCHAR)) {
			class_ptr = &Am_Lang_UByte;
//		} else if (__any_has_flags(any_ref, PRIMITIVE_DOUBLE)) {
//			class_ptr = &Am_Lang_Double;
//		} else if (__any_has_flags(any_ref, PRIMITIVE_FLOAT)) {
//			class_ptr = &Am_Lang_Float;
		} else {
			__throw_simple_exception("Invalid primitive type", "initFromAny", &__result);
			goto __exit;
		}
	} else {
		aobject * obj = any.value.object_value;
		class_ptr = obj->class_ptr;
	}

	aobject *class_name = __create_string_constant(class_ptr->name, &Am_Lang_String);
	__set_property(this, Am_Lang_ClassRef_P_className, (nullable_value) { .flags = 0, .value.object_value = class_name } );
	__decrease_reference_count(class_name);

__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	__decrease_reference_count_nullable_value(any);
	return __result;
}
*/

function_result Am_Lang_ClassRef_getClassRefFromAny_0(nullable_value any)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__increase_reference_count_nullable_value(any);
	aobject * class_ref = NULL;
	if (any.flags == 0) {

		class_ref = any.value.object_value->class_ptr->class_ref_singleton;
	} else {

		ctype ctype = __value_flags_to_ctype(any.flags);

		switch(ctype) {
			case long_type:
				class_ref = Am_Lang_Long.class_ref_singleton;
				break;
			case int_type:
				class_ref = Am_Lang_Int.class_ref_singleton;
				break;
			case short_type:
				class_ref = Am_Lang_Short.class_ref_singleton;
				break;
			case char_type:
				class_ref = Am_Lang_Byte.class_ref_singleton;
				break;
			case ulong_type:
				class_ref = Am_Lang_ULong.class_ref_singleton;
				break;
			case uint_type:
				class_ref = Am_Lang_UInt.class_ref_singleton;
				break;
			case ushort_type:
				class_ref = Am_Lang_UShort.class_ref_singleton;
				break;
			case uchar_type:
				class_ref = Am_Lang_UByte.class_ref_singleton;
				break;
				/* TODO: add float and double
			case float_type:
				class_ref = &Am_Lang_Float;
				break;
			case double_type:
				class_ref = &Am_Lang_Double;
				break;
				*/
			case bool_type:
				class_ref = Am_Lang_Bool.class_ref_singleton;
				break;
			default:
				__throw_simple_exception("Invalid primitive type", "getClassRefFromAny", &__result);
				goto __exit;
		}
	}

	if (class_ref == NULL) {
	}

	__increase_reference_count(class_ref); // one extra, because we want to keep the object until the end.
	__result.return_value.value.object_value = class_ref;

__exit: ;
	__decrease_reference_count_nullable_value(any);
	return __result;
}
