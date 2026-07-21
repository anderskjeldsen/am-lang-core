#include <libc/core.h>
#include <Am/Threading/Thread.h>
#include <Am/Lang/Object.h>
#include <Am/Lang/Runnable.h>
#include <Am/Lang/Long.h>
#include <Am/Lang/String.h>
#include <libc/core_inline_functions.h>

#include <amigaos/amiga.h>


#include <exec/types.h>
#include <dos/dostags.h>
#include <dos/dos.h>
#include <exec/exec.h>
#include <exec/tasks.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <utility/tagitem.h>

typedef struct _Am_Threading_Thread_data Am_Threading_Thread_data;
struct _Am_Threading_Thread_data {
	ULONG stack_size;
	bool done;
};

function_result Am_Threading_Thread__native_init_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;

	SysBase = *((struct ExecBase **)4UL);
	// dos.library is opened by amiga-gcc's C runtime before main()
	// runs (printf above would crash without it), so the previous
	// defensive __ensure_library("dos.library") here was dead code.
	Am_Threading_Thread_data *data = malloc(sizeof(Am_Threading_Thread_data));
	// Unwrap for the data write — if `this` was passed in as a
	// cross-thread wrapper the store would land on the wrapper's own
	// union variant, not the real object's object_data slot, and any
	// subsequent read (or reader on another task) would see NULL.
	__unwrap(this)->object_properties.class_object_properties.object_data.value.custom_value = data;
	data->stack_size = 4000 * 1024; // TODO
	data->done = false;

__exit: ;
	return __result;
};

function_result Am_Threading_Thread__native_release_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;

	aobject * const real = __unwrap(this);
	Am_Threading_Thread_data *data = (Am_Threading_Thread_data *) real->object_properties.class_object_properties.object_data.value.custom_value;
	free(data);
	real->object_properties.class_object_properties.object_data.value.custom_value = NULL;

__exit: ;
	return __result;
};

function_result Am_Threading_Thread__native_mark_children_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
__exit: ;
	return __result;
};

void Am_Threading_Thread__InitTask()
{
	struct Task *own_task = NULL;
	struct Process *own_process = NULL;
	
	own_task = FindTask(NULL);
	if ( own_task != NULL )
	{
		own_process = (struct Process *) own_task;
//		OwnTask->tc_Switch = NULL; // SwitchEvent;
//		OwnTask->tc_Launch = NULL; // LaunchEvent;
//		UBYTE   Flags = OwnTask->tc_Flags;
//		UBYTE	OldFlags = Flags;
//		Flags = Flags | TF_SWITCH | TF_LAUNCH;
//		OwnTask->tc_Flags = Flags;

		// `tc_UserData` was set by `start_0` with the wrapper pointer it
		// received (so refcount ops on both sides balance). Every data
		// read below must unwrap first — a cross-thread wrapper's
		// properties[] and iface_reference union variants overlap with
		// unrelated wrapper metadata; reading through the raw pointer
		// yields garbage that silently no-ops `run()`.
		aobject * thread_ref = (aobject *) own_task->tc_UserData;

		while ( thread_ref == NULL )
		{
			Am_Threading_Thread_sleep_0(100);
			thread_ref = (aobject *) own_task->tc_UserData;
		}

		if ( thread_ref != NULL )
		{
			aobject * thread = __unwrap(thread_ref);
			aobject * runnable_ref = thread->object_properties.class_object_properties.properties[0].nullable_value.value.object_value;
			aobject * runnable = __unwrap(runnable_ref);
			// Runnable is an interface, so dispatch through the iface_implementation
			// the wrapper carries. run() is at functions[3], NOT [0]: the interface
			// function table starts with the three inherited AnyInterface methods
			// (indices 0..2), so index 0 dispatches an AnyInterface method instead —
			// which SILENTLY NO-OPS the thread body (run() never executes; every
			// cross-thread write the worker was supposed to make just never happens).
			// This is exactly how libc (Thread.c:142) and morphos-ppc dispatch it; the
			// amigaos mirror was stuck on the pre-AnyInterface layout and was caught
			// by the first real multitasking test run under Amiberry (BRC stress
			// suite: worker sums came back 0). Going via runnable->class_ptr instead
			// would pick up the Am.Lang.Runnable aclass's `functions` field, which is
			// left NULL for interfaces — silent crash on the worker task.
			Am_Lang_Runnable_f_run_0_T rFunc = (Am_Lang_Runnable_f_run_0_T) runnable->object_properties.iface_reference.iface_implementation->functions[3];
			rFunc(runnable->object_properties.iface_reference.implementation_object);

			printf("[_InitTask] rFunc returned; about to runFinalizers (task=%p, thread=%p)\n",
			       (void *) own_task, (void *) thread);
			fflush(stdout);

			// Run user-registered finalizers on this task, in reverse
			// order, before flipping `done`. The typical caller is the
			// per-task bsdsocket / amissl cleanup that needs to call
			// CloseLibrary from inside the same task that called
			// OpenLibrary — see Am.Net.Socket's nativeInit pattern.
			// Calling on this task (not the joiner) means finalizers
			// still see FindTask(NULL) == own_task.
			// Pass the wrapper pointer we were handed — the AmLang-side
			// callee will unwrap where needed and matches how any other
			// dispatch on this Thread instance from this task looks.
			Am_Threading_Thread_f_runFinalizers_0(thread_ref);
			printf("[_InitTask] runFinalizers returned\n");
			fflush(stdout);

			Am_Threading_Thread_data *data = (Am_Threading_Thread_data *) thread->object_properties.class_object_properties.object_data.value.custom_value;
			data->done = true;
			printf("[_InitTask] flagged done; dropping worker ref\n");
			fflush(stdout);

			// Drop the worker's FOREIGN reference on the Thread object.
			// The worker is a non-owner of thread_ref, so this
			// __decrease_reference_count routes to the atomic foreign
			// counter — balancing the foreign bump start_0 took on the
			// worker's behalf. Every access above this line runs while
			// that ref (and thus the object) is still alive.
			__decrease_reference_count(thread_ref);
			printf("[_InitTask] worker exiting cleanly\n");
			fflush(stdout);
		}
		else
		{
			printf("Thread not found\n");
		}
	}
	else
	{
		printf("Process not found\n");
	}
}

