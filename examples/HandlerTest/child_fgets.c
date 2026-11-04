// child_fgets — libnix child that reads ONE line via stdio fgets()
// (same path bebbossh's amiprintf / FGetC family uses) and reports
// the raw bytes it received, in hex.
//
// Diagnostic intent: prove what byte sequence libnix fgets accepts
// as "end-of-line" when reading from our custom DOS handler. Possible
// outcomes:
//   * If LF (0x0A) terminates -> CliView should send 0x0A.
//   * If CR (0x0D) terminates -> CliView should send 0x0D.
//   * If only CRLF terminates -> CliView should send 0x0D 0x0A.
//   * If NEITHER terminates fgets -> something else in the handler
//     is wrong (e.g. WAIT_CHAR / WAIT semantics) and SSH won't be
//     the bottleneck.

#include <stdio.h>
#include <string.h>

int main(int argc, char ** argv) {
    printf("[fg] before fgets\n");
    fflush(stdout);

    char buf[128];
    char * r = fgets(buf, sizeof buf, stdin);
    if (r == NULL) {
        printf("[fg] fgets returned NULL\n");
        fflush(stdout);
        return 1;
    }
    int n = (int) strlen(buf);
    printf("[fg] fgets ok len=%d bytes=", n);
    for (int i = 0; i < n; i++) {
        printf("%02x ", (unsigned char) buf[i]);
    }
    printf("\n");
    fflush(stdout);
    return 0;
}
