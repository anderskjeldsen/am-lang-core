#include <stdio.h>
#include <string.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <dos/dosextens.h>

static void (*capture_exit_hook)(void);

/* Forward the DOS register ABI to the capture hook, then keep this task
 * alive while stdout is still owned by DOS. A fixed post-hook delay must
 * not make the parent read that exclusive handle before cleanup finishes. */
static void delayed_exit(register LONG status __asm("d0"),
                         register LONG data __asm("d1"))
{
    if (capture_exit_hook) {
        register LONG rc __asm("d0") = status;
        register LONG context __asm("d1") = data;
        register void (*hook)(void) __asm("a0") = capture_exit_hook;
        __asm volatile("jsr (%2)"
                       : "+r"(rc), "+r"(context), "+a"(hook)
                       : : "a1", "cc", "memory");
    }
    Delay(25);
}

/* Deliberately use libc stderr: testing only Write(pr_CES, ...) would miss
 * a libc which opens the inherited console instead of using that stream. */
int main(int argc, char **argv)
{
    const char *mode = argc > 1 ? argv[1] : "";
    if (!strcmp(mode, "stderr")) {
        fprintf(stderr, "STDERR-ONLY\n");
        fflush(stderr);
        return 10;
    }
    if (!strcmp(mode, "mixed")) {
        printf("STDOUT-FIRST\n");
        fflush(stdout);
        Delay(10);
        fprintf(stderr, "STDERR-MIDDLE\n");
        fflush(stderr);
        Delay(10);
        printf("STDOUT-LAST\n");
        fflush(stdout);
        return 0;
    }
    if (!strcmp(mode, "silent")) {
        return 23;
    }
    if (!strcmp(mode, "renamed")) {
        struct Task *task = FindTask(NULL);
        char *old_name = task->tc_Node.ln_Name;
        Forbid();
        task->tc_Node.ln_Name = "capture-child-renamed";
        Permit();
        Delay(25);
        fprintf(stderr, "RENAMED-STDERR\n");
        fflush(stderr);
        Forbid();
        task->tc_Node.ln_Name = old_name;
        Permit();
        return 0;
    }
    if (!strcmp(mode, "exit-cleanup")) {
        struct Process *proc = (struct Process *)FindTask(NULL);
        capture_exit_hook = proc->pr_ExitCode;
        proc->pr_ExitCode = delayed_exit;
        puts("EXIT-STDOUT");
        fprintf(stderr, "EXIT-STDERR\n");
        fflush(stdout);
        fflush(stderr);
        return 0;
    }
    if (!strcmp(mode, "identity")) {
        char name[128];
        /* Both children must exist before reading the DOS-owned CLI name. */
        Delay(15);
        if (GetProgramName(name, sizeof(name))) {
            printf("NAME=%s\n", name);
        } else {
            puts("NAME-MISSING");
        }
        fflush(stdout);
        return 0;
    }
    if (!strcmp(mode, "metadata")) {
        struct Process *proc = (struct Process *)FindTask(NULL);
        struct CommandLineInterface *cli =
            (struct CommandLineInterface *)BADDR(proc->pr_CLI);
        BPTR marker = Open("cwd-marker", MODE_OLDFILE);
        if (marker) {
            Close(marker);
            puts("CWD-OK");
        } else {
            puts("CWD-MISSING");
        }
        puts(cli && cli->cli_Module ? "MODULE-OK" : "MODULE-MISSING");
        puts(proc->pr_CES ? "CES-OK" : "CES-MISSING");
        /* ixnet reserves nonzero ExitData for an inetd DaemonMessage. */
        puts(proc->pr_ExitData == 0 ? "EXITDATA-CLEAR" : "EXITDATA-OCCUPIED");
        puts(!strcmp(argv[0], "DH1:child") ? "ARGV0-OK" : "ARGV0-MISMATCH");
        fflush(stdout);
        return 0;
    }
    fprintf(stderr, "UNKNOWN-MODE\n");
    return 20;
}
