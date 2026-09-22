#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

// ---------------------------------------------------------------------------
// Threading model: ISOLATES (share-nothing).
//
// Every AmLang thread is an isolate: it owns its objects, its own copy of
// every mutable static property, and a mailbox. Nothing is shared between
// isolates except immutable class metadata (aclass, vtables, string-constant
// bytes), so reference counts are plain non-atomic ++/-- and the object hot
// paths take no lock at all. The only cross-isolate operations are
//   * Thread.send / Thread.receive — the payload graph is deep-copied by the
//     sender into a staging list and spliced into the receiver's heap when
//     received (__amlc_send / __amlc_receive), and
//   * the isolate handle counts and the mailbox queues, guarded by the small
//     global lock (__amlc_global_lock: a pthread mutex, or Forbid/Permit on
//     the Amiga family).
// See am-lang-compiler-code/docs/TASK_ISOLATE_THREADS.md.
// ---------------------------------------------------------------------------


//#define CLASS_TYPE_PRIMITIVE 1
//#define CLASS_TYPE_NORMAL 0


// 10000000 = primitive
// 10000001 = primitive + nullable
// 10000011 = primitive + null
// 10000010 = invalid state because it's null but not nullable
// 10000100 = unsigned byte (1 << 0)
// 10010100 = unsigned short (1 << 1)
// 10100100 = unsigned int (1 << 2)
// 10110100 = unsigned long (1 << 3) 
// 10000000 = signed byte (1 << 0)
// 10010000 = signed short (1 << 1)
// 10100000 = signed int (1 << 2)
// 10110000 = signed long (1 << 3)
// 10101000 = float (1 << 2)
// 10111000 = double (1 << 3)
// 11000000 = bool false
// 11010000 = bool true
// 00000000 = object
#define PRIMITIVE_NULLABLE 1 // can it be null?
#define PRIMITIVE_NULL 2 // is it null?
#define PRIMITIVE_UNSIGNED 4 // is it unsigned?
#define PRIMITIVE_FLOATING_POINT_NUMBER 8
#define PRIMITIVE_BYTE_SIZE_EXP_1 16
#define PRIMITIVE_BYTE_SIZE_EXP_2 32
#define PRIMITIVE 128

#define PRIMITIVE_BOOL 64 | PRIMITIVE // is it a bool type?
//#define PRIMITIVE_BOOL_FALSE PRIMITIVE_BOOL
//#define PRIMITIVE_BOOL_TRUE PRIMITIVE_BOOL | 16 

#define PRIMITIVE_LONG PRIMITIVE_BYTE_SIZE_EXP_1 | PRIMITIVE_BYTE_SIZE_EXP_2 | PRIMITIVE
#define PRIMITIVE_INT PRIMITIVE_BYTE_SIZE_EXP_2 | PRIMITIVE
#define PRIMITIVE_SHORT PRIMITIVE_BYTE_SIZE_EXP_1 | PRIMITIVE
#define PRIMITIVE_BYTE PRIMITIVE
#define PRIMITIVE_CHAR PRIMITIVE_BYTE

#define PRIMITIVE_ULONG PRIMITIVE_LONG | PRIMITIVE_UNSIGNED
#define PRIMITIVE_UINT PRIMITIVE_INT | PRIMITIVE_UNSIGNED
#define PRIMITIVE_USHORT PRIMITIVE_SHORT | PRIMITIVE_UNSIGNED
#define PRIMITIVE_UBYTE PRIMITIVE_BYTE | PRIMITIVE_UNSIGNED
#define PRIMITIVE_UCHAR PRIMITIVE_UBYTE

#define PRIMITIVE_DOUBLE PRIMITIVE_LONG | PRIMITIVE_FLOATING_POINT_NUMBER
#define PRIMITIVE_FLOAT PRIMITIVE_INT | PRIMITIVE_FLOATING_POINT_NUMBER

#define MODIFIER_PRIVATE 1
#define MODIFIER_PROTECTED 2
#define MODIFIER_STATIC 4


/*
BYTE_SIZE_EXP lookup (order: 2,1)
00 => 0 => 1^0 => 1
01 => 1 => 2^1 => 2
10 => 2 => 2^2 => 4
11 => 3 => 2^3 => 8
*/
// BYTE_SIZE_EXP 111 = 7

