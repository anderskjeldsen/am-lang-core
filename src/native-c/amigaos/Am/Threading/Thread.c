#include <libc/core.h>
#include <Am/Threading/Thread.h>
#include <Am/Lang/Object.h>
#include <Am/Lang/Runnable.h>
#include <Am/Lang/Long.h>
#include <Am/Lang/String.h>
#include <Am/Lang/Exception.h>
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
#include <string.h>

// AmigaOS m68k backend of Am.Threading.Thread under the ISOLATE model (see
// core.h and the libc twin, ../../../libc/Am/Threading/Thread.c, which this
// mirrors). A Thread aobject is a HANDLE on an aisolate; the isolate owns the
// Exec process, the mailbox (an Exec signal, see core.c) and the thread's
// private heap and statics. The worker process finds its isolate in its own
// tc_UserData, which the creator sets right after CreateNewProc — and which
// is also what __amlc_isolate() reads on this platform.

typedef struct {
	struct Process * process;
	bool launched;
} thread_native;

static aisolate * handle_isolate(aobject * const this)
{
	return (aisolate *) this->object_properties.class_object_properties.object_data.value.custom_value;
}

static void bind_handle(aobject * const this, aisolate * const iso)
{
	this->object_properties.class_object_properties.object_data.value.custom_value = iso;
}

static aobject * make_handle(aisolate * const iso)
{
	aobject * h = __allocate_object(&Am_Threading_Thread);
	if (h == NULL) return NULL;
	function_result r = Am_Threading_Thread___init_instance((nullable_value) { .flags = 0, .value.object_value = h });
	if (r.exception != NULL) __decrease_reference_count(r.exception);
	__amlc_isolate_retain(iso);
	bind_handle(h, iso);
	if (iso->name != NULL) {
		aobject * name = __create_string(iso->name, &Am_Lang_String);
		if (name != NULL) {
			__set_property(h, Am_Threading_Thread_P_name, (nullable_value) { .flags = 0, .value.object_value = name });
			__decrease_reference_count(name);
		}
	}
	return h;
}

function_result Am_Threading_Thread__native_init_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	bind_handle(this, NULL);
	return __result;
};

function_result Am_Threading_Thread__native_release_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	aisolate * iso = handle_isolate(this);
	if (iso != NULL) {
		bind_handle(this, NULL);
		__amlc_isolate_release(iso);
	}
	return __result;
};

function_result Am_Threading_Thread__native_mark_children_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	(void) this;
	return __result;
};

aobject * Am_Threading_Thread__native_copy_0(aobject * const src, void * const copy_ctx)
{
	aobject * dst = __allocate_object(src->class_ptr);
	if (dst == NULL) return NULL;
	function_result r = Am_Threading_Thread___init_instance((nullable_value) { .flags = 0, .value.object_value = dst });
	if (r.exception != NULL) __decrease_reference_count(r.exception);
	aobject * name = src->object_properties.class_object_properties.properties[Am_Threading_Thread_P_name].nullable_value.value.object_value;
	if (name != NULL) {
		aobject * c = __amlc_copy_child(copy_ctx, name);
		if (c == NULL) {
			__decrease_reference_count(dst);
			return NULL;
		}
		__set_property(dst, Am_Threading_Thread_P_name, (nullable_value) { .flags = 0, .value.object_value = c });
		__decrease_reference_count(c);
	}
	aisolate * iso = handle_isolate(src);
	if (iso != NULL) {
		__amlc_isolate_retain(iso);
		bind_handle(dst, iso);
	}
	return dst;
}

