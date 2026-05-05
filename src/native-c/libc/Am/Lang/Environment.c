#include <libc/core.h>
#include <Am/Lang/Environment.h>
#include <libc/Am/Lang/Environment.h>
#include <Am/Lang/ClassRef.h>
#include <Am/Lang/Bool.h>
#include <Am/Lang/String.h>
#include <Am/Lang/Object.h>
#include <stdlib.h>
#include <libc/core_inline_functions.h>

function_result Am_Lang_Environment__native_init_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
	__increase_reference_count(this);
__exit: ;
	__decrease_reference_count(this);
	return __result;
}

function_result Am_Lang_Environment__native_release_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
__exit: ;
	return __result;
}

function_result Am_Lang_Environment__native_mark_children_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
__exit: ;
	return __result;
}

function_result Am_Lang_Environment_get_0(aobject * name)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	if (name != NULL) {
		__increase_reference_count(name);
	}

	string_holder *name_holder = (string_holder *) (name + 1);
	const char *value = getenv(name_holder->string_value);

	if (value == NULL) {
		__result.return_value.value.object_value = NULL;
	} else {
		aobject *str = __create_string(value, &Am_Lang_String);
		__result.return_value.value.object_value = str;
	}

__exit: ;
	if (name != NULL) {
		__decrease_reference_count(name);
	}
	return __result;
}

function_result Am_Lang_Environment_set_0(aobject * name, aobject * value)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	if (name != NULL) {
		__increase_reference_count(name);
	}
	if (value != NULL) {
		__increase_reference_count(value);
	}

	string_holder *name_holder = (string_holder *) (name + 1);
	string_holder *value_holder = (string_holder *) (value + 1);

	int rc = setenv(name_holder->string_value, value_holder->string_value, 1);
	__result.return_value.value.bool_value = (rc == 0);

__exit: ;
	if (name != NULL) {
		__decrease_reference_count(name);
	}
	if (value != NULL) {
		__decrease_reference_count(value);
	}
	return __result;
}

function_result Am_Lang_Environment_unset_0(aobject * name)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	if (name != NULL) {
		__increase_reference_count(name);
	}

	string_holder *name_holder = (string_holder *) (name + 1);

	bool existed = (getenv(name_holder->string_value) != NULL);
	if (existed) {
		unsetenv(name_holder->string_value);
	}
	__result.return_value.value.bool_value = existed;

__exit: ;
	if (name != NULL) {
		__decrease_reference_count(name);
	}
	return __result;
}
