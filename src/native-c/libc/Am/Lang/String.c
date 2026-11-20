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
	// TODO: implement native function Am_Lang_String__native_init_0
__exit: ;
*/
	return __result;
};

function_result Am_Lang_String__native_release_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };

	// The holder and string_value are allocated INLINE with the aobject
	// (__allocate_object_with_extra_size), so they are freed with it and must
	// never be freed here -- which is why this hook was empty for years. The
	// lazily built UTF-16 view is the one exception: it is a separate malloc,
	// so it is the only thing a String owns and the only thing to release.
	//
	// This runs for constants too. That is correct and necessary: a constant's
	// string_value points into rodata and is untouched, but a view built on one
	// is ordinary heap and would otherwise leak.
	string_holder *holder = this->object_properties.class_object_properties.object_data.value.custom_value;
	__string_wide_free(holder);

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
	function_result __result = { .has_return_value = false };
	bool __returning = false;
	// TODO: implement native function Am_Lang_String_getLength_0
	// CHARACTERS (UTF-16 code units), not bytes. characterAt(), substring() and
	// indexOf() all index in these units, so this is the length that pairs with
	// them. byteLength() reports the storage size.
	string_holder *holder = this->object_properties.class_object_properties.object_data.value.custom_value;
	if ( holder != NULL ) {
		__result.return_value.value.int_value = holder->char_length;
	} else {
		__result.return_value.value.int_value = 0;
	}

__exit: ;
	return __result;
};

function_result Am_Lang_String_print_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
	string_holder *holder = this->object_properties.class_object_properties.object_data.value.custom_value;
	if ( holder != NULL ) {
		// fwrite of the known length, not printf("%s"): libnix (the
		// AmigaOS -noixemul libc) caps a %s conversion at 32767 bytes,
		// so a long string -- a JSON reply a tool prints for its caller
		// -- came out truncated at exactly that size. (holder->length
		// counts characters, not bytes, so measure the NUL-terminated
		// buffer.) Also never trips over a '%' in the text.
		if (holder->string_value != NULL) {
			size_t n = strlen(holder->string_value);
			if (n > 0 && !__amlc_stdout_held_write(holder->string_value, n)) {
				fwrite(holder->string_value, 1, n, stdout);
			}
		}
	} else if (!__amlc_stdout_held_write("null", 4)) {
		fputs("null", stdout);
	}
	// Held (see core.h): nothing may reach the OS, so no flush either.
	if (__amlc_stdout_is_held()) goto __exit;
	// When stdout is redirected to a file on AmigaOS (Startup-Sequence
	// `app >output.log` pattern), libnix's stdio defaults to full
	// buffering. A long-running program that doesn't exit cleanly
	// (e.g. cli-search waiting on async network) never flushes — so a
	// reader watching output.log sees nothing even though the prints
	// ran. Flushing after every line is the simplest fix and matches
	// what a developer reading print output expects.
	fflush(stdout);


__exit: ;
	return __result;
};

// Byte-wise, locale-free ordering of the UTF-8 storage (strcmp's order; for UTF-8
// that is also code point order): -1, 0 or 1. A null other orders first.
function_result Am_Lang_String_compareTo_0(aobject * const this, aobject * other)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
	int r = 1;
	if (other != NULL) {
		string_holder *a = this->object_properties.class_object_properties.object_data.value.custom_value;
		string_holder *b = other->object_properties.class_object_properties.object_data.value.custom_value;
		unsigned int la = a != NULL ? a->length : 0;
		unsigned int lb = b != NULL ? b->length : 0;
		unsigned int n = la < lb ? la : lb;
		int c = n > 0 ? memcmp(a->string_value, b->string_value, n) : 0;
		if (c < 0) { r = -1; }
		else if (c > 0) { r = 1; }
		else if (la < lb) { r = -1; }
		else if (la > lb) { r = 1; }
		else { r = 0; }
	}
	__result.return_value.value.int_value = r;

__exit: ;
	return __result;
};

function_result Am_Lang_String_equals_0(aobject * const this, aobject * other)
{	
	function_result __result = { .has_return_value = true };
	bool __returning = false;

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

	bool res = holder1->hash == holder2->hash && holder1->length == holder2->length
        && memcmp(holder1->string_value, holder2->string_value, holder1->length) == 0;
	__result.return_value = (nullable_value) { .value.bool_value = res, .flags = PRIMITIVE_BOOL };

__exit: ;
	return __result;
};

