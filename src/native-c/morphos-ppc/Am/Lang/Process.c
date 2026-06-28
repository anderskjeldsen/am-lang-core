#include <libc/core.h>
#include <Am/Lang/Process.h>
#include <morphos-ppc/Am/Lang/Process.h>
#include <Am/Lang/ClassRef.h>
#include <Am/Lang/Int.h>
#include <Am/Lang/String.h>
#include <Am/Lang/Object.h>
#include <libc/core_inline_functions.h>

#include <morphos-ppc/morphos.h>

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <dos/dosextens.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <utility/tagitem.h>

function_result Am_Lang_Process__native_init_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
__exit: ;
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

	string_holder *cmd_holder = (string_holder *) (command + 1);
	STRPTR cmd_strptr = (STRPTR) cmd_holder->string_value;

	// SYS_Input/SYS_Output omitted on purpose: SystemTagList then inherits the
	// caller's stdin/stdout, so the command's output reaches the user's shell.
	// Passing NULL would silently redirect to NIL:.
	struct TagItem tags[] = {
		{ SYS_Asynch,    FALSE },
		{ SYS_UserShell, TRUE },
		{ TAG_DONE,      0 },
	};

	LONG status = SystemTagList(cmd_strptr, tags);
	__result.return_value.value.int_value = (int) status;

__exit: ;
	return __result;
}

function_result Am_Lang_Process_runAndCaptureOutput_0(aobject * command)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;

	string_holder *cmd_holder = (string_holder *) (command + 1);
	STRPTR cmd_strptr = (STRPTR) cmd_holder->string_value;

	// Build a unique temp filename in T: (the conventional MorphOS temp dir,
	// usually assigned to RAM:T so it self-cleans on reboot).
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
		// Build a message that includes the dos.library IoErr() code.
		static char err_msg[80];
		const char *prefix = "Failed to open temp file (IoErr=0x";
		int p = 0;
		while (prefix[p] != 0) { err_msg[p] = prefix[p]; p++; }
		LONG ioerr = IoErr();
		for (LONG nibble = 7; nibble >= 0; nibble--) {
			LONG v = (ioerr >> (nibble * 4)) & 0xF;
			err_msg[p++] = (char)(v < 10 ? ('0' + v) : ('a' + (v - 10)));
		}
		err_msg[p++] = ')';
		err_msg[p] = 0;
		__throw_simple_exception(err_msg, "in Am_Lang_Process_runAndCaptureOutput_0", &__result);
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
	// In synchronous mode (SYS_Asynch=FALSE), SystemTagList does NOT close the
	// streams — the caller owns them. Close before re-opening for read, otherwise
	// the file stays locked and subsequent Open(MODE_NEWFILE) on the same path
	// will fail with ERROR_OBJECT_IN_USE.
	Close(out_file);
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
	return __result;
}

function_result Am_Lang_Process_canonicalPath_0(aobject * path)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	if (path != NULL) {
		__increase_reference_count(path);
	}

	string_holder *in_holder = (string_holder *) (path + 1);
	const char *in_str = in_holder->string_value;

	BPTR lock = Lock((CONST_STRPTR) in_str, ACCESS_READ);
	if (lock == (BPTR) NULL) {
		__result.return_value.value.object_value = path;
		__increase_reference_count(path);
		goto __exit;
	}

	UBYTE buffer[260];
	if (NameFromLock(lock, buffer, sizeof(buffer)) == 0) {
		UnLock(lock);
		__result.return_value.value.object_value = path;
		__increase_reference_count(path);
		goto __exit;
	}
	UnLock(lock);
	buffer[sizeof(buffer) - 1] = 0;

	aobject *out_str = __create_string((char const *) buffer, &Am_Lang_String);
	__result.return_value.value.object_value = out_str;

__exit: ;
	if (path != NULL) {
		__decrease_reference_count(path);
	}
	return __result;
}

function_result Am_Lang_Process_getCwd_0()
{
	function_result __result = { .has_return_value = true };

	// pr_CurrentDir is the lock the process inherited (or was set
	// to via CurrentDir()). Read directly off the Process struct
	// — no need to do the CurrentDir(BNULL)/CurrentDir(saved)
	// dance, which would race against any other code that touches
	// the current dir. NameFromLock fills `buffer` with the
	// canonical (volume-name) form.
	struct Process *proc = (struct Process *)FindTask(NULL);
	BPTR currentLock = proc->pr_CurrentDir;
	UBYTE buffer[260];
	if (currentLock != (BPTR)NULL && NameFromLock(currentLock, (STRPTR)buffer, sizeof(buffer))) {
		__result.return_value.value.object_value = __create_string((const char *)buffer, &Am_Lang_String);
	} else {
		__result.return_value.value.object_value = __create_string("", &Am_Lang_String);
	}
	return __result;
}

