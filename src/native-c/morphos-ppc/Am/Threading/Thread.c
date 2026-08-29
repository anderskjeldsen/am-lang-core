#include <libc/core.h>
#include <Am/Threading/Thread.h>
#include <Am/Lang/Object.h>
#include <Am/Lang/Runnable.h>
#include <Am/Lang/Long.h>
#include <Am/Lang/String.h>
#include <libc/core_inline_functions.h>

#include <morphos-ppc/morphos.h>


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

	// printf("[native_init] enter (this=%p)\n", (void *) this); fflush(stdout);
	SysBase = *((struct ExecBase **)4UL);
	// dos.library is opened by amiga-gcc's C runtime before main()
	// runs (printf above would crash without it), so the previous
	// defensive __ensure_library("dos.library") here was dead code.
	Am_Threading_Thread_data *data = malloc(sizeof(Am_Threading_Thread_data));
	// Unwrap for the write — a cross-thread wrapper's object_data union
	// variant is unrelated to the real object's; storing through the
	// wrapper would leave the real's custom_value at NULL, and every
	// subsequent read (start/join/native_release) would see NULL.
	__unwrap(this)->object_properties.class_object_properties.object_data.value.custom_value = data;
	data->stack_size = 4000 * 1024; // TODO
	data->done = false;
	// printf("[native_init] done (this=%p, data=%p, stack=%lu)\n",
	//        (void *) this, (void *) data, (unsigned long) data->stack_size); fflush(stdout);

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
	// Very first thing: prove the worker process actually started.
	// If this print never lands, CreateNewProc handed control to
	// something else (or trance never bridged to our entry).
//	printf("[_InitTask] entry (FindTask=%p)\n", (void *) FindTask(NULL)); fflush(stdout);

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

		// `tc_UserData` holds the wrapper pointer that `start_0` stashed
		// (matches the ref we took there). All DATA reads on the
		// Thread/Runnable below must unwrap — cross-thread wrapper
		// aobjects overlay their union variants, so raw derefs would
		// read garbage and silently no-op run().
		aobject * thread_ref = (aobject *) own_task->tc_UserData;
//		printf("[_InitTask] initial tc_UserData=%p\n", (void *) thread_ref); fflush(stdout);

		while ( thread_ref == NULL )
		{
			Am_Threading_Thread_sleep_0(100);
			thread_ref = (aobject *) own_task->tc_UserData;
		}