/*
#define PRIMITIVE_LONG 4 // is it a long?
#define PRIMITIVE_INT 5
#define PRIMITIVE_SHORT 6
#define PRIMITIVE_CHAR 7
#define PRIMITIVE_BOOL 8
*/
// #define PRIMITIVE_ALL PRIMITIVE_LONG | PRIMITIVE_INT | PRIMITIVE_SHORT | PRIMITIVE_CHAR | PRIMITIVE_BOOL // just check if the flags are 0 to see if its an object and not a primitive

extern bool __conditional_logging_on;

typedef struct _aobject aobject;
typedef struct _class_static class_static;
typedef struct _aclass aclass;
typedef void (*__anonymous_function)();
//typedef struct _stack_trace_item stack_trace_item;
//typedef struct _exception_holder exception_holder;
typedef struct _function_result function_result;
typedef union _value value;
typedef struct _nullable_value nullable_value;
typedef struct _property property;
typedef struct _property_info property_info;
typedef struct _iface_implementation iface_implementation;
typedef struct _iface_reference iface_reference;
typedef struct _string_holder string_holder;
typedef struct _array_holder array_holder;
typedef struct _suspend_state suspend_state;
typedef void (*__suspend_function)(suspend_state *);
typedef enum _ctype ctype;
typedef enum _class_type class_type;
typedef struct _class_object_properties class_object_properties;

typedef union _object_properties object_properties;
typedef struct _anonymous_class_state_data anonymous_class_state_data;
typedef struct _sweep_result sweep_result;

enum _ctype { 
    object_type, // 0
    long_type, // 1
    int_type,
    short_type,
    char_type,
    ulong_type,
    uint_type,
    ushort_type,
    uchar_type,
    float_type,
    double_type,
    bool_type,
    any_type,
    void_type
 };

enum _class_type {
    class,
    interface,
    function_reference
};

union _value {
        aobject * object_value; // 0
        void * custom_value; // 1
        long long long_value; // 2
        unsigned long long ulong_value; // 3
        int int_value; // 4
        unsigned int uint_value; // 5
        short short_value; // 6
        unsigned short ushort_value; // 7
        char char_value; // 8
        unsigned char uchar_value; // 9
        bool bool_value; // 10
        #ifdef FEATURE_FLOATING_POINT
        float float_value; // 11
        double double_value; // 12
        #endif
};

struct _string_holder {
    bool is_string_constant;
    // BYTES of UTF-8, not characters -- the comment here used to claim the
    // opposite, which is where a long line of mojibake bugs started. This is
    // what every native passes to fwrite/Text/strstr, and what String.length()
    // reports today.
    unsigned int length;
    char * string_value;
    unsigned int hash;
    // UTF-16 code units, the count Java and JavaScript call a string's length.
    // Computed once at construction in the same pass as the hash, so reading it
    // is free. `char_length == length` is exactly the test for "pure ASCII",
    // which is what lets the indexed operations keep their O(1) byte paths.
    unsigned int char_length;
    // Lazily built UTF-16 view, `char_length` units long, or NULL.
    //
    // Only ever allocated for a NON-ASCII string: when `char_length == length`
    // the byte index IS the unit index, so indexed access is already O(1) off
    // string_value and a cache would be pure waste. That is the overwhelming
    // majority of strings here -- paths, identifiers, HashMap keys, C source --
    // so most strings never allocate it at all.
    //
    // This is the ONLY heap a String owns: `string_value` and this holder are
    // allocated inline with the aobject and die with it, which is why the
    // release hook used to be empty. It is not any more -- see
    // Am_Lang_String__native_release_0.
    unsigned short * wide;
};

struct _array_holder {
    unsigned int size; // number of items
    aclass * item_class;
    ctype ctype;
    unsigned char item_size; // size per item
    char * array_data;
};

// rename to: any_value
struct _nullable_value {
    unsigned short flags; // 1 = nullable primitive (this is a primitive that CAN be set to NULL), 2 = primitive null (this is a primitive that is CURRENTLY NULL)
    value value;
};
// object_data.value.object_value

struct _property_info {
    char * name;
    unsigned char modifier;
};

struct _property {
    nullable_value nullable_value;    
};

struct _class_static {
    char * name; // not includng generic param type names
    unsigned int static_properties_count;
    unsigned char annotations_count;
    aobject ** annotations;
    // Index of this class's first static slot in every isolate's `statics`
    // array, assigned by __register_class. Static property i lives at
    // __amlc_isolate()->statics[slot_base + i]: one copy PER ISOLATE.
    unsigned int slot_base;
    class_type type;
    class_static *next;
};

