#include <libc/core.h>
#include <string.h>
#include <Am/Lang/ObjectHelper.h>
#include <libc/Am/Lang/ObjectHelper.h>
#include <Am/Lang/Object.h>
#include <Am/Lang/Bool.h>
#include <libc/core_inline_functions.h>

function_result Am_Lang_ObjectHelper__native_init_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
__exit: ;
	return __result;
};

function_result Am_Lang_ObjectHelper__native_release_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
__exit: ;
	return __result;
};

function_result Am_Lang_ObjectHelper__native_mark_children_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
__exit: ;
	return __result;
};

function_result Am_Lang_ObjectHelper_equals_0(aobject * a, aobject * b)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value.bool_value = (a == b), .flags = PRIMITIVE_BOOL };
	return __result;
};

function_result Am_Lang_ObjectHelper_hash_0(aobject * o)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	// Add reference count for o in ObjectHelper.hash
	if (o != NULL) {
		__increase_reference_count(o);
	}
	__result.return_value = (nullable_value) { .value = { .uint_value = (unsigned int) (size_t) o }, .flags = 0 };
__exit: ;
	if (o != NULL) {
		__decrease_reference_count(o);
	}
	return __result;
};

// Raw dump of one property slot. "As-is" means: the flags word and the
// value union's bytes exactly as they sit in memory, printed BEFORE any
// interpretation, so a slot that reads wrong (a zero that should not be
// zero, a torn pointer) is visible even when the flags themselves lie.
// The decoded line after it is a convenience, not the evidence.
//
// Byte-wise hex rather than %llx: libnix's printf has no dependable
// long-long conversion on m68k, and this helper has to work on the
// platform where the ARC/property bugs actually show up.
static void __print_property_nv(unsigned int index, const nullable_value nv)
{
	const unsigned char * bytes = (const unsigned char *) &nv.value;
	unsigned int b = 0;
	printf("  [%2u] flags=0x%04x raw=", index, (unsigned int) nv.flags);
	while (b < sizeof(value)) {
		printf("%02x", (unsigned int) bytes[b]);
		b = b + 1;
	}
	if (nv.flags == 0) {
		aobject * ov = nv.value.object_value;
		if (ov == NULL) {
			printf(" object=NULL\n");
			return;
		}
		printf(" object=%p", (void *) ov);
		if (ov->class_ptr == NULL) {
			// A cross-thread wrapper: class_ptr is NULL and the real sits
			// in the dedicated `wrapped_object` field (deliberately NOT a
			// view of the object_properties union -- see core.h).
			aobject * real = ov->wrapped_object;
			printf(" (wrapper -> %p", (void *) real);
			if (real != NULL && real->class_ptr != NULL && real->class_ptr->name != NULL) {
				printf(" %s", real->class_ptr->name);
			}
			printf(")");
			ov = real;
		} else if (ov->class_ptr->name != NULL) {
			printf(" %s", ov->class_ptr->name);
		}
		if (ov != NULL && ov->class_ptr != NULL) {
			printf(" rc=%d prc=%d", ov->reference_count, ov->property_reference_count);
			if (ov->class_ptr->name != NULL && strcmp(ov->class_ptr->name, "Am.Lang.String") == 0) {
				string_holder * holder = (string_holder *)
					ov->object_properties.class_object_properties.object_data.value.custom_value;
				if (holder != NULL && holder->string_value != NULL) {
					printf(" \"%s\"", holder->string_value);
				}
			}
		}
		printf("\n");
		return;
	}
	if ((nv.flags & PRIMITIVE_NULL) != 0) {
		printf(" (null primitive)\n");
		return;
	}
	if ((nv.flags & 64) != 0) {
		printf(" bool=%s\n", nv.value.bool_value ? "true" : "false");
		return;
	}
#ifdef FEATURE_FLOATING_POINT
	if ((nv.flags & PRIMITIVE_FLOATING_POINT_NUMBER) != 0) {
		if ((nv.flags & PRIMITIVE_BYTE_SIZE_EXP_1) != 0) {
			printf(" double=%f\n", nv.value.double_value);
		} else {
			printf(" float=%f\n", (double) nv.value.float_value);
		}
		return;
	}
#endif
	{
		int is_unsigned = (nv.flags & PRIMITIVE_UNSIGNED) != 0;
		int e1 = (nv.flags & PRIMITIVE_BYTE_SIZE_EXP_1) != 0;
		int e2 = (nv.flags & PRIMITIVE_BYTE_SIZE_EXP_2) != 0;
		if (e1 && e2) {
			unsigned int hi = (unsigned int) ((unsigned long long) nv.value.ulong_value >> 32);
			unsigned int lo = (unsigned int) ((unsigned long long) nv.value.ulong_value & 0xffffffffU);
			printf(" %s hi=%u lo=%u\n", is_unsigned ? "ulong" : "long", hi, lo);
		} else if (e2) {
			if (is_unsigned) { printf(" uint=%u\n", nv.value.uint_value); }
			else { printf(" int=%d\n", nv.value.int_value); }
		} else if (e1) {
			if (is_unsigned) { printf(" ushort=%u\n", (unsigned int) nv.value.ushort_value); }
			else { printf(" short=%d\n", (int) nv.value.short_value); }
		} else {
			if (is_unsigned) { printf(" ubyte=%u\n", (unsigned int) nv.value.uchar_value); }
			else { printf(" byte=%d\n", (int) nv.value.char_value); }
		}
	}
}

