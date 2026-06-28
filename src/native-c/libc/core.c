// #include <stdlib.h>
#include <libc/core.h>
#include <string.h>
#include <stdarg.h>

// Thread-safe ARC platform layer. Linux/macOS use a recursive
// pthread_mutex (recursive so a single thread can take the lock more
// than once while nested inside ARC machinery — destructors that
// recursively dec refs would otherwise self-deadlock). AmigaOS m68k
// classic doesn't need the lock at all for correctness (cooperative
// inside the task; preemption is between tasks, and inter-task object
// sharing on classic Amiga goes through Message ports anyway). The
// helpers stay defined as no-ops so generated code links cleanly.
#ifndef __AMIGA__
#include <pthread.h>
static pthread_mutex_t __arc_shared_mutex;
static bool __arc_shared_mutex_initialised = false;
void __arc_shared_mutex_init(void) {
    if (__arc_shared_mutex_initialised) return;
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&__arc_shared_mutex, &attr);
    pthread_mutexattr_destroy(&attr);
    __arc_shared_mutex_initialised = true;
}
void __arc_shared_lock(void)   { pthread_mutex_lock(&__arc_shared_mutex); }
void __arc_shared_unlock(void) { pthread_mutex_unlock(&__arc_shared_mutex); }
void * __current_thread(void)  { return (void *) pthread_self(); }
#else
// AmigaOS: no real shared-memory contention for ARC; lock/unlock are
// no-ops, and the thread identity is FindTask(NULL) (the Task pointer
// is stable for the lifetime of the task).
#include <proto/exec.h>
void __arc_shared_mutex_init(void) {}
void __arc_shared_lock(void)   {}
void __arc_shared_unlock(void) {}
void * __current_thread(void)  { return (void *) FindTask(NULL); }
#endif
#include <Am/Lang/Exception.h>
#include <Am/Lang/Object.h>
#include <Am/Lang/Annotations/UseMemoryPool.h>
#include <Am/Lang/PropertyInfo.h>
#include <Am/Lang/ClassRef.h>

#include <Am/Lang/Byte.h>
#include <Am/Lang/Bool.h>
#include <Am/Lang/Int.h>
#include <Am/Lang/Long.h>
#include <Am/Lang/Short.h>
#include <Am/Lang/UByte.h>
#include <Am/Lang/UInt.h>
#include <Am/Lang/ULong.h>
#include <Am/Lang/UShort.h>
#ifdef FEATURE_FLOATING_POINT
#include <Am/Lang/Float.h>
#include <Am/Lang/Double.h>
#endif

#include <libc/core_inline_functions.h>

bool __conditional_logging_on = false;

// Thread-safe ARC runtime flag — see core.h for the contract. Starts
// false; flipped on by `__create_wrapper`. Never flips back: even if
// every wrapper later dies, the flag stays on because we don't want
// to track the live-wrapper count across the codebase. The cost is
// that long-running programs which briefly share an object once pay
// the unwrap branch forever after — fine in practice.
bool __amlc_any_wrappers_alive = false;

// Always-defined so callers compiled with DEBUG can link even when
// core.c itself was compiled without DEBUG. Body only does anything
// when DEBUG/TRACKOBJECTS is on (object_id only exists then).
void __print_memory_header(aobject * const obj, const char * prefix) {
#if defined(DEBUG) || defined(TRACKOBJECTS)
    if (obj != NULL && obj->object_properties.class_object_properties.object_id == 946) {
        unsigned char *p = (unsigned char *)obj - 16;
        printf("%s - Memory header bytes: ", prefix);
        for (int i = 0; i < 16; i++) {
            printf("%02x ", p[i]);
        }
        printf("\n");
    }
#else
    (void)obj; (void)prefix;
#endif
}

int __allocation_count = 0;
#define MAX_ALLOCATIONS 1024 * 50
#if defined(DEBUG) || defined(TRACKOBJECTS)
int __last_object_id = 0;
aobject * allocations[MAX_ALLOCATIONS];
int allocation_index = 0;
#endif

aobject * __bla = NULL;
aobject * __first_object = NULL;
aobject * __first_detached_object = NULL;
//aclass * __first_class = NULL;
class_static *__first_class_static = NULL;

#ifdef DEBUG
void __debug_print_string_if_string(aobject * const obj, const char * prefix) {
    if (obj != NULL && obj->class_ptr != NULL && strcmp(obj->class_ptr->name, "Am.Lang.String") == 0) {
        string_holder * holder = (string_holder *) obj->object_properties.class_object_properties.object_data.value.custom_value;
        if (holder != NULL && holder->string_value != NULL) {
            printf("%s: String content: \"%s\" (len=%u, hash=%u, const=%s, addr=%p)\n", 
                   prefix, holder->string_value, holder->length, holder->hash, 
                   holder->is_string_constant ? "true" : "false", obj);
        } else {
            printf("%s: String object with NULL holder or string_value (addr=%p)\n", prefix, obj);
        }
    }
}
#endif 


void __mark_root_objects() {
    class_static *current = __first_class_static;
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
        #ifdef CONDLOG 
        if (__conditional_logging_on) {
        #endif
        printf("Mark object of type %s...%p, refs, ref: %d, prop ref: %d\n", obj->class_ptr->name, obj, obj->reference_count, obj->property_reference_count);
        printf("Mark object...%p\n", obj->class_ptr->name);    
        #ifdef CONDLOG 
        }
        #endif        
        #endif

        obj->marked = true;
        if (obj->class_ptr->mark_children != NULL) {
            ((__mark_children_T) obj->class_ptr->mark_children)(obj);
        }

        #ifdef DEBUG
        #ifdef CONDLOG 
        if (__conditional_logging_on) {
        #endif
        printf("Mark properties for %s\n", obj->class_ptr->name);
        #ifdef CONDLOG 
        }
        #endif        
        #endif

        for(int i = 0; i < obj->class_ptr->properties_count; i++) {
            property * const __prop = &obj->object_properties.class_object_properties.properties[i];
            if (!__is_primitive(__prop->nullable_value) && __prop->nullable_value.value.object_value != NULL) {
                aobject *o = __prop->nullable_value.value.object_value;
                __mark_object(__prop->nullable_value.value.object_value);
            }
        }

        #ifdef DEBUG
        #ifdef CONDLOG 
        if (__conditional_logging_on) {
        #endif
        printf("Object marked %s\n", obj->class_ptr->name);
        #ifdef CONDLOG 
        }
        #endif
        #endif
    }
}

