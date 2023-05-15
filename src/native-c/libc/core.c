// #include <stdlib.h>
#include <libc/core.h>
#include <string.h>
#include <Am/Lang/Exception.h>
#include <Am/Lang/Object.h>
#include <Am/Lang/Annotations/UseMemoryPool.h>
#include <libc/memory_pools.h>

int __allocation_count = 0;
#define MAX_ALLOCATIONS 1024 * 50

#ifdef DEBUG
int __last_object_id = 0;
aobject * allocations[MAX_ALLOCATIONS];
int allocation_index = 0;
#endif

void __decrease_reference_count(aobject * const __obj) {
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

void __increase_reference_count(aobject * const __obj) {
    __obj->reference_count++;
    #ifdef DEBUG
    printf("increase reference count of object of type %s (address: %p, object_id: %d), new reference count %d\n", __obj->class_ptr->name, __obj, __obj->object_properties.class_object_properties.object_id, __obj->reference_count);
    #endif
}

aobject * __allocate_iface_object(aclass * const __class, aobject * const implementation_object) {
    aobject * iface_object = __allocate_object(__class);
//    iface_reference * ref = (iface_reference *) malloc(sizeof(iface_reference)); // TODO: Uhm, what is this actually used for?

    __increase_reference_count(implementation_object); // decreased when this iface object is deallocted

    iface_implementation * impl = NULL;
    for(int i = 0; i < implementation_object->class_ptr->iface_implementation_count; i++) {
        iface_implementation * impl2 = &implementation_object->class_ptr->iface_implementations[i];
        if (impl2->iface_class == __class) {
            impl = impl2;
            break;
        }
    }

    iface_reference ref_t = { .implementation_object = implementation_object, .iface_implementation = impl };
    memcpy(&iface_object->object_properties.iface_reference, &ref_t, sizeof(iface_reference));

//    iface_object->object_properties.iface_reference = ref_t;
    return iface_object;
}

unsigned int __string_hash(const char * const str) {
    unsigned int hash = 0;
    unsigned int bit = 0;
    const char *str2 = str;
    bool x = 0; //strlen(str2) < 2;
    while(*str2 != 0) {
        unsigned int c = (unsigned int) *str2++;
        hash += (c << bit);
        bit += 5;
        bit &= 0x1f;
//        str2++;
    }
    return hash;
}

aobject * __allocate_object(aclass * const __class) {
    return __allocate_object_with_extra_size(__class, 0);
}

aobject * __allocate_object_with_extra_size(aclass * const __class, size_t extra_size) {
    __allocation_count++;
    #ifdef DEBUG
    printf("Allocate object of type %s (count: %d, object_id: %d) \n", __class->name, __allocation_count, ++__last_object_id);
    #endif

    size_t size_with_properties = sizeof(aobject) + (sizeof(property) * __class->properties_count) + extra_size;

    aobject * __obj = NULL;

    if (__class->memory_pool == NULL) {
        for(int i = 0; i < __class->annotations_count; i++) {
            if (__class->annotations[i]->class_ptr == &Am_Lang_Annotations_UseMemoryPool) {
                __class->memory_pool = create_memory_pool(size_with_properties);
            }
        }
    }

    if (__class->memory_pool != NULL) {
        __obj = alloc_from_pool(__class->memory_pool);
        __obj->memory_pooled = false;
    } else if (size_with_properties <= small_object_memory_pool->unit_size) {
        if (small_object_memory_pool->first_bank == NULL) {
            small_object_memory_pool->first_bank = create_pool_bank(small_object_memory_pool, 256); // calloc(1, sizeof(pool_bank));
        }
        __obj = alloc_from_pool(small_object_memory_pool);
        __obj->memory_pooled = true;
    } else {
        __obj = (aobject *) calloc(1, size_with_properties);
        __obj->memory_pooled = false;
//        __obj = (aobject *) malloc(size_with_properties + extra_size);
    }

    if (__class->type == class && __class->properties_count > 0) {
        __obj->object_properties.class_object_properties.properties = (property *) (__obj + 1);;
    }

    __obj->class_ptr = __class;
    __obj->reference_count = 1;

    #ifdef DEBUG
    allocations[allocation_index++] = __obj;
    __obj->object_properties.class_object_properties.object_id = __last_object_id;
    #endif

    // memset(__obj, 0, sizeof(aobject));
    // __obj->class_ptr = __class;

    // if (__class->type == class) {
    //     __obj->properties = malloc(sizeof(property) * __class->properties_count);
    //     memset(__obj->properties, 0, sizeof(property) * __class->properties_count);
    // }

    // __obj->reference_count = 1;

    return __obj;
}

/*
void * __allocate_object_data(aobject * const __obj, int __size) {
    if ( __obj->object_data.custom_value != NULL ) {
        free(__obj->object_data.custom_value);
        __obj->object_data.custom_value = NULL;
    }
    void * const __data = malloc(__size);
    memset(__data, 1, sizeof(aobject *));

    __obj->object_data.custom_value = __data;
//    __obj->object_data_size = __size;
    return __data;
}
*/

void __deallocate_object(aobject * const __obj) {
    __allocation_count--;
    #ifdef DEBUG
    printf("Deallocate object of type %s (total object allocation count: %d)\n", __obj->class_ptr->name, __allocation_count);
    #endif

    if ( __obj->class_ptr->release != NULL ) {
        function_result release_result = ((__release_T) __obj->class_ptr->release)(__obj);
        // TODO: handle exceptions
    }

    if (__obj->class_ptr->type == interface) {
        iface_reference const *ref = &__obj->object_properties.iface_reference;

        __decrease_reference_count(ref->implementation_object);

//        free(ref);
//        __obj->object_data.value.custom_value = NULL;

        // interface
    }

// let the native release method handle this
    // if ( !__is_primitive_nullable(__obj->object_data) && __obj->object_data.value.custom_value != NULL) {
    //     free(__obj->object_data.value.custom_value);
    //     __obj->object_data.value.custom_value = NULL;
    // }

    if (__obj->class_ptr->type == class) { // && __obj->object_properties.class_object_properties.properties != NULL ) {
        for(int i = 0; i < __obj->class_ptr->properties_count; i++) {
            property * const __prop = &__obj->object_properties.class_object_properties.properties[i];
            // TODO: use __decrease_reference_count_nullable_value
            if (!__is_primitive(__prop->nullable_value) && __prop->nullable_value.value.object_value != NULL) {
                #ifdef DEBUG
                printf("Detach property %s:\n", __prop->nullable_value.value.object_value->class_ptr->name);
                #endif
                __decrease_reference_count(__prop->nullable_value.value.object_value);
                __prop->nullable_value.value.object_value = NULL;
            }
        }
        __obj->object_properties.class_object_properties.properties = NULL;
    }

    weak_reference_node *current_weak_reference_node = __obj->first_weak_reference_node;
    __obj->first_weak_reference_node = NULL;
    while (current_weak_reference_node != NULL) {
        weak_reference_node * const next_weak_reference_node = current_weak_reference_node->next;
        current_weak_reference_node->next = NULL;
        current_weak_reference_node->object = NULL;
        current_weak_reference_node = next_weak_reference_node;
    }

    #ifdef DEBUG
    for(int i = 0; i < MAX_ALLOCATIONS; i++) {
        if ( allocations[i] == __obj) {
            allocations[i] = NULL;
        }
    }
    #endif

    if (__obj->class_ptr->memory_pool != NULL) {
        free_from_pool(__obj->class_ptr->memory_pool, __obj);
    } else if (__obj->memory_pooled) {
        free_from_pool(small_object_memory_pool, __obj);
    } else {
        free(__obj);
    }
}

void print_allocated_objects() {
    #ifdef DEBUG
    for(int i = 0; i < MAX_ALLOCATIONS; i++) {
        if ( allocations[i] != NULL) {
            #ifdef DEBUG
            printf("Object still alive: %s (address: %p, object_id: %d)\n", allocations[i]->class_ptr->name, allocations[i], allocations[i]->object_properties.class_object_properties.object_id);
            #endif
        }
    }
    #endif
}

void clear_allocated_objects() {
    #ifdef DEBUG
    for(int i = 0; i < MAX_ALLOCATIONS; i++) {
        allocations[i] = NULL;
    }
    #endif
}

void __set_property(aobject * const __obj, int const __index, nullable_value __prop_value) {
    property * __prop = &__obj->object_properties.class_object_properties.properties[__index];
    if ( !__is_primitive(__prop->nullable_value) && __prop->nullable_value.value.object_value != NULL ) {
        __decrease_reference_count(__prop->nullable_value.value.object_value);
    }

    if ( !__is_primitive(__prop_value) && __prop_value.value.object_value != NULL ) {
        __increase_reference_count(__prop_value.value.object_value);
    }

    __prop->nullable_value = __prop_value;

//    printf("New prop int value: %d\n", __prop->nullable_value.value.int_value);

//    if (__is_primitive_nullable(__prop->nullable_value)) {
//        __set_primitive_null(&__prop->nullable_value, __is_primitive_null(__prop_value));
//    }
//    __prop->nullable_value.value = __prop_value.value;
}

void __set_static_property(aclass * const __class, int const __index, nullable_value __prop_value) {
    property * __prop = &__class->static_properties[__index];
    if ( !__is_primitive(__prop->nullable_value) && __prop->nullable_value.value.object_value != NULL ) {
        __decrease_reference_count(__prop->nullable_value.value.object_value);
    }

    if ( !__is_primitive(__prop_value) && __prop_value.value.object_value != NULL ) {
        __increase_reference_count(__prop_value.value.object_value);
    }

    __prop->nullable_value = __prop_value;
}

void __decrease_reference_count_nullable_value(nullable_value __value) {
    if ( !__is_primitive(__value) && __value.value.object_value != NULL ) {
        __decrease_reference_count(__value.value.object_value);
    }
}

void __increase_reference_count_nullable_value(nullable_value __value) {
    if ( !__is_primitive(__value) && __value.value.object_value != NULL ) {
        __increase_reference_count(__value.value.object_value);
    }
}

void deallocate_annotations(aclass * const __class) {
    for(int i = 0; i < __class->annotations_count; i++) {
        aobject * const a = __class->annotations[i];
        __decrease_reference_count(a);
        __class->annotations[i] = NULL;
    }
}

// function_result const __default_return() {
//     function_result res;
//     res.has_return_value = 0;
//     return res;
// }

// function_result const __return_int(int const value) {
//     function_result res;
//     res.has_return_value = 1;
//     res.return_value.int_value = value;
//     return res;
// }

// function_result const __return_long(long long const value) {
//     function_result res;
//     res.has_return_value = 1;
//     res.return_value.long_value = value;
//     return res;
// }

// function_result const __return_object(aobject * const value) {
//     function_result res;
//     res.has_return_value = 1;
//     res.return_value.object_value = value;
//     return res;
// }

// stack_trace_item * const __create_stack_trace_item(stack_trace_item * const previous_item, aobject * const item_text) {
//     __increase_reference_count(item_text);

//     stack_trace_item * const item = (stack_trace_item *) calloc(1, sizeof(stack_trace_item));

//     item->item_text = item_text;

//     if (previous_item != NULL) {
//         previous_item->next_item = item;
//     }
//     return item;
// }

void __throw_exception(function_result *result, aobject * const exception, aobject * const stack_trace_item_text) {

//    exception_holder * const holder = (exception_holder *) calloc(1, sizeof(exception_holder));
//    holder->exception = exception;
//    holder->first_stack_trace_item = __create_stack_trace_item(NULL, stack_trace_item_text);
//    holder->last_stack_trace_item  = holder->first_stack_trace_item;

    Am_Lang_Exception_addStackTraceItem_0(exception, stack_trace_item_text);

//    result.has_return_value = 0;
    if (result->exception) {
        __decrease_reference_count(result->exception);
    }
    result->exception = exception;
    __increase_reference_count(exception);
}

void __pass_exception(function_result *result, aobject * const exception, aobject * const stack_trace_item_text) {

    Am_Lang_Exception_addStackTraceItem_0(exception, stack_trace_item_text);
    if (result->exception) {
        __decrease_reference_count(result->exception);
    }
    result->exception = exception;

//    stack_trace_item *new_item = __create_stack_trace_item(result.exception_holder->last_stack_trace_item, stack_trace_item_text);
//    result.exception_holder->last_stack_trace_item = new_item;
}

// void __deallocate_stack_trace_item_chain(stack_trace_item *first_item) {
//     stack_trace_item *current = first_item;
//     while(current != NULL) {
//         __decrease_reference_count(current->item_text);
//         stack_trace_item *next = current->next_item;
//         free(current);
//         current = next;
//     }
// }

// void __deallocate_exception_holder(exception_holder * const holder) {
//     __decrease_reference_count(holder->exception);

//     __deallocate_stack_trace_item_chain(holder->first_stack_trace_item);
//     free(holder);
// }

void __deallocate_function_result(function_result const result) {
    if (result.exception != NULL) {
        __decrease_reference_count(result.exception);
    }
}

void __set_primitive_nullable(nullable_value * nullable_value, bool is_primitive_nullable) {
    unsigned char f = nullable_value->flags;
    f &= ~PRIMITIVE_NULLABLE;
    f |= is_primitive_nullable ? PRIMITIVE_NULLABLE : 0;
    nullable_value->flags = f; 
}

bool __is_primitive_nullable(const nullable_value nullable_value) {
    return nullable_value.flags & PRIMITIVE_NULLABLE;
}

bool __is_primitive(const nullable_value nullable_value) {
    return nullable_value.flags != 0;
}

void __set_primitive_null(nullable_value * nullable_value, bool is_primitive_null) {
    unsigned char f = nullable_value->flags;
    f &= ~PRIMITIVE_NULL;
    f |= is_primitive_null ? PRIMITIVE_NULL : 0;
    nullable_value->flags = f; 
}

bool __is_primitive_null(const nullable_value nullable_value) {
    return nullable_value.flags & PRIMITIVE_NULL;
}

bool __any_has_flags(const nullable_value *nv, unsigned short flags) {
    return (nv->flags & flags) == flags;
}

bool __any_equals(const nullable_value a, const nullable_value b) {
    if (__is_primitive_nullable(a)) {
        if (__is_primitive_nullable(b)) {
            // both nullable
            if (__is_primitive_null(a)) {
                if (__is_primitive_null(b)) {
                    return true;
                } else {
                    return false;
                }
            } else {
                if (__is_primitive_null(b)) {
                    return false;
                } else {
                    return memcmp(&a.value, &b.value, sizeof(value)); // TODO: let's hope there's no garbage here
                }
            }
        } else {
            // a nullable primitive, b not - can't be the same.
            return false;
        }
    } else {
        if (__is_primitive_nullable(b)) {
            // b nullable, a not - can't be the same
            return false;
        } else {
            // both are objects
            if (a.value.object_value == b.value.object_value) {
                return true;
            } else if (a.value.object_value != NULL && b.value.object_value != NULL) {
                // both not null
                return __object_equals(a.value.object_value, b.value.object_value);
            } else {
                return false;
                // one is null, the other not
            }
        }
    }
}

bool __object_equals(aobject * const a, aobject * const b) {
    if (a != NULL) {
        Am_Lang_Object_equals_0_T af = (Am_Lang_Object_equals_0_T) a->class_ptr->functions[Am_Lang_Object_equals_0_index];
        function_result res = af(a, b);
        // Am_Lang_Object_equals_0(a, b);
        return res.return_value.value.bool_value;
    }
    return a == b;
}

/* From constant */
aobject * __create_string_constant(char const * const str, aclass * const string_class) {
    size_t len = strlen(str);
    aobject * str_obj = __allocate_object_with_extra_size(string_class, sizeof(string_holder));
    string_holder * const holder = (string_holder *) (str_obj + 1);
    str_obj->object_properties.class_object_properties.object_data.value.custom_value = holder;
    int hash = __string_hash(str);
    holder->is_string_constant = true;
    holder->length = len;
    holder->string_value = (char *) str;
    holder->hash = hash;

//    *holder = (string_holder) { .is_string_constant = true, .length = strlen(str), .string_value = (char *) str, .hash = hash };
//    memcpy(holder, &t_holder, sizeof(string_holder));
    // holder->string_value = str; // assume that string constants will never change
    // holder->length = strlen(str); // TODO: how many characters exactly?
    // holder->is_string_constant = true;
    return str_obj;
}

aobject * __create_string(char const * const str, aclass * const string_class) {
    size_t len = strlen(str);
    aobject * str_obj = __allocate_object_with_extra_size(string_class, sizeof(string_holder) + len + 1);
    string_holder * const holder = (string_holder *) (str_obj + 1);
    str_obj->object_properties.class_object_properties.object_data.value.custom_value = holder;
    char * const newStr = (char * const) (holder + 1);
    strcpy(newStr, str);
    int hash = __string_hash(str);
    holder->is_string_constant = false;
    holder->length = len;
    holder->string_value = newStr;
    holder->hash = hash;
//    *holder = (string_holder) { .is_string_constant = false, .length = len, .string_value = newStr, .hash = hash };
//    memcpy(holder, &t_holder, sizeof(string_holder));
//    holder->string_value = newStr;
//    holder->length = len; // TODO: how many characters exactly?
//    holder->is_string_constant = false;
    return str_obj;
}

aobject * __create_array(size_t const size, size_t const item_size, aclass * const array_class, ctype const ctype) {
    aobject * array_obj = __allocate_object(array_class);
    array_holder * const holder = malloc(sizeof(array_holder));
    array_obj->object_properties.class_object_properties.object_data.value.custom_value = holder;
//    size_t const data_size = size * item_size;
//    unsigned char * const array_data = malloc(data_size);
    *holder = (array_holder) { .array_data = malloc(size * item_size), .size = size, .ctype = ctype, .item_size = item_size };
    memset(holder->array_data, 0, size * item_size);

//    memcpy(holder, &t_holder, sizeof(array_holder));

    // holder->array_data = malloc(data_size);
    // holder->size = size;    
    // holder->item_class = item_class;
    return array_obj;
}

aobject * __create_exception(aobject * const message) {
    aobject *ex = __allocate_object(&Am_Lang_Exception);
    Am_Lang_Exception_Exception_0(ex, message);
    Am_Lang_Exception___init_instance((nullable_value){ .value.object_value = ex });
    return ex;
}

void __throw_simple_exception(const char * const message, const char * const stack_trace_item_text, function_result * const result) {
    aobject * ex_msg = __create_string_constant(message, &Am_Lang_String);
    aobject * stit = __create_string_constant(stack_trace_item_text, &Am_Lang_String);
    aobject * ex = __create_exception(ex_msg);
    __throw_exception(result, ex, stit);
    __decrease_reference_count(ex_msg); // it's in the exception stack trace list now, we don't need it anymore.
    __decrease_reference_count(stit); // it's in the exception stack trace list now, we don't need it anymore.
    __decrease_reference_count(ex); // it's in the exception stack trace list now, we don't need it anymore.
}

bool is_descendant_of(aclass const * const cls, aclass const * const base) {
    if (cls == base) {
        return true;
    } else {
        if (cls->base) {
            return is_descendant_of(cls->base, base);
        }
    }
    return false;
}

void attach_weak_reference_node(weak_reference_node * const node, aobject * const object) {
    node->object = object;
    weak_reference_node * const first = object->first_weak_reference_node;
    node->next = first;
    object->first_weak_reference_node = node;
}

void detach_weak_reference_node(weak_reference_node * const node) {
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
//    free(node); We do this when Weak is released in native/Weak.c
}
