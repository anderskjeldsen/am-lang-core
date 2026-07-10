// Async child-process wrapper for libc. Uses forkpty() to give the
// child a real pseudo-terminal so interactive programs (ssh, vim,
// nano, etc.) see a tty on stdin/stdout/stderr — without this the
// SSH client refuses to allocate a remote pty and prints
// "Pseudo-terminal will not be allocated because stdin is not a
// terminal" before sending what amounts to a non-interactive
// session. Same public behaviour as the previous pipe-based
// implementation (non-blocking tryReadOutput, line-by-line
// writeInput); the read and write halves now share the pty master
// fd, since the kernel pty's master is bidirectional.
//
// Used for desktop testing of the IDE (macOS / Linux); the IDE
// itself runs on AmigaOS/MorphOS in practice (RunningProcess.c
// there speaks to a custom DOS handler instead).

#include <libc/core.h>
#include <Am/Lang/RunningProcess.h>
#include <libc/Am/Lang/RunningProcess.h>
#include <Am/Lang/ClassRef.h>
#include <Am/Lang/String.h>
#include <Am/Lang/Object.h>
#include <Am/Lang/Bool.h>
#include <Am/Lang/UByte.h>
#include <Am/Lang/Array.h>
#include <libc/core_inline_functions.h>

#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <termios.h>
#include <poll.h>
#if defined(__APPLE__)
  #include <util.h>                // forkpty on macOS
#else
  #include <pty.h>                 // forkpty on Linux (link with -lutil)
#endif

typedef struct _running_process_data running_process_data;
struct _running_process_data {
    int  stdin_writer_fd;   // parent writes -> child stdin
    int  stdout_reader_fd;  // parent reads  <- child stdout
    pid_t child_pid;
    int  child_exited;      // 0 until waitpid reports the child gone
    int  exit_code;         // unix exit code, post-waitpid (0 default)
    int  binary;            // 1 = raw socketpair backend (Process.startBinary)
};

static running_process_data * rp_data(aobject * const this) {
    if (this == NULL) return NULL;
    return (running_process_data *) this->object_properties.class_object_properties.object_data.value.custom_value;
}

function_result Am_Lang_RunningProcess__native_init_0(aobject * const this) {
    function_result __result = { .has_return_value = false };
    running_process_data * d = calloc(1, sizeof(running_process_data));
    if (d != NULL) {
        d->stdin_writer_fd  = -1;
        d->stdout_reader_fd = -1;
        d->child_pid = -1;
        d->child_exited = 0;
        d->exit_code = 0;
        this->object_properties.class_object_properties.object_data.value.custom_value = d;
    }
    return __result;
}

function_result Am_Lang_RunningProcess__native_mark_children_0(aobject * const this) {
    function_result __result = { .has_return_value = false };
    (void) this;
    return __result;
}