void __sweep_unmarked_objects() {
    #ifdef DEBUG
    #ifdef CONDLOG 
    if (__conditional_logging_on) {
    #endif

    printf("Allocated objects before sweep %d\n", __allocation_count);
    #ifdef CONDLOG 
    }
    #endif
    #endif

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
    #ifdef CONDLOG 
    if (__conditional_logging_on) {
    #endif
    printf("clear marks\n");
    #ifdef CONDLOG 
}
    #endif
    #endif

    aobject * current = __first_object;
    while(current != NULL) {
        if (current->marked) {
            current->marked = false;
        }   
        current = current->next;
    }
    #ifdef DEBUG
    #ifdef CONDLOG 
    if (__conditional_logging_on) {
    #endif
    printf("clear marks DONE\n");
    #ifdef CONDLOG 
    }
    #endif
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
                #ifdef CONDLOG 
                if (__conditional_logging_on) {
                #endif
                printf("Sweep object %s, m: %d, ref: %d, propref: %d\n", obj->class_ptr->name, obj->marked, obj->reference_count, obj->property_reference_count);
                #ifdef CONDLOG 
                }
                #endif
                #endif

                return __detach_object_from_sweep(obj);
            } else {
                #ifdef DEBUG
                #ifdef CONDLOG 
                if (__conditional_logging_on) {
                #endif
                printf("Don't sweep referenced object %s, m: %d, rc: %d\n", obj->class_ptr->name, obj->marked, obj->reference_count);
                #ifdef CONDLOG 
                }
                #endif
                #endif
            }
        }
    }
    return (sweep_result) { .next = obj->next, .is_swept = false };
}

void __register_class(class_static * const __class_static) {
    #ifdef DEBUG
    #ifdef CONDLOG 
    if (__conditional_logging_on) {
    #endif
    printf("Register class %s, %p\n", __class_static->name, __class_static);
    #ifdef CONDLOG 
    }
    #endif
    #endif
    __class_static->next = __first_class_static;
    __first_class_static = __class_static;
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
    
    #if defined(DEBUG) || defined(TRACKOBJECTS)
    __last_object_id++;   
    #endif
    #ifdef DEBUG
    #ifdef CONDLOG 
    if (__conditional_logging_on) {
    #endif
    printf("Allocate object of type %s (count: %d, object_id: %d) \n", __class->name, __allocation_count, __last_object_id);
    #ifdef CONDLOG 
    }
    #endif
    #endif



    size_t size_with_properties = sizeof(aobject) + (sizeof(property) * __class->properties_count) + extra_size;

    aobject * __obj = NULL;

    __obj = (aobject *) calloc(1, size_with_properties);

    if (__obj != NULL) {

        #ifdef DEBUG
        #ifdef CONDLOG 
        if (__conditional_logging_on) {
        #endif
        printf("Allocated object of type %s at address %p\n", __class->name, __obj);
        __debug_print_string_if_string(__obj, "allocate string");
        #ifdef CONDLOG 
        }
        #endif
        #endif

        if (__class->statics->type == class && __class->properties_count > 0) {
            __obj->object_properties.class_object_properties.properties = (property *) (__obj + 1);;
        }

            __obj->class_ptr = __class;
            __obj->reference_count = 1;
            // Thread-safe ARC: every newly allocated object is owned
            // by the thread that allocated it. This is the cheap fast
            // path for refcount mutation later — owner-thread inc/dec
            // skips locks; cross-thread access goes through wrapper
            // machinery. `first_object_wrapper` stays NULL until
            // someone actually shares this object across threads.
            __obj->owner_thread = __current_thread();
            __obj->first_object_wrapper = NULL;

            #if defined(DEBUG) || defined(TRACKOBJECTS)
            allocations[allocation_index++] = __obj;
            if (allocation_index % 1000 == 0) {
                printf("Another 1000 allocations: %d\n", allocation_index);
            }

            __obj->object_properties.class_object_properties.object_id = __last_object_id;
            #endif
        } else {
            #ifdef DEBUG
            #ifdef CONDLOG 
            if (__conditional_logging_on) {
            #endif
            printf("Failed to allocate object of type %s\n", __class->name);
            #ifdef CONDLOG 
            }
            #endif
            #endif
        }
 
    #ifdef DEBUG
    if (__obj->object_properties.class_object_properties.object_id == 946) {
        printf("Allocated File\n");
        __print_memory_header(__obj, "Allocation");
    }
    #endif


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
        return (sweep_result) { .is_swept = false, .next = __obj->next };
    }

    #ifdef DEBUG
    #ifdef CONDLOG 
    if (__conditional_logging_on) {
    #endif
    printf("Detach (sweep) object of type %s (total object allocation count: %d)\n", __obj->class_ptr->name, __allocation_count);
    #ifdef CONDLOG 
    }
    #endif
    #endif
   
    __detach_object(__obj);

    #ifdef DEBUG
    #ifdef CONDLOG 
    if (__conditional_logging_on) {
    #endif
    printf("Almost done Detaching (sweep) object of type %s (total object allocation count: %d)\n", __obj->class_ptr->name, __allocation_count);
    #ifdef CONDLOG 
    }
    #endif
    #endif

    if (__obj == __first_object) {
//        printf("Detaching first object\n");
        __set_first_object(__obj->next, "detach_object_from_sweep");
        if (__first_object != NULL) {
            __first_object->prev = NULL;
        }
    } else {
//        printf("Detaching not first object\n");
        if (__obj->prev == NULL) {
//            printf("prev is null\n");
        }
        if (__obj->next == NULL) {
//            printf("next is null\n");
        }
        __obj->prev->next = __obj->next;
        if (__obj->next != NULL) {
            __obj->next->prev = __obj->prev;
        }
    }

    aobject *next_to_sweep = __obj->next;

    if (__first_detached_object == NULL) {
//        printf("set first detached object\n");

        __first_detached_object = __obj;
        __obj->next = NULL;
        __obj->prev = NULL;
    } else {
//        printf("link to first detached object\n");

        __obj->next = __first_detached_object;
        __first_detached_object->prev = __obj;
        __first_detached_object = __obj;
    }

    sweep_result result = { .next = next_to_sweep, .is_swept = true };
    #ifdef DEBUG
    #ifdef CONDLOG 
    if (__conditional_logging_on) {
    #endif
    printf("Done Detaching (sweep) object of type %s (total object allocation count: %d)\n", __obj->class_ptr->name, __allocation_count);
    #ifdef CONDLOG 
    }
    #endif
    #endif

    return result;
}

