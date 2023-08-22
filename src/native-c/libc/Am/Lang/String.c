#include <libc/core.h>
#include <Am/Lang/String.h>
#include <Am/Lang/Object.h>
#include <Am/Lang/Int.h>
#include <string.h>
#include <libc/core_inline_functions.h>

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

	string_holder *holder = this->object_properties.class_object_properties.object_data.value.custom_value;
	if ( !holder->is_string_constant ) {
//		free(holder->string_value);
	}
//	free(holder);
	this->object_properties.class_object_properties.object_data.value.custom_value = NULL;

__exit: ;
	return __result;
};

function_result Am_Lang_String_hash_0(nullable_value const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	// Add reference count for this in String.hash
	__increase_reference_count_nullable_value(this);

	string_holder *holder = this.value.object_value->object_properties.class_object_properties.object_data.value.custom_value;
	if ( holder != NULL ) {
		/*
		unsigned int hash = 0;
		char *str = holder->string_value;
		int bit = 0;
		while(*str != 0) {
			int c = (int) *str;
			hash += (c << bit);
			bit += 3;
			bit &= 0x1f;
			str++;
		}
		*/
		__result.return_value = (nullable_value) { .value = { .uint_value = holder->hash }, .flags = 0 };
//		printf("string hash for '%s': %d\n", holder->string_value, hash);
	} else {
		__result.return_value = (nullable_value) { .value = { .uint_value = 0 }, .flags = 0 };
	}


__exit: ;
	__decrease_reference_count_nullable_value(this);
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
	string_holder *holder = this->object_properties.class_object_properties.object_data.value.custom_value;
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
	string_holder *holder = this->object_properties.class_object_properties.object_data.value.custom_value;
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

function_result Am_Lang_String_equals_0(aobject * const this, aobject * other)
{	
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	if (this != NULL) {
		__increase_reference_count(this);
	}
	if (other != NULL) {
		__increase_reference_count(other);
	}

	if (other == NULL) {
		__result.return_value = (nullable_value) { .value.bool_value = false, .flags = PRIMITIVE_BOOL };
		goto __exit;
	}

	if (other->class_ptr != &Am_Lang_String) {
		__result.return_value = (nullable_value) { .value.bool_value = false, .flags = PRIMITIVE_BOOL };
		goto __exit;
	}

	string_holder *holder1 = this->object_properties.class_object_properties.object_data.value.custom_value;
	string_holder *holder2 = other->object_properties.class_object_properties.object_data.value.custom_value;

//	printf("compare strings %s vs %s\n", holder1->string_value, holder2->string_value);

	bool res = holder1->hash == holder2->hash && strcmp(holder1->string_value, holder2->string_value) == 0;
	__result.return_value = (nullable_value) { .value.bool_value = res, .flags = PRIMITIVE_BOOL };

__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	if (other != NULL) {
		__decrease_reference_count(other);
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


	string_holder *holder1 = this->object_properties.class_object_properties.object_data.value.custom_value;
	string_holder *holder2 = s->object_properties.class_object_properties.object_data.value.custom_value;

	if ( holder1 != NULL && holder2 != NULL ) {
		aobject * str_obj = __allocate_object_with_extra_size(&Am_Lang_String, sizeof(string_holder) + holder1->length + holder2->length + 1);
		string_holder *holder = (string_holder *) (str_obj + 1);
		str_obj->object_properties.class_object_properties.object_data.value.custom_value = holder;
		char * new_str = (char *) (holder + 1);
//		printf("copy %s\n", holder1->string_value);
//		printf("append %s\n", holder2->string_value);
		strcpy(new_str, holder1->string_value);
	    strcat(new_str, holder2->string_value);
//		printf("new string: %s\n", newStr);
		unsigned int hash = __string_hash(new_str);
		*holder = (string_holder) { .is_string_constant = false, .length = holder1->length + holder2->length, .string_value = new_str, .hash = hash };
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
	function_result __result = { .has_return_value = true };
	bool __returning = false;

	array_holder *a_holder = (array_holder *) &bytes[1]; // bytes->object_properties.class_object_properties.object_data.value.custom_value;

    int const len = a_holder->size; // TODO: support different character sizes
	aobject * str_obj = __allocate_object_with_extra_size(&Am_Lang_String, sizeof(string_holder) + len + 1);
	string_holder *holder = (string_holder *) (str_obj + 1);
	str_obj->object_properties.class_object_properties.object_data.value.custom_value = holder;
	char * new_str = (char *) (holder + 1);

    // aobject * const str_obj = __allocate_object(&Am_Lang_String);
    // string_holder * const holder = calloc(1, sizeof(string_holder));
    // str_obj->object_properties.class_object_properties.object_data.value.custom_value = holder;
    // char * const new_str = malloc(len + 1);
    strncpy(new_str, a_holder->array_data, len);
	new_str[len] = 0;
	unsigned int hash = __string_hash(new_str);
    *holder = (string_holder) { .is_string_constant = false, .length = len, .string_value = new_str, .hash = hash };

//	aobject * new_string = __create_string(array_holder->array_data, &Am_Lang_String);

	__result.return_value.value.object_value = str_obj;
	__result.return_value.flags = 0;

__exit: ;
	if (bytes != NULL) {
		__increase_reference_count(bytes);
	}
	if (encoding != NULL) {
		__decrease_reference_count(encoding);
	}
	return __result;
};

function_result Am_Lang_String_toBytes_0(aobject * const this, aobject * encoding)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	if (this != NULL) {
		__increase_reference_count(this);
	}
	if (encoding != NULL) {
		__increase_reference_count(encoding);
	}
	string_holder *string_holder = this->object_properties.class_object_properties.object_data.value.custom_value;
	aobject *array = __create_array(string_holder->length, 1, &Am_Lang_Array_v_uchar, uchar_type);

	array_holder *a_holder = (array_holder *) &array[1]; // array->object_properties.class_object_properties.object_data.value.custom_value;
	strcpy(a_holder->array_data, string_holder->string_value);
	__result.return_value.flags = 0;
	__result.return_value.value.object_value = array;

__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	if (encoding != NULL) {
		__decrease_reference_count(encoding);
	}
	return __result;
};

function_result Am_Lang_String_characterAt_0(aobject * const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	if (this != NULL) {
		__increase_reference_count(this);
	}

	string_holder *string_holder = this->object_properties.class_object_properties.object_data.value.custom_value;
//	__result.return_value.value.ushort_value = string_holder->string_value[...]


__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	return __result;
};

function_result Am_Lang_String_indexOf_0(aobject * const this, aobject * s)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	if (this != NULL) {
		__increase_reference_count(this);
	}
	if (s != NULL) {
		__increase_reference_count(s);
	}

	string_holder *sh1 = this->object_properties.class_object_properties.object_data.value.custom_value;
	string_holder *sh2 = s->object_properties.class_object_properties.object_data.value.custom_value;

	unsigned char *strpos = strstr(sh1->string_value, sh2->string_value);
	if (strpos != NULL) {
		__result.return_value.value.int_value = strpos - sh1->string_value;
	} else {
		__result.return_value.value.int_value = -1;
	}

__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	if (s != NULL) {
		__decrease_reference_count(s);
	}
	return __result;
};

function_result Am_Lang_String_lastIndexOf_0(aobject * const this, aobject * s)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	if (this != NULL) {
		__increase_reference_count(this);
	}
	if (s != NULL) {
		__increase_reference_count(s);
	}

	int last_index = -1;

	string_holder *sh1 = this->object_properties.class_object_properties.object_data.value.custom_value;
	string_holder *sh2 = s->object_properties.class_object_properties.object_data.value.custom_value;

	for(int i = 0; i < sh1->length; i++) {
		unsigned char *strpos = strstr(&sh1->string_value[i], sh2->string_value);
		if (strpos != NULL) {
			last_index = strpos - sh1->string_value;
			i = last_index + 1;
		}
	}
	__result.return_value.value.int_value = last_index;

__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	if (s != NULL) {
		__decrease_reference_count(s);
	}
	return __result;
};