static void rp_close_internal(running_process_data * d) {
    if (d == NULL) return;
    // With the pty backend stdin_writer_fd and stdout_reader_fd
    // hold the same master fd. Close it once, null both slots.
    int writer = d->stdin_writer_fd;
    int reader = d->stdout_reader_fd;
    if (writer >= 0) {
        close(writer);
        d->stdin_writer_fd = -1;
        if (reader == writer) {
            d->stdout_reader_fd = -1;
            reader = -1;
        }
    }
    if (reader >= 0) {
        close(reader);
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

function_result Am_Lang_RunningProcess_enableBinaryMode_0(aobject * const this) {
    function_result __result = { .has_return_value = false };
    running_process_data * d = rp_data(this);
    if (d != NULL) d->binary = 1;
    return __result;
}

function_result Am_Lang_RunningProcess_startNative_0(aobject * const this, aobject * command, aobject * workingDir) {
    function_result __result = { .has_return_value = false };

    running_process_data * d = rp_data(this);
    if (d == NULL) {
        __throw_simple_exception("RunningProcess: object_data missing", "in Am_Lang_RunningProcess_startNative_0", &__result);
        goto __exit;
    }
    string_holder * cmd_holder = (string_holder *) (command + 1);
    const char * cmd_str = cmd_holder->string_value;

    // Binary mode: give the child a raw socketpair (no pty, no termios
    // cooking) so a binary protocol survives byte-for-byte. Same shell
    // (/bin/sh -c) so command strings behave like the pty path.
    if (d->binary) {
        int sv[2];
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
            __throw_simple_exception("RunningProcess: socketpair failed", "in Am_Lang_RunningProcess_startNative_0", &__result);
            goto __exit;
        }
        pid_t bpid = fork();
        if (bpid < 0) {
            close(sv[0]); close(sv[1]);
            __throw_simple_exception("RunningProcess: fork failed", "in Am_Lang_RunningProcess_startNative_0", &__result);
            goto __exit;
        }
        if (bpid == 0) {
            dup2(sv[1], 0);
            dup2(sv[1], 1);
            if (sv[0] > 2) close(sv[0]);
            if (sv[1] > 2) close(sv[1]);
            if (workingDir != NULL) {
                string_holder * wd = (string_holder *) (workingDir + 1);
                if (wd != NULL && wd->string_value != NULL && wd->string_value[0] != 0) {
                    if (chdir(wd->string_value) != 0) { /* best-effort */ }
                }
            }
            execl("/bin/sh", "sh", "-c", cmd_str, (char *) NULL);
            _exit(127);
        }
        close(sv[1]);
        signal(SIGPIPE, SIG_IGN);
        int bfl = fcntl(sv[0], F_GETFL, 0);
        fcntl(sv[0], F_SETFL, bfl | O_NONBLOCK);
        d->stdin_writer_fd  = sv[0];
        d->stdout_reader_fd = sv[0];
        d->child_pid = bpid;
        d->child_exited = 0;
        d->exit_code = 0;
        goto __exit;
    }

    // Hand the child a PTY in cooked+echo defaults. That's
    // deliberately NOT raw mode: ssh's pty-req protocol message
    // copies our local termios state to the remote PTY, so if we
    // start in raw (cfmakeraw) the remote shell ends up with ECHO
    // off and the user never sees the bytes they type echoed back
    // over the connection — even though Enter still reaches the
    // remote and runs commands. Interactive children (ssh, nano,
    // htop, bash) all do their own tcsetattr to switch our local
    // PTY into raw mode the moment they start, saving the cooked
    // state for restore on exit. Until then ssh has already
    // forwarded the cooked+echo modes to the remote and the remote
    // shell echoes typing normally.
    //
    // Pass NULL termios to forkpty so we get the kernel defaults
    // (the values openpty would seed in `struct termios` after a
    // freshly-opened ptmx). On macOS that's: ICANON, ECHO, ICRNL,
    // OPOST, ONLCR, ISIG, all on — exactly what a brand-new
    // controlling tty looks like.
    struct winsize slave_win;
    memset(&slave_win, 0, sizeof(slave_win));
    slave_win.ws_row = 24;
    slave_win.ws_col = 80;

    int master_fd = -1;
    pid_t pid = forkpty(&master_fd, NULL, NULL, &slave_win);
    if (pid < 0) {
        __throw_simple_exception("RunningProcess: forkpty failed", "in Am_Lang_RunningProcess_startNative_0", &__result);
        goto __exit;
    }
    if (pid == 0) {
        // Child. forkpty has already wired the slave pty onto stdin /
        // stdout / stderr and made it the controlling terminal. All
        // that's left is chdir (best-effort) and exec via /bin/sh -c
        // so shell features (quoting, &&, etc.) work the same as
        // runAndCaptureOutput.
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
        // Hint the child about its terminal type. Many curses-based
        // programs (nano, htop) fall over with a bare "dumb" TERM;
        // xterm-256color is the closest match to what amStudio's
        // grid actually understands.
        setenv("TERM", "xterm-256color", 1);
        execl("/bin/sh", "sh", "-c", cmd_str, (char *) NULL);
        // execl returns only on failure.
        _exit(127);
    }

    // Parent. Make the pty master non-blocking so tryReadOutput
    // returns immediately when no bytes are buffered.
    int flags = fcntl(master_fd, F_GETFL, 0);
    fcntl(master_fd, F_SETFL, flags | O_NONBLOCK);

    // The master pty fd is bidirectional — reads pick up the
    // child's stdout/stderr stream, writes are delivered to the
    // child's stdin. Park the same fd in both slots so the
    // tryReadOutput / writeInput paths above keep working without
    // structural changes.
    d->stdin_writer_fd  = master_fd;
    d->stdout_reader_fd = master_fd;
    d->child_pid = pid;
    d->child_exited = 0;
    d->exit_code = 0;

__exit: ;
    return __result;
}

function_result Am_Lang_RunningProcess_tryReadOutput_0(aobject * const this) {
    function_result __result = { .has_return_value = true };
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
        fprintf(stderr, "[rp.read] fd=%d n=%zd byte0=0x%02x\n",
            d->stdout_reader_fd, n, (unsigned) (unsigned char) buf[0]);
        fflush(stderr);
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
    return __result;
}

function_result Am_Lang_RunningProcess_writeInput_0(aobject * const this, aobject * text) {
    function_result __result = { .has_return_value = false };

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
        fprintf(stderr, "[rp.writeInput] fd=%d len=%zu wrote=%zd byte0=0x%02x\n",
            d->stdin_writer_fd, len, w,
            (unsigned) (unsigned char) h->string_value[0]);
        fflush(stderr);
        (void) w;  // best-effort
    }

__exit: ;
    return __result;
}

