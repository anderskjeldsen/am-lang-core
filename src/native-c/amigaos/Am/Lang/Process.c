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
#include <dos/dosextens.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <utility/tagitem.h>

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
	if (command != NULL) {
		__decrease_reference_count(command);
	}
	return __result;
}

// Read the entire contents of a closed temp file at `path` into a
// freshly-AllocVec'd buffer. Caller frees with FreeVec on success.
// Returns the number of bytes read on success, -1 on any failure
// (caller treats as "empty file" and just doesn't append anything).
// Deletes the temp file before returning regardless of success.
static LONG am_proc_read_and_delete_temp(const UBYTE *path, UBYTE **out_buf)
{
	*out_buf = NULL;
	BPTR in_file = Open((CONST_STRPTR) path, MODE_OLDFILE);
	if (in_file == 0) {
		DeleteFile((CONST_STRPTR) path);
		return -1;
	}
	(void) Seek(in_file, 0, OFFSET_END);
	LONG size = Seek(in_file, 0, OFFSET_BEGINNING);
	if (size < 0) {
		Close(in_file);
		DeleteFile((CONST_STRPTR) path);
		return -1;
	}
	if (size == 0) {
		Close(in_file);
		DeleteFile((CONST_STRPTR) path);
		return 0;
	}
	UBYTE *buf = (UBYTE *) AllocVec((ULONG) (size + 1), MEMF_ANY | MEMF_CLEAR);
	if (buf == NULL) {
		Close(in_file);
		DeleteFile((CONST_STRPTR) path);
		return -1;
	}
	LONG read = Read(in_file, buf, size);
	Close(in_file);
	DeleteFile((CONST_STRPTR) path);
	if (read < 0) {
		FreeVec(buf);
		return -1;
	}
	buf[read] = 0;
	*out_buf = buf;
	return read;
}

// Build a unique-per-task temp path under T:. The optional suffix is
// appended after the task pointer so callers can produce paired names
// (`..._XXXX` for stdout, `..._XXXX_e` for stderr) without clashing.
static void am_proc_build_temp_path(UBYTE *temp_path, struct Task *self, const char *suffix)
{
	const STRPTR prefix = (STRPTR) "T:am_proc_";
	ULONG i = 0;
	while (prefix[i] != 0) { temp_path[i] = prefix[i]; i++; }
	ULONG addr = (ULONG) self;
	for (LONG nibble = 7; nibble >= 0; nibble--) {
		ULONG v = (addr >> (nibble * 4)) & 0xF;
		temp_path[i++] = (UBYTE) (v < 10 ? ('0' + v) : ('a' + (v - 10)));
	}
	if (suffix != NULL) {
		ULONG j = 0;
		while (suffix[j] != 0) {
			temp_path[i++] = (UBYTE) suffix[j++];
		}
	}
	temp_path[i] = 0;
}

