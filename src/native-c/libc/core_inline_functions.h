#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
// Generated class header: carries the __DIRECT_NOTHROW_ABI marker macros
// that select vtable-dispatch consumption ABI below. Must be included
// here (not relied on from the including TU) — startup.c includes this
// file before any class header.
#include <Am/Lang/Object.h>
#include <libc/core.h>

#include <Am/Lang/Exception.h>
#include <Am/Lang/Object.h>
#include <Am/Lang/String.h>

/*
extern function_result Am_Lang_Exception_addStackTraceItem_0(aobject * const this, aobject * var_stackTraceItem);
extern function_result Am_Lang_Object_equals_0(aobject * const this, aobject * var_other);
typedef function_result (*Am_Lang_Object_equals_0_T)(aobject * const this, aobject *);
extern Am_Lang_Object_equals_0_index;
*/
static inline void __set_primitive_nullable(nullable_value * nullable_value, bool is_primitive_nullable) {
    unsigned char f = nullable_value->flags;
    f &= ~PRIMITIVE_NULLABLE;
    f |= is_primitive_nullable ? PRIMITIVE_NULLABLE : 0;
    nullable_value->flags = f;
}

// Predicate: does `v` match the corruption fingerprint we keep hitting
// at __first_object? Non-null pointer with low 16 bits zero and high
// 16 bits non-zero — real malloc'd aobjects don't land on 64K-aligned
// addresses by chance.
static inline int __is_suspicious_object_ptr(aobject *v) {
    if (v == NULL) return 0;
    long lv = (long) v;
    if (lv > 0) {
        return 0;
    }
/*
    unsigned long uv = (unsigned long) v;
    if ((uv & 0xFFFFUL) != 0) return 0;
    if ((uv >> 16) == 0) return 0;
    */
    return 1;
}

// Instrumented setter for the calling isolate's object-list head. Quiet on
// normal writes — only logs when the new value matches the corruption
// fingerprint we're hunting. The site label says which writer (increase /
// decrease / detach) saw the bad value.
static inline void __set_first_object(aobject *v, const char *site) {
    if (__is_suspicious_object_ptr(v)) {
        printf("[firstobj.set] SUSPICIOUS site=%s value=%p\n", site, (void*)v);
        fflush(stdout);
        exit(0);
    }
    __amlc_isolate()->first_object = v;
}

static inline void __set_primitive_null(nullable_value * nullable_value, bool is_primitive_null) {
    unsigned char f = nullable_value->flags;
    f &= ~PRIMITIVE_NULL;
    f |= is_primitive_null ? PRIMITIVE_NULL : 0;
    nullable_value->flags = f; 
}

static inline bool __is_primitive_null(const nullable_value nullable_value) {
    return nullable_value.flags & PRIMITIVE_NULL;
}

static inline bool __is_primitive_nullable(const nullable_value nullable_value) {
    return nullable_value.flags & PRIMITIVE_NULLABLE;
}

static inline bool __is_primitive(const nullable_value nullable_value) {
    return nullable_value.flags != 0;
}

static inline bool __any_has_flags(const nullable_value *nv, unsigned short flags) {
    return (nv->flags & flags) == flags;
}

// `new` of a class whose every property slot the generated code stores
// itself (see the compiler's inline defaults): the fast lane hands out an
// arena block with only the HEADER initialised — no memset, which at a
// runtime-variable size is a library call and was the largest single cost
// of an allocation-heavy loop. Outside the fast lane it is a plain calloc.
static inline aobject * __allocate_object_prepared(aclass * const __class) {
#if !defined(DEBUG) && !defined(TRACKOBJECTS) && !defined(AMLC_WATCH)
    aisolate * const __iso = __amlc_isolate();
    if (__iso->fast_depth > 0) {
        aarena * const __a = __iso->arena;
        size_t const __size = sizeof(aobject) + sizeof(property) * __class->properties_count;
        size_t const __slot = AMLC_ARENA_ROUND(sizeof(aarena *)) + AMLC_ARENA_ROUND(__size);
        if (__a != NULL && __a->used + __slot <= __a->capacity) {
            char * const __base = __a->data + __a->used;
            __a->used += __slot;
            __a->live++;
            aobject * const __obj = (aobject *) (__base + AMLC_ARENA_ROUND(sizeof(aarena *)));
            ((aarena **) __obj)[-1] = __a;
            __iso->allocation_count++;
            __obj->class_ptr = __class;
            __obj->reference_count = 1;
            __obj->property_reference_count = 0;
            __obj->object_properties.class_object_properties.object_data = (nullable_value) { .flags = 0, .value = (value) { .object_value = NULL } };
            __obj->object_properties.class_object_properties.properties =
                (__class->statics->type == class && __class->properties_count > 0) ? (property *) ((char *) __obj + sizeof(aobject)) : NULL;
            __obj->marked = false;
            __obj->pending_deallocation = false;
            __obj->immortal = false;
            __obj->chunked = true;
            __obj->prev = NULL;
            __obj->next = NULL;
            return __obj;
        }
    }
#endif
    return __allocate_object_with_extra_size(__class, 0);
}

