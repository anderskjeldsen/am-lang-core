#include <libc/core.h>
#include <Am/Lang/PropertyInfo.h>
#include <libc/Am/Lang/PropertyInfo.h>
#include <Am/Lang/Object.h>
#include <Am/Lang/Any.h>
#include <Am/Lang/String.h>
#include <Am/Lang/ClassRef.h>
#include <libc/core_inline_functions.h>

// The descriptor entry a PropertyInfo describes lives in its object_data slot
// (set by ClassRef.ensureProperties).
static const amlc_prop_desc * pi_desc(aobject * const this)
{
	return (const amlc_prop_desc *) this->object_properties.class_object_properties.object_data.value.custom_value;
}

function_result Am_Lang_PropertyInfo__native_init_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	return __result;
};

function_result Am_Lang_PropertyInfo__native_release_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	return __result;
};

function_result Am_Lang_PropertyInfo__native_mark_children_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	return __result;
}

function_result Am_Lang_PropertyInfo_getPropertyClassRef_0(aobject * const this)
{
	function_result __result = { .has_return_value = true };
	const amlc_prop_desc * const pd = pi_desc(this);
	aobject * ref = (pd != NULL && pd->type != NULL) ? __amlc_class_ref(pd->type) : NULL;
	if (ref == NULL) {
		__throw_simple_exception("PropertyInfo has no type", "in Am_Lang_PropertyInfo_getPropertyClassRef_0", &__result);
		return __result;
	}
	__increase_reference_count(ref);
	__result.return_value = (nullable_value) { .flags = 0, .value.object_value = ref };
	return __result;
}

function_result Am_Lang_PropertyInfo_getValue_0(aobject * const this, aobject * target)
{
	function_result __result = { .has_return_value = true };
	const amlc_prop_desc * const pd = pi_desc(this);
	if (pd == NULL || target == NULL) {
		__throw_simple_exception("getValue: no target", "in Am_Lang_PropertyInfo_getValue_0", &__result);
		return __result;
	}
	nullable_value v = target->object_properties.class_object_properties.properties[pd->slot].nullable_value;
	__increase_reference_count_nullable_value(v);
	__result.return_value = v;
	return __result;
};

function_result Am_Lang_PropertyInfo_setValue_0(aobject * const this, aobject * target, nullable_value value)
{
	function_result __result = { .has_return_value = false };
	__increase_reference_count_nullable_value(value);
	const amlc_prop_desc * const pd = pi_desc(this);
	if (pd == NULL || target == NULL) {
		__throw_simple_exception("setValue: no target", "in Am_Lang_PropertyInfo_setValue_0", &__result);
		goto __exit;
	}
	{
		nullable_value v = value;
		ctype const slot_kind = pd->type != NULL ? __amlc_class_ctype(pd->type) : object_type;
		if (slot_kind != object_type) {
			// Primitive slot: convert the incoming primitive; null is only
			// legal when the property was declared nullable.
			if (v.flags == 0 && v.value.object_value == NULL) {
				if ((pd->flags & AMLC_PROP_NULLABLE) == 0) {
					__throw_simple_exception("setValue: a primitive property cannot be null", "in Am_Lang_PropertyInfo_setValue_0", &__result);
					goto __exit;
				}
				v = (nullable_value) { .flags = __amlc_ctype_flags(slot_kind) | PRIMITIVE_NULLABLE | PRIMITIVE_NULL };
			} else {
				v = __amlc_coerce_primitive(v, slot_kind);
				if (pd->flags & AMLC_PROP_NULLABLE) v.flags |= PRIMITIVE_NULLABLE;
			}
		}
		else if (v.flags == 0 && v.value.object_value != NULL && pd->type != NULL
				&& pd->type->statics != NULL && pd->type->statics->type != interface
				&& !is_descendant_of(v.value.object_value->class_ptr, pd->type)) {
			// Object slot: the value must be of the declared class (or a
			// subclass). Interface-typed slots are not checked here.
			char msg[256];
			snprintf(msg, sizeof(msg), "setValue: %s is not a %s", v.value.object_value->class_ptr->name, pd->type->name);
			__throw_simple_exception_copy(msg, "in Am_Lang_PropertyInfo_setValue_0", &__result);
			goto __exit;
		}
		if ((int) pd->slot >= (int) target->class_ptr->properties_count) {
			__throw_simple_exception("setValue: target is not an instance of the property's class", "in Am_Lang_PropertyInfo_setValue_0", &__result);
			goto __exit;
		}
		__set_property(target, (int) pd->slot, v);
	}
__exit: ;
	__decrease_reference_count_nullable_value(value);
	return __result;
};
