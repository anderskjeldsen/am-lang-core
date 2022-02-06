#ifndef native_amigaos_aclass_Am_Threading_Thread_c
#define native_amigaos_aclass_Am_Threading_Thread_c
#include <core.h>
#include <Am/Threading/Thread.h>
#include <Am/Lang/Object.h>
#include <Am/Lang/Runnable.h>
#include <Am/Lang/Long.h>
#include <Am/Lang/String.h>

function_result Am_Threading_Thread__native_init_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bool __returning = false;
	if (this != NULL) {
		__increase_reference_count(this);
	}

	SysBase = *((struct ExecBase **)4UL);
	printf("Dos base: %d\n", (unsigned int) DOSBase);
	if (DOSBase == NULL) {
		DOSBase = (struct DosLibrary *) __ensure_library("dos.library", 0L);
	}

	printf("TODO: implement native function Am_Threading_Thread__native_init_0\n");
	Am_Threading_Thread_data *data = malloc(sizeof(Am_Threading_Thread_data));
	this->object_data.value.custom_value = data;
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

    Am_Threading_Thread_data *data = (Am_Threading_Thread_data *) this->object_data.value.custom_value;
	free(data);
	this->object_data.value.custom_value = NULL;

__exit: ;
	return __result;
};

void Am_Threading_Thread__InitTask()
{
//	printf("InitTask...\n");
	struct Task *own_task = NULL;
	struct Process *own_process = NULL;
	
	own_task = FindTask(NULL);
	if ( own_task != NULL )
	{
//		printf("Task found\n");

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
			Am_Threading_Thread_sleep_0(1000);
			thread = (aobject *) own_task->tc_UserData;
		}

		if ( thread != NULL )
		{
//			printf("Thread found\n");
			aobject * runnable = thread->properties[0].nullable_value.value.object_value;
			Am_Threading_Runnable_run_0_T rFunc = (Am_Threading_Runnable_run_0_T) runnable->class_ptr->functions[3]; // TODO: Create index constants
			rFunc(runnable);
			Am_Threading_Thread_data *data = (Am_Threading_Thread_data *) thread->object_data.value.custom_value;
			data->done = true;
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

	aobject * name = this->properties[1].nullable_value.value.object_value;
	string_holder *name_holder = name->object_data.value.custom_value;
	STRPTR name_strptr = "";
	if ( name_holder->string_value != NULL ) {
		name_strptr = name_holder->string_value;
	}

	Am_Threading_Thread_data *data = (Am_Threading_Thread_data *) this->object_data.value.custom_value;

//	printf("stack_size: %d\n", data->stack_size);
//	printf("thread name: %s\n", name_holder->string_value);

	struct TagItem tags[] = {
		NP_Entry, (ULONG) fptr,
		NP_StackSize, data->stack_size,
		NP_Name, (ULONG) name_strptr,
		TAG_DONE, TAG_DONE,
	};


//	printf("CreateNewProc\n");

	struct Process * process = CreateNewProc(tags);
		
//	printf("CreateNewProc Done %d\n", process);

	if ( process == NULL )
	{
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
	Am_Threading_Thread_data *data = (Am_Threading_Thread_data *) this->object_data.value.custom_value;

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
#endif
