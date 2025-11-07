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

//	bool __returning = false;

//	string_holder *holder = this->object_properties.class_object_properties.object_data.value.custom_value;
//	if ( !holder->is_string_constant ) {
//		free(holder->string_value);
//	}
//	free(holder);
//	this->object_properties.class_object_properties.object_data.value.custom_value = NULL;

//__exit: ;
	return __result;
};

function_result Am_Lang_String__native_mark_children_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
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
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	if (this != NULL) {
		__increase_reference_count(this);
	}
	
	string_holder *holder = this->object_properties.class_object_properties.object_data.value.custom_value;
	if (holder == NULL || holder->string_value == NULL) {
		__result.return_value.value.int_value = 0;
	} else {
		// Count Unicode characters in UTF-8 string
		char *str = holder->string_value;
		unsigned int byte_length = holder->length;
		unsigned int char_count = 0;
		unsigned int byte_pos = 0;
		
		while (byte_pos < byte_length) {
			unsigned char first_byte = (unsigned char)str[byte_pos];
			
			if ((first_byte & 0x80) == 0) {
				// ASCII character (0xxxxxxx)
				byte_pos += 1;
			} else if ((first_byte & 0xE0) == 0xC0) {
				// 2-byte UTF-8 (110xxxxx 10xxxxxx)
				byte_pos += 2;
			} else if ((first_byte & 0xF0) == 0xE0) {
				// 3-byte UTF-8 (1110xxxx 10xxxxxx 10xxxxxx)
				byte_pos += 3;
			} else if ((first_byte & 0xF8) == 0xF0) {
				// 4-byte UTF-8 (11110xxx 10xxxxxx 10xxxxxx 10xxxxxx)
				byte_pos += 4;
			} else {
				// Invalid UTF-8 start byte
				byte_pos += 1;
			}
			char_count++;
		}
		
		__result.return_value.value.int_value = char_count;
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

	if (bytes != NULL) {
		__increase_reference_count(bytes);
	}
	if (encoding != NULL) {
		__increase_reference_count(encoding);
	}


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
		__decrease_reference_count(bytes);
	}
	if (encoding != NULL) {
		__decrease_reference_count(encoding);
	}
	return __result;
};

function_result Am_Lang_String_fromChars_0(aobject * chars)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;

	if (chars != NULL) {
		__increase_reference_count(chars);
	}

	array_holder *a_holder = (array_holder *) &chars[1];
    int const char_count = a_holder->size;
    
    // Convert chars to UTF-8 bytes - estimate worst case (3 bytes per char for UTF-8)
    char *utf8_buffer = malloc(char_count * 3 + 1);
    int utf8_len = 0;
    
    unsigned short *char_data = (unsigned short*) a_holder->array_data;
    for (int i = 0; i < char_count; i++) {
        unsigned short codepoint = char_data[i];
        
        if (codepoint < 0x80) {
            // ASCII (0-127): single byte
            utf8_buffer[utf8_len++] = (char) codepoint;
        } else if (codepoint < 0x800) {
            // Two-byte UTF-8 sequence (128-2047)
            utf8_buffer[utf8_len++] = (char) (0xC0 | (codepoint >> 6));
            utf8_buffer[utf8_len++] = (char) (0x80 | (codepoint & 0x3F));
        } else {
            // Three-byte UTF-8 sequence (2048-65535)
            utf8_buffer[utf8_len++] = (char) (0xE0 | (codepoint >> 12));
            utf8_buffer[utf8_len++] = (char) (0x80 | ((codepoint >> 6) & 0x3F));
            utf8_buffer[utf8_len++] = (char) (0x80 | (codepoint & 0x3F));
        }
    }
    utf8_buffer[utf8_len] = '\0';

    // Create string object with the UTF-8 data
	aobject * str_obj = __allocate_object_with_extra_size(&Am_Lang_String, sizeof(string_holder) + utf8_len + 1);
	string_holder *holder = (string_holder *) (str_obj + 1);
	str_obj->object_properties.class_object_properties.object_data.value.custom_value = holder;
	char * new_str = (char *) (holder + 1);

    strcpy(new_str, utf8_buffer);
    free(utf8_buffer);
    
	unsigned int hash = __string_hash(new_str);
    *holder = (string_holder) { .is_string_constant = false, .length = utf8_len, .string_value = new_str, .hash = hash };

	__result.return_value.value.object_value = str_obj;
	__result.return_value.flags = 0;

