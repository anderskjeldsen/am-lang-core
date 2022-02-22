#ifndef native_libc_aclass_Am_Lang_UByte_c
#define native_libc_aclass_Am_Lang_UByte_c
#include <core.h>
#include <Am/Lang/UByte.h>
#include <Am/Lang/Any.h>
#include <Am/Lang/String.h>

function_result Am_Lang_UByte_toString_0(nullable_value const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	// TODO: implement native function Am_Lang_UByte_toString_0
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


#endif