// Reflection (`#reflect` classes, see __amlc_class_ref): one read-only
// descriptor per class (variant), generated into the class's .c and hung
// on the aclass in <Class>_init(). Everything else is materialised lazily.
typedef struct _amlc_prop_desc amlc_prop_desc;
typedef struct _amlc_method_desc amlc_method_desc;
typedef struct _amlc_reflect_desc amlc_reflect_desc;

struct _aclass {
    char * name;
    class_static * const statics;
    aclass * const base;
    __anonymous_function release;
    __anonymous_function mark_children;
    // Deep-copy hook of a NATIVE class that opted into being sendable
    // (`#sendable native class`), see __amlc_native_copy_T. NULL on plain
    // classes (their property slots are copied generically) and on natives
    // that are not sendable — sending one of those throws.
    __anonymous_function copy;
    __anonymous_function * functions;
    aobject * class_ref_singleton;
    unsigned int iface_implementation_count;
    unsigned int functions_count;
    unsigned int properties_count;
    // How many of the property slots can hold an object. 0 means the
    // release and mark walks can skip the slot loop altogether.
    unsigned int object_property_count;
    // Bytes every instance carries past its property array. Only Struct<T>
    // boxes set it (sizeof(struct T)) so the generic copier can clone the
    // value tail; 0 otherwise.
    unsigned int extra_size;
    iface_implementation * iface_implementations;
    aclass *next;
    // TRACKOBJECTS live-instance counter feeding the `instances(Class)` test
    // intrinsic. A per-class (shared) word touched from every isolate that
    // allocates the class, so concurrent isolates can lose an update —
    // diagnostic accuracy only. Always present so aclass literals stay valid.
    int instance_count;
    // `#reflect`: the class's property descriptor, or NULL. A class without
    // one still answers getClassRef() (name only, created on first use).
    const amlc_reflect_desc * reflect;
    // Registry of reflected classes for ClassRef.getClassRef(name).
    aclass * reflected_next;
};

// Reflected calls pass their arguments in a small fixed array.
#define AMLC_MAX_REFLECT_ARGS 8

#define AMLC_PROP_NULLABLE 1   // declared `T?`
#define AMLC_PROP_STRUCT 2     // slot holds a Struct<T> box

struct _amlc_prop_desc {
    const char * name;
    unsigned int slot;         // index into object_properties...properties[]
    unsigned int flags;        // AMLC_PROP_*
    aclass * type;             // declared type (a variant aclass for generics)
};

// One invokable instance method. `thunk` is a generated
// `function_result (*)(aobject * this, nullable_value * args)` that unpacks
// the argument array into the method's real C parameter types and calls it.
struct _amlc_method_desc {
    const char * name;
    unsigned int arg_count;
    __anonymous_function thunk;
};

struct _amlc_reflect_desc {
    const amlc_prop_desc * props;
    unsigned int count;
    __anonymous_function init_instance;   // <Class>___init_instance, or NULL
    aclass * const * type_args;           // the class's own type arguments, NULL-terminated, or NULL
    const amlc_method_desc * methods;
    unsigned int method_count;
};

// Reflection runtime (core.c / Am/Lang/ClassRef.c).
void __amlc_register_reflected(aclass * const cls);          // from <Class>_init()
aclass * __amlc_find_reflected_class(const char * const name);
aobject * __amlc_class_ref(aclass * const cls);              // the class's ClassRef, created on first use
aobject * __amlc_allocate_immortal(aclass * const cls, size_t extra_size);
void __amlc_make_immortal(aobject * const obj);
void __amlc_reflection_teardown(void);                       // from the generated startup, at exit
ctype __amlc_class_ctype(aclass * const cls);                // primitive class -> value kind, else object_type
unsigned char __amlc_ctype_flags(ctype const t);
nullable_value __amlc_coerce_primitive(nullable_value v, ctype const target);

struct _iface_implementation {    
    aclass * iface_class;
    __anonymous_function * functions;
//    unsigned int functions_count;
};

struct _iface_reference {
    aobject * const implementation_object;
    iface_implementation const * const iface_implementation;
};

struct _class_object_properties {
    nullable_value object_data;
    #if defined(DEBUG) || defined(TRACKOBJECTS)
    int object_id;
    #endif
    property * properties;
};

union _object_properties {
    class_object_properties class_object_properties;
    iface_reference iface_reference;
};