__exit: ;
	if (chars != NULL) {
		__decrease_reference_count(chars);
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
	aobject *array = __create_array(string_holder->length, 1, &Am_Lang_Array_ta_Am_Lang_UByte, uchar_type);

	array_holder *a_holder = (array_holder *) &array[1]; // array->object_properties.class_object_properties.object_data.value.custom_value;
	memcpy(a_holder->array_data, string_holder->string_value, string_holder->length);
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

function_result Am_Lang_String_characterAtNative_0(aobject * const this, unsigned int index)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	if (this != NULL) {
		__increase_reference_count(this);
	}

	string_holder *string_holder = this->object_properties.class_object_properties.object_data.value.custom_value;
	char *str = string_holder->string_value;
	
	// Convert character index to Unicode character
	// Since we store UTF-8 internally, we need to count Unicode characters, not bytes
	unsigned int char_count = 0;
	unsigned int byte_pos = 0;
	
	while (byte_pos < string_holder->length && char_count <= index) {
		if (char_count == index) {
			// Found the character at the requested index
			unsigned char first_byte = (unsigned char)str[byte_pos];
			unsigned short unicode_char = 0;
			
			if ((first_byte & 0x80) == 0) {
				// ASCII character (0xxxxxxx)
				unicode_char = first_byte;
			} else if ((first_byte & 0xE0) == 0xC0) {
				// 2-byte UTF-8 (110xxxxx 10xxxxxx)
				if (byte_pos + 1 < string_holder->length) {
					unsigned char second_byte = (unsigned char)str[byte_pos + 1];
					unicode_char = ((first_byte & 0x1F) << 6) | (second_byte & 0x3F);
				}
			} else if ((first_byte & 0xF0) == 0xE0) {
				// 3-byte UTF-8 (1110xxxx 10xxxxxx 10xxxxxx)
				if (byte_pos + 2 < string_holder->length) {
					unsigned char second_byte = (unsigned char)str[byte_pos + 1];
					unsigned char third_byte = (unsigned char)str[byte_pos + 2];
					unicode_char = ((first_byte & 0x0F) << 12) | ((second_byte & 0x3F) << 6) | (third_byte & 0x3F);
				}
			} else {
				// Invalid UTF-8 or 4-byte sequence (not supported in 16-bit)
				unicode_char = 0xFFFD; // Unicode replacement character
			}
			
			__result.return_value.value.ushort_value = unicode_char;
			goto __exit;
		}
		
		// Move to next Unicode character
		unsigned char first_byte = (unsigned char)str[byte_pos];
		if ((first_byte & 0x80) == 0) {
			// ASCII character
			byte_pos += 1;
		} else if ((first_byte & 0xE0) == 0xC0) {
			// 2-byte UTF-8
			byte_pos += 2;
		} else if ((first_byte & 0xF0) == 0xE0) {
			// 3-byte UTF-8
			byte_pos += 3;
		} else if ((first_byte & 0xF8) == 0xF0) {
			// 4-byte UTF-8 (skip, not supported in 16-bit)
			byte_pos += 4;
		} else {
			// Invalid UTF-8
			byte_pos += 1;
		}
		char_count++;
	}
	
	// Index out of bounds
	__throw_simple_exception("Index out of bounds", "in Am_Lang_String_characterAtNative_0", &__result);

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

	char *strpos = strstr(sh1->string_value, sh2->string_value);
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
		char *strpos = strstr(&sh1->string_value[i], sh2->string_value);
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

function_result Am_Lang_String_substring_0(aobject * const this, unsigned int start, unsigned int length)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	if (this != NULL) {
		__increase_reference_count(this);
	}

	string_holder *holder = this->object_properties.class_object_properties.object_data.value.custom_value;

	if (length < 0) {
		__throw_simple_exception("Length can't be lower than 0", "in Am_Lang_String_substring_0", &__result);
		goto __exit;
	}

	unsigned int end = start + length;
	if (end > holder->length) { // end char isn't included
		__throw_simple_exception("End index can't be higher than string length", "in Am_Lang_String_substring_0", &__result);
		goto __exit;
	}

	aobject * str_obj = __allocate_object_with_extra_size(&Am_Lang_String, sizeof(string_holder) + length + 1);
	if (str_obj == NULL) {
		__throw_simple_exception("Out of memory", "in Am_Lang_String_substring_0", &__result);
		goto __exit;
	}

	string_holder *substr_holder = (string_holder *) (str_obj + 1);
	str_obj->object_properties.class_object_properties.object_data.value.custom_value = substr_holder;
	char * new_str = (char *) (substr_holder + 1);
	strncpy(new_str, &holder->string_value[start], length);
	new_str[length] = 0;
	unsigned int hash = __string_hash(new_str);
	*substr_holder = (string_holder) { .is_string_constant = false, .length = length, .string_value = new_str, .hash = hash };
	__result.return_value.value.object_value = str_obj;

__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	return __result;
};

