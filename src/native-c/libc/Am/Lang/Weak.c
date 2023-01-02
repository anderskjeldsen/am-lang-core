#ifndef native_libc_aclass_Am_Lang_Weak_c
#define native_libc_aclass_Am_Lang_Weak_c
#include <libc/core.h>
#include <Am/Lang/Weak.h>
#include <libc/Am/Lang/Weak.h>
#include <Am/Lang/Object.h>

weak_reference_node * get_weak_reference_node(aobject * const this) {
	return (weak_reference_node *) this->object_properties.class_object_properties.object_data.value.custom_value;
}

function_result Am_Lang_Weak__native_init_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
	// Add reference count for this in Weak._native_init
	if (this != NULL) {
		__increase_reference_count(this);
	}
	weak_reference_node * const node = calloc(1, sizeof(weak_reference_node));
	this->object_properties.class_object_properties.object_data.value.custom_value = node;
__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	return __result;
};

function_result Am_Lang_Weak__native_release_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;

	weak_reference_node * node = get_weak_reference_node(this);
	if (node->object != NULL) {
		detach_weak_reference_node(node);
	}
	free(node);
	this->object_properties.class_object_properties.object_data.value.custom_value = NULL;

__exit: ;
	return __result;
};

function_result Am_Lang_Weak_set_0(aobject * const this, aobject * t)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
	// Add reference count for this in Weak.set
	if (this != NULL) {
		__increase_reference_count(this);
	}
	// Add reference count for t in Weak.set
	if (t != NULL) {
		__increase_reference_count(t);
	}
	weak_reference_node * node = get_weak_reference_node(this);
	if (node->object != NULL) {
		detach_weak_reference_node(node);
	}

	if (t != NULL) {
		attach_weak_reference_node(node, t);
	}

__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	if (t != NULL) {
		__increase_reference_count(t);
	}
	return __result;
};

function_result Am_Lang_Weak_get_0(aobject * const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	// Add reference count for this in Weak.get
	if (this != NULL) {
		__increase_reference_count(this);
	}
	// TODO: implement native function Am_Lang_Weak_get_0
	weak_reference_node * node = get_weak_reference_node(this);
	if (node->object) {
		__increase_reference_count(node->object);
	}
	__result.return_value.value.object_value = node->object;
__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	return __result;
};

#endif