function_result Am_Lang_RunningProcess_tryReadOutputBytes_0(aobject * const this) {
    function_result __result = { .has_return_value = true };
    running_process_data * d = rp_data(this);
    unsigned char buf[4096];
    ssize_t n = 0;
    if (d != NULL && d->stdout_reader_fd >= 0) {
        n = read(d->stdout_reader_fd, buf, sizeof(buf));
        if (n == 0) {
            // EOF — child closed stdout.
            close(d->stdout_reader_fd);
            d->stdout_reader_fd = -1;
        } else if (n < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                close(d->stdout_reader_fd);
                d->stdout_reader_fd = -1;
            }
            n = 0;  // no data right now
        }
    }
    aobject * arr = __create_array((unsigned int) n, 1, &Am_Lang_Array_ta_Am_Lang_UByte, uchar_type);
    if (n > 0) {
        array_holder * ah = (array_holder *) &arr[1];
        memcpy(ah->array_data, buf, (size_t) n);
    }
    __result.return_value.value.object_value = arr;
    return __result;
}

function_result Am_Lang_RunningProcess_writeInputBytes_0(aobject * const this, aobject * data, const long long offset, const unsigned int length) {
    function_result __result = { .has_return_value = true };
    running_process_data * d = rp_data(this);
    unsigned int wrote = 0;
    if (d != NULL && d->stdin_writer_fd >= 0 && data != NULL && !d->child_exited) {
        array_holder * ah = (array_holder *) &data[1];
        if ((unsigned long long) offset + length <= ah->size) {
            ssize_t w = write(d->stdin_writer_fd, (unsigned char *) ah->array_data + offset, length);
            if (w > 0) wrote = (unsigned int) w;
        }
    }
    __result.return_value.value.uint_value = wrote;
    __result.return_value.flags = PRIMITIVE_UINT;
    return __result;
}

function_result Am_Lang_RunningProcess_isAlive_0(aobject * const this) {
    function_result __result = { .has_return_value = true };
    running_process_data * d = rp_data(this);
    int alive = 0;
    if (d != NULL && d->child_pid > 0 && !d->child_exited) {
        int status = 0;
        pid_t r = waitpid(d->child_pid, &status, WNOHANG);
        if (r == 0) {
            alive = 1;  // still running
        } else if (r == d->child_pid) {
            d->child_exited = 1;
            // Capture exit code in Amiga-compatible shape: just the
            // unix exit status, masked to the low byte for normal
            // exits. New CLI's retry gate compares against 161 (the
            // ixemul libc-init-failed sentinel) so this only matters
            // on Amiga, but track it on libc too for parity.
            if (WIFEXITED(status)) {
                d->exit_code = WEXITSTATUS(status);
            } else {
                d->exit_code = -1;  // killed by signal / crash
            }
        }
    }
    // Child has been reaped — but the PTY master may still hold
    // a few buffered bytes the slave TTY hadn't flushed at exit.
    // Use poll(timeout=0) to ask the kernel "anything readable
    // right now?". If yes → keep claiming alive so the caller
    // drains the buffer. If no → return alive=0 and let the
    // caller close.
    //
    // The earlier "fd open" override unconditionally returned 1
    // here, which on macOS turns into an infinite re-schedule
    // loop in the AmLang drain task: macOS's non-blocking PTY
    // master returns EAGAIN rather than EIO for a long window
    // after the slave hangs up, so tryReadOutput keeps coming
    // back empty without ever closing the fd. The caller can't
    // close on its own because it only closes when isAlive==false.
    // poll() resolves the deadlock cleanly: an actually-drained
    // master reports neither POLLIN nor POLLHUP/POLLERR, so we
    // tell the caller "done" and the close fires.
    if (!alive && d != NULL && d->stdout_reader_fd >= 0) {
        struct pollfd pfd;
        pfd.fd = d->stdout_reader_fd;
        pfd.events = POLLIN;
        pfd.revents = 0;
        int pr = poll(&pfd, 1, 0);
        if (pr > 0 && (pfd.revents & POLLIN) != 0) {
            alive = 1;  // bytes ready — keep draining
        } else if (pr > 0 && (pfd.revents & (POLLHUP | POLLERR | POLLNVAL)) != 0) {
            // Slave hung up. Eagerly close so future isAlive
            // calls take the cheap fd<0 path instead of polling.
            close(d->stdout_reader_fd);
            d->stdout_reader_fd = -1;
        }
        // pr == 0: no data, no hup. Fall through with alive=0 —
        // child is reaped and the kernel has no pending bytes to
        // give us, so further polling would just spin.
    }
    __result.return_value.value.bool_value = alive ? true : false;
    return __result;
}

