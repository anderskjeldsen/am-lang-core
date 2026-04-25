#include <libc/core.h>
#include <Am/Lang/Process.h>
#include <amigaos/Am/Lang/Process.h>
#include <Am/Lang/ClassRef.h>
#include <Am/Lang/Int.h>
#include <Am/Lang/String.h>
#include <Am/Lang/Object.h>
#include <libc/core_inline_functions.h>

#include <amigaos/amiga.h>

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <utility/tagitem.h>

static void __ensure_dos_base(void)
{
	if (DOSBase == NULL) {
		DOSBase = (struct DosLibrary *) __ensure_library("dos.library", 0L);
	}
}

function_result Am_Lang_Process__native_init_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
	__increase_reference_count(this);
__exit: ;
	__decrease_reference_count(this);
	return __result;
}

function_result Am_Lang_Process__native_release_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
__exit: ;
	return __result;
}

function_result Am_Lang_Process__native_mark_children_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
__exit: ;
	return __result;
}

function_result Am_Lang_Process_run_0(aobject * command)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	if (command != NULL) {
		__increase_reference_count(command);
	}

	__ensure_dos_base();

	string_holder *cmd_holder = (string_holder *) (command + 1);
	STRPTR cmd_strptr = (STRPTR) cmd_holder->string_value;

	struct TagItem tags[] = {
		{ SYS_Input,     (ULONG) NULL },
		{ SYS_Output,    (ULONG) NULL },
		{ SYS_Asynch,    FALSE },
		{ SYS_UserShell, TRUE },
		{ TAG_DONE,      0 },
	};

	LONG status = SystemTagList(cmd_strptr, tags);
	__result.return_value.value.int_value = (int) status;

__exit: ;
	if (command != NULL) {
		__decrease_reference_count(command);
	}
	return __result;
}

function_result Am_Lang_Process_runAndCaptureOutput_0(aobject * command)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	if (command != NULL) {
		__increase_reference_count(command);
	}

	__ensure_dos_base();

	string_holder *cmd_holder = (string_holder *) (command + 1);
	STRPTR cmd_strptr = (STRPTR) cmd_holder->string_value;

	// Build a unique temp filename in T: from the calling task's address.
	UBYTE temp_path[64];
	struct Task *self = FindTask(NULL);
	{
		const STRPTR prefix = (STRPTR) "T:am_proc_";
		ULONG i = 0;
		while (prefix[i] != 0) { temp_path[i] = prefix[i]; i++; }
		ULONG addr = (ULONG) self;
		for (LONG nibble = 7; nibble >= 0; nibble--) {
			ULONG v = (addr >> (nibble * 4)) & 0xF;
			temp_path[i++] = (UBYTE) (v < 10 ? ('0' + v) : ('a' + (v - 10)));
		}
		temp_path[i] = 0;
	}

	BPTR out_file = Open((CONST_STRPTR) temp_path, MODE_NEWFILE);
	if (out_file == 0) {
		__throw_simple_exception("Failed to open temp file for command output", "in Am_Lang_Process_runAndCaptureOutput_0", &__result);
		goto __exit;
	}

	struct TagItem run_tags[] = {
		{ SYS_Input,     (ULONG) NULL },
		{ SYS_Output,    (ULONG) out_file },
		{ SYS_Asynch,    FALSE },
		{ SYS_UserShell, TRUE },
		{ TAG_DONE,      0 },
	};

	LONG status = SystemTagList(cmd_strptr, run_tags);
	// SystemTagList consumes the output stream; do not Close(out_file) here.
	if (status == -1) {
		DeleteFile((CONST_STRPTR) temp_path);
		__throw_simple_exception("Failed to execute command", "in Am_Lang_Process_runAndCaptureOutput_0", &__result);
		goto __exit;
	}

	BPTR in_file = Open((CONST_STRPTR) temp_path, MODE_OLDFILE);
	if (in_file == 0) {
		DeleteFile((CONST_STRPTR) temp_path);
		__throw_simple_exception("Failed to read back command output", "in Am_Lang_Process_runAndCaptureOutput_0", &__result);
		goto __exit;
	}

	// Two-step Seek pattern: Seek-to-end discards previous position; Seek-back returns
	// the previous position, which is the file size in bytes.
	(void) Seek(in_file, 0, OFFSET_END);
	LONG size = Seek(in_file, 0, OFFSET_BEGINNING);
	if (size < 0) {
		Close(in_file);
		DeleteFile((CONST_STRPTR) temp_path);
		__throw_simple_exception("Failed to position in command-output file", "in Am_Lang_Process_runAndCaptureOutput_0", &__result);
		goto __exit;
	}

	UBYTE *buffer = (UBYTE *) AllocVec((ULONG) (size + 1), MEMF_ANY | MEMF_CLEAR);
	if (buffer == NULL) {
		Close(in_file);
		DeleteFile((CONST_STRPTR) temp_path);
		__throw_simple_exception("Out of memory reading command output", "in Am_Lang_Process_runAndCaptureOutput_0", &__result);
		goto __exit;
	}

	LONG read = (size > 0) ? Read(in_file, buffer, size) : 0;
	Close(in_file);
	DeleteFile((CONST_STRPTR) temp_path);

	if (read < 0) {
		FreeVec(buffer);
		__throw_simple_exception("Failed to read command output", "in Am_Lang_Process_runAndCaptureOutput_0", &__result);
		goto __exit;
	}
	buffer[read] = 0;

	aobject *str = __create_string((char const *) buffer, &Am_Lang_String);
	FreeVec(buffer);
	__result.return_value.value.object_value = str;

__exit: ;
	if (command != NULL) {
		__decrease_reference_count(command);
	}
	return __result;
}
