// Default (libc) Runtime native implementation. The string property
// store lives entirely in AmLang; the only native function is
// onRuntimeShutdown(), which on generic platforms has no process-global
// native resources to release and is therefore a no-op. AmigaOS overrides
// this file (see native-c/amigaos/Am/Lang/Runtime.c).

#include <libc/core.h>
#include <Am/Lang/Runtime.h>
#include <libc/Am/Lang/Runtime.h>
#include <Am/Lang/Object.h>
#include <libc/core_inline_functions.h>

function_result Am_Lang_Runtime__native_init_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
__exit: ;
	return __result;
}

function_result Am_Lang_Runtime__native_release_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
__exit: ;
	return __result;
}

function_result Am_Lang_Runtime__native_mark_children_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
__exit: ;
	return __result;
}

// No process-global native resources to release on libc platforms.
function_result Am_Lang_Runtime_onRuntimeShutdown_0(void)
{
	function_result __result = { .has_return_value = false };
	return __result;
}
