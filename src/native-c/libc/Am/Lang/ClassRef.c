#include <libc/core.h>
#include <Am/Lang/ClassRef.h>
#include <libc/Am/Lang/ClassRef.h>
#include <Am/Lang/Object.h>
#include <Am/Lang/Any.h>
#include <Am/Lang/String.h>
#include <Am/Lang/Array.h>
#include <Am/Lang/PropertyInfo.h>
#include <Am/Lang/Int.h>
#include <Am/Lang/Long.h>
#include <Am/Lang/Short.h>
#include <Am/Lang/Byte.h>
#include <Am/Lang/UInt.h>
#include <Am/Lang/ULong.h>
#include <Am/Lang/UShort.h>
#include <Am/Lang/UByte.h>
#include <Am/Lang/Bool.h>
#ifdef FEATURE_FLOATING_POINT
#include <Am/Lang/Float.h>
#include <Am/Lang/Double.h>
#endif
#include <libc/core_inline_functions.h>
#include <string.h>

// The aclass a ClassRef describes lives in its object_data slot.
static aclass * cr_class(aobject * const ref)
{
	return (aclass *) ref->object_properties.class_object_properties.object_data.value.custom_value;
}

// ---- primitive classes <-> value kinds ------------------------------------

ctype __amlc_class_ctype(aclass * const cls)
{
	if (cls == &Am_Lang_Int) return int_type;
	if (cls == &Am_Lang_Long) return long_type;
	if (cls == &Am_Lang_Short) return short_type;
	if (cls == &Am_Lang_Byte) return char_type;
	if (cls == &Am_Lang_UInt) return uint_type;
	if (cls == &Am_Lang_ULong) return ulong_type;
	if (cls == &Am_Lang_UShort) return ushort_type;
	if (cls == &Am_Lang_UByte) return uchar_type;
	if (cls == &Am_Lang_Bool) return bool_type;
#ifdef FEATURE_FLOATING_POINT
	if (cls == &Am_Lang_Float) return float_type;
	if (cls == &Am_Lang_Double) return double_type;
#endif
	return object_type;
}

unsigned char __amlc_ctype_flags(ctype const t)
{
	switch (t) {
		case int_type: return PRIMITIVE_INT;
		case long_type: return PRIMITIVE_LONG;
		case short_type: return PRIMITIVE_SHORT;
		case char_type: return PRIMITIVE_CHAR;
		case uint_type: return PRIMITIVE_UINT;
		case ulong_type: return PRIMITIVE_ULONG;
		case ushort_type: return PRIMITIVE_USHORT;
		case uchar_type: return PRIMITIVE_UCHAR;
		case bool_type: return PRIMITIVE_BOOL;
#ifdef FEATURE_FLOATING_POINT
		case float_type: return PRIMITIVE_FLOAT;
		case double_type: return PRIMITIVE_DOUBLE;
#endif
		default: return 0;
	}
}

static unsigned int ctype_size(ctype const t)
{
	switch (t) {
		case int_type: return sizeof(int);
		case long_type: return sizeof(long long);
		case short_type: return sizeof(short);
		case char_type: return sizeof(char);
		case uint_type: return sizeof(unsigned int);
		case ulong_type: return sizeof(unsigned long long);
		case ushort_type: return sizeof(unsigned short);
		case uchar_type: return sizeof(unsigned char);
		case bool_type: return sizeof(bool);
#ifdef FEATURE_FLOATING_POINT
		case float_type: return sizeof(float);
		case double_type: return sizeof(double);
#endif
		case any_type: return sizeof(nullable_value);
		default: return sizeof(aobject *);
	}
}

