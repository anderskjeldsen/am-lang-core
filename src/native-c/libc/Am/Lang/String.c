#ifndef native_libc_aclass_Am_Lang_String_c
#define native_libc_aclass_Am_Lang_String_c
#include <core.h>
#include <Am/Lang/String.h>
#include <Am/Lang/Object.h>
#include <Am/Lang/Int.h>

function_result Am_Lang_String__native_init_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
	// Add reference count for this in String._native_init
	if (this != NULL) {
		__increase_reference_count(this);
	}
	// TODO: implement native function Am_Lang_String__native_init_0
__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	return __result;
};

function_result Am_Lang_String__native_release_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
	// TODO: implement native function Am_Lang_String__native_release_0
__exit: ;
	return __result;
};

function_result Am_Lang_String_getLength_0(aobject * const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	// Add reference count for this in String.getLength
	if (this != NULL) {
		__increase_reference_count(this);
	}
	// TODO: implement native function Am_Lang_String_getLength_0
__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	return __result;
};

function_result Am_Lang_String_print_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
	// Add reference count for this in String.print
	if (this != NULL) {
		__increase_reference_count(this);
	}
	// TODO: implement native function Am_Lang_String_print_0
__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	return __result;
};

function_result Am_Lang_String__op__plus_0(aobject * const this, aobject * s)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	// Add reference count for this in String._op__plus
	if (this != NULL) {
		__increase_reference_count(this);
	}
	// TODO: implement native function Am_Lang_String__op__plus_0
__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	if (s != NULL) {
		__increase_reference_count(s);
	}
	return __result;
};

#endif
