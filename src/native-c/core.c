#ifndef am_lang_core_c
#define am_lang_core_c

// #include <stdlib.h>
#include <core.h>
#include <string.h>

int __allocation_count = 0;

#ifdef DEBUG
aobject * allocations[256];
int allocation_index = 0;
#endif

void __decrease_reference_count(aobject * const __obj) {
    __obj->reference_count--;
   #ifdef DEBUG
    printf("decrease reference count of object of type %s (address: %p), new reference count %d\n", __obj->class_ptr->name, __obj, __obj->reference_count);
    #endif
    if ( __obj->reference_count == 0 ) {
        __deallocate_object(__obj);
    }
}

void __increase_reference_count(aobject * const __obj) {
    __obj->reference_count++;
    #ifdef DEBUG
    printf("increase reference count of object of type %s (address: %p), new reference count %d\n", __obj->class_ptr->name, __obj, __obj->reference_count);
    #endif
}

aobject * __allocate_object(aclass const * const __class) {
    __allocation_count++;
    #ifdef DEBUG
    printf("Allocate object of type %s (count: %d)\n", __class->name, __allocation_count);
    #endif
    aobject * __obj = (aobject *) malloc(sizeof(aobject));
    // DEBUG:
    #ifdef DEBUG
    allocations[allocation_index++] = __obj;
    #endif
    aobject __objt = { .class_ptr = __class, .properties = malloc(sizeof(property) * __class->properties_count), .reference_count = 1 };
    memcpy(__obj, &__objt, sizeof(aobject));
//    memset(__obj, 0, sizeof(aobject));

//    *__obj = (aobject) ;
//    __obj->class_ptr = __class;

////    __obj->object_data = NULL;
////    __obj->object_data_size = 0;
//    __obj->properties = malloc(sizeof(property) * __class->properties_count);
    memset(__obj->properties, 0, sizeof(property) * __class->properties_count);

//    __obj->reference_count = 1;
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

// let the native release method handle this
    // if ( !__is_primitive_nullable(__obj->object_data) && __obj->object_data.value.custom_value != NULL) {
    //     free(__obj->object_data.value.custom_value);
    //     __obj->object_data.value.custom_value = NULL;
    // }

    if ( __obj->properties != NULL ) {
        for(int i = 0; i < __obj->class_ptr->properties_count; i++) {
            property * __prop = &__obj->properties[i];
            if (!__is_primitive(__prop->nullable_value) && __prop->nullable_value.value.object_value != NULL) {
                #ifdef DEBUG
                printf("Detach property:\n");
                #endif
                __decrease_reference_count(__prop->nullable_value.value.object_value);
                __prop->nullable_value.value.object_value = NULL;
            }
        }
        free(__obj->properties);
        __obj->properties = NULL;
    }

    #ifdef DEBUG
    for(int i = 0; i < 256; i++) {
        if ( allocations[i] == __obj) {
            allocations[i] = NULL;
        }
    }
    #endif

    free(__obj);
}

void print_allocated_objects() {
    #ifdef DEBUG
    for(int i = 0; i < 256; i++) {
        if ( allocations[i] != NULL) {
            printf("Object still alive: %s\n", allocations[i]->class_ptr->name);
        }
    }
    #endif
}

