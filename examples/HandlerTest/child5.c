// child5 — uses dos.library FPuts (same path as bebbossh's amiprintf).
// If we see ACTION_WRITE through the handler when this runs, FPuts
// works on our FakeFileHandle. If silent, FPuts requires fh_Buf or
// fh_Funcs setup that bebbossh's writes are silently failing on.
#include <exec/types.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
int main(int argc, char ** argv) {
    FPuts(Output(), (CONST_STRPTR) "[child5] hello via FPuts\n");
    Flush(Output());
    return 0;
}