static inline aobject * __allocate_object(aclass * const __class) {
#if !defined(DEBUG) && !defined(TRACKOBJECTS) && !defined(AMLC_WATCH)
    // Fast lane, inline: bump the current arena without a call. Falls back
    // to the full allocator when there is no room (which also starts the
    // next arena) or outside a `fast { }` block.
    aisolate * const __iso = __amlc_isolate();
    if (__iso->fast_depth > 0) {
        aarena * const __a = __iso->arena;
        size_t const __size = sizeof(aobject) + sizeof(property) * __class->properties_count;
        size_t const __slot = AMLC_ARENA_ROUND(sizeof(aarena *)) + AMLC_ARENA_ROUND(__size);
        if (__a != NULL && __a->used + __slot <= __a->capacity) {
            char * const __base = __a->data + __a->used;
            __a->used += __slot;
            __a->live++;
            aobject * const __obj = (aobject *) (__base + AMLC_ARENA_ROUND(sizeof(aarena *)));
            ((aarena **) __obj)[-1] = __a;
            memset(__obj, 0, __size);
            __iso->allocation_count++;
            if (__class->statics->type == class && __class->properties_count > 0) {
                __obj->object_properties.class_object_properties.properties = (property *) ((char *) __obj + sizeof(aobject));
            }
            __obj->class_ptr = __class;
            __obj->reference_count = 1;
            __obj->chunked = true;
            return __obj;
        }
    }
#endif
    return __allocate_object_with_extra_size(__class, 0);
}

// `__ALWAYS_INLINE` — gcc-specific attribute that forces inlining even at
// -O0. Without this, am-lang programs built with the default `gcc` (no
// `-O` flag) end up with `static inline` helpers compiled as real
// function calls, which adds a full stack frame per property read /
// unwrap. That blows the JS interpreter's recursion guard well before
// the configured maxCallDepth fires. The attribute is supported by gcc
// 3.1+ and clang; portable enough for every am-lang target.
#if defined(__GNUC__) || defined(__clang__)
#define __AMLC_ALWAYS_INLINE static inline __attribute__((always_inline))
#else
#define __AMLC_ALWAYS_INLINE static inline
#endif

// Read a property's stored nullable_value. A macro rather than
// `static inline` because at gcc -O0 (the default for am-lang builds), even
// `always_inline` wraps the return in a stack-allocated `nullable_value`
// temporary per call site. In huge functions with thousands of property
// reads (e.g. `JsBytecodeVm.run` — 83k lines, many reads per opcode) those
// 12-byte temps accumulate into multi-MB frames and overflow the C stack on
// the JS interpreter's recursive eval. A macro is pure text substitution.
#define __get_property_nv(__obj, __index) \
    ((__obj)->object_properties.class_object_properties.properties[__index].nullable_value)

// Pointer form — needed when codegen wants the slot's address rather
// than its value. Same macro rationale as above.
#define __get_property_nv_ptr(__obj, __index) \
    (&(__obj)->object_properties.class_object_properties.properties[__index].nullable_value)

