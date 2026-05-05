#include <libc/core.h>
#include <Am/Lang/Environment.h>
#include <amigaos/Am/Lang/Environment.h>
#include <Am/Lang/ClassRef.h>
#include <Am/Lang/Bool.h>
#include <Am/Lang/String.h>
#include <Am/Lang/Object.h>
#include <libc/core_inline_functions.h>

#include <amigaos/amiga.h>

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/var.h>
#include <proto/exec.h>
#include <proto/dos.h>

// AmigaOS uses dos.library's variable system: local variables belong to the
// current process (CLI/Shell) and globals are stored as files in ENV: (with
// optional copy in ENVARC: via GVF_SAVE_VAR). GetVar with flags=0 searches
// local first then global, which matches libc getenv semantics most closely.
// SetVar/DeleteVar with flags=0 target local-only — the lifetime then matches
// libc setenv (process-scoped, gone when the process exits).

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
	STRPTR name_strptr = (STRPTR) name_holder->string_value;

	// Probe at 4KB; grow once if the value is larger than that. GetVar returns
	// the number of bytes actually copied (or that would have been copied if
	// the buffer were big enough — behavior varies, so we re-allocate on the
	// boundary case to be safe).
	ULONG capacity = 4096;
	UBYTE *buffer = (UBYTE *) AllocVec(capacity, MEMF_ANY);
	if (buffer == NULL) {
		__throw_simple_exception("Out of memory", "in Am_Lang_Environment_get_0", &__result);
		goto __exit;
	}

	LONG len = GetVar(name_strptr, buffer, (LONG) capacity, 0);
	if (len < 0) {
		// IoErr() == ERROR_OBJECT_NOT_FOUND when the var doesn't exist.
		FreeVec(buffer);
		__result.return_value.value.object_value = NULL;
		goto __exit;
	}

	if ((ULONG) len >= capacity - 1) {
		// Value may have been truncated; grow and retry.
		FreeVec(buffer);
		capacity = (ULONG) len + 64;
		buffer = (UBYTE *) AllocVec(capacity, MEMF_ANY);
		if (buffer == NULL) {
			__throw_simple_exception("Out of memory", "in Am_Lang_Environment_get_0", &__result);
			goto __exit;
		}
		len = GetVar(name_strptr, buffer, (LONG) capacity, 0);
		if (len < 0) {
			FreeVec(buffer);
			__result.return_value.value.object_value = NULL;
			goto __exit;
		}
	}

	buffer[len] = 0;
	aobject *str = __create_string((char const *) buffer, &Am_Lang_String);
	FreeVec(buffer);
	__result.return_value.value.object_value = str;

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
	STRPTR name_strptr = (STRPTR) name_holder->string_value;
	STRPTR value_strptr = (STRPTR) value_holder->string_value;

	// size = -1 tells SetVar to use strlen on the value.
	BOOL ok = SetVar(name_strptr, value_strptr, -1, 0);
	__result.return_value.value.bool_value = (ok != 0);

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
	STRPTR name_strptr = (STRPTR) name_holder->string_value;

	BOOL ok = DeleteVar(name_strptr, 0);
	__result.return_value.value.bool_value = (ok != 0);

__exit: ;
	if (name != NULL) {
		__decrease_reference_count(name);
	}
	return __result;
}
