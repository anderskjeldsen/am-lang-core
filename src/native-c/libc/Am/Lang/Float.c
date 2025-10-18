
#include <libc/core.h>
#include <Am/Lang/Float.h>
#include <Am/Lang/Any.h>
#include <Am/Lang/String.h>
#include <string.h>
#include <libc/core_inline_functions.h>

function_result Am_Lang_Float_toString_0(nullable_value const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;

	aobject * str_obj = __allocate_object_with_extra_size(&Am_Lang_String, sizeof(string_holder) + 50);
	string_holder *holder = (string_holder *) (str_obj + 1);
	str_obj->object_properties.class_object_properties.object_data.value.custom_value = holder;
	char * new_str = (char *) (holder + 1);
	int len = sprintf(new_str, "%g", this.value.float_value);

	holder->string_value = new_str;
	holder->length = len;
	holder->is_string_constant = false;
	holder->hash = __string_hash(new_str);

	__result.return_value.value.object_value = str_obj;

__exit: ;
	return __result;
};

function_result Am_Lang_Float_hash_0(nullable_value const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .uint_value = (unsigned int)this.value.float_value }, .flags = 0 };
__exit: ;
	return __result;
};

function_result Am_Lang_Float_toByte_0(float const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;

	__result.return_value = (nullable_value) { .value = { .char_value = (char)this }, .flags = 0 };
__exit: ;
	return __result;
};

function_result Am_Lang_Float_toShort_0(float const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;

	__result.return_value = (nullable_value) { .value = { .short_value = (short)this }, .flags = 0 };
__exit: ;
	return __result;
};

function_result Am_Lang_Float_toInt_0(float const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;

	__result.return_value = (nullable_value) { .value = { .int_value = (int)this }, .flags = 0 };
__exit: ;
	return __result;
};

function_result Am_Lang_Float_toLong_0(float const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;

	__result.return_value = (nullable_value) { .value = { .long_value = (long long)this }, .flags = 0 };
__exit: ;
	return __result;
};

function_result Am_Lang_Float_toDouble_0(float const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .double_value = (double)this }, .flags = 0 };
__exit: ;
	return __result;
};

function_result Am_Lang_Float_toBool_0(float const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .bool_value = (bool)this }, .flags = 0 };
__exit: ;
	return __result;
};