static inline void __decrease_reference_count(aobject * const __obj) {
    if ( __obj != NULL) {
        #ifdef DEBUG
        __print_memory_header(__obj, "Decrease reference count");
        #endif

        __obj->reference_count--;
        #if defined(DEBUG) && defined(ARCLOG)
        #ifdef CONDLOG
        if (__conditional_logging_on) {
        #endif
        printf("decrease reference count of object of type %s (address: %p, object_id: %d), property reference count %d, new reference count %d\n", __obj->class_ptr->name, __obj, __obj->object_properties.class_object_properties.object_id, __obj->property_reference_count, __obj->reference_count);
        #ifdef CONDLOG
        }
        #endif
        #endif

        // An object dies when nothing holds it: no local/return references
        // and no property holder. If a property still holds it, the eventual
        // property release (or the exit-time cycle sweep) finishes it.
        if (__obj->reference_count == 0 && __obj->property_reference_count == 0) {
#if !defined(DEBUG) && !defined(TRACKOBJECTS) && !defined(AMLC_WATCH)
            // Inline death for the trivial case: a plain class with no
            // release hook and no object slots that lives in a fast-lane
            // arena needs no detach and no free — just the arena count.
            aclass * const __cls = __obj->class_ptr;
            if (__obj->chunked && !__obj->pending_deallocation
                    && __cls->release == NULL && __cls->object_property_count == 0 && __cls->statics->type == class) {
                aisolate * const __iso = __amlc_isolate();
                __iso->allocation_count--;
                aarena * const __a = ((aarena **) __obj)[-1];
                if (--__a->live == 0) {
                    __amlc_arena_recycle(__iso, __a);
                }
                return;
            }
#endif
            __deallocate_object(__obj);
        }
    }
}

static inline void __increase_reference_count(aobject * const __obj) {
    #ifdef DEBUG
    __print_memory_header(__obj, "Increase reference count");
    #endif
    __obj->reference_count++;
    #if defined(DEBUG) && defined(ARCLOG)
    #ifdef CONDLOG
    if (__conditional_logging_on) {
    #endif
    printf("increase reference count of object of type %s (address: %p, object_id: %d), propert_reference_count: %d, new reference count: %d\n", __obj->class_ptr->name, __obj, __obj->object_properties.class_object_properties.object_id, __obj->property_reference_count, __obj->reference_count);
    #ifdef CONDLOG
    }
    #endif
    #endif
}

// Take an OWNED handle on a value read out of a slot (property, static,
// array element). Plain retain; the caller releases with
// __decrease_reference_count.
static inline aobject * __retain_slot_read(aobject * const __raw) {
    if (__raw == NULL) {
        return NULL;
    }
    __increase_reference_count(__raw);
    return __raw;
}

static inline void __retain_function_return(nullable_value * const __rv) {
    if (__is_primitive(*__rv)) {
        return;
    }
    if (__rv->value.object_value == NULL) {
        return;
    }
    __rv->value.object_value = __retain_slot_read(__rv->value.object_value);
}

