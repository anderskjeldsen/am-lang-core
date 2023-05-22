#include <libc/core.h>
#include <Am/IO/BinaryStream.h>
#include <Am/IO/Stream.h>
#include <Am/Lang/Array.h>
#include <Am/Lang/Byte.h>
#include <Am/Lang/Long.h>
#include <Am/Lang/Int.h>
#include <libc/core_inline_functions.h>

function_result Am_IO_BinaryStream__native_init_0(aobject * const this)
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

function_result Am_IO_BinaryStream__native_release_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
__exit: ;
	return __result;
};
