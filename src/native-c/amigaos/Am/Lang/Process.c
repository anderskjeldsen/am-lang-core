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
	// NP_StackSize: without it the spawned command runs on the boot shell's
	// default stack (~4 KB), which silently kills any non-trivial child —
	// an AmLang binary (aminet-cli from the IDE) dies before main. Same fix
	// the capture variants below already carry.
	struct TagItem tags[] = {
		{ SYS_Asynch,    FALSE },
		{ SYS_UserShell, TRUE },
		{ NP_StackSize,  (ULONG) 262144 },
		{ TAG_DONE,      0 },
	};

	LONG status = SystemTagList(cmd_strptr, tags);
	__result.return_value.value.int_value = (int) status;

__exit: ;
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

	// V40 stderr capture: same recipe as the InDir variant — see the
	// banner there for the full rationale. Brief: CreateNewProcTags
	// with NP_ConsoleTask=NULL + manual pr_CES patch + asymmetric
	// Close (only err_file in parent; out_file + nil_in handled by
	// child's CLI cleanup).
	BPTR out_file = Open((CONST_STRPTR) temp_path, MODE_NEWFILE);
	BPTR err_file = Open((CONST_STRPTR) temp_path_err, MODE_NEWFILE);
	BPTR nil_in   = Open((CONST_STRPTR) "NIL:", MODE_OLDFILE);
	if (out_file == 0 || err_file == 0 || nil_in == 0) {
		if (out_file) Close(out_file);
		if (err_file) Close(err_file);
		if (nil_in)   Close(nil_in);
		__throw_simple_exception("Failed to open temp files / NIL:", "in Am_Lang_Process_runAndCaptureOutput_0", &__result);
		goto __exit;
	}

	static char g_bin_buf2[128];
	static char g_arg_buf2[512];
	{
		const char *src = (const char *) cmd_strptr;
		int p = 0;
		while (src[p] != 0 && src[p] != ' ' && src[p] != '\t' && p < (int)(sizeof(g_bin_buf2)-1)) {
			g_bin_buf2[p] = src[p]; p++;
		}
		g_bin_buf2[p] = 0;
		while (src[p] == ' ' || src[p] == '\t') p++;
		int a = 0;
		while (src[p] != 0 && a < (int)(sizeof(g_arg_buf2)-2)) {
			g_arg_buf2[a++] = src[p++];
		}
		g_arg_buf2[a++] = '\n';
		g_arg_buf2[a]   = 0;
	}

	BPTR seg = LoadSeg((CONST_STRPTR) g_bin_buf2);
	if (seg == 0) {
		int has_path = 0;
		for (int i = 0; g_bin_buf2[i] != 0; i++) {
			if (g_bin_buf2[i] == '/' || g_bin_buf2[i] == ':') { has_path = 1; break; }
		}
		if (!has_path) {
			char with_c[160];
			int p = 0;
			with_c[p++] = 'C'; with_c[p++] = ':';
			int k = 0;
			while (g_bin_buf2[k] != 0 && p < (int)(sizeof(with_c)-1)) with_c[p++] = g_bin_buf2[k++];
			with_c[p] = 0;
			seg = LoadSeg((CONST_STRPTR) with_c);
		}
	}
	if (seg == 0) {
		Close(out_file); Close(err_file); Close(nil_in);
		DeleteFile((CONST_STRPTR) temp_path);
		DeleteFile((CONST_STRPTR) temp_path_err);
		__throw_simple_exception("LoadSeg failed (binary not found)", "in Am_Lang_Process_runAndCaptureOutput_0", &__result);
		goto __exit;
	}

	Forbid();
	struct Process *child = CreateNewProcTags(
		NP_Seglist,     (ULONG) seg,
		NP_FreeSeglist, (ULONG) TRUE,
		NP_Cli,         (ULONG) TRUE,
		NP_Input,       (ULONG) nil_in,
		NP_Output,      (ULONG) out_file,
		NP_Error,       (ULONG) err_file,
		NP_ConsoleTask, (ULONG) NULL,
		NP_Arguments,   (ULONG) g_arg_buf2,
		NP_Name,        (ULONG) "amProcessCapture",
		NP_StackSize,   (ULONG) 65536,
		TAG_DONE);
	if (child != NULL) {
		child->pr_CIS = nil_in;
		child->pr_COS = out_file;
		child->pr_CES = err_file;
		struct CommandLineInterface *cli =
			(struct CommandLineInterface *) BADDR(child->pr_CLI);
		if (cli != NULL) {
			cli->cli_StandardInput  = nil_in;
			cli->cli_CurrentInput   = nil_in;
			cli->cli_StandardOutput = out_file;
			cli->cli_CurrentOutput  = out_file;
			// cli_CommandName is a BSTR; ixemul reads it for
			// argv[0]. Without setting it the child inherits a
			// stale value ("app" — the parent binary's name) and
			// gcc prints "app: No input files" instead of
			// "gcc: No input files". Build a length-prefixed
			// BCPL string from g_bin_buf2 into a static buffer
			// and point cli_CommandName at it (BPTR = MKBADDR).
			static UBYTE s_cmd_name_bstr[34];
			int slen = 0;
			while (slen < 30 && g_bin_buf2[slen] != 0) {
				s_cmd_name_bstr[slen + 1] = (UBYTE) g_bin_buf2[slen];
				slen++;
			}
			s_cmd_name_bstr[0] = (UBYTE) slen;
			cli->cli_CommandName = MKBADDR(s_cmd_name_bstr);
		}
	}
	Permit();

	if (child == NULL) {
		UnLoadSeg(seg);
		Close(out_file); Close(err_file); Close(nil_in);
		DeleteFile((CONST_STRPTR) temp_path);
		DeleteFile((CONST_STRPTR) temp_path_err);
		__throw_simple_exception("CreateNewProc failed", "in Am_Lang_Process_runAndCaptureOutput_0", &__result);
		goto __exit;
	}

	{
		// Break the instant the child is gone. If the child crashes on
		// exit (e.g. a -noixemul CLI child mishandling the inherited FHs ->
		// #80000008), it gets SUSPENDED on a requester and never leaves the
		// task list, so FindTask never returns NULL and we run to this cap.
		// Keep it modest so that case returns in seconds (with whatever the
		// child already wrote) rather than hanging for many minutes.
		// 375 * 40ms ~= 15s cap.
		int safety = 375;
		while (safety > 0) {
			struct Task *t;
			Forbid();
			t = FindTask((STRPTR) "amProcessCapture");
			Permit();
			if (t == NULL) break;
			Delay(2);
			safety--;
		}
		Delay(5);
	}

	Close(err_file);  // asymmetric: only err_file — out_file + nil_in handled by CLI cleanup

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

