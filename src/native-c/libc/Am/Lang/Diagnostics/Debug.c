#include <libc/core.h>
#include <Am/Lang/Diagnostics/Debug.h>
#include <libc/Am/Lang/Diagnostics/Debug.h>
#include <Am/Lang/ClassRef.h>
#include <Am/Lang/Object.h>
#include <Am/Lang/Bool.h>
#include <Am/Lang/String.h>
#include <stdio.h>
#include <libc/core_inline_functions.h>

function_result Am_Lang_Diagnostics_Debug__native_init_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
__exit: ;
	return __result;
}

function_result Am_Lang_Diagnostics_Debug__native_release_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
__exit: ;
	return __result;
}

function_result Am_Lang_Diagnostics_Debug__native_mark_children_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
__exit: ;
	return __result;
}

function_result Am_Lang_Diagnostics_Debug_setConditionalLogging_0(bool on)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
	__conditional_logging_on = on;
__exit: ;
	return __result;
}


// Packed exception site formatter — the lazy half of the packed stack-trace
// scheme (see __pass_exception_site in core.c). Generated unwinding code
// stores (aclass*, line*4+kind) per frame; this expands one pair into the
// same text the old pooled string constants carried, only when a trace is
// actually read. kind: 0 = "... in class", 1 = "Exception thrown in class",
// 2 = kind 1 + suspend-child suffix, 3 = kind 1 + resumed-continuation suffix.
function_result Am_Lang_Diagnostics_Debug_formatExceptionSite_0(long long cls, long long line_kind)
{
	function_result __result = { .has_return_value = true };
	aclass * site_class = (aclass *) (unsigned long) cls;
	unsigned long lk = (unsigned long) line_kind;
	unsigned long line = lk >> 3;
	unsigned long kind = lk & 7;
	const char * cls_name = site_class != NULL ? site_class->name : "?";
	const char * prefix = kind == 0 ? "... in class" : "Exception thrown in class";
	const char * suffix = "";
	if (kind == 2) {
		suffix = " (suspend child returned with exception)";
	} else if (kind == 3) {
		suffix = " (resumed continuation)";
	} else if (kind == 4) {
		suffix = " (allocation failed)";
	}
	char buf[512];
	sprintf(buf, "%s %.400s on line %lu%s", prefix, cls_name, line, suffix);
	aobject * str = __create_string(buf, &Am_Lang_String);
	__result.return_value.value.object_value = str;
	return __result;
}
