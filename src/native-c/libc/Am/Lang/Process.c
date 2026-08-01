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
	return __result;
}

function_result Am_Lang_Process_runAndCaptureOutput_0(aobject * command)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;

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

// On libc there's no ixemul/CreateNewProc distinction — the popen-based
// InDir capture already works, so just delegate to it.
function_result Am_Lang_Process_captureStdoutInDir_0(aobject * command, aobject * workingDir)
{
	return Am_Lang_Process_runAndCaptureOutputInDir_0(command, workingDir);
}

function_result Am_Lang_Process_runAndCaptureOutputInDir_0(aobject * command, aobject * workingDir)
{
	function_result __result = { .has_return_value = true };
	bool __returning = false;

	string_holder *cmd_holder = (string_holder *) (command + 1);
	string_holder *dir_holder = (workingDir != NULL) ? (string_holder *) (workingDir + 1) : NULL;
	const char *dir_str = (dir_holder != NULL && dir_holder->length > 0) ? dir_holder->string_value : NULL;

	// The working dir is applied INSIDE the spawned shell — `cd '<dir>'
	// && <cmd>` — not via chdir() in this process. chdir is process-
	// wide, so calling this from a worker thread (the git sidebar runs
	// `git status` on the TaskScheduler IO thread) would briefly flip
	// the cwd under every other thread. Single-quote the dir and escape
	// embedded quotes ('\'' dance) so arbitrary paths survive /bin/sh.
	char *full_cmd = NULL;
	const char *sh_cmd = cmd_holder->string_value;
	if (dir_str != NULL) {
		size_t dlen = strlen(dir_str);
		size_t clen = strlen(sh_cmd);
		// worst case: every dir byte is ' -> 4 bytes, plus wrapping.
		full_cmd = (char *) malloc(dlen * 4 + clen + 16);
		if (full_cmd == NULL) {
			__throw_simple_exception("Out of memory", "in Am_Lang_Process_runAndCaptureOutputInDir_0", &__result);
			goto __exit;
		}
		char *p = full_cmd;
		*p++ = 'c'; *p++ = 'd'; *p++ = ' '; *p++ = '\'';
		for (size_t i = 0; i < dlen; i++) {
			if (dir_str[i] == '\'') {
				*p++ = '\''; *p++ = '\\'; *p++ = '\''; *p++ = '\'';
			} else {
				*p++ = dir_str[i];
			}
		}
		*p++ = '\''; *p++ = ' '; *p++ = '&'; *p++ = '&'; *p++ = ' ';
		memcpy(p, sh_cmd, clen + 1);
		sh_cmd = full_cmd;
	}

	FILE *pipe = popen(sh_cmd, "r");
	if (!pipe) {
		if (full_cmd != NULL) free(full_cmd);
		__throw_simple_exception("Failed to execute command", "in Am_Lang_Process_runAndCaptureOutputInDir_0", &__result);
		goto __exit;
	}

	{
		size_t capacity = 4096;
		size_t size = 0;
		char *buffer = (char *) malloc(capacity);
		if (!buffer) {
			pclose(pipe);
			if (full_cmd != NULL) free(full_cmd);
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
					if (full_cmd != NULL) free(full_cmd);
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

		if (full_cmd != NULL) free(full_cmd);

		aobject *str = __create_string(buffer, &Am_Lang_String);
		free(buffer);
		__result.return_value.value.object_value = str;
	}

__exit: ;
	return __result;
}