function_result Am_Threading_Thread_start_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;

	void (*fptr)() = Am_Threading_Thread__InitTask;

	// Unwrap for every data read on the Thread instance. If the caller
	// invoked start() through a cross-thread wrapper handle, direct
	// dereferences would read the wrapper's union variant instead of
	// the real object's properties[] / object_data.
	aobject * const this_r = __unwrap(this);
	aobject * name_ref = this_r->object_properties.class_object_properties.properties[1].nullable_value.value.object_value;
	aobject * name = __unwrap(name_ref);
	string_holder *name_holder = name->object_properties.class_object_properties.object_data.value.custom_value;
	STRPTR name_strptr = "";
	if ( name_holder->string_value != NULL ) {
		name_strptr = name_holder->string_value;
	}

	Am_Threading_Thread_data *data = (Am_Threading_Thread_data *) this_r->object_properties.class_object_properties.object_data.value.custom_value;

//	printf("stack_size: %d\n", data->stack_size);
//	printf("thread name: %s\n", name_holder->string_value);

	// Inherit the calling process's CLI streams so println in the
	// worker shows up alongside main's println (notably under
	// `Startup-Sequence: app >output.log` headless setups). Without
	// NP_Output/NP_Input the new process gets NULL handles and any
	// printf is silently dropped — which masks runtime errors emitted
	// from worker tasks. NP_CloseOutput/NP_CloseInput=FALSE keeps the
	// parent owning the handles so a worker exit doesn't close the
	// CLI's stdout from under main.
	struct Process *parent_proc = (struct Process *) FindTask(NULL);
	BPTR parent_out = parent_proc->pr_COS;
	BPTR parent_in = parent_proc->pr_CIS;
	BPTR parent_err = parent_proc->pr_CES;

	struct TagItem tags[] = {
		NP_Entry, (ULONG) fptr,
		NP_StackSize, data->stack_size,
		NP_Name, (ULONG) name_strptr,
		NP_Output, (ULONG) parent_out,
		NP_Input, (ULONG) parent_in,
		NP_Error, (ULONG) parent_err,
		NP_CloseOutput, FALSE,
		NP_CloseInput, FALSE,
		NP_CloseError, FALSE,
		TAG_DONE, TAG_DONE,
	};


//	printf("CreateNewProc\n");

	// Thread-safe ARC (BRC): from here on the process is multi-threaded, so
	// the refcount helpers must classify owner-vs-foreign. One-way flag; set
	// before CreateNewProc so the worker's first refcount op is classified
	// correctly. See am-lang-compiler-code/docs/BIASED_REFCOUNT_DESIGN.md.
	__amlc_multithreaded = true;

	// Take a FOREIGN ref for the worker task on `this`; _InitTask drops it at
	// the end. The worker is NOT the owner of the Thread object (the creating
	// task is), so the ref lives on the atomic foreign counter — the worker's
	// exit __decrease_reference_count (a foreign release) routes to the same
	// counter. On Amiga the foreign op is an inline Forbid/Permit-bracketed
	// bump (see core.h). Doing this BEFORE CreateNewProc closes the race where
	// the new task starts and busy-waits on `tc_UserData` while the caller
	// could otherwise drop its last ref to `this`.
	__amlc_atomic_fetch_add(&this_r->foreign_reference_count, 1);

	struct Process * process = CreateNewProc(tags);

//	printf("CreateNewProc Done %d\n", process);

	if ( process == NULL )
	{
		// Worker won't run, so undo the foreign ref we took above.
		__amlc_atomic_fetch_sub(&this_r->foreign_reference_count, 1);
		printf("CreateNewProc returned a null-pointer\n");
// TODO:		throw( new GException("CreateNewProc returned a null-pointer") );
	}
	else
	{
		process->pr_Task.tc_UserData = (void *) this;
	}

__exit: ;
	return __result;
};

function_result Am_Threading_Thread_join_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;

	printf("Start Joining...\n");
	Am_Threading_Thread_data *data = (Am_Threading_Thread_data *) __unwrap(this)->object_properties.class_object_properties.object_data.value.custom_value;

	while( !data->done )
	{
		printf("Done %d\n", data->done);
		Am_Threading_Thread_sleep_0(200);
	}

__exit: ;
	return __result;
};

function_result Am_Threading_Thread_getCurrent_0()
{
	function_result __result = { .has_return_value = false };

	bool __returning = false;
	struct Task *own_task = FindTask(NULL);
	if ( own_task != NULL )
	{
		__result.return_value.value.object_value = (aobject *) own_task->tc_UserData;
	}
	else
	{
		printf("Unable to get own task");
//		throw( GException("Could not get own task!") );
	}



__exit: ;
	return __result;
};

function_result Am_Threading_Thread_sleep_0(long long milliseconds)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;

	int ticks = (int) (milliseconds / 20);
//	printf("Sleep %d ticks\n", ticks);
	Delay(ticks); // Ticks (50 per second)

__exit: ;
	return __result;
};