// A primitive value converted to another primitive kind (JSON hands a
// serializer Longs and Doubles; the slot may be an Int). Objects, nulls
// and same-kind values pass through untouched.
nullable_value __amlc_coerce_primitive(nullable_value v, ctype const target)
{
	if (v.flags == 0 || (v.flags & PRIMITIVE_NULL) || target == object_type || target == any_type) return v;
	ctype const source = __value_flags_to_ctype(v.flags);
	if (source == target) return v;
	long long l = 0;
	double d = 0;
	bool floating = false;
	switch (source) {
		case int_type: l = v.value.int_value; break;
		case long_type: l = v.value.long_value; break;
		case short_type: l = v.value.short_value; break;
		case char_type: l = v.value.char_value; break;
		case uint_type: l = (long long) v.value.uint_value; break;
		case ulong_type: l = (long long) v.value.ulong_value; break;
		case ushort_type: l = v.value.ushort_value; break;
		case uchar_type: l = v.value.uchar_value; break;
		case bool_type: l = v.value.bool_value ? 1 : 0; break;
#ifdef FEATURE_FLOATING_POINT
		case float_type: d = v.value.float_value; floating = true; break;
		case double_type: d = v.value.double_value; floating = true; break;
#endif
		default: return v;
	}
	if (floating) l = (long long) d; else d = (double) l;
	nullable_value out = { .flags = __amlc_ctype_flags(target) };
	switch (target) {
		case int_type: out.value.int_value = (int) l; break;
		case long_type: out.value.long_value = l; break;
		case short_type: out.value.short_value = (short) l; break;
		case char_type: out.value.char_value = (char) l; break;
		case uint_type: out.value.uint_value = (unsigned int) l; break;
		case ulong_type: out.value.ulong_value = (unsigned long long) l; break;
		case ushort_type: out.value.ushort_value = (unsigned short) l; break;
		case uchar_type: out.value.uchar_value = (unsigned char) l; break;
		case bool_type: out.value.bool_value = floating ? (d != 0) : (l != 0); break;
#ifdef FEATURE_FLOATING_POINT
		case float_type: out.value.float_value = (float) d; break;
		case double_type: out.value.double_value = d; break;
#endif
		default: return v;
	}
	return out;
}

// ---- ClassRef natives -------------------------------------------------------

function_result Am_Lang_ClassRef__native_init_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	return __result;
}

function_result Am_Lang_ClassRef__native_release_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	return __result;
}

function_result Am_Lang_ClassRef__native_mark_children_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	return __result;
}

function_result Am_Lang_ClassRef_getClassRefFromAny_0(nullable_value any)
{
	function_result __result = { .has_return_value = true };
	__increase_reference_count_nullable_value(any);
	aclass * cls = NULL;
	if (any.flags == 0) {
		if (any.value.object_value == NULL) {
			__throw_simple_exception("getClassRef() on null", "getClassRefFromAny", &__result);
			goto __exit;
		}
		cls = any.value.object_value->class_ptr;
	} else {
		switch (__value_flags_to_ctype(any.flags)) {
			case long_type: cls = &Am_Lang_Long; break;
			case int_type: cls = &Am_Lang_Int; break;
			case short_type: cls = &Am_Lang_Short; break;
			case char_type: cls = &Am_Lang_Byte; break;
			case ulong_type: cls = &Am_Lang_ULong; break;
			case uint_type: cls = &Am_Lang_UInt; break;
			case ushort_type: cls = &Am_Lang_UShort; break;
			case uchar_type: cls = &Am_Lang_UByte; break;
#ifdef FEATURE_FLOATING_POINT
			case float_type: cls = &Am_Lang_Float; break;
			case double_type: cls = &Am_Lang_Double; break;
#endif
			case bool_type: cls = &Am_Lang_Bool; break;
			default:
				__throw_simple_exception("Invalid primitive type", "getClassRefFromAny", &__result);
				goto __exit;
		}
	}
	{
		aobject * ref = __amlc_class_ref(cls);
		if (ref == NULL) {
			__throw_out_of_memory_exception(&__result, "getClassRefFromAny");
			goto __exit;
		}
		__increase_reference_count(ref);
		__result.return_value.value.object_value = ref;
	}
__exit: ;
	__decrease_reference_count_nullable_value(any);
	return __result;
}

function_result Am_Lang_ClassRef_getClassRef_0(aobject * name)
{
	function_result __result = { .has_return_value = true };
	string_holder * holder = (string_holder *) ((char *) name + sizeof(aobject));
	aclass * cls = __amlc_find_reflected_class(holder->string_value);
	aobject * ref = cls != NULL ? __amlc_class_ref(cls) : NULL;
	if (ref != NULL) __increase_reference_count(ref);
	__result.return_value.value.object_value = ref;
	return __result;
}