/*
static inline void __decrease_property_reference_count(aobject * const __obj) {
    if ( __obj != NULL) {
        __obj->property_reference_count--;
        #ifdef DEBUG
        #ifdef CONDLOG 
        if (__conditional_logging_on) {
        #endif
        printf("decrease property reference count of object of type %s (address: %p, object_id: %d), new reference count %d\n", __obj->class_ptr->name, __obj, __obj->object_properties.class_object_properties.object_id, __obj->reference_count);
        #ifdef CONDLOG 
        }
        #endif        
        #endif

        if (__conditional_logging_on) {
            printf("decrease property reference count of object of type %s (address: %p, object_id: %d), new reference count %d\n", __obj->class_ptr->name, __obj, __obj->object_properties.class_object_properties.object_id, __obj->reference_count);
        }

        bool fl = false;
        if (strcmp(__obj->class_ptr->name, "String") == 0) {
            string_holder *sh = (string_holder *) __obj->object_properties.class_object_properties.object_data.value.custom_value;
            printf("String value: %s\n", sh->string_value);
            if (strcmp(sh->string_value, "Am.Lang.Short") == 0) {
                fl = true;
            }
        }

        if (__obj->property_reference_count == 0) {
            if (fl) {
                printf("check first object\n");
                sleep(1);
            }
            if (__obj == __first_object) {
                if (fl) {
                    printf("is first object\n");
                    sleep(1);
                }

                __first_object = __obj->next;                
                __obj->next = NULL;
                if (__first_object != NULL) {
                    __first_object->prev = NULL;
                }
            } else {        
                if (fl) {
                    printf("is not first object\n");
                    sleep(1);
                }

                if (fl) {
                    printf("has prev %p\n", __obj->prev);
                    sleep(1);
                }

                __obj->prev->next = __obj->next;
                if (fl) {
                    printf("1\n");
                }

                if (__obj->next != NULL) {
                    if (fl) {
                        printf("2\n");
                    }
                    __obj->next->prev = __obj->prev;
                }
                if (fl) {
                    printf("3\n");
                }

                __obj->prev = NULL;
                __obj->next = NULL;
            }


            if (__obj->reference_count == 0) {
                if (__conditional_logging_on || fl) {
                    printf("deallocate in 1s\n");
                    sleep(1);
                    printf("let's wait 5 more\n");
                    sleep(5);
                }

                __deallocate_object(__obj);
                if (__conditional_logging_on || fl) {
                    printf("deallocated\n");
                }
            }
        }
    }
}
*/
static inline void __increase_property_reference_count(aobject * const __obj) {
    // Bottleneck tripwire — every path that eventually mutates
    // first_object via increase comes through here, including the
    // direct calls amlc emits in generated C (Array.c / File.c /
    // etc.) that skip the higher-level __set_property wrappers.
    // Dumps as much context as we can safely read off the bad ptr,
    // then exits so the user has a clean log to share.
    if (__is_suspicious_object_ptr(__obj)) {
        printf("[increase_propref] SUSPICIOUS obj=%p", (void*)__obj); fflush(stdout);
        // Defensively probe class_ptr without trusting it.
        unsigned long uc = (unsigned long)__obj->class_ptr;
        printf(" class_ptr=%p", (void*)uc); fflush(stdout);
        if (__obj->class_ptr != NULL) {
            const char *nm = __obj->class_ptr->name;
            printf(" name_ptr=%p", (void*)nm); fflush(stdout);
            if (nm != NULL) {
                printf(" name=%s", nm); fflush(stdout);
            }
        }
        printf(" propref=%d ref=%d\n",
            __obj->property_reference_count, __obj->reference_count);
        fflush(stdout);
        exit(0);
    }
    // Immortal (process-global) objects stay off the per-isolate lists —
    // see aobject.immortal.
    if (__obj->immortal) {
        return;
    }
    // The property-referenced list is per isolate, and only the owning
    // isolate ever links into it, so no lock.
    if (__obj->property_reference_count == 0) {
        aisolate * const __iso = __amlc_isolate();
        __obj->next = __iso->first_object;
        if (__iso->first_object != NULL) {
            __iso->first_object->prev = __obj;
        }
        __set_first_object(__obj, "increase_property_reference_count");
    }
    __obj->property_reference_count++;
    #if defined(DEBUG) && defined(ARCLOG)
    #ifdef CONDLOG
    if (__conditional_logging_on) {
    #endif
    printf("increase property reference count of object of type %s (address: %p, object_id: %d), new reference count %d\n", __obj->class_ptr->name, __obj, __obj->object_properties.class_object_properties.object_id, __obj->reference_count);
    #ifdef CONDLOG
    }
    #endif
    #endif
}

static inline void __set_property(aobject * const __obj, int const __index, nullable_value __prop_value) {
    // Slot watch: extra resolution between allocator calls. Object-typed
    // property writes come through here, so a stray write is attributed to
    // a much narrower window than allocate/deallocate alone can give.
    __amlc_watch_probe("set_property",
        (__obj != NULL && __obj->class_ptr != NULL) ? __obj->class_ptr->name : NULL,
        __builtin_return_address(0));
    // Single-threaded by construction: an object is only ever written by the
    // isolate that owns it, so the read-modify-write needs no lock.
    property * __prop = &__obj->object_properties.class_object_properties.properties[__index];
    if ( !__is_primitive(__prop->nullable_value) && __prop->nullable_value.value.object_value != NULL ) {
        __decrease_property_reference_count(__prop->nullable_value.value.object_value);
    }
    if ( !__is_primitive(__prop_value) && __prop_value.value.object_value != NULL ) {
        // Tripwire: when the value being assigned to a property is a
        // suspicious object pointer (the 0xff1d0000 / 0xffd10000
        // fingerprint, generalised as "low 16 bits zero"), log the
        // parent class + property index so we can map back to the
        // exact field. Property name itself isn't easy to look up
        // safely from here — grep the build output for
        // `create_property_info(<index>, "<name>", ..., &<parent>)`
        // to translate.
        if (__is_suspicious_object_ptr(__prop_value.value.object_value)) {
            const char *parent = "(unknown)";
            if (__obj != NULL && __obj->class_ptr != NULL && __obj->class_ptr->name != NULL) {
                parent = __obj->class_ptr->name;
            }
            printf("[set_property] SUSPICIOUS parent=%s index=%d value=%p\n",
                parent, __index, (void*)__prop_value.value.object_value);
            fflush(stdout);
            exit(0);
        }
        __increase_property_reference_count(__prop_value.value.object_value);
    }

    __prop->nullable_value = __prop_value;
}

