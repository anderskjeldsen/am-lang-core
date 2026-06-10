// Async child-process wrapper for libc. Forks a child, execs the
// command via /bin/sh -c, and wires stdin/stdout through pipe()
// pairs. Mirror of the AmigaOS PIPE: implementation; same public
// behaviour (non-blocking tryReadOutput, line-by-line writeInput).
//
// Used for desktop testing of the IDE (macOS / Linux); the IDE
// itself runs on AmigaOS/MorphOS in practice.

#include <libc/core.h>
#include <Am/Lang/RunningProcess.h>
#include <libc/Am/Lang/RunningProcess.h>
#include <Am/Lang/ClassRef.h>
#include <Am/Lang/String.h>
#include <Am/Lang/Object.h>
#include <Am/Lang/Bool.h>
#include <libc/core_inline_functions.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

typedef struct _running_process_data running_process_data;
struct _running_process_data {
    int  stdin_writer_fd;   // parent writes -> child stdin
    int  stdout_reader_fd;  // parent reads  <- child stdout
    pid_t child_pid;
    int  child_exited;      // 0 until waitpid reports the child gone
};

static running_process_data * rp_data(aobject * const this) {
    if (this == NULL) return NULL;
    return (running_process_data *) this->object_properties.class_object_properties.object_data.value.custom_value;
}

function_result Am_Lang_RunningProcess__native_init_0(aobject * const this) {
    function_result __result = { .has_return_value = false };
    if (this != NULL) __increase_reference_count(this);
    running_process_data * d = calloc(1, sizeof(running_process_data));
    if (d != NULL) {
        d->stdin_writer_fd  = -1;
        d->stdout_reader_fd = -1;
        d->child_pid = -1;
        d->child_exited = 0;
        this->object_properties.class_object_properties.object_data.value.custom_value = d;
    }
    if (this != NULL) __decrease_reference_count(this);
    return __result;
}

function_result Am_Lang_RunningProcess__native_mark_children_0(aobject * const this) {
    function_result __result = { .has_return_value = false };
    (void) this;
    return __result;
}

static void rp_close_internal(running_process_data * d) {
    if (d == NULL) return;
    if (d->stdin_writer_fd >= 0) {
        close(d->stdin_writer_fd);
        d->stdin_writer_fd = -1;
    }
    if (d->stdout_reader_fd >= 0) {
        close(d->stdout_reader_fd);
        d->stdout_reader_fd = -1;
    }
    if (d->child_pid > 0 && !d->child_exited) {
        // Best-effort cleanup. The child may already have exited;
        // kill() is silent if it has.
        kill(d->child_pid, SIGTERM);
        int status = 0;
        // Reap without blocking. If the child is still running it
        // becomes a zombie until the OS cleans up at process exit
        // — acceptable for the IDE's typical lifecycle.
        waitpid(d->child_pid, &status, WNOHANG);
        d->child_exited = 1;
    }
}

function_result Am_Lang_RunningProcess__native_release_0(aobject * const this) {
    function_result __result = { .has_return_value = false };
    running_process_data * d = rp_data(this);
    if (d != NULL) {
        rp_close_internal(d);
        free(d);
        this->object_properties.class_object_properties.object_data.value.custom_value = NULL;
    }
    return __result;
}

function_result Am_Lang_RunningProcess_startNative_0(aobject * const this, aobject * command, aobject * workingDir) {
    function_result __result = { .has_return_value = false };
    if (this != NULL) __increase_reference_count(this);
    if (command != NULL) __increase_reference_count(command);
    if (workingDir != NULL) __increase_reference_count(workingDir);

    running_process_data * d = rp_data(this);
    if (d == NULL) {
        __throw_simple_exception("RunningProcess: object_data missing", "in Am_Lang_RunningProcess_startNative_0", &__result);
        goto __exit;
    }
    string_holder * cmd_holder = (string_holder *) (command + 1);
    const char * cmd_str = cmd_holder->string_value;

    int stdin_pipe[2];   // [0]=read end (child), [1]=write end (parent)
    int stdout_pipe[2];  // [0]=read end (parent), [1]=write end (child)
    if (pipe(stdin_pipe) != 0) {
        __throw_simple_exception("RunningProcess: pipe(stdin) failed", "in Am_Lang_RunningProcess_startNative_0", &__result);
        goto __exit;
    }
    if (pipe(stdout_pipe) != 0) {
        close(stdin_pipe[0]); close(stdin_pipe[1]);
        __throw_simple_exception("RunningProcess: pipe(stdout) failed", "in Am_Lang_RunningProcess_startNative_0", &__result);
        goto __exit;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(stdin_pipe[0]); close(stdin_pipe[1]);
        close(stdout_pipe[0]); close(stdout_pipe[1]);
        __throw_simple_exception("RunningProcess: fork failed", "in Am_Lang_RunningProcess_startNative_0", &__result);
        goto __exit;
    }
    if (pid == 0) {
        // Child. Wire stdin/stdout to our pipe ends, close the
        // parent ends, then exec via /bin/sh -c so shell features
        // (quoting, &&, etc.) work the same as runAndCaptureOutput.
        dup2(stdin_pipe[0], STDIN_FILENO);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stdout_pipe[1], STDERR_FILENO);
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        if (workingDir != NULL) {
            string_holder * wd_holder = (string_holder *) (workingDir + 1);
            const char * wd_str = wd_holder->string_value;
            if (wd_str != NULL && wd_str[0] != 0) {
                if (chdir(wd_str) != 0) {
                    // Best-effort; keep going so the child still
                    // runs, just from the wrong cwd.
                }
            }
        }
        execl("/bin/sh", "sh", "-c", cmd_str, (char *) NULL);
        // execl returns only on failure.
        _exit(127);
    }

    // Parent. Close the child ends; keep the parent ends.
    close(stdin_pipe[0]);
    close(stdout_pipe[1]);

    // Make the parent's stdout reader non-blocking so tryReadOutput
    // returns immediately when there's nothing buffered.
    int flags = fcntl(stdout_pipe[0], F_GETFL, 0);
    fcntl(stdout_pipe[0], F_SETFL, flags | O_NONBLOCK);

    d->stdin_writer_fd  = stdin_pipe[1];
    d->stdout_reader_fd = stdout_pipe[0];
    d->child_pid = pid;
    d->child_exited = 0;

