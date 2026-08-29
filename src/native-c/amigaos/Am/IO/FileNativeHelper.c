#include <libc/core.h>
#include <Am/IO/FileNativeHelper.h>
#include <amigaos/Am/IO/FileNativeHelper.h>
#include <Am/Lang/String.h>
#include <libc/core_inline_functions.h>

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
 * AmigaOS: the temporary directory is the "T:" assign, which the system sets up at
 * boot. Note this is not merely a different spelling of "/tmp" - on AmigaDOS a
 * leading "/" means the PARENT directory, so a POSIX-style absolute temp path
 * cannot be opened here at all.
 *
 * No trailing separator: "T:" already ends in the volume separator, and
 * File.joinPath() appends a bare name directly after a ":".
 */
function_result Am_IO_FileNativeHelper_getSystemTempFolder_0()
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;

	__result.return_value.value.object_value = __create_string("T:", &Am_Lang_String);
	__returning = true;

__exit: ;
	return __result;
};
