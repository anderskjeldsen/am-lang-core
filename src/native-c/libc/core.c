// #include <stdlib.h>
#include <libc/core.h>
#include <string.h>
#include <Am/Lang/Exception.h>
#include <Am/Lang/Object.h>
#include <Am/Lang/Annotations/UseMemoryPool.h>
#include <Am/Lang/PropertyInfo.h>
#include <Am/Lang/ClassRef.h>
#include <libc/memory_pools.h>
#include <libc/core_inline_functions.h>

int __allocation_count = 0;
#define MAX_ALLOCATIONS 1024 * 50

#ifdef DEBUG
int __last_object_id = 0;
aobject * allocations[MAX_ALLOCATIONS];
int allocation_index = 0;
#endif

aobject * __first_object = NULL;
aobject * __first_detached_object = NULL;
aclass * __first_class = NULL;

void __mark_root_objects() {    
    aclass *current = __first_class;
    while(current != NULL) {
        if (current->type == class) {
            __mark_static_properties(current);
        }
        current = current->next;
    }
}

void __mark_object(aobject * const obj) {
    if (obj != NULL) {
        #ifdef DEBUG
        printf("Mark object of type %s...%p, refs, ref: %d, prop ref: %d\n", obj->class_ptr->name, obj, obj->reference_count, obj->property_reference_count);
        printf("Mark object...%p\n", obj->class_ptr->name);    
        #endif

        obj->marked = true;
        if (obj->class_ptr->mark_children != NULL) {
            ((__mark_children_T) obj->class_ptr->mark_children)(obj);
        }

        #ifdef DEBUG
        printf("Mark properties for %s\n", obj->class_ptr->name);
        #endif
        for(int i = 0; i < obj->class_ptr->properties_count; i++) {
            property * const __prop = &obj->object_properties.class_object_properties.properties[i];
            if (!__is_primitive(__prop->nullable_value) && __prop->nullable_value.value.object_value != NULL) {
                aobject *o = __prop->nullable_value.value.object_value;
                __mark_object(__prop->nullable_value.value.object_value);
            }
        }

        #ifdef DEBUG
        printf("Object marked %s\n", obj->class_ptr->name);
        #endif
    }
}

void __sweep_unmarked_objects() {
    printf("Allocated objects before sweep %d\n", __allocation_count);
    int old_count = 1;
    int new_count = 0;
    aobject * current = NULL;

    while(old_count != new_count) {
        old_count = new_count;
        new_count = 0;
        current = __first_object;
        while(current != NULL) {
            sweep_result result = __sweep_object(current);
            if (result.is_swept) {
                new_count++;
            } 
            current = result.next;
        }
    }

    current = __first_detached_object;
    while(current != NULL) {
        aobject *next = current->next;
        __deallocate_detached_object(current);
        current = next;
    }
    __first_detached_object = NULL;

    __clear_marks();

}

void __clear_marks() {
    #ifdef DEBUG
    printf("clear marks\n");
    #endif
    aobject * current = __first_object;
    while(current != NULL) {
        if (current->marked) {
            current->marked = false;
        }   
        current = current->next;
    }
    #ifdef DEBUG
    printf("clear marks DONE\n");
    #endif
}

sweep_result __sweep_object(aobject * const obj) {
    if (obj != NULL) {
        if (obj->marked) {
//            obj->marked = false;
//            printf("Don't sweep marked object %s, m: %d, rc: %d\n", obj->class_ptr->name, obj->marked, obj->reference_count);
        } else {
            if (obj->reference_count == 0 && !obj->pending_deallocation) {
                #ifdef DEBUG
                printf("Sweep object %s, m: %d, ref: %d, propref: %d\n", obj->class_ptr->name, obj->marked, obj->reference_count, obj->property_reference_count);
                #endif
                return __detach_object_from_sweep(obj);
            } else {
                printf("Don't sweep referenced object %s, m: %d, rc: %d\n", obj->class_ptr->name, obj->marked, obj->reference_count);
            }
        }
    }
    return (sweep_result) { .next = obj->next, .is_swept = false };
}

