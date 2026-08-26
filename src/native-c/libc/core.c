// #include <stdlib.h>
#include <libc/core.h>
#include <string.h>
#include <stdarg.h>
#if defined(__linux__)
#include <execinfo.h>   // backtrace() for the AMLC_WRAP_TRACE diagnostics
#endif

// Thread-safe ARC platform layer. Linux/macOS use a recursive
// pthread_mutex (recursive so a single thread can take the lock more
// than once while nested inside ARC machinery — destructors that
// recursively dec refs would otherwise self-deadlock). AmigaOS is
// single-core, so the lock is an inline Forbid/Permit (raw TDNestCnt
// bump) — that IS mutual exclusion against other tasks there, and it
// nests naturally (matching the recursive mutex) since TDNestCnt is a
// counter. See the atomic-macro block in core.h for the rationale.
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
    {
        const char *e = getenv("AMLC_XTHREAD_RC");
        __amlc_xthread_rc_on = (e && atoi(e) != 0) ? 1 : 0;
    }
}
void __arc_shared_lock(void)   { pthread_mutex_lock(&__arc_shared_mutex); }
void __arc_shared_unlock(void) { pthread_mutex_unlock(&__arc_shared_mutex); }
void * __current_thread(void)  { return (void *) pthread_self(); }
#elif defined(__MORPHOS__)
// MorphOS PPC: ExecBase is opaque, so no raw TDNestCnt (see core.h). Forbid()/
// Permit() library calls give the same task-level mutual exclusion for the ARC
// critical sections, and they nest. Thread identity is FindTask(NULL).
#include <proto/exec.h>
void __arc_shared_mutex_init(void) {}
void __arc_shared_lock(void)   { Forbid(); }
void __arc_shared_unlock(void) { Permit(); }
void * __current_thread(void)  { return (void *) FindTask(NULL); }
#else
// AmigaOS: single-core, so mutual exclusion for the ARC critical sections
// (the multi-field free decisions, propref mutations) is an inline
// Forbid/Permit — raw increment/decrement of Exec's task-switch nesting
// counter, exactly what Forbid()/Permit() do minus the library-vector call.
// Balanced ++/-- so nesting returns TDNestCnt to baseline; the destructor
// itself always runs AFTER unlock (see the dec paths), so Forbid is not held
// across arbitrary release callbacks — except the __set_property → propref-dec
// nesting, where a triggered destructor still runs under the outer lock (same
// as the pthread build holds the mutex there; documented follow-up to defer).
// The thread identity is FindTask(NULL) (stable for the task's lifetime).
#include <proto/exec.h>
void __arc_shared_mutex_init(void) {}
void __arc_shared_lock(void)   { SysBase->TDNestCnt++; }  // inline Forbid()
void __arc_shared_unlock(void) { SysBase->TDNestCnt--; }  // inline raw Permit()
void * __current_thread(void)  { return (void *) FindTask(NULL); }
#endif
#include <Am/Lang/Exception.h>
#include <Am/Lang/Object.h>
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
#ifndef AM_SINGLE_THREADED
bool __amlc_any_wrappers_alive = false;
#endif

// Thread-safe ARC (BRC) — see core.h. Flipped on the first time a thread is
// spawned (Am_Threading_Thread_start_0). Gates the owner-vs-foreign branch in
// the refcount helpers so single-threaded programs never call
// __current_thread() on the hot path. One-way.
#ifndef AM_SINGLE_THREADED
bool __amlc_multithreaded = false;
#endif

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

// Atomic: allocation/deallocation happen concurrently on several threads
// under BRC; a plain int loses updates (test-diagnostic accuracy only).
__amlc_atomic_int __allocation_count = 0;
// Cross-thread wrapper diagnostics (always on, cheap). Live wrappers =
// created - deallocated; if it climbs without bound, wrappers are leaking.
long __wrapper_create_count = 0;
long __wrapper_dealloc_count = 0;
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