function_result Am_Lang_ClassRef_ensureProperties_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	aclass * const cls = cr_class(this);
	if (cls == NULL || cls->reflect == NULL) {
		char msg[256];
		snprintf(msg, sizeof(msg), "%s is not #reflect: no property information", cls != NULL ? cls->name : "?");
		__throw_simple_exception_copy(msg, "in Am_Lang_ClassRef_ensureProperties_0", &__result);
		return __result;
	}
	if (this->object_properties.class_object_properties.properties[Am_Lang_ClassRef_P_properties].nullable_value.value.object_value != NULL) {
		return __result;
	}
	__amlc_global_lock();
	if (this->object_properties.class_object_properties.properties[Am_Lang_ClassRef_P_properties].nullable_value.value.object_value == NULL) {
		const amlc_reflect_desc * const desc = cls->reflect;
		aisolate * const iso = __amlc_isolate();
		int const saved_depth = iso->fast_depth;
		iso->fast_depth = 0;
		aobject * arr = __create_array(desc->count, (unsigned char) sizeof(aobject *), &Am_Lang_Array_ta_Am_Lang_PropertyInfo, object_type);
		iso->fast_depth = saved_depth;
		if (arr != NULL) {
			__amlc_make_immortal(arr);
			array_holder * const ah = (array_holder *) ((char *) arr + sizeof(aobject));
			aobject ** const items = (aobject **) ah->array_data;
			for (unsigned int i = 0; i < desc->count; i++) {
				const amlc_prop_desc * const pd = &desc->props[i];
				aobject * pi = __amlc_allocate_immortal(&Am_Lang_PropertyInfo, 0);
				if (pi == NULL) break;
				pi->object_properties.class_object_properties.object_data.value.custom_value = (void *) pd;
				aobject * pname = __create_string_constant((char *) pd->name, &Am_Lang_String);
				if (pname != NULL) {
					__amlc_make_immortal(pname);
					__set_property(pi, Am_Lang_PropertyInfo_P_name, (nullable_value) { .flags = 0, .value.object_value = pname });
				}
				__set_property(pi, Am_Lang_PropertyInfo_P_index, (nullable_value) { .flags = PRIMITIVE_UINT, .value.uint_value = pd->slot });
				__set_property(pi, Am_Lang_PropertyInfo_P_nullable, (nullable_value) { .flags = PRIMITIVE_BOOL, .value.bool_value = (pd->flags & AMLC_PROP_NULLABLE) != 0 });
				items[i] = pi;   // immortal: no count taken
			}
			__set_property(this, Am_Lang_ClassRef_P_properties, (nullable_value) { .flags = 0, .value.object_value = arr });
		}
	}
	__amlc_global_unlock();
	return __result;
}

function_result Am_Lang_ClassRef_getTypeArguments_0(aobject * const this)
{
	function_result __result = { .has_return_value = true };
	aclass * const cls = cr_class(this);
	unsigned int n = 0;
	aclass * const * targs = (cls != NULL && cls->reflect != NULL) ? cls->reflect->type_args : NULL;
	if (targs != NULL) while (targs[n] != NULL) n++;
	aobject * arr = __create_array(n, (unsigned char) sizeof(aobject *), &Am_Lang_Array_ta_Am_Lang_ClassRef, object_type);
	if (arr == NULL) {
		__throw_out_of_memory_exception(&__result, "in Am_Lang_ClassRef_getTypeArguments_0");
		return __result;
	}
	array_holder * const ah = (array_holder *) ((char *) arr + sizeof(aobject));
	aobject ** const items = (aobject **) ah->array_data;
	for (unsigned int i = 0; i < n; i++) {
		items[i] = __amlc_class_ref(targs[i]);
		if (items[i] != NULL) __increase_property_reference_count(items[i]);
	}
	__result.return_value.value.object_value = arr;
	return __result;
}

function_result Am_Lang_ClassRef_newInstance_0(aobject * const this)
{
	function_result __result = { .has_return_value = true };
	aclass * const cls = cr_class(this);
	if (cls == NULL || cls->reflect == NULL) {
		char msg[256];
		snprintf(msg, sizeof(msg), "%s is not #reflect: cannot instantiate", cls != NULL ? cls->name : "?");
		__throw_simple_exception_copy(msg, "in Am_Lang_ClassRef_newInstance_0", &__result);
		return __result;
	}
	aobject * obj = __allocate_object(cls);
	if (obj == NULL) {
		__throw_out_of_memory_exception(&__result, "in Am_Lang_ClassRef_newInstance_0");
		return __result;
	}
	if (cls->reflect->init_instance != NULL) {
		function_result r = ((function_result (*)(nullable_value)) cls->reflect->init_instance)((nullable_value) { .flags = 0, .value.object_value = obj });
		if (r.exception != NULL) {
			__decrease_reference_count(obj);
			__result.exception = r.exception;
			return __result;
		}
	}
	__result.return_value.value.object_value = obj;
	return __result;
}

