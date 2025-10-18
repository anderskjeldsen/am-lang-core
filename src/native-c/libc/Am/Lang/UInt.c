
#include <libc/core.h>
#include <Am/Lang/UInt.h>
#include <Am/Lang/Any.h>
#include <Am/Lang/String.h>
#include <Am/Lang/Byte.h>
#include <Am/Lang/Short.h>
#include <Am/Lang/Long.h>
#include <Am/Lang/UByte.h>
#include <Am/Lang/ULong.h>
#include <Am/Lang/Bool.h>
#include <string.h>
#include <libc/core_inline_functions.h>

function_result Am_Lang_UInt_toString_0(nullable_value const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	aobject * str_obj = __allocate_object_with_extra_size(&Am_Lang_String, sizeof(string_holder) + 11);
	string_holder *holder = (string_holder *) (str_obj + 1);
	str_obj->object_properties.class_object_properties.object_data.value.custom_value = holder;
	char * new_str = (char *) (holder + 1);
	int len = sprintf(new_str, "%u", this.value.uint_value);

	holder->string_value = new_str; // assume that string constants will never change
	holder->length = len; // TODO: how many characters exactly?
	holder->is_string_constant = false;
	holder->hash = __string_hash(new_str);

	__result.return_value.value.object_value = str_obj;

__exit: ;
	return __result;
};

function_result Am_Lang_UInt_hash_0(nullable_value const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .int_value = this.value.uint_value }, .flags = 0 };
__exit: ;
	return __result;
};

function_result Am_Lang_UInt_toByte_0(unsigned int const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .char_value = this }, .flags = 0 };
__exit: ;
	return __result;
};

function_result Am_Lang_UInt_toShort_0(unsigned int const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .short_value = this }, .flags = 0 };
__exit: ;
	return __result;
};

function_result Am_Lang_UInt_toLong_0(unsigned int const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .long_value = this }, .flags = 0 };
__exit: ;
	return __result;
};

function_result Am_Lang_UInt_toUByte_0(unsigned int const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .uchar_value = this }, .flags = 0 };
__exit: ;
	return __result;
};

function_result Am_Lang_UInt_toUShort_0(unsigned int const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .ushort_value = this }, .flags = 0 };
__exit: ;
	return __result;
};

function_result Am_Lang_UInt_toInt_0(unsigned int const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .int_value = this }, .flags = 0 };
__exit: ;
	return __result;
};

function_result Am_Lang_UInt_toULong_0(unsigned int const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .ulong_value = this }, .flags = 0 };
__exit: ;
	return __result;
};

function_result Am_Lang_UInt_toBool_0(unsigned int const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .bool_value = this }, .flags = 0 };
__exit: ;
	return __result;
};

function_result Am_Lang_UInt_parse_0(aobject * const s)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	
	__increase_reference_count(s);
	
	string_holder *holder = s->object_properties.class_object_properties.object_data.value.custom_value;
	char *str = holder->string_value;
	char *endptr;
	
	unsigned long result = strtoul(str, &endptr, 10);
	
	__result.return_value = (nullable_value) { .value = { .uint_value = (unsigned int)result }, .flags = 0 };

__exit: ;
	__decrease_reference_count(s);
	return __result;
};

