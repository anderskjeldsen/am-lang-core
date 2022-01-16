#ifndef native_libc_aclass_Am_Lang_Array_c
#define native_libc_aclass_Am_Lang_Array_c
#include <core.h>
#include <Am/Lang/Array.h>
#include <Am/Lang/Object.h>
#include <Am/Lang/Long.h>

function_result Am_Lang_Array__native_init_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
	// Add reference count for this in Array._native_init
	if (this != NULL) {
		__increase_reference_count(this);
	}
	// TODO: implement native function Am_Lang_Array__native_init_0
__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	return __result;
};

function_result Am_Lang_Array__native_release_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;

	array_holder * ah = (array_holder *) this->object_data.value.custom_value;
	int const size = ah->size;

	if (ah->ctype == any_type) {
		nullable_value * const items = (nullable_value *) ah->array_data;
		for(size_t i = 0; i < size; i++) {
			nullable_value const nv = items[i];
			__decrease_reference_count_nullable_value(nv);			
		}
	} else if ( ah->ctype == object_type) {
		aobject ** const items = (aobject **) ah->array_data;
		for(size_t i = 0; i < size; i++) {
			aobject * const obj = items[i];
			__decrease_reference_count(obj);
		}
	}

	free(ah->array_data);
	ah->array_data = NULL;
	free(ah);
	this->object_data.value.custom_value = NULL;

__exit: ;
	return __result;
};

function_result Am_Lang_Array_length_0(aobject * const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	// Add reference count for this in Array.length
	if (this != NULL) {
		__increase_reference_count(this);
	}
	// TODO: implement native function Am_Lang_Array_length_0
__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	return __result;
};

#endif