void __deallocate_detached_object(aobject * const __obj) {
    #ifdef DEBUG
    #ifdef CONDLOG 
    if (__conditional_logging_on) {
    #endif
    printf("Deallocate detached object of type %s (total object allocation count: %d)\n", __obj->class_ptr->name, __allocation_count);
    #ifdef CONDLOG 
    }
    #endif
    #endif

    __allocation_count--;

    #if defined(DEBUG) || defined(TRACKOBJECTS)
    for(int i = 0; i < MAX_ALLOCATIONS; i++) {
        if ( allocations[i] == __obj) {
            allocations[i] = NULL;
        }
    }
    #endif

    free(__obj);
}

void __deallocate_object(aobject * const __obj) {
    #ifdef WRAPLOG
    fprintf(stderr, "[real.dealloc] real=%p class=%s rc=%d propref=%d owner_gone=%d wrappers=%p\n",
        __obj,
        __obj && __obj->class_ptr ? __obj->class_ptr->name : "(?)",
        __obj ? __obj->reference_count : -1,
        __obj ? __obj->property_reference_count : -1,
        __obj ? __obj->owner_gone : -1,
        __obj ? (void*)__obj->first_object_wrapper : NULL);
    fflush(stderr);
    #endif
    bool it = false;

    #if defined(DEBUG) || defined(TRACKOBJECTS)
    char *name = __obj->class_ptr->name;
    int object_id = __obj->object_properties.class_object_properties.object_id;
    char *type = __obj->class_ptr->name;    
    #endif

    #ifdef DEBUG
    #ifdef CONDLOG 
    if (__conditional_logging_on) {
    #endif
    printf("Start deallocation of object of type %s (address: %p, object id: %d, total object allocation count: %d)\n", __obj->class_ptr->name, __obj, __obj->object_properties.class_object_properties.object_id, __allocation_count);
    #ifdef CONDLOG 
    }
    #endif
    #endif

    if (__obj->pending_deallocation) {
        #ifdef DEBUG
        #ifdef CONDLOG 
        if (__conditional_logging_on) {
        #endif
        printf("Cancel (pending) deallocation of object of type %s (address: %p, object id: %d, total object allocation count: %d)\n", __obj->class_ptr->name, __obj, __obj->object_properties.class_object_properties.object_id, __allocation_count);
        #ifdef CONDLOG 
        }
        #endif
        #endif

        return;
    }

    __detach_object(__obj);

    #ifdef DEBUG
    #ifdef CONDLOG 
    if (__conditional_logging_on) {
    #endif
    printf("Finalize deallocation of object of type %s (address: %p, object id: %d, total object allocation count: %d)\n", type, __obj, object_id, __allocation_count);
    #ifdef CONDLOG 
    }
    #endif
    #endif

    __allocation_count--;

    #if defined(DEBUG) || defined(TRACKOBJECTS)
    for(int i = 0; i < MAX_ALLOCATIONS; i++) {
        if ( allocations[i] == __obj) {
            allocations[i] = NULL;
        }
    }
    #endif

    #ifdef DEBUG
    printf("About to free object %s (address: %p)\n", __obj->class_ptr->name, __obj);
    printf("Object validation - class_ptr: %p\n", __obj->class_ptr);
    if (__obj->class_ptr != NULL) {
        printf("Class name: %s\n", __obj->class_ptr->name);
    }
    printf("Reference counts: ref=%d, prop_ref=%d\n", __obj->reference_count, __obj->property_reference_count);
    #endif

    #ifdef DEBUG
    if (__obj->object_properties.class_object_properties.object_id == 946) {
        printf("Deallocating File, quitting\n");
        __print_memory_header(__obj, "Before free()");
//        exit(1);
    }
    #endif

    free(__obj);

    #ifdef DEBUG
    #ifdef CONDLOG 
    if (__conditional_logging_on) {
    #endif
    printf("End of deallocation of object of type %s (address: %p, object id: %d, total object allocation count: %d)\n", type, __obj, object_id, __allocation_count);
    #ifdef CONDLOG 
    }
    #endif
    #endif

}