function_result Am_Lang_Process_runAndCaptureOutput_0(aobject * command)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	if (command != NULL) {
		__increase_reference_count(command);
	}

	string_holder *cmd_holder = (string_holder *) (command + 1);
	STRPTR cmd_strptr = (STRPTR) cmd_holder->string_value;

	// Build a unique temp filename in T: (the conventional AmigaOS temp dir,
	// usually assigned to RAM:T so it self-cleans on reboot). Two paths:
	// one for stdout, one for stderr. Both are read back and concatenated
	// so the caller sees the command's full output — gcc and other Unix-
	// port tools write their diagnostics to stderr exclusively, and without
	// the SYS_Error redirect those would silently go to NIL: (when the
	// IDE is launched from Workbench) or to the parent shell (when from a
	// CLI), neither of which our CliView can show.
	UBYTE temp_path[64];
	UBYTE temp_path_err[64];
	struct Task *self = FindTask(NULL);
	am_proc_build_temp_path(temp_path,     self, NULL);
	am_proc_build_temp_path(temp_path_err, self, "_e");

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
	BPTR err_file = Open((CONST_STRPTR) temp_path_err, MODE_NEWFILE);
	// stderr file is best-effort — if it can't be opened we just lose
	// stderr capture, not the whole command run. NIL: is set as the
	// SYS_Error value so the System call doesn't fall back to inheriting
	// the parent's handle (which we know writes nowhere useful).
	BPTR err_value = err_file != 0 ? err_file : 0;

	struct TagItem run_tags[] = {
		{ SYS_Input,     (ULONG) NULL },
		{ SYS_Output,    (ULONG) out_file },
		{ SYS_Error,     (ULONG) err_value },
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
	if (err_file != 0) Close(err_file);
	if (status == -1) {
		DeleteFile((CONST_STRPTR) temp_path);
		DeleteFile((CONST_STRPTR) temp_path_err);
		__throw_simple_exception("Failed to execute command", "in Am_Lang_Process_runAndCaptureOutput_0", &__result);
		goto __exit;
	}

	// Read both temp files. Both reads delete the temp file as part
	// of the helper, regardless of success — no orphaned files on
	// any partial-failure path.
	UBYTE *out_buf = NULL;
	LONG out_size = am_proc_read_and_delete_temp(temp_path, &out_buf);
	UBYTE *err_buf = NULL;
	LONG err_size = am_proc_read_and_delete_temp(temp_path_err, &err_buf);
	if (out_size < 0) out_size = 0;
	if (err_size < 0) err_size = 0;

	// Concat. stdout first (program's intended output), stderr after
	// (diagnostics). Perfect interleaving would need per-write
	// timestamps the OS doesn't give us; for a typical compile this
	// "all the output, then all the warnings" ordering is fine.
	LONG total = out_size + err_size;
	UBYTE *combined = (UBYTE *) AllocVec((ULONG)(total + 1), MEMF_ANY | MEMF_CLEAR);
	if (combined == NULL) {
		if (out_buf != NULL) FreeVec(out_buf);
		if (err_buf != NULL) FreeVec(err_buf);
		__throw_simple_exception("Out of memory reading command output", "in Am_Lang_Process_runAndCaptureOutput_0", &__result);
		goto __exit;
	}
	if (out_size > 0 && out_buf != NULL) {
		for (LONG i = 0; i < out_size; i++) combined[i] = out_buf[i];
	}
	if (err_size > 0 && err_buf != NULL) {
		for (LONG i = 0; i < err_size; i++) combined[out_size + i] = err_buf[i];
	}
	combined[total] = 0;
	if (out_buf != NULL) FreeVec(out_buf);
	if (err_buf != NULL) FreeVec(err_buf);

	aobject *str = __create_string((char const *) combined, &Am_Lang_String);
	FreeVec(combined);
	__result.return_value.value.object_value = str;

__exit: ;
	if (command != NULL) {
		__decrease_reference_count(command);
	}
	return __result;
}