// Worker process entry. The creator stores the aisolate in our tc_UserData
// right after CreateNewProc returns; we may be scheduled before that, so
// poll one tick at a time until it is there.
void Am_Threading_Thread__InitTask()
{
	struct Task *own_task = FindTask(NULL);
	if (own_task == NULL) {
		printf("Thread: FindTask failed\n");
		return;
	}
	while (own_task->tc_UserData == NULL) {
		Delay(1);
	}
	aisolate * const iso = (aisolate *) own_task->tc_UserData;

	__amlc_isolate_enter(iso);
	__amlc_isolate_run_static_inits();

	amessage * boot = iso->boot;
	iso->boot = NULL;
	aobject * runnable = boot != NULL ? __amlc_message_open(boot) : NULL;
	if (runnable != NULL) {
		// Runnable is an interface: run() is at functions[3], after the three
		// inherited AnyInterface methods (see the libc twin).
		Am_Lang_Runnable_f_run_0_T rFunc = (Am_Lang_Runnable_f_run_0_T) runnable->object_properties.iface_reference.iface_implementation->functions[3];
		function_result r = rFunc(runnable->object_properties.iface_reference.implementation_object);
		if (r.exception != NULL) {
			Am_Lang_Exception_f_printWithStackTrace_0(r.exception);
			__decrease_reference_count(r.exception);
		}
		__decrease_reference_count(runnable);
	}

	// Finalizers registered on this task's own handle — the per-task
	// bsdsocket / amissl CloseLibrary case — run on this task.
	if (iso->self_handle != NULL) {
#ifdef Am_Threading_Thread_f_runFinalizers_0__DIRECT_NOTHROW_ABI
		Am_Threading_Thread_f_runFinalizers_0__direct(iso->self_handle);
#else
		function_result fr = Am_Threading_Thread_f_runFinalizers_0(iso->self_handle);
		if (fr.exception != NULL) __decrease_reference_count(fr.exception);
#endif
	}

	__amlc_isolate_teardown(iso);   // sets iso->done; join() polls it
	__amlc_isolate_release(iso);    // the running task's own handle
	__amlc_isolate_count_dec();
}

function_result Am_Threading_Thread_start_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };

	if (handle_isolate(this) != NULL) {
		__throw_simple_exception("Thread.start called on a thread that was already started", "in Am_Threading_Thread_start_0", &__result);
		return __result;
	}

	aisolate * iso = __amlc_isolate_create();
	bind_handle(this, iso);

	STRPTR name_strptr = "thread";
	aobject * name = this->object_properties.class_object_properties.properties[Am_Threading_Thread_P_name].nullable_value.value.object_value;
	if (name != NULL) {
		string_holder *nh = (string_holder *) name->object_properties.class_object_properties.object_data.value.custom_value;
		if (nh != NULL && nh->string_value != NULL) {
			iso->name = strdup(nh->string_value);
			name_strptr = iso->name;
		}
	}

	// Thread.priority (slot 3, declared last in Thread.aml). Exec priorities
	// are a signed byte.
	int priority = this->object_properties.class_object_properties.properties[Am_Threading_Thread_P_priority].nullable_value.value.int_value;
	if (priority < -128) priority = -128;
	if (priority > 127) priority = 127;
	iso->priority = priority;

	// The runnable travels to the worker as a message: a deep copy, or an
	// exception here if it references something that cannot be copied.
	aobject * runnable = this->object_properties.class_object_properties.properties[Am_Threading_Thread_P_runnable].nullable_value.value.object_value;
	iso->boot = __amlc_message_create(runnable, &__result);
	if (iso->boot == NULL) {
		return __result;
	}

	// Inherit the calling process's CLI streams so println in the worker
	// shows up alongside main's (notably under `app >output.log` headless
	// setups). NP_Close*=FALSE keeps the parent owning the handles.
	struct Process *parent_proc = (struct Process *) FindTask(NULL);
	BPTR parent_out = parent_proc->pr_COS;
	BPTR parent_in = parent_proc->pr_CIS;
	BPTR parent_err = parent_proc->pr_CES;

	void (*fptr)() = Am_Threading_Thread__InitTask;
	struct TagItem tags[] = {
		NP_Entry, (ULONG) fptr,
		NP_StackSize, iso->stack_size,
		NP_Name, (ULONG) name_strptr,
		NP_Priority, (ULONG) priority,
		NP_Output, (ULONG) parent_out,
		NP_Input, (ULONG) parent_in,
		NP_Error, (ULONG) parent_err,
		NP_CloseOutput, FALSE,
		NP_CloseInput, FALSE,
		NP_CloseError, FALSE,
		TAG_DONE, TAG_DONE,
	};

	thread_native * n = (thread_native *) calloc(1, sizeof(thread_native));
	iso->native = n;
	__amlc_isolate_retain(iso);   // the running task's own handle; dropped at the end of _InitTask
	__amlc_isolate_count_inc();

	struct Process * process = CreateNewProc(tags);
	if (process == NULL) {
		__amlc_isolate_count_dec();
		__amlc_isolate_release(iso);
		__throw_simple_exception("Thread.start: CreateNewProc failed", "in Am_Threading_Thread_start_0", &__result);
		return __result;
	}
	n->process = process;
	n->launched = true;
	// What the worker (and __amlc_isolate() on it) reads: set once, never
	// changed. The worker polls until it is non-NULL.
	process->pr_Task.tc_UserData = (void *) iso;
	return __result;
};

