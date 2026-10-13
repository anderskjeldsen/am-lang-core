#include <libc/core.h>
#include <string.h>
#include <stdarg.h>

// ---------------------------------------------------------------------------
// Isolate platform layer: the global lock, the current-isolate binding and
// the mailbox signalling primitive. Two backends:
//   * Amiga family (AmigaOS m68k, MorphOS): Forbid/Permit for the lock,
//     tc_UserData for the binding, an Exec signal per isolate to wake a
//     mailbox reader. Exec's own primitives, no pthreads.
//   * everything else (linux, macOS, AROS hosted): a pthread mutex, TLS
//     (a pthread key on AROS, whose pthreads own tc_UserData), and a
//     mutex+condvar per mailbox.
// The object hot paths never come here — only Thread.send/receive/start
// and isolate handle counting do.
// ---------------------------------------------------------------------------
#if defined(__AROS__)
    #include <pthread.h>
#elif defined(__MORPHOS__) || defined(__AMIGA__) || defined(AMIGA) || defined(PLATFORM_AMIGAOS)
    #define AMLC_AMIGA_FAMILY 1
    #include <exec/types.h>
    #include <exec/execbase.h>
    #include <exec/tasks.h>
    #include <proto/exec.h>
#else
    #include <pthread.h>
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

bool __conditional_logging_on = false;

// Live-object count of the CALLING isolate (each isolate keeps its own).
#define __allocation_count (__amlc_isolate()->allocation_count)
#define MAX_ALLOCATIONS 1024 * 50
#if defined(DEBUG) || defined(TRACKOBJECTS)
int __last_object_id = 0;
aobject * allocations[MAX_ALLOCATIONS];
int allocation_index = 0;
#endif

