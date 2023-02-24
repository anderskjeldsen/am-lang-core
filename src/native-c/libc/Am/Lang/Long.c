
#include <libc/core.h>
#include <Am/Lang/Long.h>
#include <Am/Lang/Any.h>
#include <Am/Lang/String.h>
#include <string.h>

function_result Am_Lang_Long_toString_0(nullable_value const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	char tmp[21];
	sprintf(tmp, "%lld", this.value.long_value);

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

function_result Am_Lang_Long_toShort_0(long long const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;

	__result.return_value = (nullable_value) { .value = { .short_value = this }, .flags = 0 };

__exit: ;
	return __result;
};

function_result Am_Lang_Long_toInt_0(long long const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;

	__result.return_value = (nullable_value) { .value = { .int_value = this }, .flags = 0 };
__exit: ;
	return __result;
};

function_result Am_Lang_Long_toByte_0(long long const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .char_value = this }, .flags = 0 };

__exit: ;
	return __result;
};

function_result Am_Lang_Long_toUByte_0(long long const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .uchar_value = this }, .flags = 0 };

__exit: ;
	return __result;
};

function_result Am_Lang_Long_toUShort_0(long long const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .ushort_value = this }, .flags = 0 };

__exit: ;
	return __result;
};

function_result Am_Lang_Long_toUInt_0(long long const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .uint_value = this }, .flags = 0 };

__exit: ;
	return __result;
};

function_result Am_Lang_Long_toULong_0(long long const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .ulong_value = this }, .flags = 0 };

__exit: ;
	return __result;
};

function_result Am_Lang_Long_toBool_0(long long const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .bool_value = this }, .flags = 0 };

__exit: ;
	return __result;
};

