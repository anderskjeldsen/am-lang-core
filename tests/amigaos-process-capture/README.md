# AmigaOS process capture regression

Run from this directory with `bash run.sh`. This builds the AmLang parent
against `../../` (the current core sources) with the workspace `amlc.jar`
and `amiga-gcc:13`, then compiles a small `-noixemul` C child.

Requires the existing `amiberry-headless` runner and its Docker image.
Override `AMLC_JAR`, `IMAGE` (emulator image), or `RUN_TIMEOUT` as needed.
The runner mounts only a disposable fixture directory as `DH1:`; binaries
and logs remain in that temporary directory for inspection.

The parent exercises the actual native implementations, checking:

- `fprintf(stderr, ...)` with no stdout, and mixed stdout/stderr.
- A silent child that returns 23, without treating empty output as failure.
- Delayed stderr from a child that renames its task, through both synchronous
  capture APIs, and delayed exit-hook cleanup before reading stdout.
- Working-directory inheritance and restoration, `pr_CES`, `cli_Module`,
  an empty `pr_ExitData` for ordinary commands,
  and the program name exposed through `argv[0]`.
- Streaming output observed before child exit, complete final output,
  and return statuses 0, 10, and 23.
- Two concurrent children retaining distinct DOS CLI command names.
- A 107-byte executable path still loading successfully while both APIs
  expose the basename `child-long` within the DOS command-name limit.

Synchronous capture uses a separate DOS process with a 64 KiB stack and
file handles for stdout and stderr. It waits for DOS process cleanup before
reading those files. Exit callbacks find their context by the child task;
they leave `pr_ExitData` and ixemul's `tc_UserData` available to the runtime.

Each asynchronous case has a 15-second polling bound; the emulator has a
120-second outer timeout to catch blocked native calls. Success requires
`AMIGAOS-PROCESS-CAPTURE PASSED` and no `FAIL` lines, not merely reaching
the emulator's completion sentinel. Synchronous calls check output rather
than exit status because the public capture API only returns a string.

For native ixemul GCC and GNU make as well, run:

```sh
CAPTURE_PROFILE=ade bash run.sh
```

This selects the existing `amlang-amiberry-ade:latest` image, checks that
GCC's stderr-only "no input files" diagnostic is captured through both
APIs, checks its asynchronous exit status, and runs a makefile producing
both stdout and stderr. The disposable startup assigns `AmiTCP: SYS:` to
prevent ixemul's offline startup probe waiting for an absent-volume requester.
It also configures a nonempty spawn search path, with an existing directory
without GCC followed by `ADE:bin`, and checks bare `gcc` through synchronous
capture and asynchronous execution before restoring the empty search path.

To exercise ixnet's startup checks with its AmiTCP backend active, run:

```sh
CAPTURE_PROFILE=ade-network bash run.sh
```

This assigns `AmiTCP: ADE:Sys`, exposing the image's existing
`libs/usergroup.library` alongside the emulated socket library. The ordinary
`ade` profile's dummy assign leaves this backend inactive. With networking
active, ixnet interprets a nonzero `pr_ExitData` as an inetd socket message;
the regression checks that process completion tracking does not cause GCC
and make to exit silently with status 161 before reaching their main functions.

The current diagnostic build also writes `PROGDIR:process-capture.log` from
the waiting parent. It records the binary name, completion and exit-hook
status, return code, and bytes read from each stream (`-1` means a read
failed). The completion line identifies the task registry and zero startup
exit data used by this build.
The final line records the resolved executable path, DOS/Exec versions,
CPU flags, and the loaded ixemul version (`-1.-1` if absent from the library list).
It omits command arguments and output content. Define
`AM_PROCESS_CAPTURE_DIAGNOSTICS=0` to disable this logging.
