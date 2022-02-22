#ifndef native_libc_aclass_Am_Lang_UInt_c
#define native_libc_aclass_Am_Lang_UInt_c
#include <core.h>
#include <Am/Lang/UInt.h>
#include <Am/Lang/Any.h>
#include <Am/Lang/String.h>
#include <Am/Lang/Byte.h>
#include <Am/Lang/Short.h>
#include <Am/Lang/Long.h>
#include <Am/Lang/UByte.h>
#include <Am/Lang/ULong.h>
#include <Am/Lang/Bool.h>

function_result Am_Lang_UInt_toString_0(nullable_value const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
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

function_result Am_Lang_UInt_toUInt_0(unsigned int const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .uint_value = this }, .flags = 0 };
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

#endif