function_result Am_Lang_String_substring_0(aobject * const this, unsigned int start, unsigned int end)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	if (this != NULL) {
		__increase_reference_count(this);
	}

	string_holder *holder = this->object_properties.class_object_properties.object_data.value.custom_value;
	int len = end - start;

	printf("substr %d to %d of %d\n", start, end, holder->length);
	if (len < 0) {
		__throw_simple_exception("End index can't be lower than start index", "in Am_Lang_String_substring_0", &__result);
		goto __exit;
	}

	if (start >= holder->length) {
		__throw_simple_exception("Start index can't be higher than string length", "in Am_Lang_String_substring_0", &__result);
		goto __exit;
	}

	if (end > holder->length) { // end char isn't included
		__throw_simple_exception("End index can't be higher than string length", "in Am_Lang_String_substring_0", &__result);
		goto __exit;
	}
	printf("substr2\n");

	aobject * str_obj = __allocate_object_with_extra_size(&Am_Lang_String, sizeof(string_holder) + len + 1);
	if (str_obj = NULL) {
		__throw_simple_exception("Out of memory", "in Am_Lang_String_substring_0", &__result);
		goto __exit;
	}
	printf("substr2\n");

	string_holder *substr_holder = (string_holder *) (str_obj + 1);
	str_obj->object_properties.class_object_properties.object_data.value.custom_value = substr_holder;
	unsigned char * new_str = (unsigned char *) (substr_holder + 1);
	printf("substr2\n");
	strncpy(new_str, &holder->string_value[start], len);
	new_str[len - 1] = 0;
	printf("hash\n");
	unsigned int hash = __string_hash(new_str);
	*holder = (string_holder) { .is_string_constant = false, .length = len, .string_value = new_str, .hash = hash };
	__result.return_value.value.object_value = str_obj;

__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	return __result;
};