function_result Am_Lang_Process_runAndCaptureOutputInDir_0(aobject * command, aobject * workingDir)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;

	string_holder *dir_holder = (workingDir != NULL) ? (string_holder *) (workingDir + 1) : NULL;
	const char *dir_str = (dir_holder != NULL && dir_holder->length > 0) ? dir_holder->string_value : NULL;

	// CurrentDir() returns the previous lock — restore it on the
	// way out, but DON'T UnLock it (it's owned by the caller of
	// this task). DO UnLock the lock we created here.
	BPTR new_lock = (BPTR) NULL;
	BPTR old_lock = (BPTR) NULL;
	bool did_swap = false;
	if (dir_str != NULL) {
		new_lock = Lock((CONST_STRPTR) dir_str, ACCESS_READ);
		if (new_lock == (BPTR) NULL) {
			__throw_simple_exception("Failed to lock working directory", "in Am_Lang_Process_runAndCaptureOutputInDir_0", &__result);
			goto __exit;
		}
		old_lock = CurrentDir(new_lock);
		did_swap = true;
	}

	string_holder *cmd_holder = (string_holder *) (command + 1);
	STRPTR cmd_strptr = (STRPTR) cmd_holder->string_value;

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
		if (did_swap) { CurrentDir(old_lock); UnLock(new_lock); }
		__throw_simple_exception("Failed to open temp file", "in Am_Lang_Process_runAndCaptureOutputInDir_0", &__result);
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
	Close(out_file);
	if (status == -1) {
		DeleteFile((CONST_STRPTR) temp_path);
		if (did_swap) { CurrentDir(old_lock); UnLock(new_lock); }
		__throw_simple_exception("Failed to execute command", "in Am_Lang_Process_runAndCaptureOutputInDir_0", &__result);
		goto __exit;
	}

	BPTR in_file = Open((CONST_STRPTR) temp_path, MODE_OLDFILE);
	if (in_file == 0) {
		DeleteFile((CONST_STRPTR) temp_path);
		if (did_swap) { CurrentDir(old_lock); UnLock(new_lock); }
		__throw_simple_exception("Failed to read back command output", "in Am_Lang_Process_runAndCaptureOutputInDir_0", &__result);
		goto __exit;
	}

	(void) Seek(in_file, 0, OFFSET_END);
	LONG size = Seek(in_file, 0, OFFSET_BEGINNING);
	if (size < 0) {
		Close(in_file);
		DeleteFile((CONST_STRPTR) temp_path);
		if (did_swap) { CurrentDir(old_lock); UnLock(new_lock); }
		__throw_simple_exception("Failed to position in command-output file", "in Am_Lang_Process_runAndCaptureOutputInDir_0", &__result);
		goto __exit;
	}

	UBYTE *buffer = (UBYTE *) AllocVec((ULONG) (size + 1), MEMF_ANY | MEMF_CLEAR);
	if (buffer == NULL) {
		Close(in_file);
		DeleteFile((CONST_STRPTR) temp_path);
		if (did_swap) { CurrentDir(old_lock); UnLock(new_lock); }
		__throw_simple_exception("Out of memory reading command output", "in Am_Lang_Process_runAndCaptureOutputInDir_0", &__result);
		goto __exit;
	}

	LONG read = (size > 0) ? Read(in_file, buffer, size) : 0;
	Close(in_file);
	DeleteFile((CONST_STRPTR) temp_path);

	if (did_swap) {
		CurrentDir(old_lock);
		UnLock(new_lock);
	}

	if (read < 0) {
		FreeVec(buffer);
		__throw_simple_exception("Failed to read command output", "in Am_Lang_Process_runAndCaptureOutputInDir_0", &__result);
		goto __exit;
	}
	buffer[read] = 0;

	aobject *out_str = __create_string((char const *) buffer, &Am_Lang_String);
	FreeVec(buffer);
	__result.return_value.value.object_value = out_str;

__exit: ;
	return __result;
}