void __decrease_property_reference_count(aobject * const __obj) {
    if ( __obj != NULL) {
        #ifdef WRAPLOG
        if (__obj->class_ptr == NULL) {
            fprintf(stderr, "[dec_pr] wrapper=%p propref %d->%d\n", __obj, __obj->property_reference_count, __obj->property_reference_count - 1);
            fflush(stderr);
        }
        #endif
        // Thread-safe ARC: unlinking from __first_object + the counter
        // mutation must be atomic w.r.t. concurrent increases on other
        // threads. Recursive mutex → nested set_property calls (which
        // also take the lock) are fine. The deallocate path below is
        // outside the lock — see comment near the call.
        __arc_shared_lock();
        __obj->property_reference_count--;
        #if defined(DEBUG) && defined(ARCLOG)
        #ifdef CONDLOG
        if (__conditional_logging_on) {
        #endif
        printf("decrease property reference count of object of type %s (address: %p, object_id: %d), new reference count %d, property reference count %d\n", __obj->class_ptr->name, __obj, __obj->object_properties.class_object_properties.object_id, __obj->reference_count, __obj->property_reference_count);
        #ifdef CONDLOG
        }
        #endif
        #endif

        bool should_deallocate = false;
        if (__obj->property_reference_count == 0 && !__obj->pending_deallocation) {
            if (__obj == __first_object) {

                __set_first_object(__obj->next, "decrease_property_reference_count");
                __obj->next = NULL;
                if (__first_object != NULL) {
                    __first_object->prev = NULL;
                }
            } else {
                __obj->prev->next = __obj->next;
                if (__obj->next != NULL) {
                    __obj->next->prev = __obj->prev;
                }
                __obj->prev = NULL;
                __obj->next = NULL;
            }


            if (__obj->reference_count == 0) {
                // Same Stage 8 end-of-life decision as the rc → 0 path:
                // if foreign wrappers still subscribe, set `owner_gone`
                // and let the last wrapper death finalize. Otherwise
                // claim destruction now via `destruction_claimed` —
                // NOT `pending_deallocation` (that's __detach_object's
                // recursion guard).
                if (__obj->first_object_wrapper == NULL) {
                    if (!__obj->destruction_claimed) {
                        __obj->destruction_claimed = true;
                        should_deallocate = true;
                    }
                } else {
                    __obj->owner_gone = true;
                }
            }
        }
        __arc_shared_unlock();

        // Run the destructor outside the global ARC lock — release
        // callbacks call back into the ARC machinery (dec'ing children),
        // which would re-enter the recursive mutex safely BUT might also
        // call out to user code that takes its own locks. Keeping the
        // destructor outside the global ARC lock minimises hold-time
        // and avoids surprising lock-ordering inversions.
        if (should_deallocate) {
            // Wrappers go through __deallocate_wrapper, not the
            // class_ptr->release path — calling __deallocate_object on
            // a wrapper would deref a NULL class_ptr inside
            // __detach_object's debug-print/release-dispatch. Same split
            // we already do in __decrease_reference_count for rc=0.
            if (__obj->class_ptr == NULL) {
                __deallocate_wrapper(__obj);
            } else {
                __deallocate_object(__obj);
            }
        }
    }
}
void __detach_object(aobject * const __obj) {
    #ifdef DEBUG
    #ifdef CONDLOG 
    if (__conditional_logging_on) {
    #endif
    printf("Detach object of type %s (address: %p, object_id: %d, total object allocation count: %d)\n", __obj->class_ptr->name, __obj, __obj->object_properties.class_object_properties.object_id,  __allocation_count);
    #ifdef CONDLOG 
    }
    #endif
    #endif

    // Prevent double-detachment of the same object
    if (__obj->pending_deallocation) {
        #ifdef DEBUG
        printf("WARNING: Attempted double-detachment of object %s (address: %p)\n", __obj->class_ptr->name, __obj);
        #endif
        return;
    }

    __obj->pending_deallocation = true;

    if ( __obj->class_ptr->release != NULL ) {
        function_result release_result = ((__release_T) __obj->class_ptr->release)(__obj);
        // TODO: handle exceptions
        if (release_result.exception != NULL) {
            printf("Exception in release method for : %s\n", __obj->class_ptr->name);
            __decrease_reference_count(release_result.exception);
        }

    }

    if (__obj->class_ptr->statics->type == interface) {
        iface_reference const *ref = &__obj->object_properties.iface_reference;
        #ifdef DEBUG
        #ifdef CONDLOG 
        if (__conditional_logging_on) {
        #endif

        aobject * impl_obj = ref->implementation_object;
        printf("Detach interface object of type %s, implementation type %s (address: %p, object_id: %d, total object allocation count: %d)\n", __obj->class_ptr->name, impl_obj->class_ptr->name, impl_obj, impl_obj->object_properties.class_object_properties.object_id,  __allocation_count);
        #ifdef CONDLOG 
        }
        #endif
        #endif  

        __decrease_reference_count(ref->implementation_object);
    }

// let the native release method handle this
    // if ( !__is_primitive_nullable(__obj->object_data) && __obj->object_data.value.custom_value != NULL) {
    //     free(__obj->object_data.value.custom_value);
    //     __obj->object_data.value.custom_value = NULL;
    // }

    if (__obj->class_ptr->statics->type == class) { // && __obj->object_properties.class_object_properties.properties != NULL ) {
        for(int i = 0; i < __obj->class_ptr->properties_count; i++) {
            property * const __prop = &__obj->object_properties.class_object_properties.properties[i];
            // TODO: use __decrease_reference_count_nullable_value
            if (!__is_primitive(__prop->nullable_value) && __prop->nullable_value.value.object_value != NULL) {
                #ifdef DEBUG
                #ifdef CONDLOG 
                if (__conditional_logging_on) {
                #endif
                printf("Detach property %s:\n", __prop->nullable_value.value.object_value->class_ptr->name);
                #ifdef CONDLOG 
                }
                #endif
                #endif
                __decrease_property_reference_count(__prop->nullable_value.value.object_value);
                __prop->nullable_value.value.object_value = NULL;
            }
        }
        __obj->object_properties.class_object_properties.properties = NULL;

        #ifdef DEBUG
        #ifdef CONDLOG 
        if (__conditional_logging_on) {
        #endif
        printf("Done detaching properties for object of type %s (address: %p, object_id: %d, total object allocation count: %d)\n", __obj->class_ptr->name, __obj, __obj->object_properties.class_object_properties.object_id,  __allocation_count);
        #ifdef CONDLOG 
        }
        #endif
        #endif
    }
}