struct _aobject {
    aclass * class_ptr;
    // if class_ptr is an interface, object_properties will contain a iface_implementation
    // this object will also hold a reference to the implementation object, and will remove that once this has reached 0.
    // Plain, non-atomic counts: an object belongs to exactly one isolate and
    // is only ever touched by that isolate's thread.
    int reference_count;
    int property_reference_count;
    object_properties object_properties;
    bool marked;
    bool pending_deallocation;
    // Process-global object read from every isolate (string constants,
    // annotations): its reference_count is pinned huge and it is never
    // linked into an isolate's property-referenced list, because those
    // lists are per isolate and unsynchronised.
    bool immortal;
    // Allocated inside a fast-lane arena (see aisolate.fast_depth): freed by
    // dropping the arena's live count, never by free().
    bool chunked;
    aobject *prev;
    aobject *next;
};

struct _function_result {
    bool has_return_value;
    nullable_value return_value;
    aobject * exception;
};

struct _sweep_result {
    bool is_swept;
    aobject * next;
};

struct _suspend_state {
    suspend_state * parent;
    suspend_state * child;
    __suspend_function function; // to be called by child function when returning
    unsigned int index; // to be used by parent function at re-entry
    unsigned int state_objects_count; // to be used by parent function at re-entry
    nullable_value *state_objects; // to be used by parent function at re-entry
    function_result result; // created by parent, set by child
    // Root-call rendezvous (parent == NULL only). A non-suspend caller and
    // the completing chain each bump this once via __suspend_root_rendezvous;
    // the SECOND arrival (return value != 0) owns the state: the caller
    // reads result.exception (propagating it) and frees, or — when the chain
    // completed detached, after a real suspension — the chain frees.
    // Exactly-once free with no use-after-free in either interleaving.
    int root_handoff;
};

// Returns the previous rendezvous count: 0 = arrived first (other party will
// finish the hand-off), non-zero = arrived second (this party owns cleanup).
int __suspend_root_rendezvous(suspend_state *st);

struct _anonymous_class_state_data {
    unsigned int state_objects_count; // to be used by parent function at re-entry
    nullable_value *state_objects; // to be used by parent function at re-entry
};

/*
aclass Int = {
	.name = "Int",
    .type = CLASS_TYPE_PRIMITIVE,
	.functions = NULL,
    .functions_count = 0,
    .properties_count = 0
};

aclass Long = {
	.name = "Long",
    .type = CLASS_TYPE_PRIMITIVE,
	.functions = NULL,
    .functions_count = 0,
    .properties_count = 0
};
*/

// variables
// variables
extern aclass * __first_class;
extern class_static * __first_class_static;
// Total number of static-property slots registered so far (sum over every
// __register_class'd class). Each isolate allocates this many slots.
extern unsigned int __amlc_static_slot_count;

// ---------------------------------------------------------------------------
// Isolates (share-nothing threads) — see the model note at the top of this
// file and am-lang-compiler-code/docs/TASK_ISOLATE_THREADS.md.
// ---------------------------------------------------------------------------
typedef struct _aisolate aisolate;
typedef struct _amessage amessage;
typedef struct _amailbox amailbox;
typedef struct _aarena aarena;
typedef function_result (*__amlc_static_init_fn)(void);
// Native deep-copy hook (aclass.copy): returns a fresh object of src's class
// holding a copy of src's native payload, allocated in the CURRENT isolate
// with reference_count 1. Children are copied with __amlc_copy_child.
typedef aobject * (*__amlc_native_copy_T)(aobject * const src, void * const copy_ctx);

// A message in flight: the deep-copied payload graph. `payload` carries one
// owned reference; `chain` is the list of the graph's property-referenced
// objects (what would have gone on the sender's first_object list), spliced
// into the receiver's list when the message is opened.
struct _amessage {
    amessage * next;
    aobject * payload;
    aobject * chain;
    int count;              // objects in the graph (allocation accounting)
};