static inline bool __set_property_safe(aobject * const __obj, int const __index, nullable_value __prop_value) {
    property * __prop = &__obj->object_properties.class_object_properties.properties[__index];
    ctype old_type = __value_flags_to_ctype(__prop->nullable_value.flags);
    ctype new_type = __value_flags_to_ctype(__prop_value.flags);
    if (old_type != new_type) {
        return false;
    }

    if (new_type == object_type) {
        if (!is_descendant_of(__prop_value.value.object_value->class_ptr, __prop->nullable_value.value.object_value->class_ptr)) {
            return false;
        }
    } else if (__is_primitive_nullable(__prop_value) && !__is_primitive_nullable(__prop->nullable_value)) {
        // If new value is a nullable primitive, check of the property supports that
        return false;
    }

    if ( !__is_primitive(__prop->nullable_value) && __prop->nullable_value.value.object_value != NULL ) {
        __decrease_property_reference_count(__prop->nullable_value.value.object_value);
    }
    if ( !__is_primitive(__prop_value) && __prop_value.value.object_value != NULL ) {
        if (__is_suspicious_object_ptr(__prop_value.value.object_value)) {
            const char *parent = "(unknown)";
            if (__obj != NULL && __obj->class_ptr != NULL && __obj->class_ptr->name != NULL) {
                parent = __obj->class_ptr->name;
            }
            printf("[set_property_safe] SUSPICIOUS parent=%s index=%d value=%p\n",
                parent, __index, (void*)__prop_value.value.object_value);
            fflush(stdout);
            exit(0);
        }
        __increase_property_reference_count(__prop_value.value.object_value);
    }

    __prop->nullable_value = __prop_value;
    return true;
}

static inline void __set_static_property(class_static * const __class_static, int const __index, nullable_value __prop_value) {
    // Statics are per isolate (see __static_slot), so this is a plain
    // single-threaded slot write.
    property * __prop = &__static_slot(__class_static, __index);
    if ( !__is_primitive(__prop->nullable_value) && __prop->nullable_value.value.object_value != NULL ) {
        __decrease_property_reference_count(__prop->nullable_value.value.object_value);
    }

    if ( !__is_primitive(__prop_value) && __prop_value.value.object_value != NULL ) {
        if (__is_suspicious_object_ptr(__prop_value.value.object_value)) {
            const char *parent = "(unknown)";
            if (__class_static != NULL && __class_static->name != NULL) {
                parent = __class_static->name;
            }
            printf("[set_static_property] SUSPICIOUS parent=%s index=%d value=%p\n",
                parent, __index, (void*)__prop_value.value.object_value);
            fflush(stdout);
            exit(0);
        }
        __increase_property_reference_count(__prop_value.value.object_value);
    }
    __prop->nullable_value = __prop_value;
}

static inline void __decrease_reference_count_nullable_value(nullable_value __value) {
    if ( !__is_primitive(__value) && __value.value.object_value != NULL ) {
        __decrease_reference_count(__value.value.object_value);
    }
}

static inline void __decrease_property_reference_count_nullable_value(nullable_value __value) {
    if ( !__is_primitive(__value) && __value.value.object_value != NULL ) {
        __decrease_property_reference_count(__value.value.object_value);
    }
}

static inline void __mark_nullable_value(nullable_value __value) {
    if ( !__is_primitive(__value) && __value.value.object_value != NULL ) {
        __mark_object(__value.value.object_value);
    }
}

static inline void __increase_reference_count_nullable_value(nullable_value __value) {
    if ( !__is_primitive(__value) && __value.value.object_value != NULL ) {
        __increase_reference_count(__value.value.object_value);
    }
}

static inline void __increase_property_reference_count_nullable_value(nullable_value __value) {
    if ( !__is_primitive(__value) && __value.value.object_value != NULL ) {
        if (__is_suspicious_object_ptr(__value.value.object_value)) {
            printf("[increase_propref_nv] SUSPICIOUS value=%p\n", (void*)__value.value.object_value);
            fflush(stdout);
            exit(0);
        }
        __increase_property_reference_count(__value.value.object_value);
    }
}