void __dereference_static_properties() {
    class_static *c = __first_class_static;
    aclass *class_ref = &__class_ref_class_alias;

    #if defined(DEBUG) || defined(TRACKOBJECTS)
    printf("\nStatic properties for all classes dereferenced\n");
    #endif

    __dereference_static_properties_for_class(class_ref->statics);

    while(c != NULL) {
        if (c != class_ref->statics) {
            __dereference_static_properties_for_class(c);
        }
        c = c->next;
    }
}

void __dereference_static_properties_for_class(class_static * const __class_static) {
    #if defined(DEBUG) || defined(TRACKOBJECTS)
    #ifdef CONDLOG 
    if (__conditional_logging_on) {
    #endif
    printf("Dereference static properties of class %s\n", __class_static->name);
    #ifdef CONDLOG 
    }
    #endif
    #endif

    if (__class_static->type == class) { // && __obj->object_properties.class_object_properties.properties != NULL ) {
        for(int i = 0; i < __class_static->static_properties_count; i++) {
            property * const __prop = &__class_static->static_properties[i];
            #if defined(DEBUG) || defined(TRACKOBJECTS)
            char *name = "<null>";
            if (!__is_primitive(__prop->nullable_value) && __prop->nullable_value.value.object_value != NULL) {
                name = __prop->nullable_value.value.object_value->class_ptr->name;
            }
            printf("Dereference static property %d %s\n", i, name);
            #endif
            // TODO: use __decrease_reference_count_nullable_value
            if (!__is_primitive(__prop->nullable_value) && __prop->nullable_value.value.object_value != NULL) {
                __decrease_property_reference_count(__prop->nullable_value.value.object_value);
                __prop->nullable_value.value.object_value = NULL;
            }
        }
    }
}

void __mark_static_properties(class_static * const __class_static) {
    #ifdef DEBUG
    #ifdef CONDLOG 
    if (__conditional_logging_on) {
    #endif
    printf("Mark static properties of class %s\n", __class_static->name);
    #ifdef CONDLOG 
    }
    #endif
    #endif
    
    if (__class_static->type == class) { // && __obj->object_properties.class_object_properties.properties != NULL ) {
        for(int i = 0; i < __class_static->static_properties_count; i++) {
            property * const __prop = &__class_static->static_properties[i];
            // TODO: use __decrease_reference_count_nullable_value
            if (!__is_primitive(__prop->nullable_value) && __prop->nullable_value.value.object_value != NULL) {
                __mark_object(__prop->nullable_value.value.object_value);
            }
        }
    }
}

void print_allocated_objects() {
    #if defined(DEBUG) || defined(TRACKOBJECTS)
    printf("Allocated objects %d\n", __allocation_count);

    if (__allocation_count > 0) {
        if (__first_object != NULL) {
            printf("Going through attached objects:\n");
            aobject *c = __first_object;
            while(c != NULL) {
                printf("Object alive, %s\n", c->class_ptr->name);
                c = c->next;
            }    
        }

        if (__first_detached_object != NULL) {
            printf("Going through detached objects:\n");
            aobject *c = __first_detached_object;
            while(c != NULL) {
                printf("Object alive, %s\n", c->class_ptr->name);
                c = c->next;
            }    
        }
    }

    printf("List of allocated objects:\n");
    for(int i = 0; i < MAX_ALLOCATIONS; i++) {
        if ( allocations[i] != NULL) {
            printf("Object still alive: %s (address: %p, object_id: %d, property refs: %d, inline refs: %d)\n", allocations[i]->class_ptr->name, allocations[i], allocations[i]->object_properties.class_object_properties.object_id, allocations[i]->property_reference_count, allocations[i]->reference_count);
            __debug_print_string_if_string(allocations[i], "String alive");
 
        }
    }
    printf("End of list of allocated objects\n");
    #endif
}

void clear_allocated_objects() {
    // Generated `main()` calls this first; piggy-back to bring up the
    // thread-safe ARC shared mutex before any allocation happens. The
    // call is idempotent (`__arc_shared_mutex_initialised` guard) so
    // repeat calls or test harnesses are fine.
    __arc_shared_mutex_init();
    #if defined(DEBUG) || defined(TRACKOBJECTS)
    for(int i = 0; i < MAX_ALLOCATIONS; i++) {
        allocations[i] = NULL;
    }
    #endif
}

// ------------------------------------------------------------------
// Thread-safe ARC: wrapper lifecycle.
//
// A wrapper aobject is a lean handle in some thread B's realm pointing
// at a real aobject owned by thread A. The wrapper distinguishes
// itself by `class_ptr == NULL` and stores the real in
// `object_properties.object_wrapper.wrapped_object`. The real's
// `first_object_wrapper` linked list records every live wrapper so we
// can refuse to destroy the real while any wrapper still references
// it.
//
// `__create_wrapper(real)`:
//   - allocates a wrapper aobject (raw calloc, no class)
//   - takes the shared mutex
//   - links a new object_wrapper_entry into real->first_object_wrapper
//   - bumps real->reference_count by 1 (the wrapper holds a ref on the real)
//   - releases mutex
//   - wrapper starts at reference_count = 1, owner = current thread
//
// `__deallocate_wrapper(wrapper)`:
//   - takes the shared mutex
//   - finds and unlinks our entry from real->first_object_wrapper
//   - decs real->reference_count by 1
//   - if real hits 0 + 0, ALSO dispatches the real's destructor
//   - releases mutex
//   - frees the wrapper struct
// ------------------------------------------------------------------

