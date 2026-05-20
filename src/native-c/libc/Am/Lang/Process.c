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
#include <unistd.h>
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

function_result Am_Lang_Process_canonicalPath_0(aobject * path)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;
	if (path != NULL) {
		__increase_reference_count(path);
	}

	// realpath collapses symlinks and "..", returning the canonical
	// absolute form. On failure (path missing, permission denied)
	// just hand back the original so the caller still has something
	// to work with.
	string_holder *in_holder = (string_holder *) (path + 1);
	const char *in_str = in_holder->string_value;

	char resolved[4096];
	if (realpath(in_str, resolved) == NULL) {
		__result.return_value.value.object_value = path;
		__increase_reference_count(path);
		goto __exit;
	}

	aobject *out_str = __create_string(resolved, &Am_Lang_String);
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

	// PATH_MAX-ish buffer — every libc we target accepts at least
	// 4096. Empty string on failure (permissions, deleted dir) so
	// the caller has a sentinel to test instead of an NPE.
	char buffer[4096];
	if (getcwd(buffer, sizeof(buffer)) == NULL) {
		__result.return_value.value.object_value = __create_string("", &Am_Lang_String);
	} else {
		__result.return_value.value.object_value = __create_string(buffer, &Am_Lang_String);
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

	string_holder *cmd_holder = (string_holder *) (command + 1);
	string_holder *dir_holder = (workingDir != NULL) ? (string_holder *) (workingDir + 1) : NULL;
	const char *dir_str = (dir_holder != NULL && dir_holder->length > 0) ? dir_holder->string_value : NULL;

	// Snapshot the cwd so we can restore it on every exit path
	// (success or error). Skip the whole save/chdir cycle when no
	// dir was given so the call stays equivalent to
	// runAndCaptureOutput and callers can use this method
	// unconditionally.
	char saved_cwd[4096];
	bool did_chdir = false;
	if (dir_str != NULL) {
		if (getcwd(saved_cwd, sizeof(saved_cwd)) == NULL) {
			__throw_simple_exception("getcwd failed", "in Am_Lang_Process_runAndCaptureOutputInDir_0", &__result);
			goto __exit;
		}
		if (chdir(dir_str) != 0) {
			__throw_simple_exception("Failed to chdir to working directory", "in Am_Lang_Process_runAndCaptureOutputInDir_0", &__result);
			goto __exit;
		}
		did_chdir = true;
	}

	FILE *pipe = popen(cmd_holder->string_value, "r");
	if (!pipe) {
		if (did_chdir) {
			(void) chdir(saved_cwd);
		}
		__throw_simple_exception("Failed to execute command", "in Am_Lang_Process_runAndCaptureOutputInDir_0", &__result);
		goto __exit;
	}

	{
		size_t capacity = 4096;
		size_t size = 0;
		char *buffer = (char *) malloc(capacity);
		if (!buffer) {
			pclose(pipe);
			if (did_chdir) {
				(void) chdir(saved_cwd);
			}
			__throw_simple_exception("Out of memory", "in Am_Lang_Process_runAndCaptureOutputInDir_0", &__result);
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
					if (did_chdir) {
						(void) chdir(saved_cwd);
					}
					__throw_simple_exception("Out of memory", "in Am_Lang_Process_runAndCaptureOutputInDir_0", &__result);
					goto __exit;
				}
				buffer = new_buf;
			}
			memcpy(buffer + size, tmp, len);
			size += len;
		}
		buffer[size] = 0;
		pclose(pipe);

		if (did_chdir) {
			(void) chdir(saved_cwd);
		}

		aobject *str = __create_string(buffer, &Am_Lang_String);
		free(buffer);
		__result.return_value.value.object_value = str;
	}

__exit: ;
	if (command != NULL) {
		__decrease_reference_count(command);
	}
	if (workingDir != NULL) {
		__decrease_reference_count(workingDir);
	}
	return __result;
}
