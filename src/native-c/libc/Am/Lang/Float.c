#include <libc/core.h>
#include <Am/Lang/Float.h>
#include <libc/Am/Lang/Float.h>
#include <Am/Lang/ClassRef.h>
#include <Am/Lang/Any.h>
#include <Am/Lang/String.h>
#include <Am/Lang/UInt.h>
#include <Am/Lang/Byte.h>
#include <Am/Lang/Short.h>
#include <Am/Lang/Int.h>
#include <Am/Lang/Long.h>
#include <Am/Lang/UByte.h>
#include <Am/Lang/UShort.h>
#include <Am/Lang/ULong.h>
#include <Am/Lang/Double.h>
#include <Am/Lang/Bool.h>
#include <Am/Lang/Float.h>
#include <libc/core_inline_functions.h>

function_result Am_Lang_Float_toString_0(nullable_value const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	
	// Convert float to string using sprintf
	char buffer[32];
	snprintf(buffer, sizeof(buffer), "%g", this.value.float_value);
	aobject * str_obj = __create_string(buffer, &Am_Lang_String);
	__result.return_value.value.object_value = str_obj;
	
__exit: ;
	return __result;
}

function_result Am_Lang_Float_hash_0(nullable_value const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	
	// Simple hash by casting float bits to int
	union { float f; int i; } u;
	u.f = this.value.float_value;
	__result.return_value.value.int_value = u.i;
	
__exit: ;
	return __result;
}

function_result Am_Lang_Float_toByte_0(float const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	
	__result.return_value.value.char_value = (char)this;
	
__exit: ;
	return __result;
}

function_result Am_Lang_Float_toShort_0(float const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	
	__result.return_value.value.short_value = (short)this;
	
__exit: ;
	return __result;
}

function_result Am_Lang_Float_toInt_0(float const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	
	__result.return_value.value.int_value = (long)this;
	
__exit: ;
	return __result;
}

function_result Am_Lang_Float_toLong_0(float const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	
	__result.return_value.value.long_value = (long)this;
	
__exit: ;
	return __result;
}

function_result Am_Lang_Float_toUByte_0(float const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	
	__result.return_value.value.uchar_value = (unsigned char)this;
	
__exit: ;
	return __result;
}

function_result Am_Lang_Float_toUShort_0(float const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	
	__result.return_value.value.ushort_value = (unsigned short)this;
	
__exit: ;
	return __result;
}

function_result Am_Lang_Float_toUInt_0(float const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	
	__result.return_value.value.uint_value = (unsigned long)this;
	
__exit: ;
	return __result;
}

function_result Am_Lang_Float_toULong_0(float const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	
	__result.return_value.value.ulong_value = (unsigned long)this;
	
__exit: ;
	return __result;
}

function_result Am_Lang_Float_toDouble_0(float const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	
	__result.return_value.value.double_value = (double)this;
	
__exit: ;
	return __result;
}

function_result Am_Lang_Float_toBool_0(float const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	
	__result.return_value.value.bool_value = (this != 0.0f);
	
__exit: ;
	return __result;
}

function_result Am_Lang_Float_parse_0(aobject * s)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	if (s != NULL) {
		__increase_reference_count(s);
	}
	
	// Parse string to float
	if (s != NULL) {
		string_holder * holder = (string_holder *)s;
		char * str = (char *)holder->string_value;
		char * endptr;
		float value = strtof(str, &endptr);
		
		__result.return_value.value.float_value = value;
	}
	
__exit: ;
	if (s != NULL) {
		__decrease_reference_count(s);
	}
	return __result;
}