function_result Am_Threading_Thread_join_0(aobject * const this)
{
	function_result __result = { .has_return_value = false };
	aisolate * iso = handle_isolate(this);
	if (iso == NULL || iso->native == NULL) {
		return __result;
	}
	// One Exec tick (20 ms) per poll: a real Wait(), so a same-priority
	// worker gets the CPU. `done` is set by the worker's teardown.
	while (!iso->done) {
		Delay(1);
	}
	return __result;
};

function_result Am_Threading_Thread_send_0(aobject * const this, aobject * message)
{
	function_result __result = { .has_return_value = false };
	aisolate * iso = handle_isolate(this);
	if (iso == NULL) {
		__throw_simple_exception("Thread.send: the thread has not been started", "in Am_Threading_Thread_send_0", &__result);
		return __result;
	}
	__amlc_send(iso, message, &__result);
	return __result;
}

function_result Am_Threading_Thread_receive_0()
{
	function_result __result = { .has_return_value = true };
	aobject * m = __amlc_receive(true);
	if (m == NULL) {
		__throw_simple_exception("Thread.receive: the mailbox is closed", "in Am_Threading_Thread_receive_0", &__result);
		return __result;
	}
	__result.return_value.value.object_value = m;
	return __result;
}

function_result Am_Threading_Thread_tryReceive_0()
{
	function_result __result = { .has_return_value = true };
	__result.return_value.value.object_value = __amlc_receive(false);
	return __result;
}

function_result Am_Threading_Thread_getCurrent_0()
{
	function_result __result = { .has_return_value = true };
	aisolate * iso = __amlc_isolate();
	if (iso->self_handle == NULL) {
		iso->self_handle = make_handle(iso);
	}
	if (iso->self_handle != NULL) {
		__increase_reference_count(iso->self_handle);
	}
	__result.return_value.value.object_value = iso->self_handle;
	return __result;
};

function_result Am_Threading_Thread_getParent_0()
{
	function_result __result = { .has_return_value = true };
	aisolate * parent = __amlc_isolate()->parent;
	if (parent == NULL) {
		__throw_simple_exception("Thread.getParent: the main thread has no parent", "in Am_Threading_Thread_getParent_0", &__result);
		return __result;
	}
	__result.return_value.value.object_value = make_handle(parent);
	return __result;
}

function_result Am_Threading_Thread_sleep_0(long long milliseconds)
{
	function_result __result = { .has_return_value = false };

	// Delay() is 50 Hz (1 tick = 20ms). A plain integer divide FLOORED any
	// sub-20ms sleep to 0 ticks, and Delay(0) returns immediately -- so a
	// loop like TaskScheduler.stopAll's 5ms poll never yielded the CPU.
	// Round UP: any positive sleep gives at least one tick, a real Wait().
	int ticks = (int) ((milliseconds + 19) / 20);
	if (milliseconds > 0 && ticks < 1) {
		ticks = 1;
	}
	Delay(ticks); // Ticks (50 per second)
	return __result;
};

function_result Am_Threading_Thread_setCurrentPriority_0(int priority)
{
	function_result __result = { .has_return_value = false };
	if (priority < -128) priority = -128;
	if (priority > 127) priority = 127;
	SetTaskPri(FindTask(NULL), (LONG) priority);
	return __result;
}
