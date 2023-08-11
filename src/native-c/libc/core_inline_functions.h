#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
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

static inline aobject * __allocate_object(aclass * const __class) {
    return __allocate_object_with_extra_size(__class, 0);
}

static inline void __decrease_reference_count(aobject * const __obj) {
    if ( __obj != NULL) {
        __obj->reference_count--;
        #ifdef DEBUG
        printf("decrease reference count of object of type %s (address: %p, object_id: %d), new reference count %d\n", __obj->class_ptr->name, __obj, __obj->object_properties.class_object_properties.object_id, __obj->reference_count);
        #endif
        if ( __obj->reference_count == 0 ) {
            __deallocate_object(__obj);
        }
    }
}

static inline void __increase_reference_count(aobject * const __obj) {
    __obj->reference_count++;
    #ifdef DEBUG
    printf("increase reference count of object of type %s (address: %p, object_id: %d), new reference count %d\n", __obj->class_ptr->name, __obj, __obj->object_properties.class_object_properties.object_id, __obj->reference_count);
    #endif
}

static inline void __set_property(aobject * const __obj, int const __index, nullable_value __prop_value) {
    property * __prop = &__obj->object_properties.class_object_properties.properties[__index];
    if ( !__is_primitive(__prop->nullable_value) && __prop->nullable_value.value.object_value != NULL ) {
        __decrease_reference_count(__prop->nullable_value.value.object_value);
    }

    if ( !__is_primitive(__prop_value) && __prop_value.value.object_value != NULL ) {
        __increase_reference_count(__prop_value.value.object_value);
    }

    __prop->nullable_value = __prop_value;
}

static inline void __set_static_property(aclass * const __class, int const __index, nullable_value __prop_value) {
    property * __prop = &__class->static_properties[__index];
    if ( !__is_primitive(__prop->nullable_value) && __prop->nullable_value.value.object_value != NULL ) {
        __decrease_reference_count(__prop->nullable_value.value.object_value);
    }

    if ( !__is_primitive(__prop_value) && __prop_value.value.object_value != NULL ) {
        __increase_reference_count(__prop_value.value.object_value);
    }

    __prop->nullable_value = __prop_value;
}

static inline void __decrease_reference_count_nullable_value(nullable_value __value) {
    if ( !__is_primitive(__value) && __value.value.object_value != NULL ) {
        __decrease_reference_count(__value.value.object_value);
    }
}

static inline void __increase_reference_count_nullable_value(nullable_value __value) {
    if ( !__is_primitive(__value) && __value.value.object_value != NULL ) {
        __increase_reference_count(__value.value.object_value);
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


static inline bool __object_equals(aobject * const a, aobject * const b) {
    if (a != NULL) {
        Am_Lang_Object_equals_0_T af = (Am_Lang_Object_equals_0_T) a->class_ptr->functions[Am_Lang_Object_equals_0_index];
        function_result res = af(a, b);
        // Am_Lang_Object_equals_0(a, b);
        return res.return_value.value.bool_value;
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
static inline void attach_weak_reference_node(weak_reference_node * const node, aobject * const object) {
    node->object = object;
    weak_reference_node * const first = object->first_weak_reference_node;
    node->next = first;
    object->first_weak_reference_node = node;
}

static inline void detach_weak_reference_node(weak_reference_node * const node) {
    weak_reference_node * const first = node->object->first_weak_reference_node;
    weak_reference_node * current = first;
    if (current == node) {
        node->object->first_weak_reference_node = node->next;
        node->next = NULL;
    } else {
        while(current != NULL) {
            if (current->next == node) {
                current->next = node->next;
                node->next = NULL;
                current = NULL;
            } else {
                current = current->next;
            }
        }
    }

}