void __register_class(aclass * const __class) {
    #ifdef DEBUG
    printf("Register class %s, %p\n", __class->name, __class);
    #endif
    __class->next = __first_class;
    __first_class = __class;
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

/*
    __obj->next = __first_object;

    if (__first_object != NULL) {
        __first_object->prev = __obj;
    }
    __first_object = __obj;
*/

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

sweep_result __detach_object_from_sweep(aobject * const __obj) {

    if (__obj->pending_deallocation) {
        return (sweep_result) { .is_swept = false };
    }

    #ifdef DEBUG
    printf("Detach (sweep) object of type %s (total object allocation count: %d)\n", __obj->class_ptr->name, __allocation_count);
    #endif
   
    __detach_object(__obj);

    if (__obj == __first_object) {
        __first_object = __obj->next;
        if (__first_object != NULL) {
            __first_object->prev = NULL;
        }
    } else {        
        __obj->prev->next = __obj->next;
        if (__obj->next != NULL) {
            __obj->next->prev = __obj->prev;
        }
    }

    aobject *next_to_sweep = __obj->next;

    if (__first_detached_object == NULL) {
        __first_detached_object = __obj;
        __obj->next = NULL;
        __obj->prev = NULL;
    } else {
        __obj->next = __first_detached_object;
        __first_detached_object->prev = __obj;
        __first_detached_object = __obj;
    }

    sweep_result result = { .next = next_to_sweep, .is_swept = true };
    return result;
}

void __deallocate_detached_object(aobject * const __obj) {
    #ifdef DEBUG
    printf("Deallocate detached object of type %s (total object allocation count: %d)\n", __obj->class_ptr->name, __allocation_count);
    #endif

    __allocation_count--;

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

void __deallocate_object(aobject * const __obj) {

    if (__obj->pending_deallocation) {
        return;
    }

    __detach_object(__obj);

    #ifdef DEBUG
    printf("Deallocate object of type %s (total object allocation count: %d)\n", __obj->class_ptr->name, __allocation_count);
    #endif

    __allocation_count--;

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

void __detach_object(aobject * const __obj) {
    #ifdef DEBUG
    printf("Detach object of type %s (total object allocation count: %d)\n", __obj->class_ptr->name, __allocation_count);
    #endif

    __obj->pending_deallocation = true;

    if ( __obj->class_ptr->release != NULL ) {
        function_result release_result = ((__release_T) __obj->class_ptr->release)(__obj);
        // TODO: handle exceptions
    }

    if (__obj->class_ptr->type == interface) {
        iface_reference const *ref = &__obj->object_properties.iface_reference;

        __decrease_reference_count(ref->implementation_object);
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
                __decrease_property_reference_count(__prop->nullable_value.value.object_value);
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

}

void __dereference_static_properties() {
    aclass *c = __first_class;
    while(c != NULL) {
        __dereference_static_properties_for_class(c);
        c = c->next;
    }
}

void __dereference_static_properties_for_class(aclass * const __class) {
    #ifdef DEBUG
    printf("Dereference static properties of class %s\n", __class->name);
    #endif

    if (__class->type == class) { // && __obj->object_properties.class_object_properties.properties != NULL ) {
        for(int i = 0; i < __class->static_properties_count; i++) {
            property * const __prop = &__class->static_properties[i];
            // TODO: use __decrease_reference_count_nullable_value
            if (!__is_primitive(__prop->nullable_value) && __prop->nullable_value.value.object_value != NULL) {
                __decrease_property_reference_count(__prop->nullable_value.value.object_value);
                __prop->nullable_value.value.object_value = NULL;
            }
        }
    }
}

void __mark_static_properties(aclass * const __class) {
    #ifdef DEBUG
    printf("Mark static properties of class %s\n", __class->name);
    #endif
    
    if (__class->type == class) { // && __obj->object_properties.class_object_properties.properties != NULL ) {
        for(int i = 0; i < __class->static_properties_count; i++) {
            property * const __prop = &__class->static_properties[i];
            // TODO: use __decrease_reference_count_nullable_value
            if (!__is_primitive(__prop->nullable_value) && __prop->nullable_value.value.object_value != NULL) {
                __mark_object(__prop->nullable_value.value.object_value);
            }
        }
    }
}

void print_allocated_objects() {
    printf("Allocated objects %d\n", __allocation_count);

    if (__allocation_count > 0) {
        printf("Going through attached objects:\n");
        aobject *c = __first_object;
        while(c != NULL) {
            printf("Object alive, %s\n", c->class_ptr.name);
            c = c->next;
        }    
        printf("Going through detached objects:\n");
        *c = __first_detached_object;
        while(c != NULL) {
            printf("Object alive, %s\n", c->class_ptr.name);
            c = c->next;
        }    
    }

    #ifdef DEBUG
    for(int i = 0; i < MAX_ALLOCATIONS; i++) {
        if ( allocations[i] != NULL) {
            printf("Object still alive: %s (address: %p, object_id: %d, property refs: %d, inline refs: %d)\n", allocations[i]->class_ptr->name, allocations[i], allocations[i]->object_properties.class_object_properties.object_id, allocations[i]->property_reference_count, allocations[i]->reference_count);
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

void deallocate_annotations(aclass * const __class) {
    for(int i = 0; i < __class->annotations_count; i++) {
        aobject * const a = __class->annotations[i];
        __decrease_reference_count(a);
        __class->annotations[i] = NULL;
    }
}
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

bool __any_null(const nullable_value a) {
    if (__is_primitive_nullable(a)) {
        return __is_primitive_null(a);
    } else {
        return a.value.object_value == NULL;
    }
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

aobject * __create_array(unsigned int const size, unsigned char const item_size, aclass * const array_class, ctype const ctype) {
    size_t extra_size = sizeof(array_holder) + (size * item_size);
    aobject * array_obj = __allocate_object_with_extra_size(array_class, extra_size);
//    array_holder * const holder = malloc(sizeof(array_holder));
    array_holder * const holder = (array_holder *) &array_obj[1];
    void *array_data = (void *) (holder + 1);
    array_obj->object_properties.class_object_properties.object_data.value.custom_value = holder;
//    size_t const data_size = size * item_size;
//    unsigned char * const array_data = malloc(data_size);
    holder->array_data = array_data;
    holder->ctype = ctype;
    holder->item_class = array_class;
    holder->item_size = item_size;
    holder->size = size;    
//    *holder = (array_holder) { .array_data = malloc(size * item_size), .size = size, .ctype = ctype, .item_size = item_size };
//    memset(holder->array_data, 0, size * item_size);

//    memcpy(holder, &t_holder, sizeof(array_holder));

    // holder->array_data = malloc(data_size);
    // holder->size = size;    
    // holder->item_class = item_class;
    return array_obj;
}

array_holder * get_array_holder(aobject * const array_obj) {
    return (array_holder *) &array_obj[1];
}

char * get_array_data(array_holder * holder) {
    return (char *) &holder[1];
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

// true meaning: is same class OR descendant
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

void create_property_info(const unsigned char index, char * const name, aobject ** property_infos, aclass *cls) {
    aobject * property_info = __allocate_object_with_extra_size(&Am_Lang_PropertyInfo, sizeof(cls));
    property *properties = (property *) (property_info + 1);
    aclass ** class_holder_ptr = (aclass **) &properties[3]; // given that PropertyInfo has exactly 2 properties
    *class_holder_ptr = cls;

    Am_Lang_PropertyInfo_PropertyInfo_0(property_info);
    aobject * property_name = __create_string_constant(name, &Am_Lang_String);
    __set_property(property_info, Am_Lang_PropertyInfo_P_name, (nullable_value) { .flags = 0, .value.object_value = property_name});
    __set_property(property_info, Am_Lang_PropertyInfo_P_index, (nullable_value) { .flags = PRIMITIVE_UCHAR, .value.uchar_value = index });
	property_infos[index] = property_info;
    __increase_property_reference_count(property_info);
    __decrease_reference_count(property_info);
    __decrease_reference_count(property_name);
}

