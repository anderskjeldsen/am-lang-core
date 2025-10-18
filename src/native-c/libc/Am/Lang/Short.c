#include <libc/core.h>
#include <Am/Lang/Short.h>
#include <Am/Lang/Any.h>
#include <Am/Lang/String.h>
#include <string.h>
#include <libc/core_inline_functions.h>

function_result Am_Lang_Short_toString_0(nullable_value const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	aobject * str_obj = __allocate_object_with_extra_size(&Am_Lang_String, sizeof(string_holder) + 7);
	string_holder *holder = (string_holder *) (str_obj + 1);
	str_obj->object_properties.class_object_properties.object_data.value.custom_value = holder;
	char * new_str = (char *) (holder + 1);
	int len = sprintf(new_str, "%d", this.value.short_value);

	holder->string_value = new_str; // assume that string constants will never change
	holder->length = len; // TODO: how many characters exactly?
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

function_result Am_Lang_Short_parse_1(nullable_value const s)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	
	__increase_reference_count_nullable_value(s);
	
	string_holder *holder = s.value.object_value->object_properties.class_object_properties.object_data.value.custom_value;
	char *str = holder->string_value;
	char *endptr;
	
	long result = strtol(str, &endptr, 10);
	
	__result.return_value = (nullable_value) { .value = { .short_value = (short)result }, .flags = 0 };

__exit: ;
	__decrease_reference_count_nullable_value(s);
	return __result;
};