// Capture ONLY stdout, run via the DOS System() shell with `> tempfile`
// redirection. System() is synchronous and manages the child's lifecycle
// the standard DOS way — unlike the hand-rolled CreateNewProc capture in
// runAndCaptureOutputInDir, this works for a -noixemul child (am-git),
// which the CreateNewProc recipe (tuned for ixemul children) hangs at
// startup. stderr goes wherever the shell sends it (not captured).
function_result Am_Lang_Process_captureStdoutInDir_0(aobject * command, aobject * workingDir)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;

	string_holder *cmd_holder = (string_holder *) (command + 1);
	const char *cmd_str = (const char *) cmd_holder->string_value;

	string_holder *dir_holder2 = (workingDir != NULL) ? (string_holder *) (workingDir + 1) : NULL;
	const char *dir_str2 = (dir_holder2 != NULL && dir_holder2->length > 0) ? dir_holder2->string_value : NULL;

	// Swap current dir to workingDir so the command runs there (same
	// ownership rules as runAndCaptureOutputInDir: restore the previous
	// lock, UnLock only the one we created).
	BPTR cs_new_lock = (BPTR) NULL;
	BPTR cs_old_lock = (BPTR) NULL;
	bool cs_swapped = false;
	if (dir_str2 != NULL) {
		cs_new_lock = Lock((CONST_STRPTR) dir_str2, ACCESS_READ);
		if (cs_new_lock == (BPTR) NULL) {
			__throw_simple_exception("Failed to lock working directory", "in Am_Lang_Process_captureStdoutInDir_0", &__result);
			goto __exit;
		}
		cs_old_lock = CurrentDir(cs_new_lock);
		cs_swapped = true;
	}

	// Temp file goes in the (swapped) current dir as a RELATIVE name — no
	// dependency on a T:/RAM: assign, which a stripped-down system may
	// lack. When the cwd is a git repo, put it inside `.git/` so a
	// `git status` run through here doesn't report the temp file itself as
	// untracked. Built from the task address so concurrent captures don't
	// collide. Read back (also relative) BEFORE restoring the cwd.
	UBYTE temp_path[48];
	{
		ULONG i = 0;
		BPTR git_lock = Lock((CONST_STRPTR) ".git", ACCESS_READ);
		if (git_lock != (BPTR) NULL) {
			UnLock(git_lock);
			const char *g = ".git/";
			ULONG j = 0;
			while (g[j] != 0) { temp_path[i++] = g[j++]; }
		}
		const char *pfx = "am_cap_";
		ULONG k = 0;
		while (pfx[k] != 0) { temp_path[i++] = pfx[k++]; }
		ULONG addr = (ULONG) FindTask(NULL);
		for (LONG nibble = 7; nibble >= 0; nibble--) {
			ULONG v = (addr >> (nibble * 4)) & 0xF;
			temp_path[i++] = (UBYTE) (v < 10 ? ('0' + v) : ('a' + (v - 10)));
		}
		temp_path[i] = 0;
	}

	// Build "<command> >am_cap_xxxx" — the user shell performs the redirect.
	static char cs_cmd[1024];
	{
		int p = 0;
		const char *s = cmd_str;
		while (*s != 0 && p < (int)(sizeof(cs_cmd) - 80)) { cs_cmd[p++] = *s++; }
		cs_cmd[p++] = ' ';
		cs_cmd[p++] = '>';
		const char *t = (const char *) temp_path;
		while (*t != 0 && p < (int)(sizeof(cs_cmd) - 1)) { cs_cmd[p++] = *t++; }
		cs_cmd[p] = 0;
	}

	struct TagItem cs_tags[] = {
		{ SYS_Asynch,    FALSE },
		{ SYS_UserShell, TRUE },
		{ TAG_DONE,      0 },
	};
	SystemTagList((STRPTR) cs_cmd, cs_tags);

	// Read the relative temp file while cwd is still the working dir.
	UBYTE *cs_buf = NULL;
	LONG cs_size = am_proc_read_and_delete_temp(temp_path, &cs_buf);
	if (cs_size < 0) cs_size = 0;

	if (cs_swapped) {
		CurrentDir(cs_old_lock);
		UnLock(cs_new_lock);
	}
	if (cs_buf != NULL) {
		__result.return_value.value.object_value = __create_string((const char *) cs_buf, &Am_Lang_String);
		FreeVec(cs_buf);
	} else {
		__result.return_value.value.object_value = __create_string("", &Am_Lang_String);
	}

