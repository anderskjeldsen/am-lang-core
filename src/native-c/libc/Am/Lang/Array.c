#include <libc/core.h>
#include <Am/Lang/Array.h>
#include <Am/Lang/Object.h>
#include <Am/Lang/Long.h>
#include <libc/core_inline_functions.h>
#include <string.h>

function_result Am_Lang_Array__native_init_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
	// Add reference count for this in Array._native_init
	// TODO: implement native function Am_Lang_Array__native_init_0
__exit: ;
	return __result;
};

function_result Am_Lang_Array__native_release_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;

	array_holder * ah = (array_holder *) ((char *) this + sizeof(aobject)); // this->object_properties.class_object_properties.object_data.value.custom_value;
	unsigned int const size = ah->size;

	if (ah->ctype == any_type) {
		nullable_value * const items = (nullable_value *) ((char *) ah + sizeof(array_holder));
		for(unsigned int i = 0; i < size; i++) {
			nullable_value const nv = items[i];
			__decrease_property_reference_count_nullable_value(nv);
			items[i] = (nullable_value) { .flags = 0, .value.object_value = NULL };
		}
	} else if ( ah->ctype == object_type) {
		aobject ** const items = (aobject **) (ah + 1);
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

	array_holder * ah = (array_holder *) ((char *) this + sizeof(aobject)); // this->object_properties.class_object_properties.object_data.value.custom_value;
	unsigned int const size = ah->size;

	if (ah->ctype == any_type) {
		nullable_value * const items = (nullable_value *) ((char *) ah + sizeof(array_holder));
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
	array_holder * ah = (array_holder *) ((char *) this + sizeof(aobject)); // this->object_properties.class_object_properties.object_data.value.custom_value;
//	printf("get length %ld\n", ah->size);
	__result.return_value = (nullable_value) { .value = { .uint_value = (unsigned int) ah->size }, .flags = PRIMITIVE_UINT };

__exit: ;
	return __result;
};

// Bulk element copy - the memmove behind Array.copyTo. The AmLang wrapper
// has already validated both ranges; this trusts offsets/length but still
// refuses arrays whose element kinds don't match (can't happen through the
// typed API, but a silent mismatched memmove would corrupt the heap).
//
// Three element kinds, mirroring __native_release_0's dispatch:
//   * primitives  -> one memmove. This is the whole point: byte-heavy code
//     (pack extraction, SHA wrapping, stream buffers) moves data at memcpy
//     speed instead of one bounds-checked, refcounted store per element.
//   * object_type -> aobject* slots hold PROPERTY references. memmove alone
//     would duplicate pointers without their counts, so: retain every source
//     element FIRST, then release every destination occupant, then move the
//     pointer block. That order stays correct even when the ranges overlap
//     in one array - an occupant that is also a source was retained before
//     it is released, so it can't die with its pointer bits still needed.
//   * any_type    -> same protocol on nullable_value slots.
function_result Am_Lang_Array_copyToNative_0(aobject * const this, aobject * dest, unsigned int destOffset, unsigned int srcOffset, unsigned int length)
{
	function_result __result = { .has_return_value = false };
	array_holder * const sah = (array_holder *) ((char *) this + sizeof(aobject));
	array_holder * const dah = (array_holder *) ((char *) dest + sizeof(aobject));
	if (sah->ctype != dah->ctype || sah->item_size != dah->item_size) {
		__throw_simple_exception_copy("copyTo: incompatible element types", "in Am_Lang_Array_copyToNative_0", &__result);
		return __result;
	}
	char * const sdata = (char *) sah->array_data;
	char * const ddata = (char *) dah->array_data;
	if (sah->ctype == object_type) {
		aobject ** const s = ((aobject **) sdata) + srcOffset;
		aobject ** const d = ((aobject **) ddata) + destOffset;
		for (unsigned int i = 0; i < length; i++) {
			if (s[i] != NULL) {
				__increase_property_reference_count(s[i]);
			}
		}
		for (unsigned int i = 0; i < length; i++) {
			if (d[i] != NULL) {
				__decrease_property_reference_count(d[i]);
			}
		}
		memmove(d, s, (size_t) length * sizeof(aobject *));
	} else if (sah->ctype == any_type) {
		nullable_value * const s = ((nullable_value *) sdata) + srcOffset;
		nullable_value * const d = ((nullable_value *) ddata) + destOffset;
		for (unsigned int i = 0; i < length; i++) {
			__increase_property_reference_count_nullable_value(s[i]);
		}
		for (unsigned int i = 0; i < length; i++) {
			__decrease_property_reference_count_nullable_value(d[i]);
		}
		memmove(d, s, (size_t) length * sizeof(nullable_value));
	} else {
		memmove(ddata + (size_t) destOffset * dah->item_size,
		        sdata + (size_t) srcOffset * sah->item_size,
		        (size_t) length * sah->item_size);
	}
	return __result;
};

function_result Am_Lang_Array_createEmptyArrayOfSameType_0(aobject * const this, unsigned int length)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;

	array_holder * ah = (array_holder *) ((char *) this + sizeof(aobject)); // this->object_properties.class_object_properties.object_data.value.custom_value;

	aobject *new_array = __create_array(length, ah->item_size, this->class_ptr, ah->ctype);

	__result.return_value.value.object_value = new_array;
__exit: ;
	return __result;
};