struct _aisolate {
    // This isolate's copy of every static property: static i of class C is
    // statics[C.statics->slot_base + i]. Allocated at isolate creation, sized
    // by __amlc_static_slot_count, initialised by the static-init table.
    property * statics;
    unsigned int statics_count;
    // Per-isolate heap bookkeeping (was the process-global __first_object):
    // objects held by a property, for the exit-time cycle sweep + leak report.
    aobject * first_object;
    aobject * first_detached_object;
    int allocation_count;
    amailbox * mailbox;
    aisolate * parent;          // the spawning isolate (holds a handle on it); NULL for main
    int handle_count;           // Thread handle objects + the running thread itself (global lock)
    bool started;
    bool done;                  // runnable returned, heap torn down
    bool joined;
    void * native;              // platform thread data (Thread.c)
    amessage * boot;            // the copied Runnable, opened by the worker on entry
    aobject * self_handle;      // cached Thread handle for getCurrent(), owned by this isolate
    char * name;                // malloc'd copy for the platform task name
    int priority;
    unsigned int stack_size;
    // Fast lane (`fast { ... }` blocks). While fast_depth > 0 every allocation
    // is bump-allocated from `arena` instead of calloc'd; an object's death
    // only drops its arena's live count, and an arena whose count reaches 0
    // is reset and reused. Arenas are per isolate, so none of this locks.
    int fast_depth;
    aarena * arena;             // the arena new objects go into
    aarena * arenas;            // every arena this isolate owns
    aarena * free_arenas;       // empty arenas kept for reuse
    unsigned int free_arena_count;
};

// Default arena size and how many empty arenas an isolate keeps around
// before handing them back to the system.
#ifndef AMLC_ARENA_SIZE
    #if defined(PLATFORM_AMIGAOS)
        #define AMLC_ARENA_SIZE (8 * 1024)
    #else
        #define AMLC_ARENA_SIZE (32 * 1024)
    #endif
#endif
#ifndef AMLC_ARENA_KEEP_FREE
    #define AMLC_ARENA_KEEP_FREE 4
#endif

// The calling thread's isolate — the base of every static-property access,
// so it must be one or two loads. POSIX: a TLS variable. AmigaOS/MorphOS:
// the running task's tc_UserData (SysBase->ThisTask is what FindTask(NULL)
// returns without the library call). AROS and non-gcc Amiga builds go
// through a function (AROS pthreads own tc_UserData; am-cc has no exec
// headers on the hot path).
#if defined(__AROS__)
aisolate * __amlc_isolate_current(void);
#define __amlc_isolate() (__amlc_isolate_current())
#elif defined(__MORPHOS__) || defined(__AMIGA__) || defined(AMIGA) || defined(PLATFORM_AMIGAOS)
    #if defined(__GNUC__)
    #include <exec/execbase.h>
    #include <proto/exec.h>
    #define __amlc_isolate() ((aisolate *) SysBase->ThisTask->tc_UserData)
    #else
    aisolate * __amlc_isolate_current(void);
    #define __amlc_isolate() (__amlc_isolate_current())
    #endif
#elif defined(__APPLE__)
// Darwin: every __thread access is a call to _tlv_get_addr, which showed up
// as a quarter of an allocation-heavy loop. While the process has a single
// thread the current isolate IS the main isolate, so read a global instead.
// __amlc_isolate_count is raised before a worker thread is created and
// lowered as that thread's very last runtime action, so whenever it reads
// 1 the calling thread is main.
extern __thread aisolate * __amlc_tls_isolate;
extern aisolate * __amlc_main_isolate_ptr;
extern int __amlc_isolate_count;
#define __amlc_isolate() (__amlc_isolate_count <= 1 ? __amlc_main_isolate_ptr : __amlc_tls_isolate)
#else
extern __thread aisolate * __amlc_tls_isolate;
#define __amlc_isolate() (__amlc_tls_isolate)
#endif
void __amlc_isolate_count_inc(void);   // before creating a worker thread
void __amlc_isolate_count_dec(void);   // the worker's last runtime action

// Static property `i` of the class whose class_static is `cs`, in the
// calling isolate. Yields the `property` slot (an lvalue).
#define __static_slot(cs, i) (__amlc_isolate()->statics[(cs)->slot_base + (i)])

// Every class's static initializer, in startup order. Filled in by the
// generated startup before __amlc_isolate_boot_main; run once per isolate.
extern __amlc_static_init_fn * __amlc_static_init_table;
extern unsigned int __amlc_static_init_count;

// Fast-lane arena allocator (see aisolate.fast_depth). A bump allocator:
// each object carries a pointer to its arena in the word just before the
// aobject; death decrements the arena's live count and an arena at zero is
// reset in place. Arenas never move objects.
#define AMLC_ARENA_ALIGN 8
#define AMLC_ARENA_ROUND(n) (((n) + (AMLC_ARENA_ALIGN - 1)) & ~((size_t) (AMLC_ARENA_ALIGN - 1)))
struct _aarena {
    aarena * next;          // isolate's list of every arena
    aarena * next_free;     // free-list link while empty and kept
    char * data;            // aligned start of the object area
    size_t capacity;
    size_t used;
    unsigned int live;      // objects allocated here and not yet dead
};
aobject * __amlc_arena_allocate(aisolate * const iso, size_t const size);
void __amlc_arena_recycle(aisolate * const iso, aarena * const a);
void __amlc_free_object(aobject * const obj);
void __amlc_free_object_in(aisolate * const iso, aobject * const obj);
void __amlc_arenas_free_all(aisolate * const iso);

