// child4 — like child3 but via printf (libnix stdio) instead of
// Write(Output(), ...). If we see ACTION_WRITE packets from this
// when stdout=handler-FH, the stdio path works. If silent (like
// bebbossh), libnix's stdio is dropping the writes for our FH for
// some specific reason we need to identify.
#include <stdio.h>
int main(int argc, char ** argv) {
    printf("[child4] hello via printf\n");
    printf("[child4] %d args\n", argc);
    fflush(stdout);
    printf("[child4] exiting now\n");
    return 0;
}
