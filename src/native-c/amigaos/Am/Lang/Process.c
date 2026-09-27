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
#include <exec/execbase.h>
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

	string_holder *cmd_holder = (string_holder *) ((char *) command + sizeof(aobject));
	STRPTR cmd_strptr = (STRPTR) cmd_holder->string_value;

	// SYS_Input/SYS_Output omitted on purpose: SystemTagList then inherits the
	// caller's stdin/stdout, so the command's output reaches the user's shell.
	// Passing NULL would silently redirect to NIL:.
	// NP_StackSize: without it the spawned command runs on the boot shell's
	// default stack (~4 KB), which silently kills any non-trivial child —
	// an AmLang binary (aminet-cli from the IDE) dies before main. Same fix
	// the capture variants below already carry.
	// Extension drawers etc. reach the child as a real path list: the Shell
	// ignores a PATH variable entirely (measured, both scopes). Built fresh
	// per spawn because NP_Path hands ownership to the child, which frees it
	// on exit -- reusing one chain hangs the machine. TAG_IGNORE when there
	// is nothing to add, so the child keeps its default inheritance.
	// Not freed on a failed spawn: whether ownership transfers in that case
	// is unmeasured, and leaking a lock beats a double free.
	BPTR __sp_chain = __build_spawn_path();
	struct TagItem tags[] = {
		{ SYS_Asynch,    FALSE },
		{ SYS_UserShell, TRUE },
		{ NP_StackSize,  (ULONG) 262144 },
		{ __sp_chain != 0 ? NP_Path : TAG_IGNORE, (ULONG) __sp_chain },
		{ TAG_DONE,      0 },
	};

	LONG status = SystemTagList(cmd_strptr, tags);
	__result.return_value.value.int_value = (int) status;

__exit: ;
	return __result;
}

// =================================================================
// Capture completion: wait until DOS has finished cleaning up the child
// =================================================================
//
// The two capture entry points below spawn with CreateNewProcTags --
// they have to, because the V40 stderr recipe needs the Process
// pointer in order to patch pr_CES by hand. So the parent must work
// out for itself when the child is done, before it may read the temp
// files and do the asymmetric Close.
//
// That used to be a poll of FindTask("amProcessCapture") on a 15 s
// fuse, and the fuse was the bug. When it blew, the code could not
// tell "child crashed and is suspended on a requester" from "child is
// still working" -- and it then read AND DELETED the temp file out
// from under a live child. Anything slow (a TLS request from a 68k is
// the everyday case) came back silently HALF WRITTEN, surfacing many
// layers up as a parse error with nothing pointing back here.
//
// dos.library V40 does implement NP_ExitCode / NP_ExitData. Verified
// against Kickstart 3.1's dos 40.3: CreateNewProc stores them into
// pr_ExitCode / pr_ExitData at $FA6358 / $FA636C. (Unlike NP_Error,
// whose tag constant appears NOWHERE in the 512K ROM -- which is why
// pr_CES still has to be patched by hand at each spawn site.) So let
// DOS tell us its return code. The hook runs BEFORE DOS closes the child's
// streams, however, so completion requires the task to leave Exec's lists.
// An early read of a still-open output file fails with ERROR_OBJECT_IN_USE.
typedef struct am_cap_ctx {
	struct am_cap_ctx *next;
	struct Task *child_task;
	volatile LONG done;        /* child's exit hook ran               */
	volatile LONG rc;          /* its return code                     */
} am_cap_ctx;

// IMPORTANT: keep NP_ExitData/pr_ExitData ZERO for ordinary child commands.
// Although DOS permits callback data here, ixemul 48's network startup uses
// the same field for a different purpose. With the AmiTCP backend active,
// ixnet/socket.c:init_inet_daemon() treats ANY nonzero pr_ExitData as an
// inetd DaemonMessage. Passing our am_cap_ctx pointer makes it call
// ObtainSocket() with unrelated memory as the socket parameters. Failure
// calls exit(DERR_OBTAIN), defined as 0xA1 (161) in ixemul's inetd.h.
// GCC/make then exit before main(), producing NO stdout or stderr. Changing
// redirection or switching to RunCommand cannot repair that startup failure.
//
// This caused the New CLI regression diagnosed on 2026-09-27. Tests with
// ixnet's networking backend inactive passed, hiding the bug. The relevant
// regression profile is tests/amigaos-process-capture/run.sh with
// CAPTURE_PROFILE=ade-network: it exposes AmiTCP:libs/usergroup.library so
// the backend actually activates. Merely enabling socket emulation is not
// sufficient. Test both synchronous APIs and RunningProcess with GCC/make.
//
// Keep our completion context in this task-to-context registry instead.
// Register under Forbid BEFORE the child can run, and retain it until the
// child leaves Exec's task lists (the exit hook precedes DOS stream cleanup).
// Do not move it to tc_UserData: ixemul owns that field for its runtime state.
// RunningProcess.c must preserve the same rules for asynchronous children.
static am_cap_ctx *g_cap_contexts = NULL;

