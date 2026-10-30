"""Host regression tests for Amiga helpers, with DOS calls replaced by stubs.
Actual filesystem-handler behavior still requires an Amiga runtime test.
"""
from pathlib import Path
import subprocess
import tempfile

source = (Path(__file__).parents[2] / 'src/native-c/libc/Am/IO/FileStream.c').read_text()
start = source.index('static int am_stream_flush_volume(')
end = source.index('\n#endif', start)
helpers = source[start:end]
adapter_start = source.index('// Worker isolates are separate Exec processes.')
adapter_end = source.index('\n#endif', adapter_start)
adapter = source[adapter_start:adapter_end]
stubs = r'''
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>
typedef const char *CONST_STRPTR;
typedef long LONG;
typedef FILE *BPTR;
typedef void *APTR;
#define DOSFALSE 0
#define ACTION_FLUSH 27
#define MODE_OLDFILE 1005
#define MODE_NEWFILE 1006
#define MODE_READWRITE 1004
#define OFFSET_CURRENT 0
#define OFFSET_END 1
#define OFFSET_BEGINNING -1
struct DevProc { void *dvp_Port; };
static int missing_device, fail_flush, frees, fail_open, fail_resize;
static struct DevProc device;
static struct DevProc *GetDeviceProc(const char *p, void *unused) {
    return missing_device ? NULL : &device;
}
static LONG DoPkt(void *port, LONG action, LONG a, LONG b, LONG c, LONG d, LONG e) {
    assert(action == ACTION_FLUSH);
    return fail_flush ? 0 : -1;
}
static void FreeDeviceProc(struct DevProc *p) { assert(p == &device); frees++; }
static BPTR Open(const char *path, int mode) {
    if (fail_open) { return NULL; }
    if (mode == MODE_NEWFILE) { return fopen(path, "w+b"); }
    FILE *f = fopen(path, "r+b");
    if (!f && mode == MODE_READWRITE) { f = fopen(path, "w+b"); }
    return f;
}
static LONG SetFileSize(BPTR handle, LONG length, LONG mode) {
    assert(mode == OFFSET_BEGINNING);
    return fail_resize || ftruncate(fileno(handle), length) ? -1 : length;
}
static int Close(BPTR handle) { return fclose(handle) == 0; }
static LONG Seek(BPTR handle, LONG offset, LONG mode) {
    long old = ftell(handle);
    int origin = mode == OFFSET_BEGINNING ? SEEK_SET : (mode == OFFSET_END ? SEEK_END : SEEK_CUR);
    return fseek(handle, offset, origin) ? -1 : old;
}
static LONG Read(BPTR handle, void *bytes, LONG length) { return fread(bytes, 1, length, handle); }
static LONG Write(BPTR handle, void *bytes, LONG length) { return fwrite(bytes, 1, length, handle); }
static LONG Flush(BPTR handle) { return fflush(handle) == 0; }
'''
tests = r'''
int main(int argc, char **argv) {
    assert(argc == 2);
    FILE *f = fopen(argv[1], "w+b");
    assert(f);
    assert(fwrite("abcdefgh", 1, 8, f) == 8);
    assert(am_stream_resize(f, argv[1], 4));
    assert(ftell(f) == 8);
    struct stat info;
    assert(stat(argv[1], &info) == 0 && info.st_size == 4);
    assert(am_stream_resize(f, argv[1], 12));
    assert(ftell(f) == 8);
    assert(stat(argv[1], &info) == 0 && info.st_size == 12);
    fseek(f, 0, SEEK_SET);
    char bytes[4];
    assert(fread(bytes, 1, 4, f) == 4 && bytes[3] == 'd');
    fail_resize = 1;
    assert(!am_stream_resize(f, argv[1], 1) && ftell(f) == 4);
    assert(stat(argv[1], &info) == 0 && info.st_size == 12);
    assert(!am_stream_flush_volume(NULL));
    missing_device = 1;
    assert(!am_stream_flush_volume(argv[1]) && frees == 0);
    missing_device = 0; fail_flush = 1;
    assert(!am_stream_flush_volume(argv[1]) && frees == 1);
    fail_flush = 0;
    assert(am_stream_flush_volume(argv[1]) && frees == 2);
    assert(fclose(f) == 0);
    FILE *a = fopen(argv[1], "rb"), *b = fopen(argv[1], "rb");
    assert(a && b);
    for (int i = 0; i < 1000; i++) {
        assert(fseek(a, 0, SEEK_SET) == 0);
        assert(fseek(b, 2, SEEK_SET) == 0);
        unsigned char x, y;
        assert(fread(&x, 1, 1, a) == 1 && x == 'a');
        assert(fread(&y, 1, 1, b) == 1 && y == 'c');
    }
    assert(fseek(a, 0, SEEK_END) == 0);
    assert(fread(bytes, 1, 1, a) == 0);
    assert(fseek(a, 0, SEEK_SET) == 0 && fread(bytes, 1, 4, a) == 4);
    assert(!am_stream_resize(a, argv[1], 2));
    assert(fwrite(bytes, 1, 1, a) == 0);
    fclose(a); fclose(b);
    FILE *append = fopen(argv[1], "ab");
    assert(append && fseek(append, 0, SEEK_SET) == 0);
    assert(fwrite("z", 1, 1, append) == 1 && fflush(append) == 0);
    fclose(append);
    assert(stat(argv[1], &info) == 0 && info.st_size == 13);
    puts("Amiga file helper tests passed (host DOS stubs)");
}
'''
with tempfile.TemporaryDirectory(prefix='amiga-filestream-test-') as directory:
    directory = Path(directory)
    c = directory / 'test.c'
    c.write_text(stubs + adapter + '\n' + helpers + tests)
    binary = directory / 'test'
    subprocess.run(['cc', '-Wall', '-Werror', str(c), '-o', str(binary)], check=True)
    subprocess.run([str(binary), str(directory / 'data')], check=True)
