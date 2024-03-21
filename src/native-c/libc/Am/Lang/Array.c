#include <libc/core.h>
#include <Am/Lang/Array.h>
#include <Am/Lang/Object.h>
#include <Am/Lang/Long.h>
#include <libc/core_inline_functions.h>

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

	array_holder * ah = (array_holder *) &this[1]; // this->object_properties.class_object_properties.object_data.value.custom_value;
	unsigned int const size = ah->size;

	if (ah->ctype == any_type) {
		nullable_value * const items = (nullable_value *) &ah[1];
		for(unsigned int i = 0; i < size; i++) {
			nullable_value const nv = items[i];
			__decrease_property_reference_count_nullable_value(nv);
			items[i] = (nullable_value) { .flags = 0, .value.object_value = NULL };
		}
	} else if ( ah->ctype == object_type) {
		aobject ** const items = (aobject **) &ah[1];
		for(unsigned int i = 0; i < size; i++) {
			aobject * const obj = items[i];
			if (obj != NULL) {
				__decrease_property_reference_count(obj);
				items[i] = NULL;
			}
		}
	}

//	free(ah->array_data);
//	ah->array_data = NULL;
//	free(ah);
//	this->object_properties.class_object_properties.object_data.value.custom_value = NULL;

__exit: ;
	return __result;
};

function_result Am_Lang_Array__native_mark_children_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;

	array_holder * ah = (array_holder *) &this[1]; // this->object_properties.class_object_properties.object_data.value.custom_value;
	unsigned int const size = ah->size;

	if (ah->ctype == any_type) {
		nullable_value * const items = (nullable_value *) &ah[1];
		for(unsigned int i = 0; i < size; i++) {
			nullable_value const nv = items[i];
			__mark_nullable_value(nv);			
		}
	} else if ( ah->ctype == object_type) {
		aobject ** const items = (aobject **) &ah[1];
		for(unsigned int i = 0; i < size; i++) {
			aobject * const obj = items[i];
			if (obj != NULL) {
				__mark_object(obj);
			}
		}
	}

__exit: ;
	return __result;
};

function_result Am_Lang_Array_length_0(aobject * const this)
{
//	printf("get length\n");
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	// Add reference count for this in Array.length
	if (this != NULL) {
		__increase_reference_count(this);
	}
	array_holder * ah = (array_holder *) &this[1]; // this->object_properties.class_object_properties.object_data.value.custom_value;
//	printf("get length %ld\n", ah->size);
	__result.return_value = (nullable_value) { .value = { .uint_value = (unsigned int) ah->size }, .flags = PRIMITIVE_UINT };

__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	return __result;
};

function_result Am_Lang_Array_createEmptyArrayOfSameType_internal(aobject * const this, unsigned int length)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	if (this != NULL) {
		__increase_reference_count(this);
	}

	array_holder * ah = (array_holder *) &this[1]; // this->object_properties.class_object_properties.object_data.value.custom_value;

	aobject *new_array = __create_array(length, ah->item_size, this->class_ptr, ah->ctype);

	__result.return_value.value.object_value = new_array;
__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	return __result;
};

function_result Am_Lang_Array_createEmptyArrayOfSameType_0_uchar(aobject * const this, long long length)
{
	return Am_Lang_Array_createEmptyArrayOfSameType_internal(this, length);
};

function_result Am_Lang_Array_createEmptyArrayOfSameType_0_char(aobject * const this, long long length)
{
	return Am_Lang_Array_createEmptyArrayOfSameType_internal(this, length);
};

function_result Am_Lang_Array_createEmptyArrayOfSameType_0_object(aobject * const this, long long length)
{
	return Am_Lang_Array_createEmptyArrayOfSameType_internal(this, length);
};
