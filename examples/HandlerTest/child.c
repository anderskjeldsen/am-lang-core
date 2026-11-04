// Child program for the dos.library handler test.
//
// Asks for a name on stdout, reads it from stdin via the C stdio
// library, echoes back a greeting. Compiled standalone with
// m68k-amigaos-gcc; relies on whatever libc the toolchain links by
// default (libnix on the amiga-gcc docker image).
//
// When run as DH1:child from the parent test, its stdin/stdout/
// stderr are the FakeFileHandle BPTRs the parent built — every
// printf turns into ACTION_WRITE packets on the parent handler's
// MsgPort, and fgets turns into ACTION_READ packets.

#include <stdio.h>
#include <string.h>

int main(void) {
    char name[64];
    printf("What is your name? ");
    fflush(stdout);
    if (fgets(name, sizeof(name), stdin) == NULL) {
        printf("(no input received)\n");
        return 1;
    }
    // Strip trailing newline so the greeting looks right.
    size_t n = strlen(name);
    while (n > 0 && (name[n - 1] == '\n' || name[n - 1] == '\r')) {
        name[--n] = '\0';
    }
    printf("Hello, %s!\n", name);
    return 0;
}