// Defined under TRACKOBJECTS too (not just DEBUG): print_allocated_objects()
// is compiled for both and calls this, so a TRACKOBJECTS-only build (e.g. the
// `instances(Class)` test intrinsic) would otherwise fail to link.
#if defined(DEBUG) || defined(TRACKOBJECTS)
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

    // Fallback: match by iface CLASS (same class_static) when the exact
    // variant pointer misses — e.g. shared generic code wrapping against
    // one variant of an interface while the object's table names another
    // variant of the SAME interface. Safe: the table found is the object's
    // OWN implementation table, so dispatch stays self-consistent.
    if (impl == NULL) {
        for(int i = 0; i < implementation_object->class_ptr->iface_implementation_count; i++) {
            iface_implementation * impl2 = &implementation_object->class_ptr->iface_implementations[i];
            if (impl2->iface_class != NULL && impl2->iface_class->statics == __class->statics) {
                impl = impl2;
                break;
            }
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

#if defined(__linux__)
// Opt-in object-leak forensics (AMLC_OBJ_TRACE=1): per-class NET live
// aobject counts, dumped every ~50k allocations. Same idea as the
// wrapper-site table — a climbing net names the leaking class directly.
static int __obj_trace_on = -1;
#define OBJ_TRACE_SLOTS 4096
static struct { void * cls; const char * name; long net; } __obj_trace_tab[OBJ_TRACE_SLOTS];
static long __obj_trace_alloc_total = 0;
static pthread_mutex_t __obj_trace_mutex = PTHREAD_MUTEX_INITIALIZER;

static void __obj_trace_bump(aclass * cls, int delta) {
    if (cls == NULL) return;
    pthread_mutex_lock(&__obj_trace_mutex);
    unsigned long h = ((unsigned long) cls >> 3) % OBJ_TRACE_SLOTS;
    for (int i = 0; i < OBJ_TRACE_SLOTS; i++) {
        unsigned long idx = (h + i) % OBJ_TRACE_SLOTS;
        if (__obj_trace_tab[idx].cls == (void *) cls) {
            __obj_trace_tab[idx].net += delta;
            break;
        }
        if (__obj_trace_tab[idx].cls == NULL) {
            if (delta > 0) {
                __obj_trace_tab[idx].cls = (void *) cls;
                __obj_trace_tab[idx].name = cls->name;
                __obj_trace_tab[idx].net = delta;
            }
            break;
        }
    }
    if (delta > 0 && (++__obj_trace_alloc_total % 50000) == 0) {
        fprintf(stderr, "[objtrace] ---- top net-live classes (allocs: %ld, live: %d) ----\n",
            __obj_trace_alloc_total, __allocation_count);
        // Sample the propref'd-object list: any leaked object with
        // property_reference_count > 0 is linked here. Print the first few
        // whose class name matches AMLC_OBJ_TRACE_CLASS (if set) with their
        // counters — tells us whether the pin is rc or propref.
        {
            const char *want = getenv("AMLC_OBJ_TRACE_CLASS");
            if (want != NULL) {
                __arc_shared_lock();
                int shown = 0;
                long matched = 0;
                for (aobject *o = __first_object; o != NULL && matched < 100000; o = o->next) {
                    if (o->class_ptr != NULL && strstr(o->class_ptr->name, want) != NULL) {
                        matched++;
                        if (shown < 5) {
                            fprintf(stderr, "[objtrace]   sample %s rc=%d propref=%d owner_gone=%d wrappers=%p\n",
                                o->class_ptr->name, o->reference_count,
                                o->property_reference_count, o->owner_gone,
                                (void *) o->first_object_wrapper);
                            shown++;
                        }
                    }
                }
                fprintf(stderr, "[objtrace]   (%ld '%s' objects on the propref list)\n", matched, want);
                __arc_shared_unlock();
            }
        }
        for (int pass = 0; pass < 10; pass++) {
            long best = 0; int bi = -1;
            for (int i = 0; i < OBJ_TRACE_SLOTS; i++) {
                if (__obj_trace_tab[i].cls != NULL && __obj_trace_tab[i].net > best) {
                    best = __obj_trace_tab[i].net; bi = i;
                }
            }
            if (bi < 0) break;
            fprintf(stderr, "[objtrace]   net=%-8ld %s\n", __obj_trace_tab[bi].net, __obj_trace_tab[bi].name);
            __obj_trace_tab[bi].net = -__obj_trace_tab[bi].net;
        }
        for (int i = 0; i < OBJ_TRACE_SLOTS; i++) {
            if (__obj_trace_tab[i].net < 0) __obj_trace_tab[i].net = -__obj_trace_tab[i].net;
        }
        fflush(stderr);
    }
    pthread_mutex_unlock(&__obj_trace_mutex);
}
#endif

aobject * __allocate_object_with_extra_size(aclass * const __class, size_t extra_size) {
    __amlc_atomic_fetch_add(&__allocation_count, 1);
    #if defined(__linux__)
    if (__obj_trace_on == -1) {
        const char *e = getenv("AMLC_OBJ_TRACE");
        __obj_trace_on = (e && atoi(e) != 0) ? 1 : 0;
    }
    if (__obj_trace_on == 1) __obj_trace_bump(__class, 1);
    #endif

    #if defined(DEBUG) || defined(TRACKOBJECTS)
    __last_object_id++;
    #endif
    #ifdef TRACKOBJECTS
    // Per-class live-instance counter feeding the `instances(Class)`
    // test intrinsic. Balanced by the decrement in __deallocate_object.
    if (__class != NULL) __amlc_atomic_fetch_add(&__class->instance_count, 1);
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
            // Debug registry is a FIXED array — saturate instead of writing
            // past it. An unchecked `allocations[allocation_index++]` overflowed
            // the global once a process allocated >MAX_ALLOCATIONS objects
            // (e.g. the V2 compiler compiling a whole project in-process),
            // corrupting the neighbouring globals and eventually the heap.
            // Objects beyond capacity simply aren't tracked (shutdown dump
            // misses them) — a debug-feature limitation, not a correctness one.
            if (allocation_index < MAX_ALLOCATIONS) {
                allocations[allocation_index] = __obj;
            } else if (allocation_index == MAX_ALLOCATIONS) {
                printf("TRACKOBJECTS: allocation registry full (%d) — further objects untracked\n", MAX_ALLOCATIONS);
            }
            allocation_index++;
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

    // Wrapper (class_ptr == NULL): it was linked into __first_object by its
    // property_reference_count (a cross-thread object stored into a
    // collection/property), so the cycle sweep can reach it. It must NOT go
    // through the real-object path — __detach_object immediately derefs
    // class_ptr->release, and __deallocate_detached_object would free it raw,
    // leaving a dangling entry in the real's first_object_wrapper list.
    // Unlink it here and hand it to __deallocate_wrapper, which unsubscribes
    // from the real and frees the wrapper. This mirrors the class_ptr==NULL
    // split already used in the dec-refcount paths. Safe to run mid-sweep:
    // reals detached earlier this pass aren't freed until the second loop.
    if (__obj->class_ptr == NULL) {
        aobject *next_to_sweep = __obj->next;
        if (__obj == __first_object) {
            __set_first_object(__obj->next, "detach_wrapper_from_sweep");
            if (__first_object != NULL) {
                __first_object->prev = NULL;
            }
        } else {
            __obj->prev->next = __obj->next;
            if (__obj->next != NULL) {
                __obj->next->prev = __obj->prev;
            }
        }
        __obj->next = NULL;
        __obj->prev = NULL;
        __deallocate_wrapper(__obj);
        return (sweep_result) { .is_swept = true, .next = next_to_sweep };
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

    __amlc_atomic_fetch_sub(&__allocation_count, 1);
    #if defined(__linux__)
    if (__obj_trace_on == 1) __obj_trace_bump(__obj->class_ptr, -1);
    #endif

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

    #ifdef TRACKOBJECTS
    // Balance the increment in __allocate_object_with_extra_size so the
    // `instances(Class)` test intrinsic reflects the live count.
    if (__obj != NULL && __obj->class_ptr != NULL) __amlc_atomic_fetch_sub(&__obj->class_ptr->instance_count, 1);
    #endif

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

    __amlc_atomic_fetch_sub(&__allocation_count, 1);
    #if defined(__linux__)
    if (__obj_trace_on == 1) __obj_trace_bump(__obj->class_ptr, -1);
    #endif

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
                // Same end-of-life decision as the rc → 0 path. Under BRC the
                // "foreign holders still exist?" test is `foreign_reference_count
                // != 0` (the wrapper subscription list is always empty now, but
                // the check is retained so stale wrapper state, if any, is still
                // honoured). If foreign refs remain, set `owner_gone` and let the
                // last foreign release finalise. Otherwise claim destruction now
                // via `destruction_claimed` — NOT `pending_deallocation` (that's
                // __detach_object's recursion guard). This whole block already
                // runs under __arc_shared_lock, so the foreign-count read
                // linearises with foreign decrements.
                if (__obj->first_object_wrapper == NULL
                        && __amlc_atomic_load(&__obj->foreign_reference_count) == 0) {
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
#ifndef AM_SINGLE_THREADED
aobject * __wrap_if_foreign(aobject * const __raw) {
    // Thread-safe ARC (BRC): wrappers are gone. Cross-thread references now use
    // the SAME real pointer, and foreign liveness is tracked via
    // `foreign_reference_count` on the OWNED retain paths (__retain_slot_read /
    // __increase_reference_count). This site is a BORROW — codegen emits no
    // paired release for it (see RenderHelper.kt; same-thread was already a
    // zero-cost identity), so it must NOT touch any counter. Bumping here would
    // leak; the pre-BRC same-thread path already returned the raw unchanged.
    // Return the pointer as-is.
    return __raw;
}
#endif


// AMLC_XTHREAD_RC=1: report reference_count mutations performed by a
// thread that does NOT own the object. reference_count is owner-thread-
// local BY DESIGN (plain non-atomic ++/--); any cross-thread mutation is
// a race that can lose updates and leave rc permanently pinned above
// zero — the object then never destructs (the "chunks leak with rc>0,
// propref=0, no wrappers" signature). Reports the mutating call site as
// a module-relative PC (addr2line -e app), rate-limited.
int __amlc_xthread_rc_on = 0;
#if defined(__linux__)
void __report_xthread_rc(aobject * const __obj, const char * const op) {
    static long __xthread_rc_reports = 0;
    if (__xthread_rc_reports >= 200) return;
    __xthread_rc_reports++;
    extern char __executable_start;
    void *bt[4];
    int n = backtrace(bt, 4);
    fprintf(stderr, "[xthread-rc] %s on %s owner=%p me=%p rc=%d propref=%d",
        op,
        __obj->class_ptr ? __obj->class_ptr->name : "(wrapper)",
        __obj->owner_thread, __current_thread(),
        __obj->reference_count, __obj->property_reference_count);
    for (int i = 1; i < n; i++) {
        fprintf(stderr, " pc=+0x%lx", (unsigned long) bt[i] - (unsigned long) &__executable_start);
    }
    fprintf(stderr, "\n");
    fflush(stderr);
}
#else
void __report_xthread_rc(aobject * const __obj, const char * const op) { (void)__obj; (void)op; }
#endif

#if defined(__linux__)
// AMLC_WRAP_TRACE support: per-call-site NET live-wrapper accounting.
// Fixed-size open-addressing table keyed by minting PC; mutated only
// under __arc_shared_lock (both create and dealloc already hold it).
int __wrap_trace_on = -1;                       // -1 unread, 0 off, 1 on
static __thread void * __wrap_trace_current_site = NULL;
// Generated code has thousands of distinct minting PCs — an undersized
// table silently drops late-arriving sites (including the leaky ones),
// making every visible site look balanced while the global live count
// climbs. 64k slots covers any realistic program.
#define WRAP_TRACE_SLOTS 65536
static struct { void * site; long net; long created; } __wrap_trace_tab[WRAP_TRACE_SLOTS];
static long __wrap_trace_created_total = 0;
static long __wrap_trace_dropped = 0;           // sites lost to a full table
// Companion histogram keyed by wrapped-real CLASS — tells us what TYPE of
// object the leaked wrappers pin even when the PC table can't.
#define WRAP_TRACE_CLASS_SLOTS 1024
static struct { void * cls; const char * name; long net; } __wrap_trace_cls_tab[WRAP_TRACE_CLASS_SLOTS];

static void __wrap_trace_bump_class(aobject * real, int delta) {
    void * cls = real ? (void *) real->class_ptr : NULL;
    if (cls == NULL) return;
    unsigned long h = ((unsigned long) cls >> 3) % WRAP_TRACE_CLASS_SLOTS;
    for (int i = 0; i < WRAP_TRACE_CLASS_SLOTS; i++) {
        unsigned long idx = (h + i) % WRAP_TRACE_CLASS_SLOTS;
        if (__wrap_trace_cls_tab[idx].cls == cls) {
            __wrap_trace_cls_tab[idx].net += delta;
            return;
        }
        if (__wrap_trace_cls_tab[idx].cls == NULL) {
            if (delta > 0) {
                __wrap_trace_cls_tab[idx].cls = cls;
                __wrap_trace_cls_tab[idx].name = ((aclass *) cls)->name;
                __wrap_trace_cls_tab[idx].net = delta;
            }
            return;
        }
    }
}

// Find/insert the slot for a site. Must be called under __arc_shared_lock.
static void __wrap_trace_bump(void * site, int delta) {
    if (site == NULL) return;
    unsigned long h = ((unsigned long) site >> 2) % WRAP_TRACE_SLOTS;
    for (int i = 0; i < WRAP_TRACE_SLOTS; i++) {
        unsigned long idx = (h + i) % WRAP_TRACE_SLOTS;
        if (__wrap_trace_tab[idx].site == site) {
            __wrap_trace_tab[idx].net += delta;
            if (delta > 0) __wrap_trace_tab[idx].created++;
            return;
        }
        if (__wrap_trace_tab[idx].site == NULL) {
            if (delta > 0) {
                __wrap_trace_tab[idx].site = site;
                __wrap_trace_tab[idx].net = delta;
                __wrap_trace_tab[idx].created = 1;
            }
            return;
        }
    }
    __wrap_trace_dropped++;    // table full — surfaced in the dump header
}

// Dump the sites with the highest net (still-alive) wrapper counts.
// PCs are printed relative to the main module so `addr2line -e app`
// resolves them despite PIE/ASLR.
static void __wrap_trace_dump(void) {
    extern char __executable_start;             // ld-provided module base
    unsigned long base = (unsigned long) &__executable_start;
    fprintf(stderr, "[wraptrace] ---- top net-live wrapper sites (total created: %ld, live now: %ld, dropped: %ld) ----\n",
        __wrap_trace_created_total, __wrapper_create_count - __wrapper_dealloc_count, __wrap_trace_dropped);
    for (int pass = 0; pass < 6; pass++) {      // top wrapped-real classes by net
        long best = 0; int bi = -1;
        for (int i = 0; i < WRAP_TRACE_CLASS_SLOTS; i++) {
            if (__wrap_trace_cls_tab[i].cls != NULL && __wrap_trace_cls_tab[i].net > best) {
                best = __wrap_trace_cls_tab[i].net; bi = i;
            }
        }
        if (bi < 0) break;
        fprintf(stderr, "[wraptrace]   class net=%-8ld %s\n",
            __wrap_trace_cls_tab[bi].net, __wrap_trace_cls_tab[bi].name);
        __wrap_trace_cls_tab[bi].net = -__wrap_trace_cls_tab[bi].net;
    }
    for (int i = 0; i < WRAP_TRACE_CLASS_SLOTS; i++) {
        if (__wrap_trace_cls_tab[i].net < 0) __wrap_trace_cls_tab[i].net = -__wrap_trace_cls_tab[i].net;
    }
    for (int pass = 0; pass < 8; pass++) {
        long best = 0; int bi = -1;
        for (int i = 0; i < WRAP_TRACE_SLOTS; i++) {
            if (__wrap_trace_tab[i].site != NULL && __wrap_trace_tab[i].net > best) {
                best = __wrap_trace_tab[i].net; bi = i;
            }
        }
        if (bi < 0) break;
        fprintf(stderr, "[wraptrace]   net=%-8ld created=%-10ld pc=+0x%lx\n",
            __wrap_trace_tab[bi].net, __wrap_trace_tab[bi].created,
            (unsigned long) __wrap_trace_tab[bi].site - base);
        __wrap_trace_tab[bi].net = -__wrap_trace_tab[bi].net;   // mark visited
    }
    for (int i = 0; i < WRAP_TRACE_SLOTS; i++) {                // restore marks
        if (__wrap_trace_tab[i].net < 0) __wrap_trace_tab[i].net = -__wrap_trace_tab[i].net;
    }
    fflush(stderr);
}
#endif

#ifndef AM_SINGLE_THREADED
aobject * __create_wrapper(aobject * const __realobj) {
    #if defined(__linux__)
    // Opt-in wrapper-leak forensics (AMLC_WRAP_TRACE=1): records the minting
    // call-site PC in each wrapper and keeps a per-site NET count
    // (created - freed). Sites whose net keeps climbing are the leaks —
    // creation-rate histograms can't tell leaked wrappers from the (many)
    // properly released ones. Dumps the top sites every ~1M creations as
    // "app-relative" PCs, resolvable with `addr2line -e app <pc-base>`.
    // Costs one getenv on the first call and nothing when unset.
    {
        if (__wrap_trace_on == -1) {
            const char *e = getenv("AMLC_WRAP_TRACE");
            __wrap_trace_on = (e && atoi(e) != 0) ? 1 : 0;
        }
        if (__wrap_trace_on == 1) {
            void *bt[3];
            int n = backtrace(bt, 3);
            __wrap_trace_current_site = (n >= 3) ? bt[2] : (n >= 2 ? bt[1] : NULL);
        }
    }
    #endif
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
    #if defined(__linux__)
    __wrapper->object_properties.object_wrapper.trace_site =
        (__wrap_trace_on == 1) ? __wrap_trace_current_site : NULL;
    #endif

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
    __wrapper_create_count++;
    #if defined(__linux__)
    if (__wrap_trace_on == 1) {
        __wrap_trace_bump(__wrapper->object_properties.object_wrapper.trace_site, 1);
        __wrap_trace_bump_class(__realobj, 1);
        __wrap_trace_created_total++;
        if ((__wrap_trace_created_total % 1000000) == 0) {
            __wrap_trace_dump();
        }
    }
    #endif
    __arc_shared_unlock();

    return __wrapper;
}
#endif // AM_SINGLE_THREADED

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
    #if defined(__linux__)
    if (__wrap_trace_on == 1) {
        __wrap_trace_bump(__wrapper->object_properties.object_wrapper.trace_site, -1);
        __wrap_trace_bump_class(__realobj, -1);
    }
    #endif
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
    // Counter must be mutated under the lock like its create-side twin —
    // wrappers die concurrently on several threads, and a non-atomic `++`
    // outside the lock loses increments, making the live-wrapper metric
    // (create - dealloc) climb even when every wrapper is properly freed.
    __wrapper_dealloc_count++;
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

// Packed site-info (see core.h): store (aclass*, line*4+kind) on the
// exception instead of appending a pre-formatted string. Mirrors
// __throw_exception / __pass_exception including their refcount contract
// (throw retains the exception, pass transfers the callee's reference).
void __throw_exception_site(function_result *result, aobject * const exception, void * const site_class, unsigned int line_kind) {
    __add_stack_trace_site_function_alias(exception, (long long) (unsigned long) site_class, (long long) line_kind);
    if (result->exception) {
        __decrease_reference_count(result->exception);
    }
    result->exception = exception;
    __increase_reference_count(exception);
}

void __pass_exception_site(function_result *result, aobject * const exception, void * const site_class, unsigned int line_kind) {
    __add_stack_trace_site_function_alias(exception, (long long) (unsigned long) site_class, (long long) line_kind);
    if (result->exception) {
        __decrease_reference_count(result->exception);
    }
    result->exception = exception;
}

// Packed twin of __throw_simple_exception: same fallback ladder, but the
// site travels as (aclass*, line*8+kind) instead of a .rodata string.
void __throw_simple_exception_site(const char * const message, void * const site_class, unsigned int line_kind, function_result * const result) {
    aobject * ex_msg = __create_string_constant(message, &__string_class_alias);
    if (ex_msg == NULL) {
        __throw_out_of_memory_exception_site(result, site_class, line_kind);
        return;
    }
    aobject * ex = __create_exception(ex_msg);
    __throw_exception_site(result, ex, site_class, line_kind);
    __decrease_reference_count(ex_msg);
    __decrease_reference_count(ex);
}


int __suspend_root_rendezvous(suspend_state *st) {
    return (int) __amlc_atomic_fetch_add(&st->root_handoff, 1);
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
    // Propagate the OOM cleanly. Callers that have a `function_result *`
    // in scope should check the return and route through
    // `__throw_out_of_memory_exception`; callers without a function-result
    // channel (startup, constant tables) will see the NULL propagate and
    // most likely abort with a NULL deref at their first use — still
    // better than reading random bytes past a bogus allocation.
    if (str_obj == NULL) return NULL;

    string_holder * const holder = (string_holder *) (str_obj + 1);
    str_obj->object_properties.class_object_properties.object_data.value.custom_value = holder;
    int hash = __string_hash(str);
    holder->is_string_constant = true;
    holder->length = len;
    holder->string_value = (char *) str;
    holder->hash = hash;
    return str_obj;
}

aobject * __create_string(char const * const str, aclass * const string_class) {
    size_t len = strlen(str);
    aobject * str_obj = __allocate_object_with_extra_size(string_class, sizeof(string_holder) + len + 1);
    // See __create_string_constant above for the NULL propagation
    // contract — same reasoning applies. Callers are responsible for
    // treating a NULL as "the runtime is out of memory" and routing
    // through `__throw_out_of_memory_exception` if they can.
    if (str_obj == NULL) return NULL;
    string_holder * const holder = (string_holder *) (str_obj + 1);
    str_obj->object_properties.class_object_properties.object_data.value.custom_value = holder;
    char * const newStr = (char * const) (holder + 1);
    strcpy(newStr, str);
    int hash = __string_hash(str);
    holder->is_string_constant = false;
    holder->length = len;
    holder->string_value = newStr;
    holder->hash = hash;
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
    // Overflow-safe size computation. Before this: `size * item_size`
    // did the multiply in `unsigned int * unsigned int` (item_size
    // promotes) which wraps modulo 2^32 on ANY host. Then `size_t
    // extra_size = sizeof(array_holder) + <wrapped>` silently produced
    // a much smaller total on 64-bit and satisfied the allocation with
    // a buffer far too small for the array's advertised size. Writes
    // past the true end corrupted the heap; reads returned garbage.
    // Symptom that flagged this: `new Long[Int.max]` (16 GB request)
    // succeeded on a 32 GB machine, then any element write past
    // ~512M silently smashed unrelated allocations.
    //
    // Now: promote both operands to size_t so the multiply matches the
    // destination width on every host, and check both the multiply and
    // the subsequent add against `(size_t)-1` (portable SIZE_MAX). On
    // overflow return NULL — the codegen's post-`__create_array` OOM
    // null-check will then throw `OutOfMemoryException`, which matches
    // "we cannot satisfy this allocation" semantically.
    size_t const size_z = (size_t) size;
    size_t const item_size_z = (size_t) item_size;
    if (item_size_z != 0 && size_z > ((size_t)-1 - sizeof(array_holder)) / item_size_z) {
        return NULL;
    }
    size_t extra_size = sizeof(array_holder) + (size_z * item_size_z);
    aobject * array_obj = __allocate_object_with_extra_size(array_class, extra_size);
    if (array_obj == NULL) {
        return NULL;
    }
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

// Preallocated OutOfMemoryException singleton. Held immortal — every OOM
// throw reuses the same aobject *. The `__throw_exception` path bumps
// the refcount by one on each throw (and `__decrease_reference_count`
// after a `pass` / handled site brings it back down), so a healthy
// program churns the ref by +1/-1 per catch cycle. The initial ref
// counts as one held forever by the runtime — the object is never
// deallocated.
//
// Declared here (instead of alongside `__init_oom_singleton` further
// down) so `__create_exception` — which is the immediate fallback path
// on Exception allocation failure — can see it. `static` keeps it
// file-local.
static aobject * __oom_singleton = NULL;

aobject * __create_exception(aobject * const message) {
    aobject *ex = __allocate_object(&__exception_class_alias);
    if (ex == NULL) {
        // Allocation of the ordinary Exception object failed. Rather
        // than return NULL (which every caller then dereferences to
        // populate the message), hand back the preallocated OOM
        // singleton. Callers that immediately throw the returned aobject
        // will end up throwing OOM instead of Exception — which is
        // accurate: the reason `message` couldn't be attached is that
        // we're out of memory. `__decrease_reference_count(ex)` in the
        // caller is safe on the immortal singleton — its rc just churns.
        return __oom_singleton;
    }
    __exception_constructor_alias(ex, message);
    __exception_init_instance_function_alias((nullable_value){ .value.object_value = ex });
    return ex;
}

void __throw_simple_exception(const char * const message, const char * const stack_trace_item_text, function_result * const result) {
    aobject * ex_msg = __create_string_constant(message, &__string_class_alias);
    aobject * stit = __create_string_constant(stack_trace_item_text, &__string_class_alias);
    // If either string constant allocation failed, we're out of memory
    // and can't build the intended exception. Fall back to the
    // preallocated OOM singleton — it has no per-throw message but is
    // guaranteed constructable. Callers already expect the exception
    // path to run to completion; SIGSEGV'ing the caller because we
    // couldn't attach a stack-trace string is strictly worse.
    if (ex_msg == NULL || stit == NULL) {
        if (ex_msg != NULL) __decrease_reference_count(ex_msg);
        if (stit != NULL) __decrease_reference_count(stit);
        __throw_out_of_memory_exception(result, stack_trace_item_text);
        return;
    }
    aobject * ex = __create_exception(ex_msg);
    // `__create_exception` returns the OOM singleton on allocation
    // failure (see above); throwing it here still runs cleanly through
    // __throw_exception, and the stack-trace item we built stays
    // attached, so users can still see the intended failure site.
    __throw_exception(result, ex, stit);
    __decrease_reference_count(ex_msg); // it's in the exception stack trace list now, we don't need it anymore.
    __decrease_reference_count(stit); // it's in the exception stack trace list now, we don't need it anymore.
    __decrease_reference_count(ex); // safe on the OOM singleton too — its rc just churns.
}

void __throw_simple_exception_copy(const char * const message, const char * const stack_trace_item_text, function_result * const result) {
    // `__create_string` copies `message` into the new String, so callers
    // may pass a stack buffer (snprintf'd path + strerror) safely.
    aobject * ex_msg = __create_string(message, &__string_class_alias);
    aobject * stit = __create_string_constant(stack_trace_item_text, &__string_class_alias);
    if (ex_msg == NULL || stit == NULL) {
        if (ex_msg != NULL) __decrease_reference_count(ex_msg);
        if (stit != NULL) __decrease_reference_count(stit);
        __throw_out_of_memory_exception(result, stack_trace_item_text);
        return;
    }
    aobject * ex = __create_exception(ex_msg);
    __throw_exception(result, ex, stit);
    __decrease_reference_count(ex_msg);
    __decrease_reference_count(stit);
    __decrease_reference_count(ex);
}

// The `__oom_singleton` definition + lifetime notes live above
// `__create_exception` so that fallback path can compile against it.
// If startup fails to allocate this singleton the process is already in
// a hopeless memory state; we abort rather than pretend to keep going.
// `__throw_out_of_memory_exception` re-checks the singleton at throw
// time as a belt-and-suspenders in case some caller ends up throwing OOM
// before `__init_oom_singleton` has run.

void __init_oom_singleton(void) {
    if (__oom_singleton != NULL) return;
    // Allocate directly rather than via __create_exception so we don't
    // depend on __create_string_constant succeeding (it also uses
    // calloc). The Exception class's `message` field will read as NULL
    // — printWithStackTrace / toString handle NULL by degrading to
    // "(no message)", so no user code SIGSEGVs on our singleton.
    aobject * ex = calloc(1, sizeof(aobject) + (sizeof(property) * __out_of_memory_exception_class_alias.properties_count));
    if (ex == NULL) {
        // We are in a bad enough state that even the *singleton* can't
        // fit. Nothing we can do — abort loudly so the failure isn't
        // silent.
        fprintf(stderr, "AmLang: unable to allocate OutOfMemoryException singleton; aborting.\n");
        fflush(stderr);
        abort();
    }
    if (__out_of_memory_exception_class_alias.properties_count > 0) {
        ex->object_properties.class_object_properties.properties = (property *) (ex + 1);
    }
    ex->class_ptr = &__out_of_memory_exception_class_alias;
    ex->reference_count = 1;
    ex->owner_thread = __current_thread();
    ex->first_object_wrapper = NULL;
    __oom_singleton = ex;
}

void __throw_index_bounds_site(function_result * const result, void * const site_class, unsigned int line_kind) {
    __throw_simple_exception_site("Array index out of bounds", site_class, line_kind, result);
}

void __throw_negative_size_site(function_result * const result, void * const site_class, unsigned int line_kind) {
    __throw_simple_exception_site("Array size can't be negative", site_class, line_kind, result);
}

void __throw_out_of_memory_exception_site(function_result * const result, void * const site_class, unsigned int line_kind) {
    if (__oom_singleton == NULL) {
        fprintf(stderr, "AmLang: OOM before OutOfMemoryException singleton initialised; aborting (at %s line %u)\n",
                site_class != NULL ? ((aclass *) site_class)->name : "?", line_kind >> 3);
        fflush(stderr);
        abort();
    }
    // NOTE: attaching the packed frame goes through Exception.addStackTraceSite
    // (List.add) which may allocate; under true OOM that can fail — same
    // bounded hazard as the string-based path since the packed-trace rework.
    __throw_exception_site(result, __oom_singleton, site_class, line_kind);
}

void __throw_out_of_memory_exception(function_result * const result, const char * const stack_trace_item_text) {
    if (__oom_singleton == NULL) {
        // Someone hit OOM before startup wired up the singleton. Same
        // bad state as an allocation failure inside the singleton
        // itself — abort rather than crash by throwing NULL.
        fprintf(stderr, "AmLang: OOM before OutOfMemoryException singleton initialised; aborting (stack: %s)\n",
                stack_trace_item_text ? stack_trace_item_text : "(unknown)");
        fflush(stderr);
        abort();
    }
    // Attach a stack-trace frame if the caller supplied one. The
    // singleton's stack-trace list grows without bound over the
    // process's lifetime, but that's a bounded leak — a handful of
    // frames per throw, and the process is already in OOM territory.
    // Guard against __create_string_constant itself failing (nested
    // OOM) by throwing without a stack frame in that case.
    aobject * stit = NULL;
    if (stack_trace_item_text != NULL) {
        stit = calloc(1, sizeof(aobject) + (sizeof(property) * __string_class_alias.properties_count) + sizeof(string_holder) + 1);
        if (stit != NULL) {
            if (__string_class_alias.properties_count > 0) {
                stit->object_properties.class_object_properties.properties = (property *) (stit + 1);
            }
            stit->class_ptr = &__string_class_alias;
            stit->reference_count = 1;
            stit->owner_thread = __current_thread();
            stit->first_object_wrapper = NULL;
            // The string holder lives after properties[].
            string_holder * sh = (string_holder *) ((property *) (stit + 1) + __string_class_alias.properties_count);
            stit->object_properties.class_object_properties.object_data.value.custom_value = sh;
            sh->string_value = (char *)(sh + 1);
            // Bounded copy — cap at 255 bytes so a runaway string
            // doesn't chew through what little memory we have left.
            size_t n = 0;
            while (n < 255 && stack_trace_item_text[n] != '\0') {
                sh->string_value[n] = stack_trace_item_text[n];
                n++;
            }
            sh->string_value[n] = '\0';
        }
    }
    __throw_exception(result, __oom_singleton, stit);
    if (stit != NULL) {
        __decrease_reference_count(stit);
    }
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

// True if `cls` (or any of its base classes) declares `iface` in its
// `iface_implementations` list. Backs the `x is SomeInterface` codegen
// path where `SomeInterface` is an interface — `is_descendant_of` on
// its own only walks `cls->base`, so it misses ALL interface
// implementations and `x is Iface` always returns false.
//
// Argument order matches `is_descendant_of` at the codegen site so
// both helpers slot into the same emit template: TARGET first (`iface`
// / `base`), CONCRETE-source-class second (`cls`).
//
// Walks the base-class chain because a subclass inherits its parent's
// declared interfaces (`class Sub : Super {}` where `Super : SomeIface`
// still has `Sub is SomeIface == true`). Does NOT recurse through
// interface inheritance (`iface SubIface : SuperIface`) at the entry
// side — the compiler emits `iface_implementations` entries only for
// interfaces the class *directly* declares, so callers should query
// each concrete interface they care about. If we later add compile-
// time expansion of transitive interface implementations into each
// class's `iface_implementations` array, this stays correct without
// changes.
bool implements_interface(aclass const * const iface, aclass const * const cls) {
    aclass const * cur = cls;
    while (cur != NULL) {
        for (unsigned int i = 0; i < cur->iface_implementation_count; i++) {
            if (cur->iface_implementations[i].iface_class == iface) {
                return true;
            }
        }
        cur = cur->base;
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
