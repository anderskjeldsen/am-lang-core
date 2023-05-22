#include <libc/core.h>
#include <Am/IO/TextStream.h>
#include <libc/Am/IO/TextStream.h>
#include <Am/IO/Stream.h>
#include <Am/Lang/Object.h>
#include <Am/Lang/UInt.h>
#include <Am/Lang/Array.h>
#include <Am/Lang/Byte.h>
#include <Am/Lang/Long.h>
#include <Am/Lang/Int.h>
#include <Am/Lang/String.h>
#include <libc/core_inline_functions.h>


function_result Am_IO_TextStream__native_init_0(aobject * const this)
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

function_result Am_IO_TextStream__native_release_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
__exit: ;
	return __result;
};

/*
function_result Am_IO_TextStream_readString_0(aobject * const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	if (this != NULL) {
		__increase_reference_count(this);
	}

//	array_holder *array_holder = bytes->object_properties.class_object_properties.object_data.value.custom_value;

//	aobject * new_string = __create_string(array_holder->array_data, &Am_Lang_String);

//	__result.return_value.value.object_value = new_string;
	__result.return_value.flags = 0;


__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	return __result;
};

function_result Am_IO_TextStream_writeString_0(aobject * const this, aobject * string)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
	if (this != NULL) {
		__increase_reference_count(this);
	}
	if (string != NULL) {
		__increase_reference_count(string);
	}
__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	if (string != NULL) {
		__decrease_reference_count(string);
	}
	return __result;
};
*/