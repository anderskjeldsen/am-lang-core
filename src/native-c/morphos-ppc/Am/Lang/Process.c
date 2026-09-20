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

// Ported from amigaos/Am/Lang/Process.c 2026-07-05. The prior MorphOS
// implementation used SystemTagList with SYS_Input=NULL, which was
// intended to redirect stdin to NIL: — the AmigaOS 3.x/V40 shell
// tolerates that, but MorphOS's dos.library dereferences the input FH
// and crashes on the null pointer. Same CreateNewProcTags recipe as
// the AmigaOS side works on both, so we keep them in lock-step.

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

	string_holder *cmd_holder = (string_holder *) ((char *) command + sizeof(aobject));
	STRPTR cmd_strptr = (STRPTR) cmd_holder->string_value;

	// SYS_Input/SYS_Output omitted on purpose: SystemTagList then inherits the
	// caller's stdin/stdout, so the command's output reaches the user's shell.
	// Passing NULL would silently redirect to NIL: on AmigaOS but CRASH on
	// MorphOS — omit the tag entirely instead.
	// NP_StackSize matters here: without it the spawned command gets MorphOS'
	// default ~4 KB stack (crash dumps from a failed `make` read
	// StackSize 0x1f40) and anything non-trivial dies immediately -- which is
	// why `amlc build` could not run make while running make by hand from a
	// shell worked: the shell's `Stack` setting only applies to ITS children,
	// not to a process we create here. The capture variants below already pass
	// this tag; the plain run path did not.
	// Extension drawers etc. reach the child as a real path list. Built
	// fresh per spawn because NP_Path hands ownership to the child, which
	// frees it on exit -- reusing one chain hands over dead memory.
	// TAG_IGNORE when there is nothing to add, so the child keeps its
	// default inheritance. Not freed on a failed spawn: whether ownership
	// transfers in that case is unmeasured, and leaking beats a double free.
	BPTR __sp_chain = __build_spawn_path();
	struct TagItem tags[] = {
		{ SYS_Asynch,    FALSE },
		{ SYS_UserShell, TRUE },
		// 256 KB, not 100 KB: the child may be a full AmLang binary
		// (aminet-cli) doing an OpenSSL handshake on its main stack.
		{ NP_StackSize,  (ULONG) 262144 },
		{ __sp_chain != 0 ? NP_Path : TAG_IGNORE, (ULONG) __sp_chain },
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

// Where a capture spawn's LoadSeg actually found the binary, plus a lock on
// the drawer holding it. A child created from a pre-loaded NP_Seglist gives
// DOS no file to derive pr_HomeDir from, so without NP_HomeDir the child has
// NO PROGDIR: at all — and PROGDIR: is how a bundled tool finds the files
// shipped beside it (amlc's config.json / templates/package.yml, for one:
// without this it silently scaffolded the built-in cross-compiling
// package.yml instead of the packager's native one).
// Returns 0 when the resolved name has no directory part; the tag is then
// omitted and behaviour is exactly what it was before.
static char g_cap_resolved[192];

static void am_proc_record_resolved(const char * p) {
	int i = 0;
	if (p == NULL) { g_cap_resolved[0] = 0; return; }
	while (p[i] != 0 && i < (int) sizeof(g_cap_resolved) - 1) {
		g_cap_resolved[i] = p[i];
		i++;
	}
	g_cap_resolved[i] = 0;
}

static BPTR am_proc_lock_binary_dir(void) {
	const char * p = g_cap_resolved;
	BPTR fl;
	BPTR dir;
	struct Process * me;

	// Ask DOS where the binary actually IS, rather than string-splitting the
	// name we handed to LoadSeg. A bare "amlc" resolved through the current
	// directory, an assign or the shell path has no directory part to split
	// off -- that case used to return 0, the NP_HomeDir tag was dropped, and
	// the child got NO PROGDIR: at all.
	if (p[0] != 0) {
		fl = Lock((CONST_STRPTR) p, ACCESS_READ);
		if (fl != 0) {
			dir = ParentDir(fl);
			UnLock(fl);
			if (dir != 0) return dir;
		}
	}
	// Last resort: hand the child OUR PROGDIR:. A child with no home dir has
	// no PROGDIR: at all, and DOS answers any PROGDIR: path with a modal
	// "Please insert volume PROGDIR: in any drive" requester -- which wedges
	// the machine. A home dir that merely points elsewhere is strictly
	// better: a bundled tool probing for an optional file beside itself
	// (amlc's config.json / templates/package.yml) simply misses and carries
	// on with its defaults.
	me = (struct Process *) FindTask(NULL);
	if (me != NULL && me->pr_Task.tc_Node.ln_Type == NT_PROCESS && me->pr_HomeDir != 0) {
		return DupLock(me->pr_HomeDir);
	}
	return 0;
}

/* AmigaDOS path-list node: a chain of (next, lock) pairs. */
struct am_proc_path_node {
	BPTR path_Next;
	BPTR path_Lock;
};

/* "<dir named by lock>/<name>" into out. 0 when the lock has no name.
 * A volume/assign root already ends in ':' and takes no separator. */
static int am_proc_join_lock(BPTR lock, const char * name, char * out, int outsz) {
	int len = 0;
	int n = 0;
	if (lock == 0) return 0;
	if (NameFromLock(lock, (STRPTR) out, (LONG) outsz - 1) == DOSFALSE) return 0;
	while (out[len] != 0 && len < outsz - 2) len++;
	if (len > 0 && out[len - 1] != '/' && out[len - 1] != ':' && len < outsz - 2) {
		out[len++] = '/';
		out[len] = 0;
	}
	while (name[n] != 0 && len + n < outsz - 1) {
		out[len + n] = name[n];
		n++;
	}
	out[len + n] = 0;
	return 1;
}

/* Resolve `name` the way the CLI's spawner does, then LoadSeg it.
 *
 * This capture path used to try ONLY the name verbatim and then a "C:"
 * prefix, while RunningProcess -- what the CLI panel spawns through -- also
 * walks the installed extension drawers and the Shell's own path list
 * (cli_CommandDir). The two disagreed, so a command the user can run by hand
 * in the CLI was "not found" when the AI agent ran the very same string
 * through run_command: the MorphOS SDK's `make` lives on the shell path
 * (GG:bin), not in C:. Same order as rp_loadseg_with_path so both entry
 * points agree.
 *
 * Records every attempt through am_proc_record_resolved so the child's
 * NP_HomeDir (its PROGDIR:) still names the drawer the binary came from. */
static BPTR am_proc_loadseg_with_path(const char * name) {
	BPTR seg;
	BPTR sp;
	struct CommandLineInterface * cli;
	char buf[260];
	int i;

	if (name == NULL || name[0] == 0) return 0;

	/* 1. Verbatim: absolute paths, volume-prefixed names, current dir. */
	am_proc_record_resolved(name);
	seg = LoadSeg((CONST_STRPTR) name);
	if (seg != 0) return seg;

	/* A path was spelled out, so failing WAS the answer. Don't go hunting
	 * for a same-named file somewhere else on the path. */
	i = 0;
	while (name[i] != 0) {
		if (name[i] == '/' || name[i] == ':') return 0;
		i++;
	}

	/* 2a. Installed extension drawers (am-git, the compiler, ...). */
	sp = __build_spawn_path();
	if (sp != 0) {
		struct am_proc_path_node * spn = (struct am_proc_path_node *) BADDR(sp);
		while (spn != NULL) {
			if (am_proc_join_lock(spn->path_Lock, name, buf, (int) sizeof(buf))) {
				am_proc_record_resolved(buf);
				seg = LoadSeg((CONST_STRPTR) buf);
				if (seg != 0) {
					__free_spawn_path(sp);
					return seg;
				}
			}
			spn = (struct am_proc_path_node *) BADDR(spn->path_Next);
		}
		/* Nothing matched: never handed to a child, so we free it. */
		__free_spawn_path(sp);
	}

	/* 2b. The Shell's path list -- our own CLI's, or the Workbench /
	 * Ambient process's when the studio was started from an icon. */
	cli = __effective_path_cli();
	if (cli != NULL) {
		struct am_proc_path_node * node =
			(struct am_proc_path_node *) BADDR(cli->cli_CommandDir);
		while (node != NULL) {
			if (am_proc_join_lock(node->path_Lock, name, buf, (int) sizeof(buf))) {
				am_proc_record_resolved(buf);
				seg = LoadSeg((CONST_STRPTR) buf);
				if (seg != 0) return seg;
			}
			node = (struct am_proc_path_node *) BADDR(node->path_Next);
		}
	}

	/* 3. C: -- every install has it assigned. */
	buf[0] = 'C'; buf[1] = ':';
	i = 0;
	while (name[i] != 0 && i < (int) sizeof(buf) - 3) {
		buf[2 + i] = name[i];
		i++;
	}
	buf[2 + i] = 0;
	am_proc_record_resolved(buf);
	return LoadSeg((CONST_STRPTR) buf);   /* 0 on failure */
}

function_result Am_Lang_Process_runAndCaptureOutput_0(aobject * command)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;

	string_holder *cmd_holder = (string_holder *) ((char *) command + sizeof(aobject));
	STRPTR cmd_strptr = (STRPTR) cmd_holder->string_value;

	// Build a unique temp filename in T: (the conventional temp dir,
	// usually assigned to RAM:T so it self-cleans on reboot). Two paths:
	// one for stdout, one for stderr. Both are read back and concatenated
	// so the caller sees the command's full output — Unix-port tools
	// write diagnostics to stderr exclusively, and without the SYS_Error
	// redirect those would silently go to NIL: or to the launching shell.
	UBYTE temp_path[64];
	UBYTE temp_path_err[64];
	struct Task *self = FindTask(NULL);
	am_proc_build_temp_path(temp_path,     self, NULL);
	am_proc_build_temp_path(temp_path_err, self, "_e");

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

	/* Same search the CLI uses: verbatim, extension drawers, the Shell's
	 * path list, then C:. */
	BPTR seg = am_proc_loadseg_with_path(g_bin_buf2);
	if (seg == 0) {
		Close(out_file); Close(err_file); Close(nil_in);
		DeleteFile((CONST_STRPTR) temp_path);
		DeleteFile((CONST_STRPTR) temp_path_err);
		__throw_simple_exception("LoadSeg failed (binary not found)", "in Am_Lang_Process_runAndCaptureOutput_0", &__result);
		goto __exit;
	}

	// PROGDIR: for the child. Taken BEFORE Forbid() — Lock() is a DOS call.
	BPTR child_home = am_proc_lock_binary_dir();
	Forbid();
	BPTR sp_chain = __build_spawn_path();
	struct Process *child = CreateNewProcTags(
		NP_Seglist,     (ULONG) seg,
		NP_FreeSeglist, (ULONG) TRUE,
		NP_Cli,         (ULONG) TRUE,
		NP_Priority,    (ULONG) 0,   /* not the caller's (the UI task may run at +1) */
		NP_Input,       (ULONG) nil_in,
		NP_Output,      (ULONG) out_file,
		NP_Error,       (ULONG) err_file,
		NP_ConsoleTask, (ULONG) NULL,
		NP_Arguments,   (ULONG) g_arg_buf2,
		NP_Name,        (ULONG) "amProcessCapture",
		NP_StackSize,   (ULONG) 65536,
		(child_home != 0 ? NP_HomeDir : TAG_IGNORE), (ULONG) child_home,
		(sp_chain != 0 ? NP_Path : TAG_IGNORE), (ULONG) sp_chain,
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
			// cli_CommandName is a BSTR; ixemul reads it for argv[0].
			// Without setting it the child inherits a stale value
			// ("app" — the parent binary's name) and gcc prints
			// "app: No input files" instead of "gcc: No input files".
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
		// DOS only takes ownership of the HomeDir lock on success.
		if (child_home != 0) UnLock(child_home);
		Close(out_file); Close(err_file); Close(nil_in);
		DeleteFile((CONST_STRPTR) temp_path);
		DeleteFile((CONST_STRPTR) temp_path_err);
		__throw_simple_exception("CreateNewProc failed", "in Am_Lang_Process_runAndCaptureOutput_0", &__result);
		goto __exit;
	}

	{
		int safety = 600;
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

	UBYTE *out_buf = NULL;
	LONG out_size = am_proc_read_and_delete_temp(temp_path, &out_buf);
	UBYTE *err_buf = NULL;
	LONG err_size = am_proc_read_and_delete_temp(temp_path_err, &err_buf);
	if (out_size < 0) out_size = 0;
	if (err_size < 0) err_size = 0;

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

	string_holder *in_holder = (string_holder *) ((char *) path + sizeof(aobject));
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

	string_holder *dir_holder = (workingDir != NULL) ? (string_holder *) ((char *) workingDir + sizeof(aobject)) : NULL;
	const char *dir_str = (dir_holder != NULL && dir_holder->length > 0) ? dir_holder->string_value : NULL;

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

	string_holder *cmd_holder = (string_holder *) ((char *) command + sizeof(aobject));
	STRPTR cmd_strptr = (STRPTR) cmd_holder->string_value;

	UBYTE temp_path[64];
	UBYTE temp_path_err[64];
	struct Task *self = FindTask(NULL);
	am_proc_build_temp_path(temp_path,     self, NULL);
	am_proc_build_temp_path(temp_path_err, self, "_e");

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

	/* Same search the CLI uses: verbatim, extension drawers, the Shell's
	 * path list, then C:. Previously this tried only the name and "C:", so
	 * the AI agent's run_command could not find commands the user could run
	 * by hand in the CLI panel -- SDK `make` being the obvious one. */
	BPTR seg = am_proc_loadseg_with_path(g_bin_buf);
	if (seg == 0) {
		Close(out_file); Close(err_file); Close(nil_in);
		DeleteFile((CONST_STRPTR) temp_path);
		DeleteFile((CONST_STRPTR) temp_path_err);
		if (did_swap) { CurrentDir(old_lock); UnLock(new_lock); }
		__throw_simple_exception("LoadSeg failed (binary not found)", "in Am_Lang_Process_runAndCaptureOutputInDir_0", &__result);
		goto __exit;
	}

	// PROGDIR: for the child. Taken BEFORE Forbid() — Lock() is a DOS call.
	BPTR child_home = am_proc_lock_binary_dir();
	Forbid();
	BPTR sp_chain = __build_spawn_path();
	struct Process *child = CreateNewProcTags(
		NP_Seglist,     (ULONG) seg,
		NP_FreeSeglist, (ULONG) TRUE,
		NP_Cli,         (ULONG) TRUE,
		NP_Priority,    (ULONG) 0,   /* not the caller's (the UI task may run at +1) */
		NP_Input,       (ULONG) nil_in,
		NP_Output,      (ULONG) out_file,
		NP_Error,       (ULONG) err_file,
		NP_ConsoleTask, (ULONG) NULL,
		NP_Arguments,   (ULONG) g_arg_buf,
		NP_Name,        (ULONG) "amProcessCapture",
		NP_StackSize,   (ULONG) 65536,
		(child_home != 0 ? NP_HomeDir : TAG_IGNORE), (ULONG) child_home,
		(sp_chain != 0 ? NP_Path : TAG_IGNORE), (ULONG) sp_chain,
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
		// DOS only takes ownership of the HomeDir lock on success.
		if (child_home != 0) UnLock(child_home);
		Close(out_file); Close(err_file); Close(nil_in);
		DeleteFile((CONST_STRPTR) temp_path);
		DeleteFile((CONST_STRPTR) temp_path_err);
		if (did_swap) { CurrentDir(old_lock); UnLock(new_lock); }
		__throw_simple_exception("CreateNewProc failed", "in Am_Lang_Process_runAndCaptureOutputInDir_0", &__result);
		goto __exit;
	}

	{
		int safety = 600;
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

	Close(err_file);

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

/* captureStdoutInDir: capture ONLY stdout, run through the DOS shell with
 * `> tempfile` redirection -- a real port of the AmigaOS recipe, which this
 * used to forward past by simply calling runAndCaptureOutputInDir.
 *
 * That forwarder inherited two faults from the hand-rolled LoadSeg +
 * CreateNewProc capture:
 *
 *   1. NO PATH SEARCH. LoadSeg is a file loader; it knows the literal name
 *      and one "C:" retry, and nothing about the shell's path list. So a
 *      toolchain command on the path -- `make`, `ppc-morphos-gcc` -- was
 *      "not found" unless spelled absolutely.
 *   2. The CreateNewProc recipe is tuned for ixemul children and mishandles
 *      others: output that never fills the child's stdio buffer can be lost
 *      entirely when the child exits abnormally, which surfaces as an empty
 *      capture rather than an error. am-ide read that as "clean".
 *
 * SystemTagList with SYS_UserShell is synchronous, resolves the command the
 * way a typed shell line does (path list included), and manages the child's
 * lifecycle the standard DOS way. stderr goes wherever the shell sends it
 * and is not captured -- the deliberate trade for a capture that works. */
function_result Am_Lang_Process_captureStdoutInDir_0(aobject * command, aobject * workingDir)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;

	string_holder *cmd_holder = (string_holder *) ((char *) command + sizeof(aobject));
	const char *cmd_str = (const char *) cmd_holder->string_value;

	string_holder *dir_holder2 = (workingDir != NULL) ? (string_holder *) ((char *) workingDir + sizeof(aobject)) : NULL;
	const char *dir_str2 = (dir_holder2 != NULL && dir_holder2->length > 0) ? dir_holder2->string_value : NULL;

	/* Swap cwd so the command runs in workingDir. Restore the previous
	   lock on the way out, and UnLock only the one created here. */
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

	/* Temp file as a RELATIVE name in the (swapped) cwd, so no dependency
	   on a T:/RAM: assign. Inside .git/ when the cwd is a repo, so a
	   `git status` run through here does not report its own temp file as
	   untracked. Named from the task address so concurrent captures in
	   different threads cannot collide. */
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

	/* "<command> >am_cap_xxxx" -- the user shell performs the redirect. */
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

	// NOTE: SYS_Input/SYS_Output are deliberately NOT passed. System() then
	// inherits the calling process's streams, which is what makes this work
	// -- handing it NIL: handles instead wedges the capture on the first
	// call, main process included (measured on the amiberry rig, both with
	// and without a matching Close). The cost of the inheritance is that
	// this function only works on a task that HAS console streams: called
	// from a TaskScheduler.IO worker it blocks forever and freezes the
	// machine, so callers must stay on the main process.
	struct TagItem cs_tags[] = {
		{ SYS_Asynch,    FALSE },
		{ SYS_UserShell, TRUE },
		{ TAG_DONE,      0 },
	};
	SystemTagList((STRPTR) cs_cmd, cs_tags);

	/* Read the relative temp file while cwd is still the working dir. */
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


// Store the search dirs for every later spawn. The chain itself is built
// fresh at each spawn site (see __build_spawn_path in morphos.c) because
// NP_Path hands ownership to the child.
function_result Am_Lang_Process_setSpawnSearchPath_0(aobject * dirs)
{
	function_result __result = { .has_return_value = true };
	string_holder *h = (string_holder *) ((char *) dirs + sizeof(aobject));
	__set_spawn_search_path((const char *) h->string_value);
	__result.return_value.value.bool_value = true;
	return __result;
}