//		printf("[_InitTask] tc_UserData resolved (thread=%p)\n", (void *) thread_ref); fflush(stdout);

		if ( thread_ref != NULL )
		{
			aobject * thread = __unwrap(thread_ref);
			aobject * runnable_ref = thread->object_properties.class_object_properties.properties[0].nullable_value.value.object_value;
			aobject * runnable = __unwrap(runnable_ref);
//			printf("[_InitTask] runnable=%p iface_impl=%p impl_obj=%p\n",
//			       (void *) runnable,
//			       (void *) (runnable ? runnable->object_properties.iface_reference.iface_implementation : NULL),
//			       (void *) (runnable ? runnable->object_properties.iface_reference.implementation_object : NULL));
//			fflush(stdout);

			// Runnable is an interface, so dispatch through the iface_implementation
			// the wrapper carries. run() is at functions[3], NOT [0]: the interface's
			// inherited Am.Lang.AnyInterface methods take slots 0..2 so run() lands at
			// 3 (matches the libc Thread native — see reference_amlang_thread_runnable_dispatch).
			// Calling [0] ran an AnyInterface method that returned right away, so the
			// worker's run() loop never executed ("worker exiting cleanly"). Going via
			// runnable->class_ptr
			// instead would pick up the Am.Lang.Runnable aclass's `functions` field,
			// which is left NULL for interfaces — silent crash on the worker task.
			Am_Lang_Runnable_f_run_0_T rFunc = (Am_Lang_Runnable_f_run_0_T) runnable->object_properties.iface_reference.iface_implementation->functions[3];
//			printf("[_InitTask] rFunc=%p (functions[3]); calling run()\n", (void *) rFunc); fflush(stdout);
			rFunc(runnable->object_properties.iface_reference.implementation_object);

//			printf("[_InitTask] rFunc returned; about to runFinalizers (task=%p, thread=%p)\n",
//			       (void *) own_task, (void *) thread);
//			fflush(stdout);

			// Run user-registered finalizers on this task, in reverse
			// order, before flipping `done`. The typical caller is the
			// per-task bsdsocket / amissl cleanup that needs to call
			// CloseLibrary from inside the same task that called
			// OpenLibrary — see Am.Net.Socket's nativeInit pattern.
			// Calling on this task (not the joiner) means finalizers
			// still see FindTask(NULL) == own_task. Pass the wrapper —
			// AmLang-side callee unwraps itself.
#ifdef Am_Threading_Thread_f_runFinalizers_0__DIRECT_NOTHROW_ABI
			Am_Threading_Thread_f_runFinalizers_0__direct(thread_ref);
#else
			Am_Threading_Thread_f_runFinalizers_0(thread_ref);
#endif
//			printf("[_InitTask] runFinalizers returned\n");
//			fflush(stdout);

			Am_Threading_Thread_data *data = (Am_Threading_Thread_data *) thread->object_properties.class_object_properties.object_data.value.custom_value;
			data->done = true;
//			printf("[_InitTask] flagged done; dropping worker ref\n");
//			fflush(stdout);

			// Drop the worker's reference on the wrapper we were handed
			// (matches the `__increase_reference_count(this)` implicitly
			// held via `start_0`). Every access above this line runs
			// while the wrapper — and thus the real — is alive.
			__decrease_reference_count(thread_ref);
//			printf("[_InitTask] worker exiting cleanly\n");
//			fflush(stdout);
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

	// printf("[start] enter (this=%p)\n", (void *) this); fflush(stdout);
	void (*fptr)() = Am_Threading_Thread__InitTask;

	// Unwrap for every DATA read on the Thread instance. `this` may be
	// a cross-thread wrapper handle if the caller obtained it from a
	// shared collection / another thread — direct derefs would read the
	// wrapper's union variant, not the real object's properties[] slot.
	aobject * const this_r = __unwrap(this);
	aobject * name_ref = this_r->object_properties.class_object_properties.properties[1].nullable_value.value.object_value;
	aobject * name = __unwrap(name_ref);
	string_holder *name_holder = name->object_properties.class_object_properties.object_data.value.custom_value;
	STRPTR name_strptr = "";
	if ( name_holder->string_value != NULL ) {
		name_strptr = name_holder->string_value;
	}
	// printf("[start] fptr=%p name=\"%s\"\n", (void *) fptr, name_strptr); fflush(stdout);

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

	// NP_CodeType, CODETYPE_PPC is required on MorphOS: dos.library
	// is 68k, so CreateNewProc defaults to a 68k entry point. Without
	// this tag our PPC `_InitTask` pointer is interpreted as a 68k
	// instruction address — the process is spawned, tc_UserData is
	// set, but the entry never executes (silent: the new process
	// just sits or dies on its first emulated step). Tag is a no-op
	// on AmigaOS m68k builds since it's never reached there.
	struct TagItem tags[] = {
		NP_Entry, (ULONG) fptr,
		NP_CodeType, CODETYPE_PPC,
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

	// Take an extra ref for the worker; _InitTask drops it at the end.
	// Doing this BEFORE CreateNewProc closes the existing race where
	// the new task starts running and busy-waits on `tc_UserData`
	// while the caller could otherwise drop their last ref to `this`.

	// printf("[start] calling CreateNewProc\n"); fflush(stdout);
	struct Process * process = CreateNewProc(tags);
	// printf("[start] CreateNewProc returned (process=%p)\n", (void *) process); fflush(stdout);

	if ( process == NULL )
	{
		// Worker won't run, so undo the ref we took above.
		printf("CreateNewProc returned a null-pointer\n");
// TODO:		throw( new GException("CreateNewProc returned a null-pointer") );
	}
	else
	{
		process->pr_Task.tc_UserData = (void *) this;
		// printf("[start] set tc_UserData=%p on task=%p\n",
		//        (void *) this, (void *) &process->pr_Task); fflush(stdout);
	}

__exit: ;
	return __result;
};

function_result Am_Threading_Thread_join_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;

	// printf("[join] start (thread=%p)\n", (void *) this); fflush(stdout);
	Am_Threading_Thread_data *data = (Am_Threading_Thread_data *) __unwrap(this)->object_properties.class_object_properties.object_data.value.custom_value;

	while( !data->done )
	{
		Am_Threading_Thread_sleep_0(200);
	}
	// printf("[join] done (thread=%p)\n", (void *) this); fflush(stdout);

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

	// Delay() is 50 Hz (1 tick = 20ms). A plain integer divide FLOORED any sub-20ms
	// sleep to 0 ticks, and Delay(0) returns immediately — so Thread.sleep(2) (the
	// chunk-loader worker's idle back-off) did NOT sleep, and the idle workers spun
	// hot, saturating the single CPU and starving the main thread (frozen UI, CPU
	// pegged a few seconds into play once the workers ran out of chunks). CEIL instead
	// so any positive request waits at least one tick; 0ms stays a no-op (yield).
	int ticks = (int) ((milliseconds + 19) / 20);
	Delay(ticks); // Ticks (50 per second)

__exit: ;
	return __result;
};