function_result Am_Lang_ClassRef_newArray_0(aobject * arrayType, int length)
{
	function_result __result = { .has_return_value = true };
	aclass * const cls = cr_class(arrayType);
	aclass * elem = NULL;
	if (cls != NULL && cls->reflect != NULL && cls->reflect->type_args != NULL) elem = cls->reflect->type_args[0];
	if (elem == NULL || length < 0) {
		__throw_simple_exception("newArray: not a reflected array type", "in Am_Lang_ClassRef_newArray_0", &__result);
		return __result;
	}
	ctype const t = __amlc_class_ctype(elem);
	aobject * arr = __create_array((unsigned int) length, (unsigned char) ctype_size(t), cls, t);
	if (arr == NULL) {
		__throw_out_of_memory_exception(&__result, "in Am_Lang_ClassRef_newArray_0");
		return __result;
	}
	__result.return_value.value.object_value = arr;
	return __result;
}

// Calls a reflected instance method: the name and argument count pick it out
// of the class's (or a base class's) method table, and the generated thunk
// converts the Any arguments to the method's real parameter types.
function_result Am_Lang_ClassRef_invoke_0(aobject * target, aobject * name, aobject * args)
{
	function_result __result = { .has_return_value = true };
	if (target == NULL || name == NULL) {
		__throw_simple_exception("invoke: target is null", "in Am_Lang_ClassRef_invoke_0", &__result);
		return __result;
	}
	string_holder * const nh = (string_holder *) ((char *) name + sizeof(aobject));
	nullable_value argv[AMLC_MAX_REFLECT_ARGS];
	unsigned int argc = 0;
	if (args != NULL) {
		array_holder * const ah = (array_holder *) ((char *) args + sizeof(aobject));
		argc = ah->size;
		if (argc > AMLC_MAX_REFLECT_ARGS) {
			__throw_simple_exception("invoke: too many arguments", "in Am_Lang_ClassRef_invoke_0", &__result);
			return __result;
		}
		char * const data = (char *) ah->array_data;
		for (unsigned int i = 0; i < argc; i++) {
			if (ah->ctype == any_type) {
				argv[i] = ((nullable_value *) data)[i];
			} else if (ah->ctype == object_type) {
				argv[i] = (nullable_value) { .flags = 0, .value.object_value = ((aobject **) data)[i] };
			} else {
				__throw_simple_exception("invoke: arguments must be an Any[] or object array", "in Am_Lang_ClassRef_invoke_0", &__result);
				return __result;
			}
		}
	}
	for (aclass * cls = target->class_ptr; cls != NULL; cls = cls->base) {
		if (cls->reflect == NULL) continue;
		for (unsigned int i = 0; i < cls->reflect->method_count; i++) {
			const amlc_method_desc * const m = &cls->reflect->methods[i];
			if (m->arg_count != argc || strcmp(m->name, nh->string_value) != 0) continue;
			return ((function_result (*)(aobject *, nullable_value *)) m->thunk)(target, argv);
		}
	}
	{
		char msg[256];
		snprintf(msg, sizeof(msg), "invoke: %s has no reflected method %s taking %u argument(s)", target->class_ptr->name, nh->string_value, argc);
		__throw_simple_exception_copy(msg, "in Am_Lang_ClassRef_invoke_0", &__result);
	}
	return __result;
}

function_result Am_Lang_ClassRef_arrayLength_0(aobject * array)
{
	function_result __result = { .has_return_value = true };
	array_holder * const ah = (array_holder *) ((char *) array + sizeof(aobject));
	__result.return_value = (nullable_value) { .flags = PRIMITIVE_INT, .value.int_value = (int) ah->size };
	return __result;
}