// All list access is protected by Forbid. Contexts remain allocated until
// the child has left Exec's task lists, including its DOS cleanup.
static void am_cap_register(am_cap_ctx *ctx, struct Task *child)
{
	ctx->child_task = child;
	ctx->next = g_cap_contexts;
	g_cap_contexts = ctx;
}

static am_cap_ctx *am_cap_find_current(void)
{
	struct Task *self = FindTask(NULL);
	Forbid();
	am_cap_ctx *ctx = g_cap_contexts;
	while (ctx != NULL && ctx->child_task != self) ctx = ctx->next;
	Permit();
	return ctx;
}

static void am_cap_release(am_cap_ctx *ctx)
{
	Forbid();
	am_cap_ctx **link = &g_cap_contexts;
	while (*link != NULL && *link != ctx) link = &(*link)->next;
	if (*link == ctx) *link = ctx->next;
	Permit();
	FreeVec(ctx);
}

static am_cap_ctx *am_cap_create(void)
{
	return (am_cap_ctx *) AllocVec(sizeof(am_cap_ctx), MEMF_PUBLIC | MEMF_CLEAR);
}

// Temporary capture diagnostics for this build. Disable with -D...=0.
// Log only the binary and numeric results, never arguments or output text.
#ifndef AM_PROCESS_CAPTURE_DIAGNOSTICS
#define AM_PROCESS_CAPTURE_DIAGNOSTICS 1
#endif

static void am_cap_log_result(const char *binary, const char *resolved, BOOL completed,
    const am_cap_ctx *ctx, LONG out_size, LONG err_size)
{
#if AM_PROCESS_CAPTURE_DIAGNOSTICS
	LONG hook_seen = ctx != NULL ? ctx->done : FALSE;
	LONG status = hook_seen ? ctx->rc : -1;
	LONG ix_version = -1, ix_revision = -1;
	Forbid();
	struct Library *ix = (struct Library *) FindName(&SysBase->LibList, "ixemul.library");
	if (ix != NULL) {
		ix_version = ix->lib_Version;
		ix_revision = ix->lib_Revision;
	}
	Permit();
	BPTR log = Open("PROGDIR:process-capture.log", MODE_READWRITE);
	if (log != 0) {
		if (Seek(log, 0, OFFSET_END) >= 0) {
			FPrintf(log, "binary=%s completed=%ld hook=%ld rc=%ld out=%ld err=%ld\n",
			    binary, (LONG) completed, hook_seen, status, out_size, err_size);
			FPrintf(log, "completion=task-registry spawnExitData=0\n");
			FPrintf(log, "resolved=%s dos=%ld.%ld exec=%ld.%ld cpuFlags=%ld ixemul=%ld.%ld\n",
			    resolved, (LONG) ((struct Library *) DOSBase)->lib_Version,
			    (LONG) ((struct Library *) DOSBase)->lib_Revision,
			    (LONG) SysBase->LibNode.lib_Version, (LONG) SysBase->LibNode.lib_Revision,
			    (LONG) SysBase->AttnFlags, ix_version, ix_revision);
		}
		Close(log);
	}
#endif
}

// NP_ExitCode hook. AmigaOS calls this in the CHILD's context as the
// process exits, D0 = return code, D1 = NP_ExitData. The register
// bindings are mandatory -- without them gcc reads the arguments off
// the stack and gets garbage (same note as rp_child_exit in
// RunningProcess.c). Deliberately minimal: no DOS calls, no logging,
// nothing that could block a process already tearing down.
static void __saveds am_cap_child_exit(
    register LONG status __asm("d0"),
    register LONG data   __asm("d1"))
{
	(void) data;
	am_cap_ctx *ctx = am_cap_find_current();
	if (ctx == NULL) return;
	ctx->rc = status;
	ctx->done = TRUE;
}

