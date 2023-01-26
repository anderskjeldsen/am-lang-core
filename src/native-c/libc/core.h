#ifndef am_lang_core_h
#define am_lang_core_h

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

//#define CLASS_TYPE_PRIMITIVE 1
//#define CLASS_TYPE_NORMAL 0

#define PRIMITIVE_NULLABLE 1 // can it be null?
#define PRIMITIVE_NULL 2 // is it null?
#define PRIMITIVE_UNSIGNED 4 // is it unsigned?
#define PRIMITIVE_FLOATING_POINT_NUMBER 8
#define PRIMITIVE_BYTE_SIZE_EXP_1 16
#define PRIMITIVE_BYTE_SIZE_EXP_2 32
#define PRIMITIVE 64
#define PRIMITIVE_BOOL 128 | PRIMITIVE // is it a bool type?
#define PRIMITIVE_BOOL_TRUE 16 // if it's a bool type, is it true (1) ?

#define PRIMITIVE_LONG PRIMITIVE_BYTE_SIZE_EXP_1 | PRIMITIVE_BYTE_SIZE_EXP_2 | PRIMITIVE
#define PRIMITIVE_INT PRIMITIVE_BYTE_SIZE_EXP_2 | PRIMITIVE
#define PRIMITIVE_SHORT PRIMITIVE_BYTE_SIZE_EXP_1 | PRIMITIVE
#define PRIMITIVE_CHAR PRIMITIVE

#define PRIMITIVE_ULONG PRIMITIVE_LONG | PRIMITIVE_UNSIGNED
#define PRIMITIVE_UINT PRIMITIVE_INT | PRIMITIVE_UNSIGNED
#define PRIMITIVE_USHORT PRIMITIVE_SHORT | PRIMITIVE_UNSIGNED
#define PRIMITIVE_UCHAR PRIMITIVE_CHAR | PRIMITIVE_UNSIGNED

#define PRIMITIVE_DOUBLE PRIMITIVE_LONG | PRIMITIVE_FLOATING_POINT_NUMBER
#define PRIMITIVE_FLOAT PRIMITIVE_INT | PRIMITIVE_FLOATING_POINT_NUMBER


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

typedef struct _aobject aobject;
typedef struct _aclass aclass;
typedef void (*__anonymous_function)();
//typedef struct _stack_trace_item stack_trace_item;
//typedef struct _exception_holder exception_holder;
typedef struct _function_result function_result;
typedef union _value value;
typedef struct _nullable_value nullable_value;
typedef struct _property property;
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
typedef struct _weak_reference_node weak_reference_node;

enum _ctype { 
    object_type,
    long_type,
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
};

struct _string_holder {
    bool is_string_constant;
    unsigned int length; // length of characters, not "char"/bytes
    char * string_value;
};

struct _array_holder {
    size_t size; // number of items
    aclass * item_class;
    ctype ctype;
//    unsigned char item_size; // size per item
    char * array_data;
};

// rename to: any_value
struct _nullable_value {
    unsigned char flags; // 1 = nullable primitive (this is a primitive that CAN be set to NULL), 2 = primitive null (this is a primitive that is CURRENTLY NULL)
    value value;
};
// object_data.value.object_value
struct _property {
    nullable_value nullable_value;    
};

struct _aclass {
    char * name;
    class_type type;
    aclass const * const base;
    __anonymous_function release;
    __anonymous_function * functions;
    unsigned int iface_implementation_count;
    unsigned int functions_count;
    unsigned int properties_count;
    unsigned int static_properties_count;
    property * static_properties;    
    iface_implementation * iface_implementations;
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
    #ifdef DEBUG
    int object_id;
    #endif
    property * properties;
};

union _object_properties {
    class_object_properties class_object_properties;
    iface_reference iface_reference;
};

struct _aobject {
    aclass const * const class_ptr;
    // if class_ptr is an interface, object_properties will contain a iface_implementation
    // this object will also hold a reference to the implementation object, and will remove that once this has reached 0.
    int reference_count;
    weak_reference_node * first_weak_reference_node;
    object_properties object_properties;
};

struct _function_result {
    bool has_return_value;
    nullable_value return_value;
    aobject * exception;
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

struct _weak_reference_node {
    weak_reference_node *next;
    aobject *object;
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

void __decrease_reference_count(aobject * const __obj);
void __increase_reference_count(aobject * const __obj);
void __set_property(aobject * const __obj, int const __index, nullable_value __prop_value);
void __set_static_property(aclass * const __class, int const __index, nullable_value __prop_value);
void __decrease_reference_count_nullable_value(nullable_value __value);
void __increase_reference_count_nullable_value(nullable_value __value);
void __deallocate_object(aobject * const __obj);
aobject * __allocate_iface_object(aclass const * const __class, aobject * const implementation_object);
aobject * __allocate_object(aclass const * const __class);
//void * __allocate_object_data(aobject * const __obj, int __size);
// function_result const __return_int(int const value);
// function_result const __return_long(long long const value);
// function_result const __return_object(aobject * const value);
// function_result const __default_return();
void __throw_exception(function_result *result, aobject * const exception, aobject * const stack_trace_item_text);
void __pass_exception(function_result *result, aobject * const exception, aobject * const stack_trace_item_text);
void __throw_simple_exception(const char * const message, const char * const stack_trace_item_text, function_result * const result);
void __deallocate_function_result(function_result const result);
typedef function_result (*__release_T)(aobject * const);
void __set_primitive_nullable(nullable_value * nullable_value, bool is_primitive_nullable);
bool __is_primitive_nullable(const nullable_value nullable_value);
void __set_primitive_null(nullable_value * nullable_value, bool is_primitive_null);
bool __is_primitive_null(const nullable_value nullable_value);
bool __is_primitive(const nullable_value nullable_value);
bool anyEqual(const nullable_value a, const nullable_value b);
aobject * __create_string_constant(char const * const str, aclass const * const string_class);
aobject * __create_string(char const * const str, aclass const * const string_class);
aobject * __create_array(size_t const size, size_t const item_size, aclass const * const array_class, ctype const ctype);
aobject * __create_exception(aobject * const message);
void clear_allocated_objects();
void print_allocated_objects();
bool is_descendant_of(aclass const * const cls, aclass const * const base);
void attach_weak_reference_node(weak_reference_node * const node, aobject * const object);
void detach_weak_reference_node(weak_reference_node * const node) ;

#endif