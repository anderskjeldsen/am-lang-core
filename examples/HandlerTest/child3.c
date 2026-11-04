// child3 — same as child2 minus the Read(tty). Writes a few lines
// via dos.library Write and exits cleanly. If PIPE: buffers until
// writer-close, we'd still get the bytes here because exit closes
// pipe_w. If PIPE: streams, we'd get them during execution. Either
// way bytes>0 confirms output reached parent.
#include <exec/types.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <string.h>
static void s(BPTR fh, const char * msg) { Write(fh, (APTR) msg, (LONG) strlen(msg)); }
int main(int argc, char ** argv) {
    s(Output(), "[child3] hello from child3\n");
    s(Output(), "[child3] writing some more bytes...\n");
    s(Output(), "[child3] exiting now\n");
    return 0;
}
