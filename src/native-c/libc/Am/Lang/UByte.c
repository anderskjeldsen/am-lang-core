#include <libc/core.h>
#include <Am/Lang/UByte.h>
#include <Am/Lang/Any.h>
#include <Am/Lang/String.h>
#include <string.h>
#include <libc/core_inline_functions.h>

function_result Am_Lang_UByte_toString_0(nullable_value const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;

	aobject * str_obj = __allocate_object_with_extra_size(&Am_Lang_String, sizeof(string_holder) + 4);
	string_holder *holder = (string_holder *) (str_obj + 1);
	str_obj->object_properties.class_object_properties.object_data.value.custom_value = holder;
	char * new_str = (char *) (holder + 1);
	int len = sprintf(new_str, "%u", this.value.uchar_value);

	holder->string_value = new_str; // assume that string constants will never change
	holder->length = len; // TODO: how many characters exactly?
	holder->is_string_constant = false;
	holder->hash = __string_hash(new_str);

	__result.return_value.value.object_value = str_obj;

__exit: ;
	return __result;
};

function_result Am_Lang_UByte_hash_0(nullable_value const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .int_value = this.value.uchar_value }, .flags = 0 };
__exit: ;
	return __result;
};

function_result Am_Lang_UByte_toShort_0(unsigned char const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .short_value = this }, .flags = 0 };

__exit: ;
	return __result;
};

function_result Am_Lang_UByte_toInt_0(unsigned char const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .int_value = this }, .flags = 0 };
__exit: ;
	return __result;
};

function_result Am_Lang_UByte_toLong_0(unsigned char const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .long_value = this }, .flags = 0 };
__exit: ;
	return __result;
};

function_result Am_Lang_UByte_toByte_0(unsigned char const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .uchar_value = this }, .flags = 0 };
__exit: ;
	return __result;
};

function_result Am_Lang_UByte_toUShort_0(unsigned char const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .ushort_value = this }, .flags = 0 };
__exit: ;
	return __result;
};

function_result Am_Lang_UByte_toUInt_0(unsigned char const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .uint_value = this }, .flags = 0 };
__exit: ;
	return __result;
};

function_result Am_Lang_UByte_toULong_0(unsigned char const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .ulong_value = this }, .flags = 0 };
__exit: ;
	return __result;
};

function_result Am_Lang_UByte_toBool_0(unsigned char const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .bool_value = this }, .flags = 0 };
__exit: ;
	return __result;
};