function_result Am_Lang_String__op__plus_0(aobject * const this, aobject * s)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;


	string_holder *holder1 = this->object_properties.class_object_properties.object_data.value.custom_value;
	string_holder *holder2 = s->object_properties.class_object_properties.object_data.value.custom_value;

	if (holder2 == NULL) {
		__result.return_value.value.object_value = this;
		__increase_reference_count(this);
		goto __exit;
	}

	if (holder1 == NULL) {
		__result.return_value.value.object_value = s;
		__increase_reference_count(s);
		goto __exit;
	}

	aobject * str_obj = __allocate_object_with_extra_size(&Am_Lang_String, sizeof(string_holder) + holder1->length + holder2->length + 1);
	string_holder *holder = (string_holder *) ((char *) str_obj + sizeof(aobject));
	str_obj->object_properties.class_object_properties.object_data.value.custom_value = holder;
	char * new_str = (char *) ((char *) holder + sizeof(string_holder));
	memcpy(new_str, holder1->string_value, holder1->length);
	memcpy(new_str + holder1->length, holder2->string_value, holder2->length);
	new_str[holder1->length + holder2->length] = 0;
	unsigned int units = 0;
	unsigned int total = holder1->length + holder2->length;
	// From the operands' own hash and unit count, walking only the right
	// operand's bytes (#466); it recounts the join itself in the one case
	// where the counts don't add up (a continuation byte at the seam).
	unsigned int hash = __string_hash_and_units_joined(holder1, holder2, new_str, total, &units);
	*holder = (string_holder) { .is_string_constant = false, .length = total, .string_value = new_str, .hash = hash, .char_length = units };

	__result.return_value.value.object_value = str_obj;

__exit: ;
	return __result;
};

function_result Am_Lang_String_fromBytes_0(aobject * bytes, aobject * encoding)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;



	array_holder *a_holder = (array_holder *) ((char *) bytes + sizeof(aobject)); // bytes->object_properties.class_object_properties.object_data.value.custom_value;

    int const len = a_holder->size; // TODO: support different character sizes
	aobject * str_obj = __allocate_object_with_extra_size(&Am_Lang_String, sizeof(string_holder) + len + 1);
	string_holder *holder = (string_holder *) ((char *) str_obj + sizeof(aobject));
	str_obj->object_properties.class_object_properties.object_data.value.custom_value = holder;
	char * new_str = (char *) ((char *) holder + sizeof(string_holder));

    // aobject * const str_obj = __allocate_object(&Am_Lang_String);
    // string_holder * const holder = calloc(1, sizeof(string_holder));
    // str_obj->object_properties.class_object_properties.object_data.value.custom_value = holder;
    // char * const new_str = malloc(len + 1);
    /* The byte array may contain NUL; preserve the entire counted payload. */
    memcpy(new_str, a_holder->array_data, len);
	new_str[len] = 0;
	unsigned int units = 0;
	unsigned int hash = __string_hash_and_units(new_str, (unsigned int) len, &units);
    *holder = (string_holder) { .is_string_constant = false, .length = len, .string_value = new_str, .hash = hash, .char_length = units };

//	aobject * new_string = __create_string(array_holder->array_data, &Am_Lang_String);

	__result.return_value.value.object_value = str_obj;
	__result.return_value.flags = 0;

__exit: ;
	return __result;
};

function_result Am_Lang_String_toBytes_0(aobject * const this, aobject * encoding)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	string_holder *string_holder = this->object_properties.class_object_properties.object_data.value.custom_value;
	aobject *array = __create_array(string_holder->length, 1, &Am_Lang_Array_ta_Am_Lang_UByte, uchar_type);

	array_holder *a_holder = (array_holder *) ((char *) array + sizeof(aobject)); // array->object_properties.class_object_properties.object_data.value.custom_value;
	memcpy(a_holder->array_data, string_holder->string_value, string_holder->length);
	__result.return_value.flags = 0;
	__result.return_value.value.object_value = array;

__exit: ;
	return __result;
};