class_static *__first_class_static = NULL;
unsigned int __amlc_static_slot_count = 0;
__amlc_static_init_fn * __amlc_static_init_table = NULL;
unsigned int __amlc_static_init_count = 0;

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
    // Roots are the calling isolate's static properties.
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

        if (obj->class_ptr->object_property_count > 0) {
            for(int i = 0; i < obj->class_ptr->properties_count; i++) {
                property * const __prop = &obj->object_properties.class_object_properties.properties[i];
                if (!__is_primitive(__prop->nullable_value) && __prop->nullable_value.value.object_value != NULL) {
                    __mark_object(__prop->nullable_value.value.object_value);
                }
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
        current = __amlc_isolate()->first_object;
        while(current != NULL) {
            sweep_result result = __sweep_object(current);
            if (result.is_swept) {
                new_count++;
            }
            current = result.next;
        }
    }

    aisolate * const __iso = __amlc_isolate();
    current = __iso->first_detached_object;
    while(current != NULL) {
        aobject *next = current->next;
        __deallocate_detached_object(current);
        current = next;
    }
    __iso->first_detached_object = NULL;

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

    aobject * current = __amlc_isolate()->first_object;
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
    // Hand the class its range of static slots. Every isolate created from
    // here on sizes its statics array by the running total; the main
    // isolate is booted only after every class has registered.
    __class_static->slot_base = __amlc_static_slot_count;
    __amlc_static_slot_count += __class_static->static_properties_count;
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

unsigned int __utf8_decode(const char * const s, unsigned int const len, unsigned int const i, unsigned int * const out_cp) {
    unsigned int b0, b1, b2, b3, cp;
    if (i >= len) { *out_cp = 0xfffd; return 1; }
    b0 = (unsigned char) s[i];
    if (b0 < 0x80) { *out_cp = b0; return 1; }
    // 0xc0/0xc1 are excluded: they can only encode an overlong form of an ASCII
    // character, which a decoder must reject rather than fold back to ASCII.
    if (b0 >= 0xc2 && b0 <= 0xdf && i + 1 < len) {
        b1 = (unsigned char) s[i + 1];
        if (b1 >= 0x80 && b1 <= 0xbf) {
            *out_cp = ((b0 & 0x1f) << 6) | (b1 & 0x3f);
            return 2;
        }
    } else if (b0 >= 0xe0 && b0 <= 0xef && i + 2 < len) {
        b1 = (unsigned char) s[i + 1];
        b2 = (unsigned char) s[i + 2];
        if (b1 >= 0x80 && b1 <= 0xbf && b2 >= 0x80 && b2 <= 0xbf) {
            cp = ((b0 & 0x0f) << 12) | ((b1 & 0x3f) << 6) | (b2 & 0x3f);
            // Reject overlongs and the surrogate block. Surrogates are not
            // valid UTF-8 however they were produced; accepting them would let
            // a decode/encode round trip manufacture text no other decoder agrees with.
            if (cp >= 0x800 && (cp < 0xd800 || cp > 0xdfff)) { *out_cp = cp; return 3; }
        }
    } else if (b0 >= 0xf0 && b0 <= 0xf4 && i + 3 < len) {
        b1 = (unsigned char) s[i + 1];
        b2 = (unsigned char) s[i + 2];
        b3 = (unsigned char) s[i + 3];
        if (b1 >= 0x80 && b1 <= 0xbf && b2 >= 0x80 && b2 <= 0xbf && b3 >= 0x80 && b3 <= 0xbf) {
            cp = ((b0 & 0x07) << 18) | ((b1 & 0x3f) << 12) | ((b2 & 0x3f) << 6) | (b3 & 0x3f);
            if (cp >= 0x10000 && cp <= 0x10ffff) { *out_cp = cp; return 4; }
        }
    }
    // Malformed: one replacement character, one byte consumed. Never zero, or
    // every caller's loop would spin.
    *out_cp = 0xfffd;
    return 1;
}

unsigned int __utf8_utf16_length(const char * const s, unsigned int const len) {
    unsigned int i = 0, units = 0, cp = 0;
    while (i < len) {
        i += __utf8_decode(s, len, i, &cp);
        units += (cp >= 0x10000) ? 2 : 1;   // astral = surrogate pair
    }
    return units;
}

unsigned short * __string_wide(string_holder * const holder) {
    unsigned int i, o, cp, units;
    unsigned short * buf;
    if (holder == NULL) return NULL;
    // Pure ASCII needs no view: byte index == unit index, so the caller's
    // direct string_value read is already correct AND O(1).
    if (holder->char_length == holder->length) return NULL;
    if (holder->wide != NULL) return holder->wide;

    units = holder->char_length;
    if (units == 0) return NULL;
    buf = (unsigned short *) malloc((size_t) units * sizeof(unsigned short));
    // Deliberately NOT an OOM throw: charAt getting slow on a full machine is
    // far better than charAt failing. Callers fall back to a scan on NULL.
    if (buf == NULL) return NULL;

    i = 0; o = 0; cp = 0;
    while (i < holder->length && o < units) {
        i += __utf8_decode(holder->string_value, holder->length, i, &cp);
        if (cp < 0x10000) {
            buf[o++] = (unsigned short) cp;
        } else {
            unsigned int v = cp - 0x10000;
            buf[o++] = (unsigned short) (0xd800 + (v >> 10));
            if (o < units) buf[o++] = (unsigned short) (0xdc00 + (v & 0x3ff));
        }
    }

    // Publish last. Strings are immutable and shared, so two threads can both
    // find NULL here and both build an identical view; whoever stores second
    // wins and the loser's buffer leaks. A plain aligned pointer store is
    // atomic on every target we build for, so a reader never sees a torn or
    // half-filled pointer -- and taking the shared ARC lock on every charAt
    // would cost far more than the rare leak. Under AM_SINGLE_THREADED the
    // race cannot arise at all.
    holder->wide = buf;
    return buf;
}

unsigned int __string_unit_to_byte(string_holder * const holder, unsigned int const unit_index, int * const out_split) {
    unsigned int i = 0, o = 0, cp = 0, step;
    if (out_split != NULL) *out_split = 0;
    if (holder == NULL) return 0;
    // ASCII: one byte per unit, so the two index spaces are the same number.
    if (holder->char_length == holder->length) {
        return (unit_index < holder->length) ? unit_index : holder->length;
    }
    while (i < holder->length && o < unit_index) {
        step = __utf8_decode(holder->string_value, holder->length, i, &cp);
        if (cp >= 0x10000) {
            // Astral: two units for one 4-byte character. Asking for the second
            // of them is asking for a boundary inside a character.
            if (o + 1 == unit_index) {
                if (out_split != NULL) *out_split = 1;
                return i;
            }
            o += 2;
        } else {
            o += 1;
        }
        i += step;
    }
    return i;
}

unsigned int __string_byte_to_unit(string_holder * const holder, unsigned int const byte_offset) {
    unsigned int i = 0, o = 0, cp = 0;
    if (holder == NULL) return 0;
    if (holder->char_length == holder->length) {
        return (byte_offset < holder->length) ? byte_offset : holder->char_length;
    }
    while (i < holder->length && i < byte_offset) {
        i += __utf8_decode(holder->string_value, holder->length, i, &cp);
        o += (cp >= 0x10000) ? 2 : 1;
    }
    return o;
}

void __string_wide_free(string_holder * const holder) {
    if (holder != NULL && holder->wide != NULL) {
        free(holder->wide);
        holder->wide = NULL;
    }
}

// The ONE string hash: FNV-1a over the bytes, then a final avalanche
// (murmur3's fmix32) so the LOW bits depend on every byte — HashMap/HashSet
// index buckets by masking the hash. The previous additive
// `hash += c << (5*i mod 32)` left paths differing in a digit or two in the
// same handful of buckets (10 of 32768 used for 20k paths, 2000-long chains).
// Both entry points below must stay bit-for-bit identical: the hash is
// compared directly in String.equals and keys every HashMap in the runtime.
static inline unsigned int __amlc_hash_bytes(const char * const str) {
    unsigned int hash = 2166136261u;
    const unsigned char *p = (const unsigned char *) str;
    while (*p != 0) {
        hash ^= (unsigned int) *p++;
        hash *= 16777619u;
    }
    hash ^= hash >> 16;
    hash *= 0x85ebca6bu;
    hash ^= hash >> 13;
    hash *= 0xc2b2ae35u;
    hash ^= hash >> 16;
    return hash;
}

unsigned int __string_hash_and_units(const char * const str, unsigned int const byte_len, unsigned int * const out_units) {
    if (out_units != NULL) {
        *out_units = __utf8_utf16_length(str, byte_len);
    }
    return __amlc_hash_bytes(str);
}

unsigned int __string_hash(const char * const str) {
    return __amlc_hash_bytes(str);
}

// ── Slot watch (temporary debug aid) ──────────────────────────────────
// See core.h. Snapshots 12 raw bytes at `__amlc_watch_addr` when armed and
// reports the first divergence, with the call site that was running. Costs
// one load + branch when disarmed, and allocates nothing, so it can be
// called from the allocator without shifting the heap layout it is trying
// to observe.
void * __amlc_watch_addr = NULL;
int __amlc_watch_armed = 0;
static unsigned char __amlc_watch_expect[12];
static int __amlc_watch_reported = 0;

void __amlc_watch_arm(void * addr) {
    int i = 0;
    __amlc_watch_addr = addr;
    if (addr == NULL) {
        __amlc_watch_armed = 0;
        return;
    }
    while (i < 12) {
        __amlc_watch_expect[i] = ((const unsigned char *) addr)[i];
        i = i + 1;
    }
    __amlc_watch_armed = 1;
    __amlc_watch_reported = 0;
    // Relocation anchor: AmigaOS hunks load at an arbitrary base, so the
    // site pointer in a report means nothing on its own. Printing a known
    // symbol's runtime address gives the slide to subtract before looking
    // the site up in the link map.
    printf("[watch] anchor __amlc_watch_check=%p\n", (void *) &__amlc_watch_check);
    printf("[watch] armed on %p bytes=", addr);
    i = 0;
    while (i < 12) {
        printf("%02x", (unsigned int) __amlc_watch_expect[i]);
        i = i + 1;
    }
    printf("\n");
    fflush(stdout);
}

void __amlc_watch_check(const char * where, const char * cls, void * site) {
    const unsigned char * now;
    int i = 0;
    int differs = 0;
    if (__amlc_watch_armed == 0 || __amlc_watch_addr == NULL) {
        return;
    }
    now = (const unsigned char *) __amlc_watch_addr;
    while (i < 12) {
        if (now[i] != __amlc_watch_expect[i]) {
            differs = 1;
            break;
        }
        i = i + 1;
    }
    if (differs == 0) {
        return;
    }
    if (__amlc_watch_reported == 0) {
        __amlc_watch_reported = 1;
        printf("[watch] *** CHANGED at %p during %s (class=%s, site=%p)\n",
            __amlc_watch_addr, where != NULL ? where : "?",
            cls != NULL ? cls : "?", site);
        printf("[watch]   was=");
        i = 0;
        while (i < 12) { printf("%02x", (unsigned int) __amlc_watch_expect[i]); i = i + 1; }
        printf("\n[watch]   now=");
        i = 0;
        while (i < 12) { printf("%02x", (unsigned int) now[i]); i = i + 1; }
        printf("\n");
        fflush(stdout);
    }
    // Re-sync so a single stray write is reported once rather than at every
    // later call site, while a SECOND stray write still gets caught.
    i = 0;
    while (i < 12) { __amlc_watch_expect[i] = now[i]; i = i + 1; }
    __amlc_watch_reported = 0;
}

aobject * __allocate_object_with_extra_size(aclass * const __class, size_t extra_size) {
    __amlc_watch_probe("allocate", __class != NULL ? __class->name : NULL, __builtin_return_address(0));
    // One thread-local lookup per allocation: on macOS __thread access is a
    // call (_tlv_get_addr), and this path is the hottest in the runtime.
    aisolate * const __iso = __amlc_isolate();
    __iso->allocation_count++;

    #if defined(DEBUG) || defined(TRACKOBJECTS)
    __last_object_id++;
    #endif
    #ifdef TRACKOBJECTS
    // Per-class live-instance counter feeding the `instances(Class)`
    // test intrinsic. Balanced by the decrement in __deallocate_object.
    if (__class != NULL) __class->instance_count++;
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
    bool chunked = false;
    // Outside a scratch extent fast_depth is 0 and the && short-circuits, so
    // the keep_always test costs nothing on the ordinary path.
    if (__iso->fast_depth > 0 && !__class->keep_always) {
        __obj = __amlc_arena_allocate(__iso, size_with_properties);
        chunked = (__obj != NULL);
    }
    if (__obj == NULL) {
        #ifdef AMLC_SEED_PROPERTIES
        // -fseed-properties: the generated constructor seeds every property
        // slot nothing else assigns, so for a PLAIN CLASS INSTANCE only the
        // header needs clearing here (prev/next/marked/pending_deallocation/
        // immortal and the object_properties union are all read by ARC and the
        // GC before any constructor runs, so they must be sound regardless).
        //
        // extra_size > 0 means the tail is NOT a property array and no
        // generated constructor describes it, so it must still be zeroed:
        //   - arrays  -> element storage. HashMap reads keyIndexes[i] before
        //     writing it, with 0 as the "empty bucket" sentinel, so garbage
        //     there walks a bucket chain forever. This is what hung the test
        //     suite when the malloc path was applied unconditionally.
        //   - strings -> string_holder plus the characters.
        //   - Struct<T> boxes -> the value tail.
        if (extra_size == 0) {
            __obj = (aobject *) malloc(size_with_properties);
            if (__obj != NULL) {
                memset(__obj, 0, sizeof(aobject));
            }
        } else {
            __obj = (aobject *) calloc(1, size_with_properties);
        }
        #else
        __obj = (aobject *) calloc(1, size_with_properties);
        #endif
    }

    if (__obj != NULL) {
        __obj->chunked = chunked;
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
            __obj->object_properties.class_object_properties.properties = (property *) ((char *) __obj + sizeof(aobject));
        }

        __obj->class_ptr = __class;
        __obj->reference_count = 1;

        #if defined(DEBUG) || defined(TRACKOBJECTS)
        // Debug registry is a FIXED array — saturate instead of writing
        // past it. An unchecked `allocations[allocation_index++]` overflowed
        // the global once a process allocated >MAX_ALLOCATIONS objects
        // (e.g. the V2 compiler compiling a whole project in-process),
        // corrupting the neighbouring globals and eventually the heap.
        // Objects beyond capacity simply aren't tracked (shutdown dump
        // misses them) — a debug-feature limitation, not a correctness one.
        // (The registry is process-global and unsynchronised: concurrent
        // isolates may lose entries — again diagnostic only.)
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

    aisolate * const __iso = __amlc_isolate();
    if (__obj == __iso->first_object) {
        __set_first_object(__obj->next, "detach_object_from_sweep");
        if (__iso->first_object != NULL) {
            __iso->first_object->prev = NULL;
        }
    } else {
        __obj->prev->next = __obj->next;
        if (__obj->next != NULL) {
            __obj->next->prev = __obj->prev;
        }
    }

    aobject *next_to_sweep = __obj->next;

    if (__iso->first_detached_object == NULL) {
        __iso->first_detached_object = __obj;
        __obj->next = NULL;
        __obj->prev = NULL;
    } else {
        __obj->next = __iso->first_detached_object;
        __iso->first_detached_object->prev = __obj;
        __iso->first_detached_object = __obj;
    }

    return (sweep_result) { .next = next_to_sweep, .is_swept = true };
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

    __amlc_free_object(__obj);
}

void __deallocate_object(aobject * const __obj) {
    __amlc_watch_probe("deallocate",
        (__obj != NULL && __obj->class_ptr != NULL) ? __obj->class_ptr->name : NULL,
        __builtin_return_address(0));

    #ifdef TRACKOBJECTS
    // Balance the increment in __allocate_object_with_extra_size so the
    // `instances(Class)` test intrinsic reflects the live count.
    if (__obj != NULL && __obj->class_ptr != NULL) __obj->class_ptr->instance_count--;
    #endif

    #if defined(DEBUG) || defined(TRACKOBJECTS)
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

    // A plain class with no release hook and no object-typed slot has
    // nothing to detach: skip the call (and its class-metadata loads).
    aclass * const __cls = __obj->class_ptr;
    if (__cls->release == NULL && __cls->object_property_count == 0 && __cls->statics->type == class) {
        __obj->pending_deallocation = true;
    } else {
        __detach_object(__obj);
    }

    #ifdef DEBUG
    #ifdef CONDLOG 
    if (__conditional_logging_on) {
    #endif
    printf("Finalize deallocation of object of type %s (address: %p, object id: %d, total object allocation count: %d)\n", type, __obj, object_id, __allocation_count);
    #ifdef CONDLOG 
    }
    #endif
    #endif

    aisolate * const __iso = __amlc_isolate();
    __iso->allocation_count--;

    #if defined(DEBUG) || defined(TRACKOBJECTS)
    for(int i = 0; i < MAX_ALLOCATIONS; i++) {
        if ( allocations[i] == __obj) {
            allocations[i] = NULL;
        }
    }
    #endif

    #ifdef DEBUG
    printf("About to free object %s (address: %p)\n", __obj->class_ptr->name, __obj);
    printf("Reference counts: ref=%d, prop_ref=%d\n", __obj->reference_count, __obj->property_reference_count);
    #endif

    __amlc_free_object_in(__iso, __obj);

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
        if (__obj->immortal) {
            return;
        }
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

        if (__obj->property_reference_count == 0 && !__obj->pending_deallocation) {
            aisolate * const __iso = __amlc_isolate();
            if (__obj == __iso->first_object) {
                __set_first_object(__obj->next, "decrease_property_reference_count");
                __obj->next = NULL;
                if (__iso->first_object != NULL) {
                    __iso->first_object->prev = NULL;
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

    if (__obj->class_ptr->statics->type == class && __obj->class_ptr->object_property_count > 0) {
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
    // Releases the CALLING isolate's roots: its cached Thread handle to
    // itself (getCurrent) and its copy of every static property.
    aisolate * const iso = __amlc_isolate();
    if (iso->self_handle != NULL) {
        aobject * h = iso->self_handle;
        iso->self_handle = NULL;
        __decrease_reference_count(h);
    }
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

    if (__class_static->type == class) {
        for(int i = 0; i < __class_static->static_properties_count; i++) {
            property * const __prop = &__static_slot(__class_static, i);
            #if defined(DEBUG) || defined(TRACKOBJECTS)
            char *name = "<null>";
            if (!__is_primitive(__prop->nullable_value) && __prop->nullable_value.value.object_value != NULL) {
                name = __prop->nullable_value.value.object_value->class_ptr->name;
            }
            printf("Dereference static property %d %s\n", i, name);
            #endif
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
    
    if (__class_static->type == class) {
        for(int i = 0; i < __class_static->static_properties_count; i++) {
            property * const __prop = &__static_slot(__class_static, i);
            if (!__is_primitive(__prop->nullable_value) && __prop->nullable_value.value.object_value != NULL) {
                __mark_object(__prop->nullable_value.value.object_value);
            }
        }
    }
}

void print_allocated_objects() {
    #if defined(DEBUG) || defined(TRACKOBJECTS)
    aisolate * const iso = __amlc_isolate();
    printf("Allocated objects %d\n", iso->allocation_count);

    if (iso->allocation_count > 0) {
        if (iso->first_object != NULL) {
            printf("Going through attached objects:\n");
            aobject *c = iso->first_object;
            while(c != NULL) {
                printf("Object alive, %s\n", c->class_ptr->name);
                c = c->next;
            }    
        }

        if (iso->first_detached_object != NULL) {
            printf("Going through detached objects:\n");
            aobject *c = iso->first_detached_object;
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
    #if defined(DEBUG) || defined(TRACKOBJECTS)
    for(int i = 0; i < MAX_ALLOCATIONS; i++) {
        allocations[i] = NULL;
    }
    #endif
}

void deallocate_annotations(class_static * const __class_static) {
    for(int i = 0; i < __class_static->annotations_count; i++) {
        aobject * const a = __class_static->annotations[i];
        a->reference_count = 1;   // immortal while the program runs
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
    // A continuation chain runs entirely inside one isolate, so the
    // rendezvous is a plain counter.
    return st->root_handoff++;
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

    string_holder * const holder = (string_holder *) ((char *) str_obj + sizeof(aobject));
    str_obj->object_properties.class_object_properties.object_data.value.custom_value = holder;
    unsigned int units = 0;
    unsigned int hash = __string_hash_and_units(str, (unsigned int) len, &units);
    holder->is_string_constant = true;
    holder->length = len;
    holder->string_value = (char *) str;
    holder->hash = hash;
    holder->char_length = units;
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
    string_holder * const holder = (string_holder *) ((char *) str_obj + sizeof(aobject));
    str_obj->object_properties.class_object_properties.object_data.value.custom_value = holder;
    char * const newStr = (char * const) (holder + 1);
    strcpy(newStr, str);
    unsigned int units = 0;
    unsigned int hash = __string_hash_and_units(str, (unsigned int) len, &units);
    holder->is_string_constant = false;
    holder->length = len;
    holder->string_value = newStr;
    holder->hash = hash;
    holder->char_length = units;
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
    array_holder * const holder = (array_holder *) ((char *) array_obj + sizeof(aobject));
    void *array_data = (void *) ((char *) holder + sizeof(array_holder));
    array_obj->object_properties.class_object_properties.object_data.value.custom_value = holder;
    holder->array_data = array_data;
    holder->ctype = ctype;
    holder->item_class = array_class;
    holder->item_size = item_size;
    holder->size = size;
    // The data block is already zero: __allocate_object_with_extra_size
    // callocs (or the fast-lane arena memsets) the whole allocation, so a
    // second memset here only doubled the cost of every array — for large
    // arrays it also touched every page that calloc had left lazily zeroed.
    // The zero-init contract above still holds; it is just not paid twice.
    return array_obj;
}

array_holder * get_array_holder(aobject * const array_obj) {
    return (array_holder *) ((char *) array_obj + sizeof(aobject));
}

char * get_array_data(array_holder * holder) {
    return (char *) ((char *) holder + sizeof(array_holder));
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
        ex->object_properties.class_object_properties.properties = (property *) ((char *) ex + sizeof(aobject));
    }
    ex->class_ptr = &__out_of_memory_exception_class_alias;
    ex->reference_count = 1;
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
                stit->object_properties.class_object_properties.properties = (property *) ((char *) stit + sizeof(aobject));
            }
            stit->class_ptr = &__string_class_alias;
            stit->reference_count = 1;
            // The string holder lives after properties[].
            string_holder * sh = (string_holder *) ((property *) ((char *) stit + sizeof(aobject)) + __string_class_alias.properties_count);
            stit->object_properties.class_object_properties.object_data.value.custom_value = sh;
            sh->string_value = (char *)((char *) sh + sizeof(string_holder));
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

// ---------------------------------------------------------------------------
// Reflection. ClassRef objects are process-global (one per aclass, hung on
// class_ptr->class_ref_singleton) and read from every isolate, so they and
// everything they own - the name string, the PropertyInfo array and its
// entries - are immortal: pinned counts, never released by the isolates,
// freed only by __amlc_reflection_teardown at exit. Creation happens under
// the global lock, and never inside a fast-lane arena.
// ---------------------------------------------------------------------------

static aclass * __amlc_reflected_first = NULL;

typedef struct __amlc_cr_node {
    aobject * ref;
    struct __amlc_cr_node * next;
} __amlc_cr_node;
static __amlc_cr_node * __amlc_cr_list = NULL;

void __amlc_register_reflected(aclass * const cls) {
    cls->reflected_next = __amlc_reflected_first;
    __amlc_reflected_first = cls;
}

aclass * __amlc_find_reflected_class(const char * const name) {
    for (aclass * c = __amlc_reflected_first; c != NULL; c = c->reflected_next) {
        if (strcmp(c->name, name) == 0) return c;
    }
    return NULL;
}

void __amlc_make_immortal(aobject * const obj) {
    obj->immortal = true;
    obj->reference_count = 0x40000000;
}

aobject * __amlc_allocate_immortal(aclass * const cls, size_t extra_size) {
    aisolate * const iso = __amlc_isolate();
    int const saved_depth = iso->fast_depth;
    iso->fast_depth = 0;   // never from an arena: the object outlives every fast block
    aobject * obj = extra_size > 0 ? __allocate_object_with_extra_size(cls, extra_size) : __allocate_object(cls);
    iso->fast_depth = saved_depth;
    if (obj != NULL) __amlc_make_immortal(obj);
    return obj;
}

aobject * __amlc_class_ref(aclass * const cls) {
    if (cls->class_ref_singleton != NULL) return cls->class_ref_singleton;
    __amlc_global_lock();
    if (cls->class_ref_singleton == NULL) {
        aobject * ref = __amlc_allocate_immortal(&__class_ref_class_alias, 0);
        if (ref != NULL) {
            ref->object_properties.class_object_properties.object_data.value.custom_value = cls;
            aobject * name = __create_string_constant(cls->name, &__string_class_alias);
            if (name != NULL) {
                __amlc_make_immortal(name);
                __set_property(ref, Am_Lang_ClassRef_P_className, (nullable_value) { .flags = 0, .value.object_value = name });
            }
            __set_property(ref, Am_Lang_ClassRef_P_reflected, (nullable_value) { .flags = PRIMITIVE_BOOL, .value.bool_value = cls->reflect != NULL });
            __amlc_cr_node * node = (__amlc_cr_node *) malloc(sizeof(__amlc_cr_node));
            if (node != NULL) {
                node->ref = ref;
                node->next = __amlc_cr_list;
                __amlc_cr_list = node;
            }
            cls->class_ref_singleton = ref;
        }
    }
    __amlc_global_unlock();
    return cls->class_ref_singleton;
}

// Frees an immortal object: the pinned count becomes a real one and drops.
// Children it holds are immortal too (their property counts were never
// taken), so each is freed on its own by the caller.
static void __amlc_free_immortal(aobject * const obj) {
    if (obj == NULL) return;
    obj->reference_count = 1;
    __decrease_reference_count(obj);
}

void __amlc_reflection_teardown(void) {
    __amlc_cr_node * node = __amlc_cr_list;
    __amlc_cr_list = NULL;
    while (node != NULL) {
        aobject * const ref = node->ref;
        aclass * const cls = (aclass *) ref->object_properties.class_object_properties.object_data.value.custom_value;
        aobject * const props = ref->object_properties.class_object_properties.properties[Am_Lang_ClassRef_P_properties].nullable_value.value.object_value;
        if (props != NULL) {
            array_holder * const ah = (array_holder *) ((char *) props + sizeof(aobject));
            aobject ** const items = (aobject **) ah->array_data;
            for (unsigned int i = 0; i < ah->size; i++) {
                aobject * const pi = items[i];
                if (pi == NULL) continue;
                aobject * const pname = pi->object_properties.class_object_properties.properties[Am_Lang_PropertyInfo_P_name].nullable_value.value.object_value;
                __amlc_free_immortal(pi);
                __amlc_free_immortal(pname);
            }
            __amlc_free_immortal(props);
        }
        __amlc_free_immortal(ref->object_properties.class_object_properties.properties[Am_Lang_ClassRef_P_className].nullable_value.value.object_value);
        __amlc_free_immortal(ref);
        if (cls != NULL) cls->class_ref_singleton = NULL;
        __amlc_cr_node * const next = node->next;
        free(node);
        node = next;
    }
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
    result_holder = (string_holder*)((char *) result_obj + sizeof(aobject));
    result_str = (char*)((char *) result_holder + sizeof(string_holder));
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
    result_holder->hash = __string_hash_and_units(result_str, (unsigned int) total_length,
                                                  &result_holder->char_length);
    
    return result_obj;
}

// ===========================================================================
// Isolates — share-nothing threads. Platform layer first, then the
// portable lifecycle, then messaging (deep copy + mailbox).
// ===========================================================================

// ---- global lock ----------------------------------------------------------
#ifdef AMLC_AMIGA_FAMILY
void __amlc_global_lock(void)   { Forbid(); }
void __amlc_global_unlock(void) { Permit(); }
#else
static pthread_mutex_t __amlc_global_mutex = PTHREAD_MUTEX_INITIALIZER;
void __amlc_global_lock(void)   { pthread_mutex_lock(&__amlc_global_mutex); }
void __amlc_global_unlock(void) { pthread_mutex_unlock(&__amlc_global_mutex); }
#endif

// ---- current-isolate binding ----------------------------------------------
#if defined(__AROS__)
static pthread_key_t  __amlc_iso_key;
static pthread_once_t __amlc_iso_key_once = PTHREAD_ONCE_INIT;
static void __amlc_make_iso_key(void) { pthread_key_create(&__amlc_iso_key, NULL); }
aisolate * __amlc_isolate_current(void) {
    pthread_once(&__amlc_iso_key_once, __amlc_make_iso_key);
    return (aisolate *) pthread_getspecific(__amlc_iso_key);
}
static void __amlc_bind_isolate(aisolate * const iso) {
    pthread_once(&__amlc_iso_key_once, __amlc_make_iso_key);
    pthread_setspecific(__amlc_iso_key, iso);
}
#elif defined(AMLC_AMIGA_FAMILY)
aisolate * __amlc_isolate_current(void) { return (aisolate *) FindTask(NULL)->tc_UserData; }
static void __amlc_bind_isolate(aisolate * const iso) { FindTask(NULL)->tc_UserData = (APTR) iso; }
#else
__thread aisolate * __amlc_tls_isolate = NULL;
static void __amlc_bind_isolate(aisolate * const iso) { __amlc_tls_isolate = iso; }
#endif
aisolate * __amlc_main_isolate_ptr = NULL;
int __amlc_isolate_count = 1;
void __amlc_isolate_count_inc(void) { __amlc_global_lock(); __amlc_isolate_count++; __amlc_global_unlock(); }
void __amlc_isolate_count_dec(void) { __amlc_global_lock(); __amlc_isolate_count--; __amlc_global_unlock(); }

// ---- mailbox --------------------------------------------------------------
// A FIFO of amessage owned by one isolate. Any isolate may post; only the
// owner takes. Amiga: the queue is edited under Forbid and the owner sleeps
// in Wait() on a signal it allocated when it entered — a post that lands
// between the owner's empty-check and its Wait is not lost because Exec
// signals are sticky. POSIX: mutex + condvar.
struct _amailbox {
    amessage * head;
    amessage * tail;
    bool closed;
#ifdef AMLC_AMIGA_FAMILY
    struct Task * owner;
    ULONG sigmask;
#else
    pthread_mutex_t lock;
    pthread_cond_t cond;
#endif
};

static amailbox * __amlc_mailbox_create(void) {
    amailbox * mb = (amailbox *) calloc(1, sizeof(amailbox));
    if (mb == NULL) return NULL;
#ifndef AMLC_AMIGA_FAMILY
    pthread_mutex_init(&mb->lock, NULL);
    pthread_cond_init(&mb->cond, NULL);
#endif
    return mb;
}

// Called by the owning thread once it runs as this isolate.
static void __amlc_mailbox_bind(amailbox * const mb) {
#ifdef AMLC_AMIGA_FAMILY
    BYTE bit = AllocSignal(-1);
    Forbid();
    mb->owner = FindTask(NULL);
    mb->sigmask = (bit >= 0) ? (1UL << bit) : 0;
    Permit();
#else
    (void) mb;
#endif
}

static bool __amlc_mailbox_post(amailbox * const mb, amessage * const msg) {
    msg->next = NULL;
#ifdef AMLC_AMIGA_FAMILY
    Forbid();
    if (mb->closed) { Permit(); return false; }
    if (mb->tail != NULL) mb->tail->next = msg; else mb->head = msg;
    mb->tail = msg;
    struct Task * owner = mb->owner;
    ULONG mask = mb->sigmask;
    Permit();
    if (owner != NULL && mask != 0) Signal(owner, mask);
    return true;
#else
    pthread_mutex_lock(&mb->lock);
    if (mb->closed) { pthread_mutex_unlock(&mb->lock); return false; }
    if (mb->tail != NULL) mb->tail->next = msg; else mb->head = msg;
    mb->tail = msg;
    pthread_cond_signal(&mb->cond);
    pthread_mutex_unlock(&mb->lock);
    return true;
#endif
}

static amessage * __amlc_mailbox_pop_locked(amailbox * const mb) {
    amessage * m = mb->head;
    if (m != NULL) {
        mb->head = m->next;
        if (mb->head == NULL) mb->tail = NULL;
        m->next = NULL;
    }
    return m;
}

static amessage * __amlc_mailbox_take(amailbox * const mb, bool const wait) {
#ifdef AMLC_AMIGA_FAMILY
    for (;;) {
        Forbid();
        amessage * m = __amlc_mailbox_pop_locked(mb);
        bool closed = mb->closed;
        ULONG mask = mb->sigmask;
        Permit();
        if (m != NULL || !wait || closed || mask == 0) return m;
        Wait(mask);
    }
#else
    pthread_mutex_lock(&mb->lock);
    while (wait && mb->head == NULL && !mb->closed) {
        pthread_cond_wait(&mb->cond, &mb->lock);
    }
    amessage * m = __amlc_mailbox_pop_locked(mb);
    pthread_mutex_unlock(&mb->lock);
    return m;
#endif
}

static void __amlc_mailbox_close(amailbox * const mb) {
#ifdef AMLC_AMIGA_FAMILY
    Forbid();
    mb->closed = true;
    ULONG mask = mb->sigmask;
    mb->owner = NULL;
    mb->sigmask = 0;
    Permit();
    if (mask != 0) {
        // Free the signal bit from the owning task (we are it).
        BYTE bit = 0;
        while (((1UL << bit) & mask) == 0) bit++;
        FreeSignal(bit);
    }
#else
    pthread_mutex_lock(&mb->lock);
    mb->closed = true;
    pthread_cond_broadcast(&mb->cond);
    pthread_mutex_unlock(&mb->lock);
#endif
}

static void __amlc_mailbox_destroy(amailbox * const mb) {
    if (mb == NULL) return;
#ifndef AMLC_AMIGA_FAMILY
    pthread_mutex_destroy(&mb->lock);
    pthread_cond_destroy(&mb->cond);
#endif
    free(mb);
}

// ---- fast-lane arenas -----------------------------------------------------
// A bump allocator for `fast { ... }` blocks. Each object carries a pointer
// to its arena in the word just before the aobject; death decrements the
// arena's live count and an arena at zero is reset in place. Arenas never
// move objects, so every raw pointer a native holds stays valid.
static aarena * __amlc_arena_new(aisolate * const iso) {
    aarena * a = iso->free_arenas;
    if (a != NULL) {
        iso->free_arenas = a->next_free;
        iso->free_arena_count--;
        a->next_free = NULL;
        a->used = 0;
        return a;
    }
    a = (aarena *) malloc(sizeof(aarena) + AMLC_ARENA_SIZE + AMLC_ARENA_ALIGN);
    if (a == NULL) return NULL;
    a->data = (char *) AMLC_ARENA_ROUND((size_t) ((char *) a + sizeof(aarena)));
    a->capacity = AMLC_ARENA_SIZE;
    a->used = 0;
    a->live = 0;
    a->next_free = NULL;
    a->next = iso->arenas;
    a->prev = NULL;
    if (iso->arenas != NULL) iso->arenas->prev = a;
    iso->arenas = a;
    return a;
}

aobject * __amlc_arena_allocate(aisolate * const iso, size_t const size) {
    size_t const slot = AMLC_ARENA_ROUND(sizeof(aarena *)) + AMLC_ARENA_ROUND(size);
    if (slot > AMLC_ARENA_SIZE) {
        return NULL;    // too big for an arena: the caller callocs it
    }
    aarena * a = iso->arena;
    if (a == NULL || a->used + slot > a->capacity) {
        a = __amlc_arena_new(iso);
        if (a == NULL) return NULL;
        iso->arena = a;
    }
    char * base = a->data + a->used;
    a->used += slot;
    a->live++;
    aobject * obj = (aobject *) (base + AMLC_ARENA_ROUND(sizeof(aarena *)));
    ((aarena **) obj)[-1] = a;
    memset(obj, 0, size);
    return obj;
}

void __amlc_arena_recycle(aisolate * const iso, aarena * const a) {
    a->used = 0;
    if (a == iso->arena) {
        return;                 // stays current, refills from the start
    }
    if (iso->free_arena_count < AMLC_ARENA_KEEP_FREE) {
        a->next_free = iso->free_arenas;
        iso->free_arenas = a;
        iso->free_arena_count++;
        return;
    }
    // Above the keep limit: hand it back. O(1) unlink — walking `arenas` from
    // the head cost a scan per freed arena, which bites once a program holds
    // hundreds of them.
    if (a->prev != NULL) a->prev->next = a->next; else iso->arenas = a->next;
    if (a->next != NULL) a->next->prev = a->prev;
    free(a);
}

void __amlc_free_object_in(aisolate * const iso, aobject * const obj) {
    if (!obj->chunked) {
        free(obj);
        return;
    }
    aarena * a = ((aarena **) obj)[-1];
    if (--a->live == 0) {
        __amlc_arena_recycle(iso, a);
    }
}

void __amlc_free_object(aobject * const obj) {
    __amlc_free_object_in(__amlc_isolate(), obj);
}

void __amlc_arenas_free_all(aisolate * const iso) {
    aarena * a = iso->arenas;
    while (a != NULL) {
        aarena * next = a->next;
        free(a);
        a = next;
    }
    iso->arenas = NULL;
    iso->arena = NULL;
    iso->free_arenas = NULL;
    iso->free_arena_count = 0;
}

// ---- isolate lifecycle ----------------------------------------------------
static aisolate * __amlc_main_isolate = NULL;

static void __amlc_isolate_alloc_statics(aisolate * const iso) {
    free(iso->statics);
    iso->statics_count = __amlc_static_slot_count;
    iso->statics = (property *) calloc(iso->statics_count > 0 ? iso->statics_count : 1, sizeof(property));
    if (iso->statics == NULL) {
        fprintf(stderr, "AmLang: out of memory creating an isolate\n");
        abort();
    }
}

static aisolate * __amlc_isolate_alloc(void) {
    aisolate * iso = (aisolate *) calloc(1, sizeof(aisolate));
    if (iso == NULL) {
        fprintf(stderr, "AmLang: out of memory creating an isolate\n");
        abort();
    }
    iso->mailbox = __amlc_mailbox_create();
    if (iso->mailbox == NULL) {
        fprintf(stderr, "AmLang: out of memory creating an isolate\n");
        abort();
    }
    iso->handle_count = 1;
    iso->stack_size = 4000 * 1024;
    return iso;
}

// The main isolate exists before any class has registered (string constants
// and annotations allocate during startup), so its statics are sized here,
// once the generated startup has run every __register_class.
void __amlc_isolate_statics_ready(void) {
    __amlc_isolate_alloc_statics(__amlc_isolate());
}

void __amlc_isolate_enter(aisolate * const iso) {
    __amlc_bind_isolate(iso);
    __amlc_mailbox_bind(iso->mailbox);
    iso->started = true;
}

void __amlc_isolate_boot_main(void) {
    aisolate * iso = __amlc_isolate_alloc();
    __amlc_main_isolate = iso;
    __amlc_main_isolate_ptr = iso;
    __amlc_isolate_enter(iso);
}

aisolate * __amlc_isolate_create(void) {
    aisolate * iso = __amlc_isolate_alloc();
    __amlc_isolate_alloc_statics(iso);
    iso->parent = __amlc_isolate();
    if (iso->parent != NULL) {
        __amlc_isolate_retain(iso->parent);
    }
    return iso;
}

void __amlc_isolate_run_static_inits(void) {
    for (unsigned int i = 0; i < __amlc_static_init_count; i++) {
        function_result r = __amlc_static_init_table[i]();
        if (r.exception != NULL) {
            printf("Exception in a static initializer\n");
            __decrease_reference_count(r.exception);
        }
    }
}

void __amlc_isolate_retain(aisolate * const iso) {
    __amlc_global_lock();
    iso->handle_count++;
    __amlc_global_unlock();
}

static void __amlc_isolate_free(aisolate * const iso) {
    // No handle points here any more and the thread (if it ever ran) is
    // done, so nothing can post or read: drain what is left into the
    // calling isolate and release it there.
    for (;;) {
        amessage * m = __amlc_mailbox_take(iso->mailbox, false);
        if (m == NULL) break;
        __amlc_message_discard(m);
    }
    if (iso->boot != NULL) {
        __amlc_message_discard(iso->boot);
        iso->boot = NULL;
    }
    __amlc_mailbox_destroy(iso->mailbox);
    free(iso->statics);
    free(iso->name);
    free(iso->native);
    aisolate * parent = iso->parent;
    free(iso);
    if (parent != NULL) {
        __amlc_isolate_release(parent);
    }
}

void __amlc_isolate_release(aisolate * const iso) {
    __amlc_global_lock();
    int n = --iso->handle_count;
    __amlc_global_unlock();
    if (n == 0) {
        __amlc_isolate_free(iso);
    }
}

void __amlc_isolate_teardown(aisolate * const iso) {
    // Runs on iso's own thread, after the runnable returned. Mirrors the
    // main program's exit sequence for this isolate's heap.
    __amlc_mailbox_close(iso->mailbox);
    for (;;) {
        amessage * m = __amlc_mailbox_take(iso->mailbox, false);
        if (m == NULL) break;
        __amlc_message_discard(m);
    }
    __dereference_static_properties();
    __mark_root_objects();
    __sweep_unmarked_objects();
    #ifdef TRACKOBJECTS
    if (iso->allocation_count != 0) {
        printf("Thread '%s' exited with %d objects still allocated\n", iso->name != NULL ? iso->name : "thread", iso->allocation_count);
    }
    #endif
    free(iso->statics);
    iso->statics = NULL;
    iso->statics_count = 0;
    __amlc_arenas_free_all(iso);
    __amlc_global_lock();
    iso->done = true;
    __amlc_global_unlock();
}

// ---- deep copy ------------------------------------------------------------
// Copies an object graph into fresh objects of the calling isolate. Shared
// sub-objects and cycles are preserved through a src→dst table. Every
// object comes back with ONE owned reference for the caller to place.
typedef struct { aobject * src; aobject * dst; } __amlc_copy_pair;
typedef struct {
    __amlc_copy_pair * tab;
    unsigned int cap;      // power of two
    unsigned int count;
    const char * failed_class;
} __amlc_copy_ctx;

static aobject * __amlc_copy_lookup(__amlc_copy_ctx * const ctx, aobject * const src) {
    if (ctx->tab == NULL) return NULL;
    unsigned int h = (unsigned int) (((unsigned long) src) >> 4) & (ctx->cap - 1);
    for (unsigned int i = 0; i < ctx->cap; i++) {
        __amlc_copy_pair * p = &ctx->tab[(h + i) & (ctx->cap - 1)];
        if (p->src == NULL) return NULL;
        if (p->src == src) return p->dst;
    }
    return NULL;
}

static void __amlc_copy_insert_raw(__amlc_copy_pair * const tab, unsigned int const cap, aobject * const src, aobject * const dst) {
    unsigned int h = (unsigned int) (((unsigned long) src) >> 4) & (cap - 1);
    for (unsigned int i = 0; i < cap; i++) {
        __amlc_copy_pair * p = &tab[(h + i) & (cap - 1)];
        if (p->src == NULL) { p->src = src; p->dst = dst; return; }
    }
}

static void __amlc_copy_record(__amlc_copy_ctx * const ctx, aobject * const src, aobject * const dst) {
    if (ctx->tab == NULL || (ctx->count + 1) * 2 > ctx->cap) {
        unsigned int ncap = ctx->cap == 0 ? 64 : ctx->cap * 2;
        __amlc_copy_pair * ntab = (__amlc_copy_pair *) calloc(ncap, sizeof(__amlc_copy_pair));
        if (ntab == NULL) { ctx->failed_class = "(out of memory)"; return; }
        for (unsigned int i = 0; i < ctx->cap; i++) {
            if (ctx->tab[i].src != NULL) __amlc_copy_insert_raw(ntab, ncap, ctx->tab[i].src, ctx->tab[i].dst);
        }
        free(ctx->tab);
        ctx->tab = ntab;
        ctx->cap = ncap;
    }
    __amlc_copy_insert_raw(ctx->tab, ctx->cap, src, dst);
    ctx->count++;
}

static aobject * __amlc_copy_object(__amlc_copy_ctx * const ctx, aobject * const src) {
    if (src == NULL || ctx->failed_class != NULL) return NULL;
    aobject * found = __amlc_copy_lookup(ctx, src);
    if (found != NULL) {
        __increase_reference_count(found);
        return found;
    }
    aclass * const cls = src->class_ptr;
    aobject * dst = NULL;

    if (cls->statics->type == interface) {
        // Interface reference: copy the implementation, re-wrap it.
        aobject * impl = __amlc_copy_object(ctx, src->object_properties.iface_reference.implementation_object);
        if (impl == NULL) return NULL;
        dst = __allocate_iface_object(cls, impl);   // takes its own reference on impl
        __decrease_reference_count(impl);
        if (dst == NULL) { ctx->failed_class = "(out of memory)"; return NULL; }
        __amlc_copy_record(ctx, src, dst);
        return dst;
    }

    if (cls->copy != NULL) {
        // Sendable native class: its hook clones the native payload.
        dst = ((__amlc_native_copy_T) cls->copy)(src, ctx);
        if (dst == NULL) {
            if (ctx->failed_class == NULL) ctx->failed_class = cls->name;
            return NULL;
        }
        if (__amlc_copy_lookup(ctx, src) == NULL) __amlc_copy_record(ctx, src, dst);
        return dst;
    }

    if (cls->release != NULL) {
        // A native class that did not opt in: it owns handles the runtime
        // cannot clone (files, sockets, ...).
        ctx->failed_class = cls->name;
        return NULL;
    }

    // Plain class: allocate, then copy the slots.
    dst = __allocate_object_with_extra_size(cls, cls->extra_size);
    if (dst == NULL) { ctx->failed_class = "(out of memory)"; return NULL; }
    __amlc_copy_record(ctx, src, dst);
    if (cls->extra_size > 0) {
        size_t off = sizeof(aobject) + sizeof(property) * cls->properties_count;
        memcpy((char *) dst + off, (char *) src + off, cls->extra_size);
    }
    if (__is_primitive(src->object_properties.class_object_properties.object_data)) {
        dst->object_properties.class_object_properties.object_data = src->object_properties.class_object_properties.object_data;
    }
    for (unsigned int i = 0; i < cls->properties_count; i++) {
        nullable_value const nv = src->object_properties.class_object_properties.properties[i].nullable_value;
        property * const slot = &dst->object_properties.class_object_properties.properties[i];
        if (__is_primitive(nv) || nv.value.object_value == NULL) {
            slot->nullable_value = nv;
            continue;
        }
        aobject * child = __amlc_copy_object(ctx, nv.value.object_value);
        if (child == NULL) return NULL;   // failed (partial graph is released by the caller)
        slot->nullable_value.flags = 0;
        slot->nullable_value.value.object_value = child;
        __increase_property_reference_count(child);
        __decrease_reference_count(child);
    }
    return dst;
}

aobject * __amlc_copy_child(void * const copy_ctx, aobject * const src) {
    return __amlc_copy_object((__amlc_copy_ctx *) copy_ctx, src);
}

// Splice `chain` (a detached first_object list) in front of the calling
// isolate's list.
static void __amlc_splice_chain(aobject * const chain) {
    if (chain == NULL) return;
    aisolate * const iso = __amlc_isolate();
    aobject * tail = chain;
    while (tail->next != NULL) tail = tail->next;
    tail->next = iso->first_object;
    if (iso->first_object != NULL) iso->first_object->prev = tail;
    chain->prev = NULL;
    iso->first_object = chain;
}

amessage * __amlc_message_create(aobject * const payload, function_result * const result) {
    aisolate * const iso = __amlc_isolate();
    // Stage: every property-referenced object the copy creates lands on an
    // empty list that becomes the message's chain, so the receiver can
    // adopt the whole graph in O(1) and the sender's own list is untouched.
    aobject * const saved_first = iso->first_object;
    int const saved_count = iso->allocation_count;
    iso->first_object = NULL;
    // The copies belong to another isolate: never bump-allocate them from
    // this isolate's fast-lane arena, whatever block we are inside.
    int const saved_fast_depth = iso->fast_depth;
    iso->fast_depth = 0;

    __amlc_copy_ctx ctx = { .tab = NULL, .cap = 0, .count = 0, .failed_class = NULL };
    aobject * copy = __amlc_copy_object(&ctx, payload);
    free(ctx.tab);
    iso->fast_depth = saved_fast_depth;

    if (ctx.failed_class != NULL || copy == NULL) {
        // Undo: put the partial chain back where a release can reach it,
        // drop the root, report.
        aobject * chain = iso->first_object;
        iso->first_object = saved_first;
        __amlc_splice_chain(chain);
        if (copy != NULL) __decrease_reference_count(copy);
        char msg[256];
        snprintf(msg, sizeof(msg), "Object of type %s is not sendable between threads (native class without a copy hook)",
                 ctx.failed_class != NULL ? ctx.failed_class : (payload != NULL ? payload->class_ptr->name : "null"));
        __throw_simple_exception_copy(msg, "in Thread.send", result);
        return NULL;
    }

    amessage * m = (amessage *) malloc(sizeof(amessage));
    if (m == NULL) {
        aobject * chain = iso->first_object;
        iso->first_object = saved_first;
        __amlc_splice_chain(chain);
        __decrease_reference_count(copy);
        __throw_simple_exception("Out of memory while sending a message", "in Thread.send", result);
        return NULL;
    }
    m->next = NULL;
    m->payload = copy;
    m->chain = iso->first_object;
    iso->first_object = saved_first;
    // The copies now belong to the message, not to this isolate's count;
    // the receiver adds them back when it opens the message.
    m->count = iso->allocation_count - saved_count;
    iso->allocation_count = saved_count;
    return m;
}

aobject * __amlc_message_open(amessage * const msg) {
    __amlc_splice_chain(msg->chain);
    __amlc_isolate()->allocation_count += msg->count;
    aobject * p = msg->payload;
    free(msg);
    return p;
}

void __amlc_message_discard(amessage * const msg) {
    aobject * p = __amlc_message_open(msg);
    __decrease_reference_count(p);
}

bool __amlc_send(aisolate * const target, aobject * const payload, function_result * const result) {
    if (payload == NULL) {
        __throw_simple_exception("Thread.send: the message is null", "in Thread.send", result);
        return false;
    }
    amessage * m = __amlc_message_create(payload, result);
    if (m == NULL) return false;
    if (!__amlc_mailbox_post(target->mailbox, m)) {
        // The receiver is gone: the message dies here, quietly.
        __amlc_message_discard(m);
    }
    return true;
}

aobject * __amlc_receive(bool const wait) {
    amessage * m = __amlc_mailbox_take(__amlc_isolate()->mailbox, wait);
    if (m == NULL) return NULL;
    return __amlc_message_open(m);
}
