#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
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

// Instrumented setter for the global object-list head. Quiet on normal
// writes — only logs when the new value matches the corruption
// fingerprint we're hunting. The site label says which of the three
// active __first_object writers (increase / decrease / detach) saw
// the bad value, so the chain back to the caller is one greppable
// hop away.
static inline void __set_first_object(aobject *v, const char *site) {
    if (__is_suspicious_object_ptr(v)) {
        printf("[firstobj.set] SUSPICIOUS site=%s value=%p\n", site, (void*)v);
        fflush(stdout);
        exit(0);
    }
    __first_object = v;
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

// Resolve an aobject* through a (possibly-)wrapper. A wrapper has
// `class_ptr == NULL` and stores the real aobject in its
// object_wrapper variant.
//
// Runtime flag `__amlc_any_wrappers_alive` gates the real ternary.
// Programs that never mint wrappers pay one load + branch per
// property read — and `-O3` is smart enough to hoist the load out of
// inner loops so it's nearly free. Programs that do mint wrappers
// take the wrapper-following branch only on the wrappers themselves.
//
// The flag is set in `__create_wrapper` (see core.c); never cleared.
//
// HISTORY: this used to be `#define __unwrap(__obj) (__obj)` at -O0
// (gated on `__OPTIMIZE__`) out of concern for per-call-site stack
// temps in huge generated functions. That made every cross-thread
// program built without `-O` corrupt the heap: `__wrap_if_foreign`
// still minted wrappers at -O0, but nothing followed them, so array
// accesses computed `&wrapper[1]` and read/wrote past the end of the
// wrapper allocation (SIGSEGV inside malloc a few hundred allocations
// later). The macro is a pure pointer ternary — no compound literals,
// no statement expressions — so it does not reserve per-call-site
// stack even at -O0; the multi-MB-frame problem belonged to the old
// `static inline` helpers, not to this macro. Keep wrap and unwrap
// active in ALL builds so they can never disagree again.
#define __unwrap(__obj) \
    (__amlc_any_wrappers_alive \
        ? ((__obj) == NULL ? NULL \
            : ((__obj)->class_ptr != NULL \
                ? (__obj) \
                : (__obj)->object_properties.object_wrapper.wrapped_object)) \
        : (__obj))

// Read a property's stored nullable_value, transparently unwrapping if
// `__obj` is a cross-thread wrapper. Centralised so the wrapper rule
// lives in one place — the codegen emits this at property-read sites
// instead of chasing `__obj->object_properties.class_object_properties.properties[…]`
// directly, which would read garbage off a wrapper's union variant.
//
// Implemented as a macro rather than `static inline` because at gcc -O0
// (the default for am-lang builds), even `always_inline` wraps the
// return in a stack-allocated `nullable_value` temporary per call site.
// In huge functions with thousands of property reads (e.g.
// `JsBytecodeVm.run` — 83k lines, many reads per opcode) those 12-byte
// temps accumulate into multi-MB frames and overflow the C stack on
// the JS interpreter's recursive eval. A macro is pure text
// substitution: same assembly as the original direct chain.
#define __get_property_nv(__obj, __index) \
    (__unwrap(__obj)->object_properties.class_object_properties.properties[__index].nullable_value)

// Pointer form — needed when codegen wants the slot's address rather
// than its value. Same macro rationale as above.
#define __get_property_nv_ptr(__obj, __index) \
    (&__unwrap(__obj)->object_properties.class_object_properties.properties[__index].nullable_value)

// Wrapper finalizer — invoked when a wrapper aobject's own
// reference_count reaches zero. Unsubscribes the wrapper from the
// real's `first_object_wrapper` list under the shared mutex, decs the
// real's reference_count by 1 (this thread was holding one ref on the
// real on behalf of the wrapper), and frees the wrapper struct. If
// the real's count subsequently reaches zero, the standard deallocator
// runs recursively.
void __deallocate_wrapper(aobject * const __wrapper);

static inline void __decrease_reference_count(aobject * const __obj) {
    if ( __obj != NULL) {
        #ifdef DEBUG
        __print_memory_header(__obj, "Decrease reference count");
        #endif

        // Thread-safe ARC (BRC): a non-owner thread releases via the atomic
        // foreign counter, never the owner-local `reference_count`. The
        // decrement + free-decision run UNDER the shared lock so they linearise
        // against the owner's rc==0 check — otherwise the owner could observe
        // foreign_reference_count==0 and free the object in the window between a
        // lock-free decrement and this coordination read (use-after-free).
        // (Foreign *increments* stay lock-free: you can only retain through an
        // already-live reference, which itself pins the object.) Gated on
        // __amlc_multithreaded so single-threaded programs skip __current_thread().
        if (__amlc_multithreaded && __obj->owner_thread != __current_thread()) {
            __arc_shared_lock();
            int __after = __amlc_atomic_fetch_sub(&__obj->foreign_reference_count, 1) - 1;
            bool __destroy_foreign = false;
            if (__after == 0
                    && __obj->owner_gone
                    && __obj->property_reference_count == 0
                    && __obj->first_object_wrapper == NULL
                    && !__obj->destruction_claimed) {
                __obj->destruction_claimed = true;
                __destroy_foreign = true;
            }
            __arc_shared_unlock();
            if (__destroy_foreign) {
                __deallocate_object(__obj);
            }
            return;
        }

        __obj->reference_count--;
        #if defined(DEBUG) && defined(ARCLOG)
        #ifdef CONDLOG
        if (__conditional_logging_on) {
        #endif
        // Wrapper aobjects have NULL class_ptr — guard the debug print
        // so it doesn't NPE when chasing class_ptr->name on a wrapper.
        if (__obj->class_ptr != NULL) {
            printf("decrease reference count of object of type %s (address: %p, object_id: %d), property reference count %d, new reference count %d\n", __obj->class_ptr->name, __obj, __obj->object_properties.class_object_properties.object_id, __obj->property_reference_count, __obj->reference_count);
        } else {
            printf("decrease reference count of wrapper (address: %p, wrapped: %p), new reference count %d\n", __obj, __obj->object_properties.object_wrapper.wrapped_object, __obj->reference_count);
        }
        #ifdef CONDLOG
        }
        #endif
        #endif

        if ( __obj->reference_count == 0) {
            // Owner reads its OWN reference_count above (own write,
            // sequentially consistent — no lock needed). We do NOT
            // touch property_reference_count until inside the lock —
            // it's mutated by foreign threads, so an unlocked read
            // can be stale and miss the destruction trigger.
            if (__obj->class_ptr == NULL) {
                // Wrappers go through their own finalizer — they don't
                // have class_ptr->release, just a subscription entry to
                // unlink. Wrapper's reference_count is purely per-thread,
                // no other thread mutates it, so no lock needed here —
                // BUT propref IS bumped by every cross-thread array/
                // collection store (worker writes wrapper into main's
                // mainQueue → propref=1; main wraps Task and stores in
                // drained → propref=1). If we destroy on rc=0 alone the
                // wrapper dies the moment its local goes out of scope
                // and the now-stale items_a[i] becomes a UAF; the next
                // List.get returns a freed pointer, which __wrap_if_foreign
                // dereferences and either self-loops (memory zeroed) or
                // segfaults (memory reused). Same rc==0 ∧ propref==0
                // guard as the real-aobject path below, just no Stage 9
                // wrapper-subscription dance — wrappers themselves are
                // never subscribed to by other wrappers.
                if (__obj->property_reference_count == 0) {
                    __deallocate_wrapper(__obj);
                }
                return;
            }
            // Real-aobject end-of-life protocol (Stage 9):
            //   1. Take the shared lock — coordination point with
            //      foreign-thread propref-dec and wrapper-death paths.
            //   2. If property_reference_count > 0, the object is
            //      still alive via property holders. Do nothing —
            //      eventual propref-dec will trigger destruction.
            //   3. If property_reference_count == 0 and there are no
            //      foreign wrappers, claim destruction and run the
            //      destructor outside the lock.
            //   4. If property_reference_count == 0 but foreign
            //      wrappers exist, set `owner_gone = true` and leave
            //      the object alive — the last wrapper to die will
            //      finalize. Invariant: owner_gone implies the
            //      object's only liveness is via the wrapper list.
            //      `__increase_property_reference_count` clears
            //      owner_gone if propref grows back, so wrappers can
            //      safely consult owner_gone alone (without re-checking
            //      propref) on their own death path.
            __arc_shared_lock();
            bool destroy_now = false;
            if (__obj->property_reference_count == 0) {
                // BRC: "foreign holders remain?" is `foreign_reference_count != 0`
                // (read under the same lock foreign decrements take). No foreign
                // holders and no propref → claim & destroy; otherwise set
                // owner_gone and let the last foreign release finalise.
                if (__obj->first_object_wrapper == NULL
                        && __amlc_atomic_load(&__obj->foreign_reference_count) == 0) {
                    if (!__obj->destruction_claimed) {
                        __obj->destruction_claimed = true;
                        destroy_now = true;
                    }
                } else {
                    __obj->owner_gone = true;
                }
            }
            __arc_shared_unlock();
            if (destroy_now) {
                __deallocate_object(__obj);
            }
        }
    }
}

// DEPRECATED — no longer emitted by the compiler; kept only so stale
// generated C from older compiler builds still links. It probed
// `class_ptr` on a possibly-BORROWED pointer, which is a use-after-free
// when the borrowed real died before scope exit (e.g. HashMap.set reads
// `this.keys`, resize overwrites the property, old array freed — the
// probe then read freed memory and could double-release whatever the
// allocator had reused it for). Slot reads now take OWNED handles via
// `__retain_slot_read` and release with a plain dec.
static inline void __release_if_wrapper(aobject * const __obj) {
    if (__obj != NULL && __obj->class_ptr == NULL) {
        __decrease_reference_count(__obj);
    }
}

static inline void __increase_reference_count(aobject * const __obj) {
    #ifdef DEBUG
    __print_memory_header(__obj, "Increase reference count");
    #endif

    // Thread-safe ARC (BRC): a non-owner thread bumps the foreign counter
    // instead of the owner-local `reference_count`, so the two never race.
    // The bump runs UNDER the shared lock — the same lock every free decision
    // and foreign release takes — so a foreign retain can never interleave
    // with a concurrent "all counters zero → destroy" check (the retain either
    // lands before the check, keeping the object alive, or after a claim that
    // was only legal when no counted reference to retain FROM existed). On
    // AmigaOS the lock is an inline Forbid/Permit. Gated on
    // __amlc_multithreaded so single-threaded programs skip __current_thread().
    if (__amlc_multithreaded && __obj->owner_thread != __current_thread()) {
        __arc_shared_lock();
        __amlc_atomic_fetch_add(&__obj->foreign_reference_count, 1);
        __arc_shared_unlock();
        return;
    }

    __obj->reference_count++;
    #if defined(DEBUG) && defined(ARCLOG)
    #ifdef CONDLOG
    if (__conditional_logging_on) {
    #endif
    if (__obj->class_ptr != NULL) {
    printf("increase reference count of object of type %s (address: %p, object_id: %d), propert_reference_count: %d, new reference count: %d\n", __obj->class_ptr->name, __obj, __obj->object_properties.class_object_properties.object_id, __obj->property_reference_count, __obj->reference_count);
    } else {
    printf("increase reference count of wrapper (address: %p, wrapped: %p), new reference count: %d\n", __obj, __obj->object_properties.object_wrapper.wrapped_object, __obj->reference_count);
    }
/*
    printf("increase reference count (address: %p)\n", __obj);
    printf("increase reference count (address: %p)\n", __obj->class_ptr);
    printf("increase reference count (address: %p)\n", __obj->class_ptr->name);
    printf("increase reference count of object of type %s\n", __obj->class_ptr->name);
    printf("increase reference count (object_id: %d)\n", __obj->object_properties.class_object_properties.object_id);
    printf("increase reference count (new reference count %d)\n", __obj->reference_count);
*/
    #ifdef CONDLOG
    }
    #endif
    #endif
}

// Retain a function's object return value on behalf of the caller.
// Emitted by codegen at every `return <object>` site, replacing the old
// unconditional `__increase_reference_count_nullable_value(__result.
// return_value)`.
//
// Why the old form leaked: `reference_count` is owner-thread-local by
// design. When the returned object is a REAL owned by another thread
// (e.g. a chunk/block created on a generator thread, fetched by the
// main thread through a getter), the +1 was (a) an unsynchronised
// write to a foreign counter and (b) never released — the caller's
// scope-exit dec lands on the WRAPPER that `__wrap_if_foreign` mints
// around the result, not on the real. One pinned reference per
// cross-thread call → objects that should die on release stay alive
// forever (the "memory creeps up while chunks stream" leak).
//
// New protocol — the caller's ref always lives on a thread-local object:
//   - primitive / NULL: nothing to do.
//   - wrapper or same-thread real: plain rc+1 (thread-local, safe).
//     Single-threaded programs take exactly this path → no change.
//   - foreign real: mint a wrapper (born with rc=1 = the caller's ref,
//     subscribed to the real so it can't be destroyed underneath us)
//     and return THAT. The caller's `__wrap_if_foreign` passes wrappers
//     through, and its scope-exit dec releases the wrapper, which
//     finalizes the real iff the owner side already drained (owner_gone).
// Take an OWNED, thread-safe handle on a value read out of a shared slot
// (object-array element, collection backing store). Same contract as
// __retain_function_return: wrappers and same-thread reals get a plain
// thread-local +1; a foreign real gets a fresh owned wrapper instead —
// mutating a foreign real's reference_count is a lost-update race that
// can pin it above zero forever (it is owner-thread-local by design).
// The caller releases with a plain __decrease_reference_count.
static inline aobject * __retain_slot_read(aobject * const __raw) {
    // Thread-safe ARC (BRC): take an OWNED handle on a value read from a shared
    // slot. No wrappers — the SAME real pointer is returned; the retain is
    // routed to the owner-local `reference_count` or the atomic
    // `foreign_reference_count` by __increase_reference_count based on the
    // calling thread. The caller releases with a plain __decrease_reference_count.
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
    #ifdef WRAPLOG
    if (__obj != NULL && __obj->class_ptr == NULL) {
        fprintf(stderr, "[inc_pr] wrapper=%p propref %d->%d\n", __obj, __obj->property_reference_count, __obj->property_reference_count + 1);
        fflush(stderr);
    }
    #endif
    // Bottleneck tripwire — every path that eventually mutates
    // __first_object via increase comes through here, including the
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
    // Thread-safe ARC: __first_object is the global head of every live
    // object's intrusive list. Both the head and each node's prev/next
    // are torn under concurrent property writes from multiple threads.
    // Lock the link/unlink + counter mutation together. Recursive mutex
    // → safe to call from inside __set_property which also takes it.
    __arc_shared_lock();
    if (__obj->property_reference_count == 0) {
        __obj->next = __first_object;

        if (__first_object != NULL) {
            __first_object->prev = __obj;
        }
        __set_first_object(__obj, "increase_property_reference_count");
        // Stage 9 invariant: owner_gone means "rc, propref, wrappers
        // were all observed at zero (owner-side) and the only thing
        // keeping me alive is the wrapper list". If property_ref grows
        // back to 1 (e.g. a foreign thread stores this object into
        // another property), the object is now also alive via that
        // property holder — owner_gone no longer holds. Clear it here
        // under the same lock that wrapper-death uses to read it.
        __obj->owner_gone = false;
    }
    __obj->property_reference_count++;
    __arc_shared_unlock();
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

static inline void __set_property(aobject * const __obj_in, int const __index, nullable_value __prop_value) {
    // Thread-safe ARC: unwrap both the receiver AND the stored value.
    //   - Receiver: a wrapper has no property array; writing direct
    //     would corrupt `wrapped_object`.
    //   - Stored value: fields only ever hold REAL aobjects. If a
    //     wrapper were stored, downstream threads reading the field
    //     would chase wrapper-of-wrapper. By unwrapping at write
    //     time, every property's stored aobject* is a real; the
    //     reader decides whether to mint a fresh wrapper based on
    //     its OWN thread identity.
    aobject * const __obj = __unwrap(__obj_in);
    if (!__is_primitive(__prop_value) && __prop_value.value.object_value != NULL) {
        __prop_value.value.object_value = __unwrap(__prop_value.value.object_value);
    }
    // Thread-safe ARC: take the shared lock for the whole read-modify-
    // write of `properties[__index].nullable_value`. Without it, two
    // threads writing different properties on the SAME aobject race on
    // the per-object property_reference_count chain (the inc/dec helpers
    // also take the lock, but the read of the old value + the write
    // of the new value need to be one critical section, else a reader
    // can see a torn old/new pair). Recursive mutex → nested inc/dec
    // re-entries are safe.
    __arc_shared_lock();
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
    __arc_shared_unlock();
}

static inline bool __set_property_safe(aobject * const __obj, int const __index, nullable_value __prop_value) {
    // Thread-safe ARC: same single-critical-section reasoning as
    // __set_property — the type-check + ref count adjust + slot write
    // must be one transaction. Recursive mutex lets inc/dec re-enter.
    __arc_shared_lock();
    property * __prop = &__obj->object_properties.class_object_properties.properties[__index];
    ctype old_type = __value_flags_to_ctype(__prop->nullable_value.flags);
    ctype new_type = __value_flags_to_ctype(__prop_value.flags);
    if (old_type != new_type) {
        __arc_shared_unlock();
        return false;
    }


    if (new_type == object_type) {
        if (!is_descendant_of(__prop_value.value.object_value->class_ptr, __prop->nullable_value.value.object_value->class_ptr)) {
            __arc_shared_unlock();
            return false;
        }
    } else if (__is_primitive_nullable(__prop_value) && !__is_primitive_nullable(__prop->nullable_value)) {
        // If new value is a nullable primitive, check of the property supports that
        __arc_shared_unlock();
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
    __arc_shared_unlock();
    return true;
}

static inline void __set_static_property(class_static * const __class_static, int const __index, nullable_value __prop_value) {
    // Thread-safe ARC: static slots are inherently shared across threads,
    // so a write must be atomic w.r.t. concurrent readers/writers.
    __arc_shared_lock();
    property * __prop = &__class_static->static_properties[__index];
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
    __arc_shared_unlock();
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