// Small global lock for the few cross-isolate words: isolate handle counts
// and mailbox queues. pthread mutex on POSIX, Forbid/Permit on the Amiga family.
void __amlc_global_lock(void);
void __amlc_global_unlock(void);

// Isolate lifecycle.
void __amlc_isolate_boot_main(void);              // first thing in main(), before any allocation
void __amlc_isolate_statics_ready(void);          // after every __register_class: sizes the main isolate's statics
void __amlc_isolate_run_static_inits(void);       // runs the static-init table in the calling isolate
aisolate * __amlc_isolate_create(void);           // new isolate (statics + mailbox), handle_count = 1, parent = caller
void __amlc_isolate_retain(aisolate * const iso);
void __amlc_isolate_release(aisolate * const iso); // frees once no handles remain and the isolate is done
void __amlc_isolate_enter(aisolate * const iso);   // binds iso to the calling OS thread
void __amlc_isolate_teardown(aisolate * const iso); // statics deref + sweep + mailbox close; call from iso's own thread

// Messaging. A send deep-copies `payload` (Object graph → fresh objects) and
// posts it; the receiver gets an owned reference into its own heap. Objects
// of native classes without a copy hook are not sendable: send fails with
// an exception in *result and returns false.
bool __amlc_send(aisolate * const target, aobject * const payload, function_result * const result);
aobject * __amlc_receive(bool const wait);        // NULL when empty and !wait
amessage * __amlc_message_create(aobject * const payload, function_result * const result); // copy without posting
aobject * __amlc_message_open(amessage * const msg);   // splice into the calling isolate; frees msg; owned payload
void __amlc_message_discard(amessage * const msg);
// For native copy hooks: copy a child of the object being copied. Returns the
// copy with ONE owned reference (release it after storing it with
// property-reference semantics); NULL for NULL or when the child is not
// sendable (the whole copy then fails).
aobject * __amlc_copy_child(void * const copy_ctx, aobject * const src);

// Compatibility: cross-thread wrappers no longer exist, so unwrapping is the
// identity. Still spelled at a few native call sites; remove as they are touched.
#define __unwrap(__obj) (__obj)
#define __wrap_if_foreign(__raw) (__raw)
// functions
void __register_class(class_static * const __class_static);
void __dereference_static_properties();
void __dereference_static_properties_for_class(class_static * const __class_static);

static inline void __decrease_reference_count(aobject * const __obj);
static inline void __increase_reference_count(aobject * const __obj);
static inline void __set_property(aobject * const __obj, int const __index, nullable_value __prop_value);
static inline bool __set_property_safe(aobject * const __obj, int const __index, nullable_value __prop_value);
static inline void __set_static_property(class_static * const __class_static, int const __index, nullable_value __prop_value);
static inline void __decrease_reference_count_nullable_value(nullable_value __value);
static inline void __increase_reference_count_nullable_value(nullable_value __value);
void __decrease_property_reference_count(aobject * const __obj);
static inline void __increase_property_reference_count(aobject * const __obj);
static inline void __decrease_property_reference_count_nullable_value(nullable_value __value);
static inline void __increase_property_reference_count_nullable_value(nullable_value __value);
void __deallocate_object(aobject * const __obj);

// ── Slot watch (temporary debug aid) ──────────────────────────────────
// Arm a 12-byte watch on one property slot's raw bytes, then report the
// first time those bytes change without the watch being re-armed. Exists
// because primitive property writes are emitted as raw inline struct
// stores (see __get_property_nv's rationale) -- there is no hook on the
// write itself, so the only way to catch a stray one is to notice the
// damage from a path that runs constantly and allocates nothing.
// Armed from Am.Lang.ObjectHelper.watchProperty.
extern void * __amlc_watch_addr;
extern int __amlc_watch_armed;
void __amlc_watch_arm(void * addr);
void __amlc_watch_check(const char * where, const char * cls, void * site);

