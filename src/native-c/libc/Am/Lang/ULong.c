#ifndef native_libc_aclass_Am_Lang_ULong_c
#define native_libc_aclass_Am_Lang_ULong_c
#include <core.h>
#include <Am/Lang/ULong.h>
#include <Am/Lang/Any.h>
#include <Am/Lang/String.h>
#include <Am/Lang/Byte.h>
#include <Am/Lang/Short.h>
#include <Am/Lang/Int.h>
#include <Am/Lang/Long.h>
#include <Am/Lang/UByte.h>
#include <Am/Lang/UShort.h>
#include <Am/Lang/UInt.h>
#include <Am/Lang/Bool.h>

function_result Am_Lang_ULong_toString_0(nullable_value const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
__exit: ;
	return __result;
};

function_result Am_Lang_ULong_toByte_0(unsigned long long const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .char_value = this }, .flags = 0 };
__exit: ;
	return __result;
};

function_result Am_Lang_ULong_toShort_0(unsigned long long const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .short_value = this }, .flags = 0 };
__exit: ;
	return __result;
};

function_result Am_Lang_ULong_toInt_0(unsigned long long const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .int_value = this }, .flags = 0 };
__exit: ;
	return __result;
};

function_result Am_Lang_ULong_toLong_0(unsigned long long const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .long_value = this }, .flags = 0 };
__exit: ;
	return __result;
};

function_result Am_Lang_ULong_toUByte_0(unsigned long long const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .uchar_value = this }, .flags = 0 };
__exit: ;
	return __result;
};

function_result Am_Lang_ULong_toUShort_0(unsigned long long const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .ushort_value = this }, .flags = 0 };
__exit: ;
	return __result;
};

function_result Am_Lang_ULong_toUInt_0(unsigned long long const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .uint_value = this }, .flags = 0 };
__exit: ;
	return __result;
};

function_result Am_Lang_ULong_toBool_0(unsigned long long const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .bool_value = this }, .flags = 0 };
__exit: ;
	return __result;
};

#endif
