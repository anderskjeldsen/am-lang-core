#include <libc/core.h>
#include <Am/Threading/Thread.h>
#include <aros-x86-64/Am/Threading/Thread.h>
#include <Am/Lang/ClassRef.h>
#include <Am/Lang/Long.h>
#include <Am/Lang/Object.h>
#include <Am/Lang/Runnable.h>
#include <Am/Lang/String.h>
#include <libc/core_inline_functions.h>

function_result Am_Threading_Thread_setCurrentPriority_0(int priority)
{
	// The AROS build reuses the hosted thread implementation; no exec
	// priorities are applied here.
	function_result __result = { .has_return_value = false };
	(void) priority;
	return __result;
}
