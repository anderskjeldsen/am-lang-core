#ifndef native_libc_aclass_Am_Lang_Byte_c
#define native_libc_aclass_Am_Lang_Byte_c
#include <libc/core.h>
#include <Am/Lang/Byte.h>
#include <Am/Lang/Any.h>
#include <Am/Lang/String.h>

function_result Am_Lang_Byte_toString_0(nullable_value const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	char tmp[5];
	sprintf(tmp, "%d", this.value.int_value);

	aobject * str_obj = __allocate_object(&Am_Lang_String);
	string_holder *holder = malloc(sizeof(string_holder));
	str_obj->object_properties.class_object_properties.object_data.value.custom_value = holder;
	int tmp_len = strlen(tmp);
	char * new_str = malloc(tmp_len + 1);
	strcpy(new_str, tmp);
	holder->string_value = new_str; // assume that string constants will never change
	holder->length = tmp_len; // TODO: how many characters exactly?
	holder->is_string_constant = false;

	__result.return_value.value.object_value = str_obj;

__exit: ;
	return __result;
};

function_result Am_Lang_Byte_toShort_0(char const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;

	__result.return_value = (nullable_value) { .value = { .short_value = this }, .flags = 0 };
__exit: ;
	return __result;
};

function_result Am_Lang_Byte_toInt_0(char const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .int_value = this }, .flags = 0 };
__exit: ;
	return __result;
};

function_result Am_Lang_Byte_toLong_0(char const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .long_value = this }, .flags = 0 };
__exit: ;
	return __result;
};

function_result Am_Lang_Byte_toUByte_0(char const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .uint_value = this }, .flags = 0 };

__exit: ;
	return __result;
};

function_result Am_Lang_Byte_toUShort_0(char const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .ushort_value = this }, .flags = 0 };
__exit: ;
	return __result;
};

function_result Am_Lang_Byte_toUInt_0(char const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .uint_value = this }, .flags = 0 };
__exit: ;
	return __result;
};

function_result Am_Lang_Byte_toULong_0(char const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .ulong_value = this }, .flags = 0 };
__exit: ;
	return __result;
};

function_result Am_Lang_Byte_toBool_0(char const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .bool_value = this }, .flags = 0 };
__exit: ;
	return __result;
};

#endif