function_result Am_Lang_String_characterAtNative_0(aobject * const this, unsigned int index)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;

	string_holder *holder = this->object_properties.class_object_properties.object_data.value.custom_value;
	if ( holder == NULL || index >= holder->char_length ) {
		__throw_simple_exception("Index out of bounds", "in Am_Lang_String_characterAt_0", &__result);
		goto __exit;
	}

	// ASCII: the character index IS the byte index, so this stays the single
	// load it always was -- no cache, no scan, no allocation. Cast through
	// unsigned char so a byte >= 0x80 cannot sign-extend into the UShort.
	if (holder->char_length == holder->length) {
		__result.return_value.value.ushort_value = (unsigned char) holder->string_value[index];
		goto __exit;
	}

	{
		// Non-ASCII: decode once into the cached UTF-16 view, O(1) thereafter.
		unsigned short *view = __string_wide(holder);
		if (view != NULL) {
			__result.return_value.value.ushort_value = view[index];
			goto __exit;
		}
	}

	// No view (out of memory): walk to the index rather than failing.
	{
		unsigned int i = 0, o = 0, cp = 0;
		unsigned short unit = 0;
		while (i < holder->length && o <= index) {
			i += __utf8_decode(holder->string_value, holder->length, i, &cp);
			if (cp < 0x10000) {
				if (o == index) { unit = (unsigned short) cp; }
				o++;
			} else {
				unsigned int v = cp - 0x10000;
				if (o == index) { unit = (unsigned short) (0xd800 + (v >> 10)); }
				o++;
				if (o == index) { unit = (unsigned short) (0xdc00 + (v & 0x3ff)); }
				o++;
			}
		}
		__result.return_value.value.ushort_value = unit;
	}

__exit: ;
	return __result;
};

function_result Am_Lang_String_indexOf_0(aobject * const this, aobject * s)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;

	string_holder *sh1 = this->object_properties.class_object_properties.object_data.value.custom_value;
	string_holder *sh2 = s->object_properties.class_object_properties.object_data.value.custom_value;

	// strstr is still the right search: UTF-8 is self-synchronizing, so a valid
	// needle can only ever match at a character boundary. Only the RESULT needs
	// converting -- callers index in characters now.
	char *strpos = strstr(sh1->string_value, sh2->string_value);
	if (strpos != NULL) {
		__result.return_value.value.int_value =
			(int) __string_byte_to_unit(sh1, (unsigned int) (strpos - sh1->string_value));
	} else {
		__result.return_value.value.int_value = -1;
	}

__exit: ;
	return __result;
};

// One UTF-16 code unit of a string: the byte itself for ASCII, else the cached
// UTF-16 view, else (no memory for a view) characterAtNative's scan.
static unsigned short __string_unit_at(aobject * const str, string_holder * const h, unsigned short * const wide, unsigned int const i)
{
	if (h->char_length == h->length) {
		return (unsigned char) h->string_value[i];
	}
	if (wide != NULL) {
		return wide[i];
	}
	return Am_Lang_String_characterAtNative_0(str, i).return_value.value.ushort_value;
}

// indexOfAfter: the first match of `s` at or after character `startIndex`, as
// a character (UTF-16 unit) index; -1 when there is none, when s is empty or
// when startIndex is out of range. It was AmLang, comparing two characterAt()
// calls per step, each a checked call: scanning a megabyte of generated C was
// a measurable part of a v2 self-build (#466). Same results, in C: a byte
// search with memchr + memcmp when both strings are ASCII (byte index ==
// character index), and a unit-by-unit comparison otherwise, so surrogate
// halves match exactly as they did.
function_result Am_Lang_String_indexOfAfter_0(aobject * const this, aobject * s, int startIndex)
{
	function_result __result = { .has_return_value = true };
	string_holder *h = this->object_properties.class_object_properties.object_data.value.custom_value;
	string_holder *n = s != NULL ? s->object_properties.class_object_properties.object_data.value.custom_value : NULL;
	unsigned int hlen = h != NULL ? h->char_length : 0;
	unsigned int nlen = n != NULL ? n->char_length : 0;
	int found = -1;

	if (startIndex < 0 || (unsigned int) startIndex >= hlen || nlen == 0 || nlen > hlen) {
		goto __exit;
	}
	if (h->char_length == h->length && n->char_length == n->length) {
		const char *base = h->string_value;
		const char *p = base + startIndex;
		const char *last = base + (hlen - nlen);
		char first = n->string_value[0];
		while (p <= last) {
			const char *q = (const char *) memchr(p, first, (size_t) (last - p) + 1);
			if (q == NULL) {
				break;
			}
			if (memcmp(q, n->string_value, nlen) == 0) {
				found = (int) (q - base);
				break;
			}
			p = q + 1;
		}
	} else {
		unsigned short *hw = __string_wide(h);
		unsigned short *nw = __string_wide(n);
		unsigned short first = __string_unit_at(s, n, nw, 0);
		unsigned int i, j;
		for (i = (unsigned int) startIndex; i <= hlen - nlen; i++) {
			if (__string_unit_at(this, h, hw, i) != first) {
				continue;
			}
			for (j = 1; j < nlen; j++) {
				if (__string_unit_at(this, h, hw, i + j) != __string_unit_at(s, n, nw, j)) {
					break;
				}
			}
			if (j == nlen) {
				found = (int) i;
				break;
			}
		}
	}

__exit: ;
	__result.return_value.value.int_value = found;
	return __result;
};

