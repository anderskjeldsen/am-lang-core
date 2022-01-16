#ifndef native_libc_aclass_Am_Lang_Long_c
#define native_libc_aclass_Am_Lang_Long_c
#include <core.h>
#include <Am/Lang/Long.h>
#include <Am/Lang/Any.h>
#include <Am/Lang/String.h>
#include <string.h>

function_result Am_Lang_Long_toString_0(nullable_value const this)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	char tmp[21];
	sprintf(tmp, "%lld", this.value.long_value);

	aobject * str_obj = __allocate_object(&Am_Lang_String);
	string_holder *holder = malloc(sizeof(string_holder));
	str_obj->object_data.value.custom_value = holder;
	int tmp_len = strlen(tmp);
	char * new_str = malloc(tmp_len + 1);
	strcpy(new_str, tmp);
	holder->string_value = new_str; // assume that string constants will never change
	holder->length = tmp_len; // TODO: how many characters exactly?
	holder->is_string_constant = false;

	__result.return_value.value.object_value = str_obj;

__exit: ;
	return __result;
};

#endif
