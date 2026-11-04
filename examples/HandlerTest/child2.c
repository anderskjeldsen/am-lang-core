// child2 — reproduces bebbossh's password-prompt pattern.
//
// Instead of reading from stdin (the SYS_Input BPTR the parent set
// up), we open "*" via dos.library Open() — that's the AmigaOS
// /dev/tty equivalent SSH-family programs use to read passwords
// without echo, BYPASSING stdin entirely.
//
// Open("*") sends ACTION_FINDINPUT to whatever pr_ConsoleTask points
// at. If NP_ConsoleTask was honoured by SystemTagList, that's the
// parent's handler. If not, it's wherever the parent's own
// pr_ConsoleTask was inherited from — typically nowhere useful.
//
// We log everything we do via Write() on whichever fh we opened, so
// the parent handler (if it received the FINDINPUT) sees both the
// prompt write AND any subsequent Read. If the open succeeds but
// goes to a DIFFERENT handler, the parent sees nothing.

#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <proto/exec.h>
#include <proto/dos.h>

#include <string.h>

static void s(BPTR fh, const char * msg) {
    Write(fh, (APTR) msg, (LONG) strlen(msg));
}

int main(int argc, char ** argv) {
    // First, log to OUR stdout (= parent's fh_out) that we're
    // starting, so the parent knows the child reached main().
    s(Output(), "[child2] started\n");

    // Print the current process's pr_ConsoleTask via Output so the
    // parent can confirm whether NP_ConsoleTask was inherited.
    struct Process * self = (struct Process *) FindTask(NULL);
    Write(Output(), (APTR) "[child2] pr_ConsoleTask=0x", 26);
    {
        char buf[12]; int n = 0;
        ULONG v = (ULONG) self->pr_ConsoleTask;
        if (v == 0) {
            buf[n++] = '0';
        } else {
            int leading = 1;
            for (int nib = 7; nib >= 0; nib--) {
                char c = "0123456789abcdef"[(v >> (nib * 4)) & 0xF];
                if (leading && c == '0' && nib > 0) continue;
                leading = 0;
                buf[n++] = c;
            }
        }
        Write(Output(), buf, n);
    }
    Write(Output(), (APTR) "\n", 1);

    // Now try to Open("*"). This is what ssh's getpass-style code
    // does. Goes to pr_ConsoleTask. If our parent handler is bound
    // there, the parent's handler will see ACTION_FINDINPUT for
    // "*".
    s(Output(), "[child2] trying Open(\"*\", MODE_OLDFILE)\n");
    BPTR tty = Open((CONST_STRPTR) "*", MODE_OLDFILE);
    if (tty == 0) {
        s(Output(), "[child2] Open(\"*\") FAILED — pr_ConsoleTask isn't reachable\n");
        return 1;
    }
    s(Output(), "[child2] Open(\"*\") succeeded\n");

    // Write the password prompt and read a line via the tty fh.
    s(tty, "Password: ");

    char buf[64];
    LONG got = Read(tty, buf, sizeof(buf) - 1);
    if (got <= 0) {
        s(Output(), "[child2] Read returned 0 / -1 — no password received\n");
        Close(tty);
        return 1;
    }
    buf[got] = 0;
    // Strip trailing LF for display.
    while (got > 0 && (buf[got - 1] == '\n' || buf[got - 1] == '\r')) {
        buf[--got] = 0;
    }
    s(Output(), "[child2] got password of length ");
    {
        char nb[8]; int n = 0; LONG v = got;
        if (v == 0) { nb[n++] = '0'; }
        while (v > 0) { nb[n++] = (char)('0' + (v % 10)); v /= 10; }
        for (int a = 0, b = n - 1; a < b; a++, b--) { char t = nb[a]; nb[a] = nb[b]; nb[b] = t; }
        Write(Output(), nb, n);
    }
    s(Output(), ": [");
    s(Output(), buf);
    s(Output(), "]\n");

    Close(tty);
    return 0;
}