// lastIndexOf: the last match of `s`, as a character (UTF-16 unit) index, the
// same units indexOf and indexOfAfter use; -1 when there is none. It used to
// call strstr again from every match onwards: quadratic on "aaaa...a" / "aa",
// and blind to anything after a NUL (#469). Now one backwards scan: memcmp from
// the last possible start when both strings are ASCII (byte index == character
// index), a unit-by-unit comparison otherwise (surrogate halves match exactly
// as in indexOfAfter). Overlapping matches count ("aaaa" / "aa" is 2).
// An empty needle keeps its old answer: the index of the last character
// (-1 for an empty string).
function_result Am_Lang_String_lastIndexOf_0(aobject * const this, aobject * s)
{
	function_result __result = { .has_return_value = true };
	string_holder *h = this->object_properties.class_object_properties.object_data.value.custom_value;
	string_holder *n = s != NULL ? s->object_properties.class_object_properties.object_data.value.custom_value : NULL;
	int found = -1;

	if (h == NULL || n == NULL) {
		goto __exit;
	}
	if (n->length == 0) {
		if (h->length > 0) {
			found = (int) __string_byte_to_unit(h, h->length - 1);
		}
		goto __exit;
	}
	if (n->char_length > h->char_length) {
		goto __exit;
	}
	if (h->char_length == h->length && n->char_length == n->length) {
		unsigned int nlen = n->length;
		unsigned int i = h->length - nlen + 1;
		char first = n->string_value[0];
		while (i > 0) {
			i--;
			if (h->string_value[i] == first && memcmp(h->string_value + i, n->string_value, nlen) == 0) {
				found = (int) i;
				break;
			}
		}
	} else {
		unsigned int hlen = h->char_length;
		unsigned int nlen = n->char_length;
		unsigned short *hw = __string_wide(h);
		unsigned short *nw = __string_wide(n);
		unsigned short first = __string_unit_at(s, n, nw, 0);
		unsigned int i = hlen - nlen + 1;
		unsigned int j;
		while (i > 0) {
			i--;
			if (__string_unit_at(this, h, hw, i) != first) {
				continue;
			}
			for (j = 1; j < nlen; j++) {
				if (__string_unit_at(this, h, hw, i + j) != __string_unit_at(s, n, nw, j)) {
					break;
				}
			}
			if (j == nlen) {
				found = (int) i;
				break;
			}
		}
	}

__exit: ;
	__result.return_value.value.int_value = found;
	return __result;
};

function_result Am_Lang_String_substring_0(aobject * const this, unsigned int start, unsigned int length)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;

	string_holder *holder = this->object_properties.class_object_properties.object_data.value.custom_value;

	// start and length are CHARACTER counts (UTF-16 code units), matching
	// getLength() and characterAt().
	unsigned int end_unit = start + length;
	if (end_unit > holder->char_length || end_unit < start /* overflow */) {
		__throw_simple_exception("End index can't be higher than string length", "in Am_Lang_String_substring_0", &__result);
		goto __exit;
	}

	{
		int split_start = 0, split_end = 0;
		unsigned int byte_start = __string_unit_to_byte(holder, start, &split_start);
		unsigned int byte_end   = __string_unit_to_byte(holder, end_unit, &split_end);
		unsigned int byte_len;

		if (split_start || split_end) {
			// A boundary landed on the low half of a surrogate pair, i.e. inside
			// a character. UTF-8 cannot encode half of one, so take the whole
			// character rather than emitting a broken sequence. Only reachable
			// with astral text and a hand-computed index.
			__throw_simple_exception("Substring boundary falls inside a character", "in Am_Lang_String_substring_0", &__result);
			goto __exit;
		}

		byte_len = byte_end - byte_start;
		aobject * str_obj = __allocate_object_with_extra_size(&Am_Lang_String, sizeof(string_holder) + byte_len + 1);
		if (str_obj == NULL) {
			__throw_simple_exception("Out of memory", "in Am_Lang_String_substring_0", &__result);
			goto __exit;
		}

		string_holder *substr_holder = (string_holder *) ((char *) str_obj + sizeof(aobject));
		str_obj->object_properties.class_object_properties.object_data.value.custom_value = substr_holder;
		char * new_str = (char *) ((char *) substr_holder + sizeof(string_holder));
		memcpy(new_str, &holder->string_value[byte_start], byte_len);
		new_str[byte_len] = 0;
		unsigned int units = 0;
		unsigned int hash = __string_hash_and_units(new_str, byte_len, &units);
		*substr_holder = (string_holder) { .is_string_constant = false, .length = byte_len, .string_value = new_str, .hash = hash, .char_length = units };
		__result.return_value.value.object_value = str_obj;
	}