// Per-capture task label, for diagnostics only. Programs may rename their
// task, so its name must never be used to decide whether capture is done.
static void am_proc_format_cap_name(char *out, const struct Task *self)
{
	static const char hex[] = "0123456789abcdef";
	const char *pfx = "amProcCap.";
	ULONG v = (ULONG) self;
	int i = 0, k;
	while (pfx[i] != 0) { out[i] = pfx[i]; i++; }
	for (k = 28; k >= 0; k -= 4) out[i++] = hex[(v >> k) & 0xF];
	out[i] = 0;
}

// Compare task addresses under Forbid without dereferencing the saved child
// pointer: its Process structure may already have been freed. This is the
// same lifetime check used by RunningProcess before stopping its handler.
static BOOL am_cap_task_exists(const struct Task *child)
{
	BOOL found = FALSE;
	if (child == NULL) return FALSE;
	Forbid();
	if (child == SysBase->ThisTask) {
		found = TRUE;
	} else {
		struct Node *node;
		for (node = SysBase->TaskReady.lh_Head; node->ln_Succ != NULL; node = node->ln_Succ) {
			if ((const struct Task *) node == child) { found = TRUE; break; }
		}
		if (!found) {
			for (node = SysBase->TaskWait.lh_Head; node->ln_Succ != NULL; node = node->ln_Succ) {
				if ((const struct Task *) node == child) { found = TRUE; break; }
			}
		}
	}
	Permit();
	return found;
}

// The exit hook and a fixed delay cannot guarantee closed streams. Wait for
// actual removal, even if the program changed its task name while running.
static BOOL am_cap_wait_for_child(const struct Task *child, LONG max_ticks)
{
	LONG waited = 0;
	while (waited < max_ticks) {
		if (!am_cap_task_exists(child)) return TRUE;
		Delay(2);
		waited += 2;
	}
	return !am_cap_task_exists(child);
}

// Backstop for a child that never exits: 5 minutes at 20 ms a tick.
// Only ever reached by a wedged child, so the length costs nothing in
// the normal case.
#define AM_CAP_MAX_TICKS  (15000)

// Appended to the captured text when the wait gave up, so a partial
// capture can never again be mistaken for a complete one by whatever
// is parsing it.
static const char AM_CAP_INCOMPLETE_NOTE[] =
	"\n[am-lang-core] capture gave up waiting for the command; it was still "
	"running, so this output is INCOMPLETE.\n";

// Read and delete a closed temp file. Never call this for a live child:
// opening can fail while its handle is exclusive, and deletion loses output.
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
// the drawer holding it. A child created with a preloaded NP_Seglist gives
// DOS no filename to derive pr_HomeDir from, so without NP_HomeDir the child has
// NO PROGDIR: at all — and PROGDIR: is how a bundled tool finds the files
// shipped beside it (amlc's config.json / templates/package.yml, for one:
// without this it silently scaffolded the built-in cross-compiling
// package.yml instead of the packager's native one).
// Returns 0 when the resolved name has no directory part; the tag is then
// omitted and behaviour is exactly what it was before.
static char g_cap_resolved[192];

