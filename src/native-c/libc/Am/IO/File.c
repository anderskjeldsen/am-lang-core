#include <libc/core.h>
#include <Am/IO/File.h>
#include <Am/Lang/Object.h>
#include <Am/Lang/String.h>
#include <limits.h>
#include <stdio.h>
#include <unistd.h>
#include <libc/core_inline_functions.h>

function_result Am_IO_File__native_init_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
	if (this != NULL) {
		__increase_reference_count(this);
	}
__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	return __result;
};

function_result Am_IO_File__native_release_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
__exit: ;
	return __result;
};

function_result Am_IO_File_getCurrentDirectory_0()
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;

	char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
		aobject * path = __create_string(cwd, &Am_Lang_String);
		__result.return_value.value.object_value = path;
		__returning = true;
    } else {
		__result.return_value.value.object_value = NULL;
		__returning = true;
    }

__exit: ;
	return __result;
};