function_result Am_Lang_RunningProcess_exitCode_0(aobject * const this) {
    function_result __result = { .has_return_value = true };
    running_process_data * d = rp_data(this);
    int code = 0;
    if (d != NULL) {
        code = d->exit_code;
    }
    __result.return_value.value.int_value = code;
    return __result;
}

function_result Am_Lang_RunningProcess_close_0(aobject * const this) {
    function_result __result = { .has_return_value = false };
    rp_close_internal(rp_data(this));
    return __result;
}

// On libc we could send SIGINT to the child via kill(2) here to
// mirror the amigaos graceful-kill path, but the confirm-close
// flow on Mac/Linux already goes through close_0 (which SIGKILLs
// the child on shutdown) without the fh_Type-freed-port hazard
// that motivated terminateChild_0 on amigaos. Keeping the libc
// implementation as a no-op means killRunningChild() → close_0()
// continues to work exactly as it did before the amigaos split.
function_result Am_Lang_RunningProcess_terminateChild_0(aobject * const this) {
    function_result __result = { .has_return_value = false };
    (void) this;
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

// Raw-mode hook — asks the kernel "is the slave PTY currently in
// raw mode?" the same way every Unix program would (a tcgetattr
// on the fd). On a forkpty pair, `tcgetattr(master_fd)` returns
// the SLAVE's termios — so when the child runs `tcsetattr(raw)`
// (which ssh, nano, vim, htop, less, mc all do at startup via
// cfmakeraw / curses initscr / readline's prep_terminal), the
// master observes it on the next call.
//
// "Raw" here is the standard test curses uses: canonical input
// off (ICANON) AND local echo off (ECHO). Either alone would
// false-positive — a program that just wants per-char input but
// leaves echo on (uncommon) wouldn't want grid rendering, and an
// echo-off-but-line-buffered program (`stty -echo`) shouldn't
// either. Both bits being clear is the unambiguous "this is a
// full-screen / interactive TUI" signal.
//
// Returning false on a closed / non-tty fd is correct fallback —
// the CLI panel renders through the line-history path until the
// child actually switches mode.
function_result Am_Lang_RunningProcess_isRawMode_0(aobject * const this) {
    function_result __result = { .has_return_value = true };
    bool raw = false;
    running_process_data * d = rp_data(this);
    if (d != NULL && d->stdout_reader_fd >= 0) {
        struct termios t;
        if (tcgetattr(d->stdout_reader_fd, &t) == 0) {
            raw = ((t.c_lflag & ICANON) == 0) && ((t.c_lflag & ECHO) == 0);
        }
    }
    __result.return_value = (nullable_value){ .flags = PRIMITIVE_BOOL, .value.bool_value = raw };
    return __result;
}

function_result Am_Lang_RunningProcess_setReportedSize_0(aobject * const this, int var_rows, int var_cols) {
    function_result __result = { .has_return_value = false };
    running_process_data * d = rp_data(this);
    // Forward the AmLang panel's grid dimensions to the child's
    // pty via TIOCSWINSZ. Programs like ssh, nano and htop read
    // this on startup (and reread on SIGWINCH) to decide where to
    // wrap output. Without it the child stays at 24x80 and the
    // CLI panel looks like a wide window with truncated text.
    if (d != NULL && d->stdout_reader_fd >= 0 && var_rows > 0 && var_cols > 0) {
        struct winsize ws;
        memset(&ws, 0, sizeof(ws));
        ws.ws_row = (unsigned short) var_rows;
        ws.ws_col = (unsigned short) var_cols;
        ioctl(d->stdout_reader_fd, TIOCSWINSZ, &ws);
        if (d->child_pid > 0 && !d->child_exited) {
            // SIGWINCH so a child that's already running (re-resize
            // after the user changes panel size) notices the new dims
            // and repaints its UI to fit. Best-effort.
            kill(d->child_pid, SIGWINCH);
        }
    }
    return __result;
}