__exit: ;
	return __result;
};

function_result Am_Lang_String_byteLength_0(aobject * const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	string_holder *holder = this->object_properties.class_object_properties.object_data.value.custom_value;
	__result.return_value.value.int_value = (holder != NULL) ? (int) holder->length : 0;

__exit: ;
	return __result;
};

function_result Am_Lang_String_charLength_0(aobject * const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	// Pre-calculated at construction, so this is a field read -- no scan, no
	// allocation, safe to use as a loop bound.
	string_holder *holder = this->object_properties.class_object_properties.object_data.value.custom_value;
	__result.return_value.value.int_value = (holder != NULL) ? (int) holder->char_length : 0;

__exit: ;
	return __result;
};

function_result Am_Lang_String_getChars_0(aobject * const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;

	string_holder *holder = this->object_properties.class_object_properties.object_data.value.custom_value;
	unsigned int units = (holder != NULL) ? holder->char_length : 0;

	aobject *array = __create_array(units, 2, &Am_Lang_Array_ta_Am_Lang_UShort, ushort_type);
	if (array == NULL) {
		__throw_simple_exception("Out of memory", "in Am_Lang_String_getChars_0", &__result);
		goto __exit;
	}
	if (holder != NULL && units > 0) {
		array_holder *a_holder = (array_holder *) ((char *) array + sizeof(aobject));
		unsigned short *out = (unsigned short *) a_holder->array_data;
		unsigned int i = 0, o = 0, cp = 0;
		if (holder->char_length == holder->length) {
			// Pure ASCII: one byte per unit, so widen straight across without
			// touching the decoder or the cache.
			for (i = 0; i < units; i++) {
				out[i] = (unsigned char) holder->string_value[i];
			}
			__result.return_value.flags = 0;
			__result.return_value.value.object_value = array;
			goto __exit;
		}
		{
			unsigned short *view = __string_wide(holder);
			if (view != NULL) {
				memcpy(out, view, (size_t) units * sizeof(unsigned short));
				__result.return_value.flags = 0;
				__result.return_value.value.object_value = array;
				goto __exit;
			}
		}
		// View could not be built (out of memory): decode directly rather
		// than failing the call.
		while (i < holder->length && o < units) {
			i += __utf8_decode(holder->string_value, holder->length, i, &cp);
			if (cp < 0x10000) {
				out[o++] = (unsigned short) cp;
			} else {
				// Astral: the surrogate pair JavaScript would report.
				unsigned int v = cp - 0x10000;
				out[o++] = (unsigned short) (0xd800 + (v >> 10));
				if (o < units) out[o++] = (unsigned short) (0xdc00 + (v & 0x3ff));
			}
		}
	}
	__result.return_value.flags = 0;
	__result.return_value.value.object_value = array;

__exit: ;
	return __result;
};


// #sendable: a String travelling in a message is re-created in the receiving
// isolate. Constants keep pointing at the shared literal bytes (immutable, so
// safe to share); everything else is copied byte for byte, embedded NULs
// included, which is why this does not go through __create_string.
aobject * Am_Lang_String__native_copy_0(aobject * const src, void * const copy_ctx)
{
	(void) copy_ctx;
	string_holder * const h = (string_holder *) src->object_properties.class_object_properties.object_data.value.custom_value;
	if (h == NULL) {
		return NULL;
	}
	if (h->is_string_constant) {
		return __create_string_constant(h->string_value, src->class_ptr);
	}
	aobject * const dst = __allocate_object_with_extra_size(src->class_ptr, sizeof(string_holder) + h->length + 1);
	if (dst == NULL) {
		return NULL;
	}
	string_holder * const dh = (string_holder *) ((char *) dst + sizeof(aobject));
	dst->object_properties.class_object_properties.object_data.value.custom_value = dh;
	char * const bytes = (char *) (dh + 1);
	memcpy(bytes, h->string_value, h->length);
	bytes[h->length] = '\0';
	dh->is_string_constant = false;
	dh->length = h->length;
	dh->string_value = bytes;
	dh->hash = h->hash;
	dh->char_length = h->char_length;
	dh->wide = NULL;
	dh->unit_marks = NULL;
	return dst;
}