function_result Am_Lang_ObjectHelper_printDebug_0(aobject * o)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
	if (o != NULL) {
		__increase_reference_count(o);
	}
	{
		aobject * subject = __unwrap(o);
		printf("==========================\n");
		printf("Object info\n");
		printf("Ptr: %p\n", (void *) o);
		if (subject != o) {
			printf("Unwrapped: %p\n", (void *) subject);
		}
		if (subject == NULL || subject->class_ptr == NULL) {
			printf("Class: (none)\n");
			printf("==========================\n");
			goto __exit;
		}
		printf("Class: %s\n", subject->class_ptr->name);
		printf("Ref count: %d\n", subject->reference_count);
		printf("Prop ref count: %d\n", subject->property_reference_count);
		if (subject->class_ptr->statics != NULL
			&& subject->class_ptr->statics->type == interface) {
			printf("Interface -> implementation %p\n",
				(void *) subject->object_properties.iface_reference.implementation_object);
			printf("==========================\n");
			goto __exit;
		}
		{
			unsigned int count = subject->class_ptr->properties_count;
			property * props = subject->object_properties.class_object_properties.properties;
			printf("Properties: %u\n", count);
			if (props == NULL) {
				printf("  (property array is NULL)\n");
			} else {
				unsigned int i = 0;
				while (i < count) {
					__print_property_nv(i, props[i].nullable_value);
					i = i + 1;
				}
			}
		}
		printf("==========================\n");
	}

__exit: ;
	if (o != NULL) {
		__decrease_reference_count(o);
	}
	return __result;
};

function_result Am_Lang_ObjectHelper_watchProperty_0(aobject * o, int index)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
	if (o != NULL) {
		__increase_reference_count(o);
	}
	{
		aobject * subject = __unwrap(o);
		if (subject == NULL || subject->class_ptr == NULL) {
			printf("[watch] refusing to arm: no class\n");
			goto __exit;
		}
		if (index < 0 || (unsigned int) index >= subject->class_ptr->properties_count) {
			printf("[watch] refusing to arm: index %d out of range for %s (%u properties)\n",
				index, subject->class_ptr->name, subject->class_ptr->properties_count);
			goto __exit;
		}
		printf("[watch] %s[%d] on object %p\n", subject->class_ptr->name, index, (void *) subject);
		__amlc_watch_arm((void *)
			&subject->object_properties.class_object_properties.properties[index].nullable_value);
	}
__exit: ;
	if (o != NULL) {
		__decrease_reference_count(o);
	}
	return __result;
};