// Codegen-facing helper. Fast path is "same-thread, return as-is";
// slow path mints a wrapper. Same-thread is a single load + compare,
// no allocation. At -O0 we still pay the function-call frame
// overhead, but unlike inlined ternaries this doesn't blow the
// 26 k-line JsBytecodeVm.run frame — function calls reserve a fixed
// per-call stack chunk, not per-call-site stack.
aobject * __wrap_if_foreign(aobject * const __raw) {
    if (__raw == NULL) return NULL;
    // If __raw is already a wrapper, it must already be in the
    // current thread's realm (wrappers don't leave their owner). Pass
    // through. We test this BEFORE owner_thread so we don't deref a
    // wrapper's owner_thread when the wrapper itself is what we want.
    if (__raw->class_ptr == NULL) return __raw;
    if (__raw->owner_thread == __current_thread()) return __raw;
    return __create_wrapper(__raw);
}

aobject * __create_wrapper(aobject * const __realobj) {
    #ifdef WRAPLOG
    fprintf(stderr, "[wrap.create] real=%p class=%s rc=%d propref=%d wrappers=%p\n",
        __realobj,
        __realobj && __realobj->class_ptr ? __realobj->class_ptr->name : "(?)",
        __realobj ? __realobj->reference_count : -1,
        __realobj ? __realobj->property_reference_count : -1,
        __realobj ? (void*)__realobj->first_object_wrapper : NULL);
    fflush(stderr);
    #endif
    // First wrapper ever? Flip the global so __unwrap stops being a
    // free-zero-instruction identity. Set BEFORE the wrapper is
    // visible to any other thread — the wrapper-list mutex below
    // provides the release-side synchronisation that publishes both
    // this flag and the new wrapper entry together.
    __amlc_any_wrappers_alive = true;

    // Allocate the wrapper aobject itself. It has no class (class_ptr
    // stays NULL — that's the discriminator) and zero properties, so
    // the base aobject struct alone is enough.
    aobject * __wrapper = (aobject *) calloc(1, sizeof(aobject));
    if (__wrapper == NULL) return NULL;
    __wrapper->class_ptr = NULL;                         // wrapper sentinel
    __wrapper->reference_count = 1;
    __wrapper->property_reference_count = 0;
    __wrapper->owner_thread = __current_thread();
    __wrapper->object_properties.object_wrapper.wrapped_object = __realobj;

    // Build the subscription entry the real will hold on to.
    object_wrapper_entry * __entry =
        (object_wrapper_entry *) calloc(1, sizeof(object_wrapper_entry));
    if (__entry == NULL) { free(__wrapper); return NULL; }
    __entry->subscriber = __wrapper;
    __entry->next = NULL;

    // Link into the real's wrapper list under the shared mutex.
    //
    // NOTE: we do NOT bump real->reference_count anymore — wrappers are
    // tracked separately via `first_object_wrapper`. The owner's
    // reference_count stays solely "owner-thread local refs". This
    // splits ownership so owner-thread inc/dec on its own count doesn't
    // need the lock, except on the 1→0 transition (see
    // __decrease_reference_count). End-of-life coordination is via the
    // `owner_gone` flag set by whoever drains rc+propref to zero first.
    __arc_shared_lock();
    __entry->next = __realobj->first_object_wrapper;
    __realobj->first_object_wrapper = __entry;
    __arc_shared_unlock();

    return __wrapper;
}

void __deallocate_wrapper(aobject * const __wrapper) {
    aobject * const __realobj =
        __wrapper->object_properties.object_wrapper.wrapped_object;
    #ifdef WRAPLOG
    fprintf(stderr, "[wrap.dealloc] wrapper=%p real=%p real_class=%s real_rc=%d real_propref=%d real_owner_gone=%d\n",
        __wrapper, __realobj,
        __realobj && __realobj->class_ptr ? __realobj->class_ptr->name : "(?)",
        __realobj ? __realobj->reference_count : -1,
        __realobj ? __realobj->property_reference_count : -1,
        __realobj ? __realobj->owner_gone : -1);
    fflush(stderr);
    #endif

    // Unlink self under the shared mutex. The unlink walks the
    // singly-linked list looking for the entry whose `subscriber`
    // matches `__wrapper`. The list is typically short (number of
    // threads currently borrowing the object — usually 1-3) so the
    // linear walk is fine.
    __arc_shared_lock();
    object_wrapper_entry * prev = NULL;
    object_wrapper_entry * cur = __realobj->first_object_wrapper;
    while (cur != NULL) {
        if (cur->subscriber == __wrapper) {
            if (prev == NULL) {
                __realobj->first_object_wrapper = cur->next;
            } else {
                prev->next = cur->next;
            }
            free(cur);
            break;
        }
        prev = cur;
        cur = cur->next;
    }

    // Real-destruction protocol (new):
    //   - Wrappers do NOT contribute to real->reference_count anymore.
    //   - Whoever drains rc+propref to zero on the owner side sets
    //     `owner_gone = true` UNDER THIS SAME LOCK if any wrappers
    //     still subscribe; otherwise they destroy outright.
    //   - On wrapper death (here), if we were the last subscriber AND
    //     the owner has already flagged owner_gone, we finalize.
    // This means a typical wrapper death only locks long enough to
    // unsubscribe — no owner-side counter mutation, no decrement-then-
    // check ping-pong.
    bool destroy_real = (__realobj->first_object_wrapper == NULL
                      && __realobj->owner_gone
                      && !__realobj->destruction_claimed);
    if (destroy_real) {
        __realobj->destruction_claimed = true;
    }
    __arc_shared_unlock();

    if (destroy_real) {
        __deallocate_object(__realobj);
    }

    free(__wrapper);
}

void deallocate_annotations(class_static * const __class_static) {
    for(int i = 0; i < __class_static->annotations_count; i++) {
        aobject * const a = __class_static->annotations[i];
        __decrease_reference_count(a);
        __class_static->annotations[i] = NULL;
    }
}
void __throw_exception(function_result *result, aobject * const exception, aobject * const stack_trace_item_text) {

//    exception_holder * const holder = (exception_holder *) calloc(1, sizeof(exception_holder));
//    holder->exception = exception;
//    holder->first_stack_trace_item = __create_stack_trace_item(NULL, stack_trace_item_text);
//    holder->last_stack_trace_item  = holder->first_stack_trace_item;

    __add_stack_trace_item_function_alias(exception, stack_trace_item_text);

//    result.has_return_value = 0;
    if (result->exception) {
        __decrease_reference_count(result->exception);
    }
    result->exception = exception;
    __increase_reference_count(exception);
}