/*
inline void __throw_exception(function_result *result, aobject * const exception, aobject * const stack_trace_item_text) {

//    Am_Lang_Exception_addStackTraceItem_0(exception, stack_trace_item_text);

    if (result->exception) {
        __decrease_reference_count(result->exception);
    }
    result->exception = exception;
    __increase_reference_count(exception);
}

inline void __pass_exception(function_result *result, aobject * const exception, aobject * const stack_trace_item_text) {

//    Am_Lang_Exception_addStackTraceItem_0(exception, stack_trace_item_text);
    if (result->exception) {
        __decrease_reference_count(result->exception);
    }
    result->exception = exception;
}
*/
static inline void __deallocate_function_result(function_result const result) {
    if (result.exception != NULL) {
        __decrease_reference_count(result.exception);
    }
}


static inline bool __object_equals(aobject * const a_in, aobject * const b_in) {
    // Thread-safe ARC: codegen emits `__object_equals(x, y)` for the
    // `x == y` / `x != y` comparison without going through Stage 6's
    // unwrap-at-native-dispatch (this runtime helper isn't a `native`
    // fn). If either side is a foreign-thread wrapper, reading
    // `class_ptr->statics->type` walks into NULL because wrappers have
    // `class_ptr == NULL`. Unwrap at the entry so the rest of the
    // helper operates on reals and the comparison semantics stay
    // pointer-identity-after-unwrap (two wrappers for the same real
    // compare equal).
    aobject * const a = __unwrap(a_in);
    aobject * const b = __unwrap(b_in);
    if (a != NULL) {
        if (a->class_ptr->statics->type == interface) {
            return __object_equals(a->object_properties.iface_reference.implementation_object, b);
        }
        if (b != NULL && b->class_ptr->statics->type == interface) {
            return __object_equals(a, b->object_properties.iface_reference.implementation_object);
        }
        __object_equals_alias af = (__object_equals_alias) a->class_ptr->functions[__object_equals_index];
        // ABI-follows-throws: under -fthrows-opt the equals chain is
        // contract-nothrow, so the slot (and its generated typedef, which
        // __object_equals_alias is #defined to) returns plain bool. The
        // generated Am/Lang/Object.h defines the marker macro.
#ifdef Am_Lang_Object_f_equals_0__DIRECT_NOTHROW_ABI
        return af(a, b);
#else
        function_result res = af(a, b);
        // Am_Lang_Object_equals_0(a, b);
        return res.return_value.value.bool_value;
#endif
    }
    return a == b;
}
/*
inline aobject * __create_exception(aobject * const message) {
    aobject *ex = __allocate_object(&Am_Lang_Exception);
    Am_Lang_Exception_Exception_0(ex, message);
    Am_Lang_Exception___init_instance((nullable_value){ .value.object_value = ex });
    return ex;
}

inline void __throw_simple_exception(const char * const message, const char * const stack_trace_item_text, function_result * const result) {
    aobject * ex_msg = __create_string_constant(message, &Am_Lang_String);
    aobject * stit = __create_string_constant(stack_trace_item_text, &Am_Lang_String);
    aobject * ex = __create_exception(ex_msg);
    __throw_exception(result, ex, stit);
    __decrease_reference_count(ex_msg); // it's in the exception stack trace list now, we don't need it anymore.
    __decrease_reference_count(stit); // it's in the exception stack trace list now, we don't need it anymore.
    __decrease_reference_count(ex); // it's in the exception stack trace list now, we don't need it anymore.
}
*/

static inline ctype __value_flags_to_ctype(unsigned char flags) {
    unsigned char stripped_flags = flags & 0b11111100;
    switch(stripped_flags) {
        case PRIMITIVE_BOOL:
            return bool_type;
        case PRIMITIVE_LONG:
            return long_type;
        case PRIMITIVE_INT: 
            return int_type;
        case PRIMITIVE_SHORT:  
            return short_type;
        case PRIMITIVE_CHAR:
            return char_type;
        case PRIMITIVE_ULONG:
            return ulong_type;
        case PRIMITIVE_UINT:
            return uint_type;
        case PRIMITIVE_USHORT:
            return ushort_type;
        case PRIMITIVE_UCHAR:
            return uchar_type;
        #ifdef FEATURE_FLOATING_POINT
        case PRIMITIVE_FLOAT:
            return float_type;
        case PRIMITIVE_DOUBLE:
            return double_type;
        #endif
        default:
            return object_type;
    }
}
