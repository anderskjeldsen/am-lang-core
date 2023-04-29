#include <libc/core.h>
#include <Am/Lang/Short.h>
#include <Am/Lang/Any.h>
#include <Am/Lang/String.h>
#include <string.h>

function_result Am_Lang_Short_toString_0(nullable_value const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	char tmp[7];
	sprintf(tmp, "%d", this.value.short_value);

	int tmp_len = strlen(tmp);
	aobject * str_obj = __allocate_object_with_extra_size(&Am_Lang_String, sizeof(string_holder) + tmp_len + 1);
	string_holder *holder = (string_holder *) (str_obj + 1);
	str_obj->object_properties.class_object_properties.object_data.value.custom_value = holder;
	char * new_str = (char *) (holder + 1);

	strcpy(new_str, tmp);
	holder->string_value = new_str; // assume that string constants will never change
	holder->length = tmp_len; // TODO: how many characters exactly?
	holder->is_string_constant = false;
	holder->hash = __string_hash(new_str);

	__result.return_value.value.object_value = str_obj;

__exit: ;
	return __result;
};

function_result Am_Lang_Short_hash_0(nullable_value const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .int_value = this.value.short_value }, .flags = 0 };
__exit: ;
	return __result;
};

function_result Am_Lang_Short_toByte_0(short const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .char_value = this }, .flags = 0 };
__exit: ;
	return __result;
};

function_result Am_Lang_Short_toInt_0(short const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;

	__result.return_value = (nullable_value) { .value = { .int_value = this }, .flags = 0 };

__exit: ;
	return __result;
};

function_result Am_Lang_Short_toLong_0(short const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;

	__result.return_value = (nullable_value) { .value = { .long_value = this }, .flags = 0 };

__exit: ;
	return __result;
};

function_result Am_Lang_Short_toUByte_0(short const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .uchar_value = this }, .flags = 0 };
__exit: ;
	return __result;
};

function_result Am_Lang_Short_toUShort_0(short const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .ushort_value = this }, .flags = 0 };
__exit: ;
	return __result;
};

function_result Am_Lang_Short_toUInt_0(short const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .uint_value = this }, .flags = 0 };
__exit: ;
	return __result;
};

function_result Am_Lang_Short_toULong_0(short const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .ulong_value = this }, .flags = 0 };
__exit: ;
	return __result;
};

function_result Am_Lang_Short_toBool_0(short const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .bool_value = this }, .flags = 0 };
__exit: ;
	return __result;
};