function_result Am_Lang_Process_canonicalPath_0(aobject * path)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	if (path != NULL) {
		__increase_reference_count(path);
	}

	// Lock + NameFromLock turns "DH0:Tools/" into the assigned
	// volume name form ("Workbench:Tools"). Best-effort: if any of
	// the steps fails (path missing, no read access, buffer too
	// small) we return the original input so the caller still has
	// something usable.
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
	if (command != NULL) {
		__increase_reference_count(command);
	}
	if (workingDir != NULL) {
		__increase_reference_count(workingDir);
	}

	string_holder *dir_holder = (workingDir != NULL) ? (string_holder *) (workingDir + 1) : NULL;
	const char *dir_str = (dir_holder != NULL && dir_holder->length > 0) ? dir_holder->string_value : NULL;

	// Swap the process's current dir to `workingDir` before running.
	// CurrentDir() returns the previous lock so we can restore on
	// the way out — we must NOT UnLock() that previous lock (it
	// belongs to whoever set up our CD originally). We DO UnLock
	// the lock we created here, once the previous CD is back in
	// place.
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

	// Body mirrors runAndCaptureOutput. Duplicated rather than
	// refactored so the cwd save/restore stays tight against the
	// actual SystemTagList call.
	string_holder *cmd_holder = (string_holder *) (command + 1);
	STRPTR cmd_strptr = (STRPTR) cmd_holder->string_value;

	UBYTE temp_path[64];
	UBYTE temp_path_err[64];
	struct Task *self = FindTask(NULL);
	am_proc_build_temp_path(temp_path,     self, NULL);
	am_proc_build_temp_path(temp_path_err, self, "_e");

	BPTR out_file = Open((CONST_STRPTR) temp_path, MODE_NEWFILE);
	if (out_file == 0) {
		if (did_swap) { CurrentDir(old_lock); UnLock(new_lock); }
		__throw_simple_exception("Failed to open temp file", "in Am_Lang_Process_runAndCaptureOutputInDir_0", &__result);
		goto __exit;
	}
	BPTR err_file = Open((CONST_STRPTR) temp_path_err, MODE_NEWFILE);
	BPTR err_value = err_file != 0 ? err_file : 0;

	struct TagItem run_tags[] = {
		{ SYS_Input,     (ULONG) NULL },
		{ SYS_Output,    (ULONG) out_file },
		{ SYS_Error,     (ULONG) err_value },
		{ SYS_Asynch,    FALSE },
		{ SYS_UserShell, TRUE },
		{ TAG_DONE,      0 },
	};

	LONG status = SystemTagList(cmd_strptr, run_tags);
	Close(out_file);
	if (err_file != 0) Close(err_file);
	if (status == -1) {
		DeleteFile((CONST_STRPTR) temp_path);
		DeleteFile((CONST_STRPTR) temp_path_err);
		if (did_swap) { CurrentDir(old_lock); UnLock(new_lock); }
		__throw_simple_exception("Failed to execute command", "in Am_Lang_Process_runAndCaptureOutputInDir_0", &__result);
		goto __exit;
	}

	UBYTE *out_buf = NULL;
	LONG out_size = am_proc_read_and_delete_temp(temp_path, &out_buf);
	UBYTE *err_buf = NULL;
	LONG err_size = am_proc_read_and_delete_temp(temp_path_err, &err_buf);
	if (out_size < 0) out_size = 0;
	if (err_size < 0) err_size = 0;

	if (did_swap) {
		CurrentDir(old_lock);
		UnLock(new_lock);
	}

	LONG total = out_size + err_size;
	UBYTE *combined = (UBYTE *) AllocVec((ULONG)(total + 1), MEMF_ANY | MEMF_CLEAR);
	if (combined == NULL) {
		if (out_buf != NULL) FreeVec(out_buf);
		if (err_buf != NULL) FreeVec(err_buf);
		__throw_simple_exception("Out of memory reading command output", "in Am_Lang_Process_runAndCaptureOutputInDir_0", &__result);
		goto __exit;
	}
	if (out_size > 0 && out_buf != NULL) {
		for (LONG i = 0; i < out_size; i++) combined[i] = out_buf[i];
	}
	if (err_size > 0 && err_buf != NULL) {
		for (LONG i = 0; i < err_size; i++) combined[out_size + i] = err_buf[i];
	}
	combined[total] = 0;
	if (out_buf != NULL) FreeVec(out_buf);
	if (err_buf != NULL) FreeVec(err_buf);

	aobject *out_str = __create_string((char const *) combined, &Am_Lang_String);
	FreeVec(combined);
	__result.return_value.value.object_value = out_str;

__exit: ;
	if (command != NULL) {
		__decrease_reference_count(command);
	}
	if (workingDir != NULL) {
		__decrease_reference_count(workingDir);
	}
	return __result;
}
