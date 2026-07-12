#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

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
typedef struct _object_wrapper object_wrapper;
typedef struct _object_wrapper_entry object_wrapper_entry;
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
    unsigned int length; // length of characters, not "char"/bytes
    char * string_value;
    unsigned int hash;
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
    property * static_properties;
    class_type type;
    class_static *next;
};

struct _aclass {
    char * name;
    class_static * const statics;
    aclass * const base;
    __anonymous_function release;
    __anonymous_function mark_children;
    __anonymous_function * functions;
    aobject * class_ref_singleton;
    unsigned int iface_implementation_count;
    unsigned int functions_count;
    unsigned int properties_count;
    iface_implementation * iface_implementations;
    aclass *next;
    // Live-instance counter for the `instances(Class)` test intrinsic.
    // Bumped in __allocate_object_with_extra_size / dropped in
    // __deallocate_object, but ONLY when compiled with -DTRACKOBJECTS
    // (the `-to` compiler flag; auto-enabled for `amlc test`). Always
    // present in the struct so generated aclass literals stay valid via
    // designated initializers (defaults to 0); it just stays 0 unless
    // TRACKOBJECTS is defined.
    int instance_count;
// meta:
//    aobject *properties;
};

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

struct _object_wrapper {
    aobject * wrapped_object;
};

union _object_properties {
    class_object_properties class_object_properties;
    iface_reference iface_reference;
    object_wrapper object_wrapper;
};

struct _object_wrapper_entry {
    aobject * subscriber;
    // Self-reference uses the struct-tag form rather than the typedef
    // alias (`object_wrapper_entry`) — the typedef binding sits on the
    // `typedef struct _object_wrapper_entry object_wrapper_entry;` line
    // above, but `struct object_wrapper_entry` is a different name and
    // would forward-declare an unrelated, never-defined type.
    struct _object_wrapper_entry * next;
};

struct _aobject {
    aclass * class_ptr;
    // if class_ptr is an interface, object_properties will contain a iface_implementation
    // this object will also hold a reference to the implementation object, and will remove that once this has reached 0.
    int reference_count;
    int property_reference_count;
    object_properties object_properties;
    void * owner_thread;
    object_wrapper_entry * first_object_wrapper; // lock a app shared mutext to read/write
    bool marked;
    bool pending_deallocation;
    // Thread-safe ARC: set under the shared mutex when this real's
    // owner-side liveness (reference_count + property_reference_count
    // + traversal-from-live-roots) has fully drained but foreign-thread
    // wrappers still subscribe to it. Wrappers consult this on their
    // own death — if `owner_gone == true` AND they were the last
    // subscriber, they finalize the real. One-way: never cleared.
    // Replaces the prior protocol where wrappers contributed +1 to
    // `reference_count`, which forced every wrapper birth/death to
    // mutate the owner's counter under the lock.
    bool owner_gone;
    // Thread-safe ARC: lock-protected "I will call __deallocate_object
    // on this real" claim. Set by whichever of owner-dec / propref-dec
    // / last-wrapper-death observes the all-zero condition first.
    // Separate from `pending_deallocation` (which is the in-progress
    // recursion guard set by __detach_object) so the destructor path
    // still runs after we claim.
    bool destruction_claimed;
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
};

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
extern aobject * __first_object;
extern aclass * __first_class;
extern class_static * __first_class_static;

// Thread-safe ARC infrastructure.
// `owner_thread` on every aobject records the thread that allocated it
// (set in __allocate_object_with_extra_size). When a thread other than
// the owner wants to reference the object, a wrapper aobject is created
// in the borrowing thread's realm pointing at the real via
// object_properties.object_wrapper.wrapped_object — wrappers are
// identified by `class_ptr == NULL`. Owner-thread inc/dec on the real's
// reference_count happens lock-free (only one writer). Wrapper-list
// mutations (subscribing/unsubscribing) and property_reference_count
// writes happen under this shared mutex.
//
// Implemented as a function-call boundary so the core stays portable —
// libc gets a pthread_mutex_t behind the scenes, AmigaOS will get a
// SignalSemaphore.
void __arc_shared_lock(void);
void __arc_shared_unlock(void);
void __arc_shared_mutex_init(void);

// Returns an opaque thread identifier. Owners are compared via
// pointer-equality so the implementation just has to be unique per
// thread and stable for that thread's lifetime. libc returns
// (void*)pthread_self(), AmigaOS returns FindTask(NULL).
void * __current_thread(void);

// Set to `true` the first time a cross-thread wrapper is minted (in
// `__create_wrapper`). `__unwrap` consults this flag to short-circuit
// the wrapper-following ternary when no wrappers have ever existed —
// the common case for single-threaded programs and even for
// multi-threaded programs that don't share objects between threads.
//
// Stays `false` for the lifetime of programs that never share — `gcc
// -O3` then constant-propagates and DCEs the wrapper-following branch
// entirely. At `gcc -O0` (dev builds) we still pay one load + branch
// per property read; that's manageable.
extern bool __amlc_any_wrappers_alive;

// Allocate a wrapper aobject in the current thread, pointing at the
// real aobject (which is owned by some other thread). Subscribes the
// wrapper into `real->first_object_wrapper` under the shared mutex
// and bumps `real->reference_count`. The wrapper starts at
// `reference_count = 1` and `class_ptr == NULL` (the wrapper sentinel).
// `__realobj` is named with that suffix because gcc treats `__real`
// as a reserved keyword (the `__real__`/`__imag__` complex-number
// builtins) and rejects it as a parameter name.
aobject * __create_wrapper(aobject * const __realobj);

// Called by codegen whenever an aobject reference is acquired from a
// nullable_value source (property read, function return, etc.). If
// the underlying object is owned by another thread, mints a wrapper
// in the current thread's realm. Otherwise (the common single-thread
// case), returns the original pointer unchanged — very cheap.
//
// NULL passes through. Already-wrapped pointers (class_ptr == NULL,
// owner_thread == current) pass through. Wrappers from OTHER threads
// shouldn't normally appear here because `__set_property` unwraps
// before storing; if one does, we'd re-wrap (correct but wasteful).
aobject * __wrap_if_foreign(aobject * const __raw);


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
void __throw_simple_exception(const char * const message, const char * const stack_trace_item_text, function_result * const result);
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
void deallocate_annotations(class_static * const __class_static);
array_holder * get_array_holder(aobject * const array_obj);
char * get_array_data(array_holder * holder);
void create_property_info(const unsigned char index, char * const name, aobject ** property_infos, aclass *cls);

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
#define __exception_constructor_alias Am_Lang_Exception_f_Exception_0
#define __exception_init_instance_function_alias Am_Lang_Exception___init_instance
#define __property_info_class_alias Am_Lang_PropertyInfo
#define __property_info_constructor_alias Am_Lang_PropertyInfo_f_PropertyInfo_0
#define __char_class_alias Am_Lang_Char