// The probe sites (allocator, deallocator, __set_property) are compiled out
// unless -DAMLC_WATCH: they sit on hot paths, and an always-on watch costs a
// call plus two loads per object allocation and per object-typed property
// write. Arming still works in an AMLC_WATCH build via
// Am.Lang.ObjectHelper.watchProperty.
#ifdef AMLC_WATCH
#define __amlc_watch_probe(where, cls, site) __amlc_watch_check((where), (cls), (site))
#else
#define __amlc_watch_probe(where, cls, site) ((void) 0)
#endif
void __detach_object(aobject * const __obj);

aobject * __allocate_iface_object(aclass * const __class, aobject * const implementation_object);
//aobject * __allocate_object(aclass * const __class);
aobject * __allocate_object_with_extra_size(aclass * const __class, size_t extra_size);
//void * __allocate_object_data(aobject * const __obj, int __size);
// function_result const __return_int(int const value);
// function_result const __return_long(long long const value);
// function_result const __return_object(aobject * const value);
// function_result const __default_return();
void __throw_exception(function_result *result, aobject * const exception, aobject * const stack_trace_item_text);
void __pass_exception(function_result *result, aobject * const exception, aobject * const stack_trace_item_text);
// Packed site-info variants: generated code passes the site as
// (aclass*, line*8+kind) instead of a pooled/literal pre-formatted string;
// the Exception stores the pair (addStackTraceSite) and formats it lazily
// when the trace is read (Debug.formatExceptionSite). kind: 0 pass,
// 1 throw, 2 throw at a suspend-child return, 3 throw at a resumed
// continuation, 4 allocation failed (OOM guard sites).
void __throw_exception_site(function_result *result, aobject * const exception, void * const site_class, unsigned int line_kind);
void __pass_exception_site(function_result *result, aobject * const exception, void * const site_class, unsigned int line_kind);
// Packed twins of __throw_simple_exception / __throw_out_of_memory_exception:
// the MESSAGE stays a C literal (it carries per-site variable names); only
// the stack-trace site is packed.
void __throw_simple_exception_site(const char * const message, void * const site_class, unsigned int line_kind, function_result * const result);
// Fixed-message hot checks (array subscripts, array sizing): the message
// literal lives ONCE here instead of in every translation unit that
// bounds-checks an array.
void __throw_index_bounds_site(function_result * const result, void * const site_class, unsigned int line_kind);
void __throw_negative_size_site(function_result * const result, void * const site_class, unsigned int line_kind);
void __throw_out_of_memory_exception_site(function_result * const result, void * const site_class, unsigned int line_kind);
void __throw_simple_exception(const char * const message, const char * const stack_trace_item_text, function_result * const result);
// Like __throw_simple_exception, but COPIES `message` into the exception
// (the plain variant wraps the pointer as a string constant and must only
// be given literals). Use this for messages built at runtime — e.g. an
// error text that embeds the filename that failed to open.
void __throw_simple_exception_copy(const char * const message, const char * const stack_trace_item_text, function_result * const result);
// Preallocated OutOfMemoryException singleton — throw when `calloc` returns
// NULL in generated `new` codegen. Uses a frozen instance built at startup
// (`__init_oom_singleton`), so the throw itself never touches `calloc`.
// Never call before startup has initialised the singleton; the check-null
// fallback aborts the process.
void __throw_out_of_memory_exception(function_result * const result, const char * const stack_trace_item_text);
void __init_oom_singleton(void);
//void __deallocate_function_result(function_result const result);
typedef function_result (*__release_T)(aobject * const);
typedef function_result (*__mark_children_T)(aobject * const);
static inline void __set_primitive_nullable(nullable_value * nullable_value, bool is_primitive_nullable);
static inline bool __is_primitive_nullable(const nullable_value nullable_value);
static inline void __set_primitive_null(nullable_value * nullable_value, bool is_primitive_null);
static inline bool __is_primitive_null(const nullable_value nullable_value);
static inline bool __is_primitive(const nullable_value nullable_value);
static inline bool __any_has_flags(const nullable_value *nv, unsigned short flags);
static inline ctype __value_flags_to_ctype(unsigned char flags);
bool __any_equals(const nullable_value a, const nullable_value b);
bool __any_null(const nullable_value a);
//bool __object_equals(aobject * const a, aobject * const b);
aobject * __create_string_constant(char const * const str, aclass * const string_class);
aobject * __create_string(char const * const str, aclass * const string_class);
aobject * __create_array(unsigned int const size, unsigned char const item_size, aclass * const array_class, ctype const ctype);
aobject * __create_exception(aobject * const message);
void clear_allocated_objects();
void print_allocated_objects();
bool is_descendant_of(aclass const * const cls, aclass const * const base);
bool implements_interface(aclass const * const iface, aclass const * const cls);
unsigned int __string_hash(const char * const str);
// Decodes the UTF-8 sequence starting at byte `i`, writing the code point to
// *out_cp (U+FFFD for anything malformed) and returning how many bytes it
// consumed -- always at least 1, so a caller's walk cannot stall. This is the
// ONE decoder: the unit count, String.getChars() and the indexed operations all
// go through it, so they cannot drift apart.
unsigned int __utf8_decode(const char * const s, unsigned int const len, unsigned int const i, unsigned int * const out_cp);
// Number of UTF-16 code units `len` bytes of UTF-8 decode to. Astral characters
// count as 2 (a surrogate pair), matching what getChars() produces.
unsigned int __utf8_utf16_length(const char * const s, unsigned int const len);
// Hash and UTF-16 unit count in ONE pass over the string. The hash is bit-for-bit
// what __string_hash returns -- construction already walked every byte, so the
// unit count rides along for free rather than costing a second pass.
unsigned int __string_hash_and_units(const char * const str, unsigned int const byte_len, unsigned int * const out_units);
// Returns the holder's UTF-16 view, building it on first use. NULL when the
// string is pure ASCII (the caller should index string_value directly) or when
// the allocation failed -- callers must handle NULL by falling back to a scan
// rather than failing, so a low-memory machine gets slow, not broken.
unsigned short * __string_wide(string_holder * const holder);
// Frees a holder's UTF-16 view. Safe on a holder that never built one.
void __string_wide_free(string_holder * const holder);
// Byte offset of UTF-16 unit `unit_index`. Sets *out_split when the index lands
// on the low half of a surrogate pair -- i.e. inside a character, which UTF-8
// cannot represent as a boundary. O(1) for ASCII, a walk otherwise.
unsigned int __string_unit_to_byte(string_holder * const holder, unsigned int const unit_index, int * const out_split);
// UTF-16 unit index of byte offset `byte_offset`. The inverse of the above,
// used to report indexOf results in the same units the caller indexes in.
unsigned int __string_byte_to_unit(string_holder * const holder, unsigned int const byte_offset);
void deallocate_annotations(class_static * const __class_static);
array_holder * get_array_holder(aobject * const array_obj);
char * get_array_data(array_holder * holder);