void __pass_exception(function_result *result, aobject * const exception, aobject * const stack_trace_item_text) {

    __add_stack_trace_item_function_alias(exception, stack_trace_item_text);
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
        ctype type =__value_flags_to_ctype(a.flags);

        switch(type) {
            case char_type:
            case short_type:
            case int_type:
            case long_type:
    #ifdef FEATURE_FLOATING_POINT
            case float_type:
            case double_type:
    #endif
            case bool_type:
            case uchar_type:
            case ushort_type:
            case uint_type:
            case ulong_type:
                return false;
            case object_type:
            default:
                return a.value.object_value == NULL;

        }
    }
}

bool primitive_or_object_equals(const value a, const value b, const ctype type) {
    if (type == bool_type) {
        // boolean comparison
        return a.bool_value == b.bool_value;
    } else if (type == char_type) {
        // char comparison
        return a.char_value == b.char_value;
    } else if (type == short_type) {
        // short comparison
        return a.short_value == b.short_value;
    } else if (type == int_type) {
        // int comparison
        return a.int_value == b.int_value;
    } else if (type == long_type) {
        // long comparison
        return a.long_value == b.long_value;
    } else if (type == uchar_type) {
        // uchar comparison
        return a.uchar_value == b.uchar_value;
    } else if (type == ushort_type) {
        // ushort comparison
        return a.ushort_value == b.ushort_value;
    } else if (type == uint_type) {
        // uint comparison
        return a.uint_value == b.uint_value;
    } else if (type == ulong_type) {
        // ulong comparison
        return a.ulong_value == b.ulong_value;
#ifdef FEATURE_FLOATING_POINT
    } else if (type == float_type) {
        // float comparison
        return a.float_value == b.float_value;
    } else if (type == double_type) {
        // double comparison
        return a.double_value == b.double_value;
#endif
    } else if (a.object_value == b.object_value) {
        return true;
    } else if (a.object_value != NULL && b.object_value != NULL) {
        // both not null
        return __object_equals(a.object_value, b.object_value);
    } else {
        return false;
        // one is null, the other not
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
                    return primitive_or_object_equals(a.value, b.value, __value_flags_to_ctype(a.flags));
//                    return memcmp(&a.value, &b.value, sizeof(value)); // TODO: let's hope there's no garbage here
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

            ctype at = __value_flags_to_ctype(a.flags);
            ctype bt = __value_flags_to_ctype(b.flags);
            if (at != bt) {
                return false;
            }

            return primitive_or_object_equals(a.value, b.value, at);
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

// Zero-init contract for `__create_array`:
//
//   For `any_type` arrays (element = `nullable_value`), a slot of all zeros
//   must read as "absent object". Two invariants make that true today:
//     1. `object_type == 0` in the `ctype` enum.
//     2. `flags = 0` doesn't match any `PRIMITIVE_X` bit pattern in
//        `__value_flags_to_ctype`, so it falls through to `object_type`.
//        Every `PRIMITIVE_X` constant carries the high `PRIMITIVE = 128`
//        bit (or PRIMITIVE_BOOL = 64 | PRIMITIVE), so an all-zero byte
//        never collides with a primitive tag.
//
//   We pin both invariants with `_Static_assert` so a future flag-bit
//   reshuffle or enum reorder breaks the build rather than silently
//   producing arrays whose slots read as garbage primitives.
//
//   For non-`any_type` arrays the slots are plain bytes (pointer for
//   `object_type`, primitive value otherwise). Zero-init is the right
//   default there too: NULL pointer for objects, 0 for primitives.
//
// The explicit `memset` below makes the zero-init visible at the call
// site instead of relying on `__allocate_object_with_extra_size` using
// `calloc` internally — that's an implementation detail of the
// allocator, not a documented part of the array contract.
_Static_assert(object_type == 0, "any_type arrays rely on object_type==0 for zero-init slots to read as absent");
_Static_assert((PRIMITIVE & 0xFC) != 0, "every PRIMITIVE_X tag must have a non-zero high bit so flags=0 reads as object_type");

aobject * __create_array(unsigned int const size, unsigned char const item_size, aclass * const array_class, ctype const ctype) {
    size_t extra_size = sizeof(array_holder) + (size * item_size);
    aobject * array_obj = __allocate_object_with_extra_size(array_class, extra_size);
    array_holder * const holder = (array_holder *) &array_obj[1];
    void *array_data = (void *) (holder + 1);
    array_obj->object_properties.class_object_properties.object_data.value.custom_value = holder;
    holder->array_data = array_data;
    holder->ctype = ctype;
    holder->item_class = array_class;
    holder->item_size = item_size;
    holder->size = size;
    // Explicit zero-init of the data block. See the contract comment above
    // `__create_array`. For `any_type` this materialises each slot as the
    // "absent object" `nullable_value`; for other ctypes it's NULL pointers
    // / zero primitives, which is what the AmLang language model expects.
    memset(array_data, 0, (size_t) size * item_size);
    return array_obj;
}

array_holder * get_array_holder(aobject * const array_obj) {
    return (array_holder *) &array_obj[1];
}

char * get_array_data(array_holder * holder) {
    return (char *) &holder[1];
}

aobject * __create_exception(aobject * const message) {
    aobject *ex = __allocate_object(&__exception_class_alias);
    __exception_constructor_alias(ex, message);
    __exception_init_instance_function_alias((nullable_value){ .value.object_value = ex });
    return ex;
}

void __throw_simple_exception(const char * const message, const char * const stack_trace_item_text, function_result * const result) {
    aobject * ex_msg = __create_string_constant(message, &__string_class_alias);
    aobject * stit = __create_string_constant(stack_trace_item_text, &__string_class_alias);
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
    aobject * property_info = __allocate_object_with_extra_size(&__property_info_class_alias, sizeof(cls));
    property *properties = (property *) (property_info + 1);
    aclass ** class_holder_ptr = (aclass **) (properties + 2); // given that PropertyInfo has exactly 2 properties
    *class_holder_ptr = cls;

    __property_info_constructor_alias(property_info);
    aobject * property_name = __create_string_constant(name, &__string_class_alias);
    __set_property(property_info, Am_Lang_PropertyInfo_P_name, (nullable_value) { .flags = 0, .value.object_value = property_name});
    __set_property(property_info, Am_Lang_PropertyInfo_P_index, (nullable_value) { .flags = PRIMITIVE_UCHAR, .value.uchar_value = index });
	property_infos[index] = property_info;
    if (__is_suspicious_object_ptr(property_info)) {
        printf("[create_property_info] SUSPICIOUS index=%d name=%s value=%p\n",
            index, name, (void*)property_info);
        fflush(stdout);
        exit(0);
    }
    __increase_property_reference_count(property_info);
    __decrease_reference_count(property_info);
    __decrease_reference_count(property_name);
}

aclass * const get_class_from_any(nullable_value const value) {
    ctype type =__value_flags_to_ctype(value.flags);

    switch(type) {
        case char_type:
            return &Am_Lang_Byte;
        case short_type:
            return &Am_Lang_Short;
        case int_type:
            return &Am_Lang_Int;
        case long_type:
            return &Am_Lang_Long;
#ifdef FEATURE_FLOATING_POINT
        case float_type:
            return &Am_Lang_Float;
        case double_type:
            return &Am_Lang_Double;
#endif
        case bool_type:
            return &Am_Lang_Bool;
        case uchar_type:
            return &Am_Lang_UByte;
        case ushort_type:
            return &Am_Lang_UShort;
        case uint_type:
            return &Am_Lang_UInt;
        case ulong_type:
            return &Am_Lang_ULong;
        case object_type:
            return value.value.object_value->class_ptr;
        default:
            return &Am_Lang_Any;

    }

/*
    if (flag & PRIMITIVE_LONG == PRIMITIVE_LONG) {
        return &Am_Lang_Long;
    } else if (flag & PRIMITIVE_INT == PRIMITIVE_INT) {
        return &Am_Lang_Int;
    } else if (flag & PRIMITIVE_SHORT == PRIMITIVE_SHORT) {
        return &Am_Lang_Short;
    } else if (flag & PRIMITIVE_BYTE == PRIMITIVE_BYTE) {
        return &Am_Lang_Byte;
    } else if (flag & PRIMITIVE_BOOL == PRIMITIVE_BOOL) {
        return &Am_Lang_Bool;
//    } else if (flag & PRIMITIVE_FLOAT == PRIMITIVE_FLOAT) {
//        return &Am_Lang_Float;
//    } else if (flag & PRIMITIVE_DOUBLE == PRIMITIVE_DOUBLE) {
//        return &Am_Lang_Double;
    } else if (flag & PRIMITIVE_UBYTE == PRIMITIVE_UBYTE) {
        return &Am_Lang_UByte;
    } else if (flag & PRIMITIVE_USHORT == PRIMITIVE_USHORT) {
        return &Am_Lang_UShort;
    } else if (flag & PRIMITIVE_UINT == PRIMITIVE_UINT) {
        return &Am_Lang_UInt;
    } else if (flag & PRIMITIVE_ULONG == PRIMITIVE_ULONG) {
        return &Am_Lang_ULong;
    } else if (flag == 0) {
        return value.value.object_value->class_ptr;
    } else {
        #ifdef DEBUG
        printf("Unknown type %d\n", flag);
        #endif
        return NULL;
    }
        */
}

aobject * __concatenate_strings(int count, ...) {
    va_list args;
    va_list args_copy;
    size_t total_length;
    aobject* result_obj;
    string_holder* result_holder;
    char* result_str;
    int i;
    
    va_start(args, count);
    
    /* Calculate total length needed */
    total_length = 0;
    va_copy(args_copy, args);
    
    for (i = 0; i < count; i++) {
        aobject* str_obj = va_arg(args_copy, aobject*);
        if (str_obj != NULL) {
            string_holder* holder = (string_holder*)str_obj->object_properties.class_object_properties.object_data.value.custom_value;
            if (holder != NULL) {
                total_length += holder->length;
            }
        }
    }
    va_end(args_copy);
    
    /* Allocate new string object with extra space for the concatenated string */
    result_obj = __allocate_object_with_extra_size(&__string_class_alias, sizeof(string_holder) + total_length + 1);
    if (result_obj == NULL) {
        va_end(args);
        return NULL;
    }
    
    /* Set up the string holder */
    result_holder = (string_holder*)(result_obj + 1);
    result_str = (char*)(result_holder + 1);
    result_obj->object_properties.class_object_properties.object_data.value.custom_value = result_holder;
    
    /* Initialize the result string buffer */
    result_str[0] = '\0';
    
    /* Concatenate all strings */
    for (i = 0; i < count; i++) {
        aobject* str_obj = va_arg(args, aobject*);
        if (str_obj != NULL) {
            string_holder* holder = (string_holder*)str_obj->object_properties.class_object_properties.object_data.value.custom_value;
            if (holder != NULL && holder->string_value != NULL) {
                strcat(result_str, holder->string_value);
            }
        }
    }
    va_end(args);
    
    /* Set up the string holder fields */
    result_holder->is_string_constant = 0; /* false */
    result_holder->length = total_length;
    result_holder->string_value = result_str;
    result_holder->hash = __string_hash(result_str);
    
    return result_obj;
}