__exit: ;
    if (this != NULL) __decrease_reference_count(this);
    if (command != NULL) __decrease_reference_count(command);
    if (workingDir != NULL) __decrease_reference_count(workingDir);
    return __result;
}

function_result Am_Lang_RunningProcess_tryReadOutput_0(aobject * const this) {
    function_result __result = { .has_return_value = true };
    if (this != NULL) __increase_reference_count(this);
    __result.return_value.value.object_value = NULL;

    running_process_data * d = rp_data(this);
    if (d == NULL || d->stdout_reader_fd < 0) {
        __result.return_value.value.object_value = __create_string("", &Am_Lang_String);
        goto __exit;
    }
    char buf[1024];
    ssize_t n = read(d->stdout_reader_fd, buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = 0;
        __result.return_value.value.object_value = __create_string(buf, &Am_Lang_String);
        goto __exit;
    }
    if (n == 0) {
        // EOF — child closed stdout.
        close(d->stdout_reader_fd);
        d->stdout_reader_fd = -1;
        __result.return_value.value.object_value = __create_string("", &Am_Lang_String);
        goto __exit;
    }
    // n < 0. EAGAIN/EWOULDBLOCK = "no data right now"; other errno =
    // real error, treat as EOF so the caller stops polling.
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        __result.return_value.value.object_value = __create_string("", &Am_Lang_String);
        goto __exit;
    }
    close(d->stdout_reader_fd);
    d->stdout_reader_fd = -1;
    __result.return_value.value.object_value = __create_string("", &Am_Lang_String);

__exit: ;
    if (this != NULL) __decrease_reference_count(this);
    return __result;
}

function_result Am_Lang_RunningProcess_writeInput_0(aobject * const this, aobject * text) {
    function_result __result = { .has_return_value = false };
    if (this != NULL) __increase_reference_count(this);
    if (text != NULL) __increase_reference_count(text);

    running_process_data * d = rp_data(this);
    if (d == NULL || d->stdin_writer_fd < 0 || text == NULL) {
        goto __exit;
    }
    string_holder * h = (string_holder *) (text + 1);
    if (h == NULL || h->string_value == NULL) {
        goto __exit;
    }
    size_t len = strlen(h->string_value);
    if (len > 0) {
        ssize_t w = write(d->stdin_writer_fd, h->string_value, len);
        (void) w;  // best-effort
    }

__exit: ;
    if (this != NULL) __decrease_reference_count(this);
    if (text != NULL) __decrease_reference_count(text);
    return __result;
}

function_result Am_Lang_RunningProcess_isAlive_0(aobject * const this) {
    function_result __result = { .has_return_value = true };
    if (this != NULL) __increase_reference_count(this);
    running_process_data * d = rp_data(this);
    int alive = 0;
    if (d != NULL && d->child_pid > 0 && !d->child_exited) {
        int status = 0;
        pid_t r = waitpid(d->child_pid, &status, WNOHANG);
        if (r == 0) {
            alive = 1;  // still running
        } else if (r == d->child_pid) {
            d->child_exited = 1;
        }
    }
    // Treat "child gone but output still pending" as alive too so
    // callers drain the buffer cleanly.
    if (!alive && d != NULL && d->stdout_reader_fd >= 0) {
        alive = 1;
    }
    __result.return_value.value.bool_value = alive ? true : false;
    if (this != NULL) __decrease_reference_count(this);
    return __result;
}

function_result Am_Lang_RunningProcess_close_0(aobject * const this) {
    function_result __result = { .has_return_value = false };
    if (this != NULL) __increase_reference_count(this);
    rp_close_internal(rp_data(this));
    if (this != NULL) __decrease_reference_count(this);
    return __result;
}

// Stubs so the libc target links — the AmLang side declares
// these globally and #runOnExit injects a call. libc child
// processes are reaped by the OS on parent exit, so no
// sweep is needed here.
function_result Am_Lang_RunningProcess_setGlobalWake_0(long long var_taskPtr, int var_sigBit) {
    function_result __result = { .has_return_value = false };
    return __result;
}

function_result Am_Lang_RunningProcess_shutdownAllNative_0(void) {
    function_result __result = { .has_return_value = false };
    return __result;
}

// Raw-mode + reported-size hooks exist for AmigaOS's custom-handler
// pty (see native-c/amigaos/Am/Lang/RunningProcess.c). The libc child
// runs against the host kernel's real PTY, so there's nothing for the
// AmLang side to query — return non-raw and accept the size silently.
function_result Am_Lang_RunningProcess_isRawMode_0(aobject * const this) {
    function_result __result = { .has_return_value = true };
    __result.return_value = (nullable_value){ .flags = PRIMITIVE_BOOL, .value.bool_value = false };
    return __result;
}

function_result Am_Lang_RunningProcess_setReportedSize_0(aobject * const this, int var_rows, int var_cols) {
    function_result __result = { .has_return_value = false };
    (void)var_rows;
    (void)var_cols;
    return __result;
}