__exit: ;
	return __result;
}

function_result Am_Lang_Process_runAndCaptureOutputInDir_0(aobject * command, aobject * workingDir)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;

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

	// V40 stderr-capture recipe (verified via gcctest harness):
	//   1. Open out_file, err_file (MODE_NEWFILE) + NIL: as stdin
	//   2. LoadSeg the command binary, parse args
	//   3. CreateNewProcTags with NP_Input=NIL, NP_Output=out,
	//      NP_Error=err, NP_ConsoleTask=NULL (breaks ixemul's
	//      "*" fallback that otherwise leaks stderr to the
	//      launching shell)
	//   4. Manually patch pr_CIS/pr_COS/pr_CES + CLI struct under
	//      Forbid (V40 silently drops NP_Error tag → pr_CES must
	//      be set manually; the cli_StandardInput/Output fields
	//      are needed for child's libc init to read consistent
	//      values)
	//   5. Wait FindTask returns NULL + small extra delay
	//   6. ASYMMETRIC Close: only err_file in parent. V40 honours
	//      NP_Input/Output so CLI cleanup closes those (parent
	//      Close = use-after-free → hang). V40 drops NP_Error so
	//      cleanup never tracked err_file → parent must close.
	//   7. Re-open both files by name for reading.
	BPTR out_file = Open((CONST_STRPTR) temp_path, MODE_NEWFILE);
	BPTR err_file = Open((CONST_STRPTR) temp_path_err, MODE_NEWFILE);
	BPTR nil_in   = Open((CONST_STRPTR) "NIL:", MODE_OLDFILE);
	if (out_file == 0 || err_file == 0 || nil_in == 0) {
		if (out_file) Close(out_file);
		if (err_file) Close(err_file);
		if (nil_in)   Close(nil_in);
		if (did_swap) { CurrentDir(old_lock); UnLock(new_lock); }
		__throw_simple_exception("Failed to open temp files / NIL:", "in Am_Lang_Process_runAndCaptureOutputInDir_0", &__result);
		goto __exit;
	}

	// Split "binary args..." for NP_Arguments and LoadSeg.
	// Same shape as rp_split_cmd in RunningProcess.c — kept local so
	// Process.c doesn't depend on RunningProcess internals.
	static char g_bin_buf[128];
	static char g_arg_buf[512];
	{
		const char *src = (const char *) cmd_strptr;
		int p = 0;
		while (src[p] != 0 && src[p] != ' ' && src[p] != '\t' && p < (int)(sizeof(g_bin_buf)-1)) {
			g_bin_buf[p] = src[p]; p++;
		}
		g_bin_buf[p] = 0;
		while (src[p] == ' ' || src[p] == '\t') p++;
		int a = 0;
		while (src[p] != 0 && a < (int)(sizeof(g_arg_buf)-2)) {
			g_arg_buf[a++] = src[p++];
		}
		g_arg_buf[a++] = '\n';
		g_arg_buf[a]   = 0;
	}

	// LoadSeg the binary — try as given, then with C: prefix on
	// failure (matches the path search a typed shell command does).
	BPTR seg = LoadSeg((CONST_STRPTR) g_bin_buf);
	if (seg == 0) {
		// Check for path separators — only retry C: if it's a bare name.
		int has_path = 0;
		for (int i = 0; g_bin_buf[i] != 0; i++) {
			if (g_bin_buf[i] == '/' || g_bin_buf[i] == ':') { has_path = 1; break; }
		}
		if (!has_path) {
			char with_c[160];
			int p = 0;
			with_c[p++] = 'C'; with_c[p++] = ':';
			int k = 0;
			while (g_bin_buf[k] != 0 && p < (int)(sizeof(with_c)-1)) with_c[p++] = g_bin_buf[k++];
			with_c[p] = 0;
			seg = LoadSeg((CONST_STRPTR) with_c);
		}
	}
	if (seg == 0) {
		Close(out_file); Close(err_file); Close(nil_in);
		DeleteFile((CONST_STRPTR) temp_path);
		DeleteFile((CONST_STRPTR) temp_path_err);
		if (did_swap) { CurrentDir(old_lock); UnLock(new_lock); }
		__throw_simple_exception("LoadSeg failed (binary not found)", "in Am_Lang_Process_runAndCaptureOutputInDir_0", &__result);
		goto __exit;
	}

	// Spawn child under Forbid + manual CLI/process field patches.
	Forbid();
	struct Process *child = CreateNewProcTags(
		NP_Seglist,     (ULONG) seg,
		NP_FreeSeglist, (ULONG) TRUE,
		NP_Cli,         (ULONG) TRUE,
		NP_Input,       (ULONG) nil_in,
		NP_Output,      (ULONG) out_file,
		NP_Error,       (ULONG) err_file,
		NP_ConsoleTask, (ULONG) NULL,
		NP_Arguments,   (ULONG) g_arg_buf,
		NP_Name,        (ULONG) "amProcessCapture",
		NP_StackSize,   (ULONG) 65536,
		TAG_DONE);
	if (child != NULL) {
		child->pr_CIS = nil_in;
		child->pr_COS = out_file;
		child->pr_CES = err_file;
		struct CommandLineInterface *cli =
			(struct CommandLineInterface *) BADDR(child->pr_CLI);
		if (cli != NULL) {
			cli->cli_StandardInput  = nil_in;
			cli->cli_CurrentInput   = nil_in;
			cli->cli_StandardOutput = out_file;
			cli->cli_CurrentOutput  = out_file;
			// cli_CommandName is a BSTR; ixemul reads it for
			// argv[0]. Without setting it the child inherits a
			// stale value ("app" — the parent binary's name) and
			// gcc prints "app: No input files" instead of
			// "gcc: No input files". Build a length-prefixed
			// BCPL string from g_bin_buf into a static buffer
			// and point cli_CommandName at it (BPTR = MKBADDR).
			static UBYTE s_cmd_name_bstr[34];
			int slen = 0;
			while (slen < 30 && g_bin_buf[slen] != 0) {
				s_cmd_name_bstr[slen + 1] = (UBYTE) g_bin_buf[slen];
				slen++;
			}
			s_cmd_name_bstr[0] = (UBYTE) slen;
			cli->cli_CommandName = MKBADDR(s_cmd_name_bstr);
		}
	}
	Permit();

	if (child == NULL) {
		UnLoadSeg(seg);
		Close(out_file); Close(err_file); Close(nil_in);
		DeleteFile((CONST_STRPTR) temp_path);
		DeleteFile((CONST_STRPTR) temp_path_err);
		if (did_swap) { CurrentDir(old_lock); UnLock(new_lock); }
		__throw_simple_exception("CreateNewProc failed", "in Am_Lang_Process_runAndCaptureOutputInDir_0", &__result);
		goto __exit;
	}

	// Wait for child to fully exit before touching the inherited FHs.
	// 600 ticks * 40ms = 24 seconds cap.
	{
		// Break the instant the child is gone. If the child crashes on
		// exit (e.g. a -noixemul CLI child mishandling the inherited FHs ->
		// #80000008), it gets SUSPENDED on a requester and never leaves the
		// task list, so FindTask never returns NULL and we run to this cap.
		// Keep it modest so that case returns in seconds (with whatever the
		// child already wrote) rather than hanging for many minutes.
		// 375 * 40ms ~= 15s cap.
		int safety = 375;
		while (safety > 0) {
			struct Task *t;
			Forbid();
			t = FindTask((STRPTR) "amProcessCapture");
			Permit();
			if (t == NULL) break;
			Delay(2);
			safety--;
		}
		Delay(5);  // extra flush window for any in-flight CLI cleanup
	}

	// Asymmetric Close — see banner above.
	Close(err_file);
	// out_file + nil_in: DON'T touch — CLI cleanup closed them, our
	// Close would be use-after-free → handler-port hang.

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
	return __result;
}
