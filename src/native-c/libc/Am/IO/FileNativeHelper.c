#include <libc/core.h>
#include <Am/IO/FileNativeHelper.h>
#include <libc/Am/IO/FileNativeHelper.h>
#include <Am/Lang/String.h>
#include <libc/core_inline_functions.h>

#include <stdlib.h>

/* A `native class` must supply the three lifecycle hooks even when, as here, it
   holds no instance data and is only ever used through its statics. */
function_result Am_IO_FileNativeHelper__native_init_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
__exit: ;
	return __result;
}

function_result Am_IO_FileNativeHelper__native_release_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
__exit: ;
	return __result;
}

function_result Am_IO_FileNativeHelper__native_mark_children_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
__exit: ;
	return __result;
}

/*
 * POSIX: honour TMPDIR when it is set - the standard says a tool should - and
 * fall back to "/tmp". No trailing separator; File.joinPath() adds the "/".
 */
function_result Am_IO_FileNativeHelper_getSystemTempFolder_0()
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;

	const char * tmp = getenv("TMPDIR");
	if (tmp == NULL || tmp[0] == 0) {
		tmp = "/tmp";
	}

	__result.return_value.value.object_value = __create_string((char *) tmp, &Am_Lang_String);
	__returning = true;

__exit: ;
	return __result;
};