// Mark & Sweep (GC)
void __mark_root_objects();
void __mark_object(aobject * const obj);
void __sweep_unmarked_objects();
sweep_result __sweep_object(aobject * const obj);
sweep_result __detach_object_from_sweep(aobject * const __obj);
void __deallocate_detached_object(aobject * const __obj);
static inline void __mark_nullable_value(nullable_value __value);
void __mark_static_properties(class_static * const __class_static);
void __clear_marks();
aclass * const get_class_from_any(nullable_value const value);
aobject * __concatenate_strings(int count, ...);

#ifdef DEBUG
void __print_memory_header(aobject * const obj, const char * prefix);
#endif

// aliases for generated code
#include <Am/Lang/Object.h>
#include <Am/Lang/ClassRef.h>
#include <Am/Lang/PropertyInfo.h>
#include <Am/Lang/Exception.h>
#include <Am/Lang/OutOfMemoryException.h>
// typedef Am_Lang_Object_equals_0_T __object_equals_alias;
#define __object_equals_alias Am_Lang_Object_f_equals_0_T
#define __object_equals_index Am_Lang_Object_f_equals_0_index
#define  __class_ref_class_alias Am_Lang_ClassRef
#define __string_class_alias Am_Lang_String
#define __exception_class_alias Am_Lang_Exception
#define __out_of_memory_exception_class_alias Am_Lang_OutOfMemoryException
#define __out_of_memory_exception_constructor_alias Am_Lang_OutOfMemoryException_f_OutOfMemoryException_0
#define __out_of_memory_exception_init_instance_function_alias Am_Lang_OutOfMemoryException___init_instance
#define __add_stack_trace_item_function_alias Am_Lang_Exception_f_addStackTraceItem_0
#define __add_stack_trace_site_function_alias Am_Lang_Exception_f_addStackTraceSite_0
#define __exception_constructor_alias Am_Lang_Exception_f_Exception_0
#define __exception_init_instance_function_alias Am_Lang_Exception___init_instance
#define __property_info_class_alias Am_Lang_PropertyInfo
#define __property_info_constructor_alias Am_Lang_PropertyInfo_f_PropertyInfo_0
#define __char_class_alias Am_Lang_Char
