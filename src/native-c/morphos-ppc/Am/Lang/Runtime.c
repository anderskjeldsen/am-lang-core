// AmigaOS Runtime native implementation. onRuntimeShutdown() CloseLibrary's
// every library opened lazily through __ensure_library (diskfont, asl,
// cybergraphics, …) tracked in amiga.c's __first_lib_node list.
//
// Invoked late at process exit via the AmLang `#onNativeTearDown(1000)`
// hook on Runtime.onRuntimeShutdown — i.e. after the GC sweep, so any
// native objects holding library resources are already released.
// __release_libraries() is idempotent (it nulls the list as it walks).

#include <libc/core.h>
#include <Am/Lang/Runtime.h>
#include <morphos-ppc/Am/Lang/Runtime.h>
#include <Am/Lang/Object.h>
#include <libc/core_inline_functions.h>

#include <morphos-ppc/morphos.h>

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

function_result Am_Lang_Runtime_onRuntimeShutdown_0(void)
{
	function_result __result = { .has_return_value = false };
	__release_libraries();
	return __result;
}