// NP_CommandName accepts at most 104 characters. Keep a short full path
// for argv[0]; for a longer path, use the filename without changing LoadSeg.
static void am_proc_command_name(const char *binary, char name[105])
{
    int i = 0;
    while (i < 104 && binary[i] != 0) i++;
    if (binary[i] != 0) binary = (const char *) FilePart((STRPTR) binary);
    i = 0;
    while (i < 104 && binary[i] != 0) {
        name[i] = binary[i];
        i++;
    }
    name[i] = 0;
}

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
			// Record the canonical path, including the volume behind an assign,
			// so diagnostics identify the actual executable that was loaded.
			char full_name[sizeof(g_cap_resolved)];
			if (NameFromLock(fl, full_name, sizeof(full_name)) != DOSFALSE) {
				am_proc_record_resolved(full_name);
			}
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
 * through run_command: MorphOS SDK `make` lives on the shell path, not in
 * C:. Same order as rp_loadseg_with_path so both entry points agree.
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

	/* 2a. Installed extension drawers (am-git, the toolchain, ...). */
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
		/* Nothing matched: this chain was never handed to a child, so we
		 * own it and must release it. */
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

	/* 3. C: -- every Workbench install has it assigned. */
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

	// V40 stderr capture: a child runs the loaded command with
	// real file handles and a manually assigned pr_CES. Only stderr is closed
	// here; DOS closes the child's inherited input and output on completion.
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

	// Keep context until the child leaves Exec's task lists, including on
	// timeout. DOS owns the command segment after a successful spawn.
	am_cap_ctx *cap_ctx = am_cap_create();
	if (cap_ctx == NULL) {
		UnLoadSeg(seg);
		if (child_home != 0) UnLock(child_home);
		Close(out_file); Close(err_file); Close(nil_in);
		DeleteFile((CONST_STRPTR) temp_path);
		DeleteFile((CONST_STRPTR) temp_path_err);
		__throw_simple_exception("Out of memory creating command context", "in Am_Lang_Process_runAndCaptureOutput_0", &__result);
		goto __exit;
	}
	BOOL cap_completed = FALSE;
	static char cap_name[32];
	am_proc_format_cap_name(cap_name, self);

	char child_command_name[105];
	am_proc_command_name(g_bin_buf2, child_command_name);
	Forbid();
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
		NP_CommandName, (ULONG) child_command_name,
		NP_Name,        (ULONG) cap_name,
		NP_StackSize,   (ULONG) 65536,
		/* Tell us when you are done instead of making us guess. Both
		   tags are real on V40; NP_Error above is not (see banner). */
		NP_ExitCode,    (ULONG) am_cap_child_exit,
		NP_ExitData,    (ULONG) 0, /* ixnet reserves nonzero data for inetd */
		(child_home != 0 ? NP_HomeDir : TAG_IGNORE), (ULONG) child_home,
		TAG_DONE);
	if (child != NULL) {
		am_cap_register(cap_ctx, (struct Task *) child);
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
			// NP_CommandName lets DOS allocate the CLI-owned BSTR.
			// Never replace it with static storage: DOS frees it on exit.
		}
	}
	Permit();

	if (child == NULL) {
		UnLoadSeg(seg);
		if (cap_ctx != NULL) FreeVec(cap_ctx);
		// DOS only takes ownership of the HomeDir lock on success.
		if (child_home != 0) UnLock(child_home);
		Close(out_file); Close(err_file); Close(nil_in);
		DeleteFile((CONST_STRPTR) temp_path);
		DeleteFile((CONST_STRPTR) temp_path_err);
		__throw_simple_exception("CreateNewProc failed", "in Am_Lang_Process_runAndCaptureOutput_0", &__result);
		goto __exit;
	}

	cap_completed = am_cap_wait_for_child((struct Task *) child, AM_CAP_MAX_TICKS);

	// Asymmetric Close: only err_file — out_file + nil_in are closed by the
	// child's CLI cleanup, and closing them here would be a use-after-free.
	// And only when the child is actually GONE: a child we gave up on still
	// has pr_CES pointing at err_file, so closing it now would leave the next
	// thing it writes to stderr going through a closed handle. Leak it
	// instead — same trade as cap_ctx.
	if (cap_completed) Close(err_file);

	UBYTE *out_buf = NULL;
	UBYTE *err_buf = NULL;
	LONG out_size = 0, err_size = 0;
	// A timed-out child still owns these files. Leave them intact and return
	// the incomplete marker; neither read nor delete a live capture.
	if (cap_completed) {
		out_size = am_proc_read_and_delete_temp(temp_path, &out_buf);
		err_size = am_proc_read_and_delete_temp(temp_path_err, &err_buf);
	}
	am_cap_log_result(g_bin_buf2, g_cap_resolved, cap_completed, cap_ctx, out_size, err_size);
	// Keep the context until diagnostics have read the exit status. A live
	// child may still write it, so the timeout path deliberately retains it.
	if (cap_completed) am_cap_release(cap_ctx);
	if (out_size < 0) out_size = 0;
	if (err_size < 0) err_size = 0;

	// Concat. stdout first (program's intended output), stderr after
	// (diagnostics). Perfect interleaving would need per-write
	// timestamps the OS doesn't give us; for a typical compile this
	// "all the output, then all the warnings" ordering is fine.
	// A capture we gave up on carries a marker, so a partial result can never
	// again be handed back looking exactly like a complete one.
	LONG note_size = cap_completed ? 0 : (LONG) (sizeof(AM_CAP_INCOMPLETE_NOTE) - 1);
	LONG total = out_size + err_size + note_size;
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
	if (note_size > 0) {
		for (LONG i = 0; i < note_size; i++)
			combined[out_size + err_size + i] = (UBYTE) AM_CAP_INCOMPLETE_NOTE[i];
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

// Shell-based stdout capture for callers that need DOS command parsing.
// On V40 System() does not propagate the parent's pr_CES. `>` redirects
// stdout only; diagnostics require the direct capture entry point below.
function_result Am_Lang_Process_captureStdoutInDir_0(aobject * command, aobject * workingDir)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;

	string_holder *cmd_holder = (string_holder *) ((char *) command + sizeof(aobject));
	const char *cmd_str = (const char *) cmd_holder->string_value;

	string_holder *dir_holder2 = (workingDir != NULL) ? (string_holder *) ((char *) workingDir + sizeof(aobject)) : NULL;
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

	// NOTE: SYS_Input/SYS_Output are deliberately NOT passed. System() then
	// inherits the calling process's streams, which is what makes this work
	// -- handing it NIL: handles instead wedges the capture on the first
	// call, main process included (measured on the amiberry rig, both with
	// and without a matching Close). The cost of the inheritance is that
	// this function only works on a task that HAS console streams: called
	// from a TaskScheduler.IO worker it blocks forever and freezes the
	// machine, so callers must stay on the main process.
	// Extension drawers etc. reach the child as a real path list: the Shell
	// ignores a PATH variable entirely (measured, both scopes). Built fresh
	// per spawn because NP_Path hands ownership to the child, which frees it
	// on exit -- reusing one chain hangs the machine. TAG_IGNORE when there
	// is nothing to add, so the child keeps its default inheritance.
	// Not freed on a failed spawn: whether ownership transfers in that case
	// is unmeasured, and leaking a lock beats a double free.
	BPTR __sp_chain = __build_spawn_path();
	struct TagItem cs_tags[] = {
		{ SYS_Asynch,    FALSE },
		{ SYS_UserShell, TRUE },
		{ __sp_chain != 0 ? NP_Path : TAG_IGNORE, (ULONG) __sp_chain },
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

	string_holder *dir_holder = (workingDir != NULL) ? (string_holder *) ((char *) workingDir + sizeof(aobject)) : NULL;
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

	// Body mirrors runAndCaptureOutput, with cwd save/restore wrapped
	// around the direct child spawn and capture.
	string_holder *cmd_holder = (string_holder *) ((char *) command + sizeof(aobject));
	STRPTR cmd_strptr = (STRPTR) cmd_holder->string_value;

	UBYTE temp_path[64];
	UBYTE temp_path_err[64];
	struct Task *self = FindTask(NULL);
	am_proc_build_temp_path(temp_path,     self, NULL);
	am_proc_build_temp_path(temp_path_err, self, "_e");

	// V40 stderr-capture recipe (verified via gcctest harness):
	//   1. Open out_file, err_file (MODE_NEWFILE) + NIL: as stdin
	//   2. LoadSeg the command binary
	//   3. CreateNewProcTags with NP_Seglist + NP_FreeSeglist,
	//      NP_Input=NIL, NP_Output=out,
	//      NP_Error=err, NP_ConsoleTask=NULL (breaks ixemul's
	//      "*" fallback that otherwise leaks stderr to the
	//      launching shell)
	//   4. Manually patch pr_CIS/pr_COS/pr_CES + CLI struct under
	//      Forbid (V40 silently drops NP_Error tag → pr_CES must
	//      be set manually; the cli_StandardInput/Output fields
	//      are needed for child's libc init to read consistent
	//      values)
	//   5. Track completion outside pr_ExitData, which ixnet uses for inetd.
	//      Wait until the child leaves Exec's task lists after DOS cleanup.
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

	// Split "binary args..." for CreateNewProc and LoadSeg.
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

	// Same search the CLI uses: verbatim, extension drawers, the Shell's
	// path list, then C:. Previously this tried only the name and "C:",
	// so the AI agent's run_command could not find commands the user
	// could run by hand in the CLI panel.
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

	// Context outlives the child, including its exit hook and DOS cleanup.
	// DOS owns the command segment after a successful spawn.
	am_cap_ctx *cap_ctx = am_cap_create();
	if (cap_ctx == NULL) {
		UnLoadSeg(seg);
		if (child_home != 0) UnLock(child_home);
		Close(out_file); Close(err_file); Close(nil_in);
		DeleteFile((CONST_STRPTR) temp_path);
		DeleteFile((CONST_STRPTR) temp_path_err);
		if (did_swap) { CurrentDir(old_lock); UnLock(new_lock); }
		__throw_simple_exception("Out of memory creating command context", "in Am_Lang_Process_runAndCaptureOutputInDir_0", &__result);
		goto __exit;
	}
	BOOL cap_completed = FALSE;
	static char cap_name[32];
	am_proc_format_cap_name(cap_name, self);

	// Spawn child under Forbid + manual CLI/process field patches.
	char child_command_name[105];
	am_proc_command_name(g_bin_buf, child_command_name);
	Forbid();
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
		NP_CommandName, (ULONG) child_command_name,
		NP_Name,        (ULONG) cap_name,
		NP_StackSize,   (ULONG) 65536,
		/* Tell us when you are done instead of making us guess. Both
		   tags are real on V40; NP_Error above is not (see banner). */
		NP_ExitCode,    (ULONG) am_cap_child_exit,
		NP_ExitData,    (ULONG) 0, /* ixnet reserves nonzero data for inetd */
		(child_home != 0 ? NP_HomeDir : TAG_IGNORE), (ULONG) child_home,
		TAG_DONE);
	if (child != NULL) {
		am_cap_register(cap_ctx, (struct Task *) child);
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
			// NP_CommandName lets DOS allocate the CLI-owned BSTR.
			// Never replace it with static storage: DOS frees it on exit.
		}
	}
	Permit();

	if (child == NULL) {
		UnLoadSeg(seg);
		if (cap_ctx != NULL) FreeVec(cap_ctx);
		// DOS only takes ownership of the HomeDir lock on success.
		if (child_home != 0) UnLock(child_home);
		Close(out_file); Close(err_file); Close(nil_in);
		DeleteFile((CONST_STRPTR) temp_path);
		DeleteFile((CONST_STRPTR) temp_path_err);
		if (did_swap) { CurrentDir(old_lock); UnLock(new_lock); }
		__throw_simple_exception("CreateNewProc failed", "in Am_Lang_Process_runAndCaptureOutputInDir_0", &__result);
		goto __exit;
	}

	// The exit hook runs before stream cleanup; wait for the actual task to
	// disappear before touching the inherited handles or reading their files.
	cap_completed = am_cap_wait_for_child((struct Task *) child, AM_CAP_MAX_TICKS);

	// Asymmetric Close — see banner above. Only when the child is actually
	// GONE, though: one we gave up on still has pr_CES pointing at err_file,
	// and closing it here would leave its next stderr write going through a
	// closed handle. Leak it instead — same trade as cap_ctx.
	if (cap_completed) Close(err_file);
	// out_file + nil_in: DON'T touch — CLI cleanup closed them, our
	// Close would be use-after-free → handler-port hang.

	UBYTE *out_buf = NULL;
	UBYTE *err_buf = NULL;
	LONG out_size = 0, err_size = 0;
	// Do not read or delete files still owned by a child that timed out.
	if (cap_completed) {
		out_size = am_proc_read_and_delete_temp(temp_path, &out_buf);
		err_size = am_proc_read_and_delete_temp(temp_path_err, &err_buf);
	}
	am_cap_log_result(g_bin_buf, g_cap_resolved, cap_completed, cap_ctx, out_size, err_size);
	// Keep the context until diagnostics have read the exit status. A live
	// child may still write it, so the timeout path deliberately retains it.
	if (cap_completed) am_cap_release(cap_ctx);
	if (out_size < 0) out_size = 0;
	if (err_size < 0) err_size = 0;

	if (did_swap) {
		CurrentDir(old_lock);
		UnLock(new_lock);
	}

	// A capture we gave up on carries a marker, so a partial result can never
	// again be handed back looking exactly like a complete one.
	LONG note_size = cap_completed ? 0 : (LONG) (sizeof(AM_CAP_INCOMPLETE_NOTE) - 1);
	LONG total = out_size + err_size + note_size;
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
	if (note_size > 0) {
		for (LONG i = 0; i < note_size; i++)
			combined[out_size + err_size + i] = (UBYTE) AM_CAP_INCOMPLETE_NOTE[i];
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


// Store the search dirs for every later spawn. The chain itself is built
// fresh at each spawn site (see __build_spawn_path in amiga.c) because
// NP_Path hands ownership to the child.
function_result Am_Lang_Process_setSpawnSearchPath_0(aobject * dirs)
{
	function_result __result = { .has_return_value = true };
	string_holder *h = (string_holder *) ((char *) dirs + sizeof(aobject));
	__set_spawn_search_path((const char *) h->string_value);
	__result.return_value.value.bool_value = true;
	return __result;
}
