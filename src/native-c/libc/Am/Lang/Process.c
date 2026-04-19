#include <libc/core.h>
#include <Am/Lang/Process.h>
#include <libc/Am/Lang/Process.h>
#include <Am/Lang/ClassRef.h>
#include <Am/Lang/Int.h>
#include <Am/Lang/String.h>
#include <Am/Lang/Object.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <libc/core_inline_functions.h>

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
	int status = system(cmd_holder->string_value);

	if (status == -1) {
		__result.return_value.value.int_value = -1;
	} else {
#ifdef WEXITSTATUS
		__result.return_value.value.int_value = WEXITSTATUS(status);
#else
		__result.return_value.value.int_value = status;
#endif
	}

__exit: ;
	if (command != NULL) {
		__decrease_reference_count(command);
	}
	return __result;
}

function_result Am_Lang_Process_runAndCaptureOutput_0(aobject * command)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	if (command != NULL) {
		__increase_reference_count(command);
	}

	string_holder *cmd_holder = (string_holder *) (command + 1);

	FILE *pipe = popen(cmd_holder->string_value, "r");
	if (!pipe) {
		__throw_simple_exception("Failed to execute command", "in Am_Lang_Process_runAndCaptureOutput_0", &__result);
		goto __exit;
	}

	{
		size_t capacity = 4096;
		size_t size = 0;
		char *buffer = (char *) malloc(capacity);
		if (!buffer) {
			pclose(pipe);
			__throw_simple_exception("Out of memory", "in Am_Lang_Process_runAndCaptureOutput_0", &__result);
			goto __exit;
		}

		char tmp[1024];
		while (fgets(tmp, sizeof(tmp), pipe)) {
			size_t len = strlen(tmp);
			if (size + len + 1 > capacity) {
				capacity = capacity * 2 + len;
				char *new_buf = (char *) realloc(buffer, capacity);
				if (!new_buf) {
					free(buffer);
					pclose(pipe);
					__throw_simple_exception("Out of memory", "in Am_Lang_Process_runAndCaptureOutput_0", &__result);
					goto __exit;
				}
				buffer = new_buf;
			}
			memcpy(buffer + size, tmp, len);
			size += len;
		}
		buffer[size] = 0;
		pclose(pipe);

		aobject *str = __create_string(buffer, &Am_Lang_String);
		free(buffer);
		__result.return_value.value.object_value = str;
	}

__exit: ;
	if (command != NULL) {
		__decrease_reference_count(command);
	}
	return __result;
}