function_result Am_Lang_ClassRef_arrayGet_0(aobject * array, int index)
{
	function_result __result = { .has_return_value = true };
	array_holder * const ah = (array_holder *) ((char *) array + sizeof(aobject));
	if (index < 0 || (unsigned int) index >= ah->size) {
		__throw_simple_exception("arrayGet: index out of bounds", "in Am_Lang_ClassRef_arrayGet_0", &__result);
		return __result;
	}
	char * const data = (char *) ah->array_data;
	nullable_value v = { .flags = __amlc_ctype_flags(ah->ctype) };
	switch (ah->ctype) {
		case object_type: v.value.object_value = ((aobject **) data)[index]; break;
		case any_type: v = ((nullable_value *) data)[index]; break;
		case int_type: v.value.int_value = ((int *) data)[index]; break;
		case long_type: v.value.long_value = ((long long *) data)[index]; break;
		case short_type: v.value.short_value = ((short *) data)[index]; break;
		case char_type: v.value.char_value = ((char *) data)[index]; break;
		case uint_type: v.value.uint_value = ((unsigned int *) data)[index]; break;
		case ulong_type: v.value.ulong_value = ((unsigned long long *) data)[index]; break;
		case ushort_type: v.value.ushort_value = ((unsigned short *) data)[index]; break;
		case uchar_type: v.value.uchar_value = ((unsigned char *) data)[index]; break;
		case bool_type: v.value.bool_value = ((bool *) data)[index]; break;
#ifdef FEATURE_FLOATING_POINT
		case float_type: v.value.float_value = ((float *) data)[index]; break;
		case double_type: v.value.double_value = ((double *) data)[index]; break;
#endif
		default: break;
	}
	__increase_reference_count_nullable_value(v);
	__result.return_value = v;
	return __result;
}

function_result Am_Lang_ClassRef_arraySet_0(aobject * array, int index, nullable_value value)
{
	function_result __result = { .has_return_value = false };
	__increase_reference_count_nullable_value(value);
	array_holder * const ah = (array_holder *) ((char *) array + sizeof(aobject));
	if (index < 0 || (unsigned int) index >= ah->size) {
		__throw_simple_exception("arraySet: index out of bounds", "in Am_Lang_ClassRef_arraySet_0", &__result);
		goto __exit;
	}
	{
		char * const data = (char *) ah->array_data;
		if (ah->ctype == object_type) {
			aobject ** const slot = ((aobject **) data) + index;
			aobject * const incoming = value.flags == 0 ? value.value.object_value : NULL;
			if (incoming != NULL) __increase_property_reference_count(incoming);
			if (*slot != NULL) __decrease_property_reference_count(*slot);
			*slot = incoming;
		} else if (ah->ctype == any_type) {
			nullable_value * const slot = ((nullable_value *) data) + index;
			__increase_property_reference_count_nullable_value(value);
			__decrease_property_reference_count_nullable_value(*slot);
			*slot = value;
		} else {
			nullable_value v = __amlc_coerce_primitive(value, ah->ctype);
			if (v.flags == 0 || (v.flags & PRIMITIVE_NULL)) {
				__throw_simple_exception("arraySet: a primitive element cannot be null", "in Am_Lang_ClassRef_arraySet_0", &__result);
				goto __exit;
			}
			switch (ah->ctype) {
				case int_type: ((int *) data)[index] = v.value.int_value; break;
				case long_type: ((long long *) data)[index] = v.value.long_value; break;
				case short_type: ((short *) data)[index] = v.value.short_value; break;
				case char_type: ((char *) data)[index] = v.value.char_value; break;
				case uint_type: ((unsigned int *) data)[index] = v.value.uint_value; break;
				case ulong_type: ((unsigned long long *) data)[index] = v.value.ulong_value; break;
				case ushort_type: ((unsigned short *) data)[index] = v.value.ushort_value; break;
				case uchar_type: ((unsigned char *) data)[index] = v.value.uchar_value; break;
				case bool_type: ((bool *) data)[index] = v.value.bool_value; break;
#ifdef FEATURE_FLOATING_POINT
				case float_type: ((float *) data)[index] = v.value.float_value; break;
				case double_type: ((double *) data)[index] = v.value.double_value; break;
#endif
				default: break;
			}
		}
	}
__exit: ;
	__decrease_reference_count_nullable_value(value);
	return __result;
}
