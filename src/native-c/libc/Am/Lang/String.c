#ifndef native_libc_aclass_Am_Lang_String_c
#define native_libc_aclass_Am_Lang_String_c
#include <core.h>
#include <Am/Lang/String.h>
#include <Am/Lang/Object.h>
#include <Am/Lang/Int.h>
#include <string.h>

function_result Am_Lang_String__native_init_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
/*
	bool __returning = false;
	if (this != NULL) {
		__increase_reference_count(this);
	}
	// TODO: implement native function Am_Lang_String__native_init_0
__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
*/
	return __result;
};

function_result Am_Lang_String__native_release_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;

	string_holder *holder = this->object_data.value.custom_value;
	if ( !holder->is_string_constant ) {
		free(holder->string_value);
	}
	free(holder);
	this->object_data.value.custom_value = NULL;

__exit: ;
	return __result;
};

function_result Am_Lang_String_getLength_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
	if (this != NULL) {
		__increase_reference_count(this);
	}
	// TODO: implement native function Am_Lang_String_getLength_0
	string_holder *holder = this->object_data.value.custom_value;
	if ( holder != NULL ) {
		__result.return_value.value.int_value = holder->length;
	} else {
		__result.return_value.value.int_value = 0;
	}

__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	return __result;
};

function_result Am_Lang_String_print_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
	if (this != NULL) {
		__increase_reference_count(this);
	}
	// TODO: implement native function Am_Lang_String_print_0
	string_holder *holder = this->object_data.value.custom_value;
	if ( holder != NULL ) {
		printf("%s", holder->string_value);
	} else {
		printf("null");
	}


__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	return __result;
};

function_result Am_Lang_String__op__plus_0(aobject * const this, aobject * s)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	if (this != NULL) {
		__increase_reference_count(this);
	}
	if (s != NULL) {
		__increase_reference_count(s);
	}


	string_holder *holder1 = this->object_data.value.custom_value;
	string_holder *holder2 = s->object_data.value.custom_value;

	if ( holder1 != NULL && holder2 != NULL ) {
		aobject * str_obj = __allocate_object(&Am_Lang_String);
		string_holder *holder = malloc(sizeof(string_holder));
		str_obj->object_data.value.custom_value = holder;
		char * new_str = malloc(holder1->length + holder2->length + 1);
//		printf("copy %s\n", holder1->string_value);
//		printf("append %s\n", holder2->string_value);
		strcpy(new_str, holder1->string_value);
	    strcat(new_str, holder2->string_value);
//		printf("new string: %s\n", newStr);
		*holder = (string_holder) { .is_string_constant = false, .length = holder1->length + holder2->length, .string_value = new_str };
//		memcpy(holder, &t_holder, sizeof(string_holder));
		// holder->string_value = newStr; // assume that string constants will never change
		// holder->length = holder1->length + holder2->length; // TODO: how many characters exactly?
		// holder->is_string_constant = false;

		__result.return_value.value.object_value = str_obj;
//		__increase_reference_count(str_obj);
	}

	// TODO: implement native function MyNamespace_CustomMyClass__op__plus_0
//	printf("TODO: implement native function Am_Lang__op__plus_0\n");
__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	if (s != NULL) {
		__decrease_reference_count(s);
	}
	return __result;
};

function_result Am_Lang_String_fromBytes_0(aobject * bytes, aobject * encoding)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;

	array_holder *array_holder = bytes->object_data.value.custom_value;

	aobject * new_string = __create_string(array_holder->array_data, &Am_Lang_String);

	__result.return_value.value.object_value = new_string;

__exit: ;
	if (bytes != NULL) {
		__increase_reference_count(bytes);
	}
	if (encoding != NULL) {
		__increase_reference_count(encoding);
	}
	return __result;
};

#endif
