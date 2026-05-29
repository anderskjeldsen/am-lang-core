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
	if (this != NULL) {
		__increase_reference_count(this);
	}

	SysBase = *((struct ExecBase **)4UL);
	// dos.library is opened by amiga-gcc's C runtime before main()
	// runs (printf above would crash without it), so the previous
	// defensive __ensure_library("dos.library") here was dead code.
	Am_Threading_Thread_data *data = malloc(sizeof(Am_Threading_Thread_data));
	this->object_properties.class_object_properties.object_data.value.custom_value = data;
	data->stack_size = 4000 * 1024; // TODO
	data->done = false;

__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	return __result;
};

function_result Am_Threading_Thread__native_release_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;

    Am_Threading_Thread_data *data = (Am_Threading_Thread_data *) this->object_properties.class_object_properties.object_data.value.custom_value;
	free(data);
	this->object_properties.class_object_properties.object_data.value.custom_value = NULL;

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

		aobject * thread = (aobject *) own_task->tc_UserData;

		while ( thread == NULL )
		{
			Am_Threading_Thread_sleep_0(100);
			thread = (aobject *) own_task->tc_UserData;
		}

		if ( thread != NULL )
		{
			aobject * runnable = thread->object_properties.class_object_properties.properties[0].nullable_value.value.object_value;
			// Runnable is an interface, so dispatch through the iface_implementation
			// the wrapper carries (functions[0] = run). Going via runnable->class_ptr
			// instead would pick up the Am.Lang.Runnable aclass's `functions` field,
			// which is left NULL for interfaces — silent crash on the worker task.
			Am_Lang_Runnable_f_run_0_T rFunc = (Am_Lang_Runnable_f_run_0_T) runnable->object_properties.iface_reference.iface_implementation->functions[0];
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
			Am_Threading_Thread_f_runFinalizers_0(thread);
			printf("[_InitTask] runFinalizers returned\n");
			fflush(stdout);

			Am_Threading_Thread_data *data = (Am_Threading_Thread_data *) thread->object_properties.class_object_properties.object_data.value.custom_value;
			data->done = true;
			printf("[_InitTask] flagged done; dropping worker ref\n");
			fflush(stdout);

			// Drop the worker's reference on `thread` last — every
			// access above this line happens while `thread` is still
			// alive courtesy of the ref taken in `start_0`. Also
			// closes the busy-wait race above where `tc_UserData` is
			// set only AFTER the new task is already running.
			__decrease_reference_count(thread);
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
	if (this != NULL) {
		__increase_reference_count(this);
	}

	void (*fptr)() = Am_Threading_Thread__InitTask;

	aobject * name = this->object_properties.class_object_properties.properties[1].nullable_value.value.object_value;
	string_holder *name_holder = name->object_properties.class_object_properties.object_data.value.custom_value;
	STRPTR name_strptr = "";
	if ( name_holder->string_value != NULL ) {
		name_strptr = name_holder->string_value;
	}

	Am_Threading_Thread_data *data = (Am_Threading_Thread_data *) this->object_properties.class_object_properties.object_data.value.custom_value;

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

	// Take an extra ref for the worker; _InitTask drops it at the end.
	// Doing this BEFORE CreateNewProc closes the existing race where
	// the new task starts running and busy-waits on `tc_UserData`
	// while the caller could otherwise drop their last ref to `this`.
	__increase_reference_count(this);

	struct Process * process = CreateNewProc(tags);

//	printf("CreateNewProc Done %d\n", process);

	if ( process == NULL )
	{
		// Worker won't run, so undo the ref we took above.
		__decrease_reference_count(this);
		printf("CreateNewProc returned a null-pointer\n");
// TODO:		throw( new GException("CreateNewProc returned a null-pointer") );
	}
	else
	{
		process->pr_Task.tc_UserData = (void *) this;
	}

__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
	return __result;
};

function_result Am_Threading_Thread_join_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
	if (this != NULL) {
		__increase_reference_count(this);
	}

	printf("Start Joining...\n");
	Am_Threading_Thread_data *data = (Am_Threading_Thread_data *) this->object_properties.class_object_properties.object_data.value.custom_value;

	while( !data->done )
	{
		printf("Done %d\n", data->done);
		Am_Threading_Thread_sleep_0(200);
	}

__exit: ;
	if (this != NULL) {
		__decrease_reference_count(this);
	}
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
