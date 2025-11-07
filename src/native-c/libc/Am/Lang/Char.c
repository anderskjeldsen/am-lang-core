#include <libc/core.h>
#include <Am/Lang/Char.h>
#include <Am/Lang/Any.h>
#include <Am/Lang/String.h>
#include <string.h>
#include <libc/core_inline_functions.h>

function_result Am_Lang_Char_toString_0(nullable_value const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	aobject * str_obj = __allocate_object_with_extra_size(&Am_Lang_String, sizeof(string_holder) + 10);
	string_holder *holder = (string_holder *) (str_obj + 1);
	str_obj->object_properties.class_object_properties.object_data.value.custom_value = holder;
	char * new_str = (char *) (holder + 1);
	
	// Convert 16-bit Unicode character to UTF-8
	unsigned short ch = this.value.ushort_value;
	int len = 0;
	
	if (ch < 0x80) {
		// ASCII character
		new_str[len++] = (char)ch;
	} else if (ch < 0x800) {
		// 2-byte UTF-8
		new_str[len++] = 0xC0 | (ch >> 6);
		new_str[len++] = 0x80 | (ch & 0x3F);
	} else {
		// 3-byte UTF-8
		new_str[len++] = 0xE0 | (ch >> 12);
		new_str[len++] = 0x80 | ((ch >> 6) & 0x3F);
		new_str[len++] = 0x80 | (ch & 0x3F);
	}
	new_str[len] = '\0';

	holder->string_value = new_str;
	holder->length = len;
	holder->is_string_constant = false;
	holder->hash = __string_hash(new_str);

	__result.return_value.value.object_value = str_obj;

__exit: ;
	return __result;
};

function_result Am_Lang_Char_hash_0(nullable_value const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .uint_value = this.value.ushort_value }, .flags = 0 };
__exit: ;
	return __result;
};

function_result Am_Lang_Char_toByte_0(unsigned short const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .char_value = (char)this }, .flags = 0 };
__exit: ;
	return __result;
};

function_result Am_Lang_Char_toShort_0(unsigned short const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .short_value = (short)this }, .flags = 0 };
__exit: ;
	return __result;
};

function_result Am_Lang_Char_toInt_0(unsigned short const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;

	__result.return_value = (nullable_value) { .value = { .int_value = this }, .flags = 0 };

__exit: ;
	return __result;
};

function_result Am_Lang_Char_toLong_0(unsigned short const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;

	__result.return_value = (nullable_value) { .value = { .long_value = this }, .flags = 0 };

__exit: ;
	return __result;
};

function_result Am_Lang_Char_toUByte_0(unsigned short const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .uchar_value = (unsigned char)this }, .flags = 0 };
__exit: ;
	return __result;
};

function_result Am_Lang_Char_toUShort_0(unsigned short const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .ushort_value = this }, .flags = 0 };
__exit: ;
	return __result;
};

function_result Am_Lang_Char_toUInt_0(unsigned short const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .uint_value = this }, .flags = 0 };
__exit: ;
	return __result;
};

function_result Am_Lang_Char_toULong_0(unsigned short const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .ulong_value = this }, .flags = 0 };
__exit: ;
	return __result;
};

function_result Am_Lang_Char_toBool_0(unsigned short const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	__result.return_value = (nullable_value) { .value = { .bool_value = this != 0 }, .flags = 0 };
__exit: ;
	return __result;
};

function_result Am_Lang_Char_parse_0(aobject * const s)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	
	__increase_reference_count(s);
	
	string_holder *holder = s->object_properties.class_object_properties.object_data.value.custom_value;
	char *str = holder->string_value;
	
	// Parse UTF-8 string to get first Unicode character
	unsigned short result = 0;
	
	if (str[0] == '\0') {
		result = 0;
	} else if ((str[0] & 0x80) == 0) {
		// ASCII character
		result = (unsigned short)str[0];
	} else if ((str[0] & 0xE0) == 0xC0) {
		// 2-byte UTF-8
		result = ((str[0] & 0x1F) << 6) | (str[1] & 0x3F);
	} else if ((str[0] & 0xF0) == 0xE0) {
		// 3-byte UTF-8
		result = ((str[0] & 0x0F) << 12) | ((str[1] & 0x3F) << 6) | (str[2] & 0x3F);
	} else {
		// Invalid UTF-8 or 4-byte sequence (not supported in 16-bit)
		result = 0xFFFD; // Unicode replacement character
	}
	
	__result.return_value = (nullable_value) { .value = { .ushort_value = result }, .flags = 0 };

__exit: ;
	__decrease_reference_count(s);
	return __result;
};

function_result Am_Lang_Char_isWhitespace_0(unsigned short const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	bool is_ws = (this == ' ' || this == '\t' || this == '\n' || this == '\r');
	__result.return_value = (nullable_value) { .value = { .bool_value = is_ws }, .flags = 0 };
__exit: ;
	return __result;
};

function_result Am_Lang_Char_isDigit_0(unsigned short const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	bool is_digit = (this >= '0' && this <= '9');
	__result.return_value = (nullable_value) { .value = { .bool_value = is_digit }, .flags = 0 };
__exit: ;
	return __result;
};

function_result Am_Lang_Char_isLetter_0(unsigned short const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	bool is_letter = ((this >= 'A' && this <= 'Z') || (this >= 'a' && this <= 'z'));
	__result.return_value = (nullable_value) { .value = { .bool_value = is_letter }, .flags = 0 };
__exit: ;
	return __result;
};

function_result Am_Lang_Char_isAlphaNumeric_0(unsigned short const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	bool is_alphanum = ((this >= 'A' && this <= 'Z') || (this >= 'a' && this <= 'z') || (this >= '0' && this <= '9'));
	__result.return_value = (nullable_value) { .value = { .bool_value = is_alphanum }, .flags = 0 };
__exit: ;
	return __result;
};

function_result Am_Lang_Char_toUpperCase_0(unsigned short const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	unsigned short result = this;
	if (this >= 'a' && this <= 'z') {
		result = this - 'a' + 'A';
	}
	__result.return_value = (nullable_value) { .value = { .ushort_value = result }, .flags = 0 };
__exit: ;
	return __result;
};

function_result Am_Lang_Char_toLowerCase_0(unsigned short const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	unsigned short result = this;
	if (this >= 'A' && this <= 'Z') {
		result = this - 'A' + 'a';
	}
	__result.return_value = (nullable_value) { .value = { .ushort_value = result }, .flags = 0 };
__exit: ;
	return __result;
};