void __set_property(aobject * const __obj, int const __index, nullable_value __prop_value) {
    property * __prop = &__obj->properties[__index];
    if ( !__is_primitive(__prop->nullable_value) && __prop->nullable_value.value.object_value != NULL ) {
        __decrease_reference_count(__prop->nullable_value.value.object_value);
    }

    if ( !__is_primitive(__prop_value) && __prop_value.value.object_value != NULL ) {
        __increase_reference_count(__prop_value.value.object_value);
    }

    __prop->nullable_value = __prop_value;

    printf("New prop int value: %d\n", __prop->nullable_value.value.int_value);

//    if (__is_primitive_nullable(__prop->nullable_value)) {
//        __set_primitive_null(&__prop->nullable_value, __is_primitive_null(__prop_value));
//    }
//    __prop->nullable_value.value = __prop_value.value;
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

stack_trace_item * const __create_stack_trace_item(stack_trace_item * const previous_item, aobject * const item_text) {
    __increase_reference_count(item_text);

    stack_trace_item * const item = (stack_trace_item *) malloc(sizeof(stack_trace_item));
    memset(item, 0, sizeof(stack_trace_item));

    item->item_text = item_text;

    if (previous_item != NULL) {
        previous_item->next_item = item;
    }
    return item;
}

void __throw_exception(function_result result, aobject * const exception, aobject * const stack_trace_item_text) {
    __increase_reference_count(exception);

    exception_holder * const holder = (exception_holder *) malloc(sizeof(exception_holder));
    memset(holder, 0, sizeof(exception_holder));

    holder->exception = exception;
    holder->first_stack_trace_item = __create_stack_trace_item(NULL, stack_trace_item_text);
    holder->last_stack_trace_item  = holder->first_stack_trace_item;

    result.has_return_value = 0;
    result.exception_holder = holder;
}

void __pass_exception(function_result result, aobject * const stack_trace_item_text) {
    stack_trace_item *new_item = __create_stack_trace_item(result.exception_holder->last_stack_trace_item, stack_trace_item_text);
    result.exception_holder->last_stack_trace_item = new_item;
}

void __deallocate_stack_trade_item_chain(stack_trace_item *first_item) {
    stack_trace_item *current = first_item;
    while(current != NULL) {
        __decrease_reference_count(current->item_text);
        stack_trace_item *next = current->next_item;
        free(current);
        current = next;
    }
}

void __deallocate_exception_holder(exception_holder * const holder) {
    __decrease_reference_count(holder->exception);

    __deallocate_stack_trade_item_chain(holder->first_stack_trace_item);
    free(holder);
}

void __deallocate_function_result(function_result const result) {
    if (result.exception_holder != NULL) {
        __deallocate_exception_holder(result.exception_holder);
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

/* From constant */
aobject * __create_string_constant(unsigned char const * const str, aclass const * const string_class) {
    aobject * str_obj = __allocate_object(string_class);
    string_holder * const holder = malloc(sizeof(string_holder));
    str_obj->object_data.value.custom_value = holder;
    string_holder const tHolder = { .is_string_constant = true, .length = strlen(str), .string_value = str };
    memcpy(holder, &tHolder, sizeof(string_holder));
    // holder->string_value = str; // assume that string constants will never change
    // holder->length = strlen(str); // TODO: how many characters exactly?
    // holder->is_string_constant = true;
    return str_obj;
}

aobject * __create_string(unsigned char const * const str, aclass const * const string_class) {
    aobject * const str_obj = __allocate_object(string_class);
    string_holder * const holder = malloc(sizeof(string_holder));
    str_obj->object_data.value.custom_value = holder;
    int const len = strlen(str);
    unsigned char * const newStr = malloc(len + 1);
    strcpy(newStr, str);
    string_holder const tHolder = { .is_string_constant = false, .length = len, .string_value = newStr };
    memcpy(holder, &tHolder, sizeof(string_holder));
//    holder->string_value = newStr;
//    holder->length = len; // TODO: how many characters exactly?
//    holder->is_string_constant = false;
    return str_obj;
}

aobject * __create_array(size_t const size, size_t const item_size, aclass const * const array_class, aclass const * const item_class) {
    aobject * array_obj = __allocate_object(array_class);
    array_holder * const holder = malloc(sizeof(array_holder));
    array_obj->object_data.value.custom_value = holder;
    size_t const data_size = size * item_size;
//    unsigned char * const array_data = malloc(data_size);
    array_holder const tHolder = { .array_data = malloc(data_size), .size = size, .item_class = item_class };
    memcpy(holder, &tHolder, sizeof(array_holder));

    // holder->array_data = malloc(data_size);
    // holder->size = size;    
    // holder->item_class = item_class;
    return array_obj;
}

#endif