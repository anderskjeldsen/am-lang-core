# am-lang-core changelog

The runtime and standard library every AmLang program links. Core is released
with the compiler and carries the same version; changes that need a compiler
change are also in the compiler's `docs/releases/CHANGELOG.md`. Redmine numbers
are issues in the private tracker.

## [0.16.0] - Unreleased

Changes since 0.15.0. Needs amlc 0.16.0.

### Added

- **LoopLabelTests** (2026-11-17, #164). Labeled loops (`outer@ for (...)`, `break@outer`, `continue@outer`) as a program: nested for/while/loop/each in both forms, to and until ranges, three levels, jumps out of a try (the finally runs, and a plain break or continue there skips the rest of the loop body, which v2 ran), inside switch and when, a label named like a local or the loop variable, a lambda reusing the outer label, a suspend function that parks inside the loops, and an allocation count that shows the objects of the left loop bodies are released. Needs amlc 0.16.0 (v1 or v2).

- **Path style conversion in `Am.IO.File`** (2026-11-17, #380). `File.fromUnixPath("./data/../assets")` gives the host's form of a path written once in source: on AmigaOS, AROS and MorphOS `.` is `""`, `..` an empty part (`a/../b` is `a//b`) and a leading `/` the volume root `:`; on Windows and DOS the separators become `\`; on Unix the path is untouched. `toUnixPath` goes back. `fromAmigaPath`/`toAmigaPath` and `fromWindowsPath`/`toWindowsPath` do the same for those styles. A root the target style can't express (`Work:x`, `C:\x`, `\\server\share`) gives null. `File.convertPath(path, fromStyle, toStyle)` works between any two styles on any host, and `File.getPathStyle()` returns `PathStyleUnix`, `PathStyleAmiga` or `PathStyleWindows`, from a new `FileNativeHelper.getPathStyle` native. `..` is never resolved against the name before it. New PathStyleTests.
- **`TaskScheduler.runAsync<T>(threadType, work)`** (2026-11-14, #299). A suspend function runs `work` on a worker thread and resumes on its own thread with the result. The continuation stays on the caller's thread; only the work lambda crosses. The v2 compiler can't compile callers yet (#301).
- **`String.compareTo(other)`** (2026-11-14, #185). Byte-wise on the UTF-8 form, which is code point order; returns -1, 0 or 1, and a prefix sorts first. The compiler now rejects `<` `>` `<=` `>=` on strings, which compared addresses. Native in libc only; the other platforms' `String.c` are stubs.
- **`#noThrow` on 63 natives whose C can't throw on any platform** (2026-11-12, #183), checked against every platform's implementation. `tools/check-nothrow.py` repeats the audit and fails if a `#noThrow` native's C can throw.

### Changed

- **Build targets name `floatingPoint`** (2026-11-11, #247). amlc 0.16.0 no longer enables every feature for a target without `features:`, so core's targets list it.
- **Core uses `as T` instead of the obsolete `toT()` conversions** (2026-11-10). A build of core prints no warnings. `amlc test` prints 3, on purpose: they are the regression tests for #149 and #158.
- **Hex constants without type suffixes** (2026-11-10, #184). The min/max constants of Byte, Short, Long and the unsigned types take their type from the declaration (`0x80B` is now a compiler error).
- **`File.list()` joins through `joinPath`** (2026-11-17, #380). Listing `""` gives `name`, not the absolute `/name`. `joinPath` accepts a trailing `\` on Windows.
- **String index lookups use checkpoints** (2026-11-08). For a string with any non-ASCII character, `substring`, `indexOf` and `lastIndexOf` decoded the whole prefix on every call, so walking such a string by index was quadratic. A checkpoint every 32 UTF-16 units makes each lookup start nearby: v2's tokenizer got about 10 times faster on files with one non-ASCII character. ASCII strings are unchanged.

### Fixed

- **`File` on AmigaOS: `"."` is the current directory** (2026-11-17, #260). AmigaDOS has no `.` entry, and joins `DH0:` and `x` without a slash. The listing natives and `statEntry` now map `.` and `./` to `""` and join the AmigaDOS way.
- **A throw or return leaving a `suspend(cont)` body no longer frees the suspend state under the running frame** (2026-11-16, #298). New `Continuation.__claim`, which the compiler calls before that release.
- **Byte is `signed char` in the natives** (2026-11-10, #209). Plain `char` is unsigned on ARM and PowerPC, so `-128B` read back as 128 there.

### Tests

New regression tests for compiler fixes, most of which also run through v2:
- BindingRegressionTests: #106, #148, #149, #150, #158.
- LambdaTests: #33, #34, #188, #217, #222.
- LambdaLocalTests: #367, #372.
- TypedLambdaTests: #189 (2026-11-17), lambda parameters with declared types, `(x: Int) => x * 2`: untyped locals and fields typed by their lambda (expression and block bodies, void), `List<String>`, `String?` and function-typed parameters, captures and nested lambdas, passing to a parameter, to `filter`/`map` and to overloads. Passes under v1 and v2; a nullable primitive parameter (`(x: Int?) =>`) and a generic static callee are left out because v2 can't compile them yet.
- From the AmigaOS examples: v2 miscompiles of catch and each variables, and interface wrapping.
- Also: SuspendArcTests (#336), SuspendBlockExitTests (#298), SuspendTempTests (#35), InitOrderTests (#25) and RunAsyncTests (#299).
- WhenValueTests (#192, 2026-11-17): `when` as a value without `else` over a fully covered enum (several members in one branch, a single-member enum), a call subject evaluated exactly once, not at all past a short-circuiting `&&`, and once per loop pass, property-chain subjects, Strings and objects as the value, a nested value `when`, a nullable subject with value and `null` branches, a statement `when` without `else` that matches nothing, and `when` as a name. Runs under v1 and v2.
- Smaller ones: `!!` (#211, #239, #253), generic `T?` (#238), string templates (#179), bare calls after operators (#160), `this` in lambdas (#39), catch lines (#180) and a local named `primitive` (#181).
- ArrayLiteralTests (#193): array literals with locals, parameters, calls, enum values, objects, string concatenation and null, as arguments, return values, field and static initializers, in a lambda and a suspend function, evaluation order and a live-object count; v1 and v2. The v1-only cases (nested literals, `Int[][]`, `Int?[]`) are a separate program, `examples/array-literals-v1` (`amlc run`).

Totals: linux-x64 344/344 (v1 and v2); AmigaOS under amiberry 326/326 before #193 (not rerun since).
Totals: linux-x64 326/326, and AmigaOS under amiberry 326/326 (before WhenValueTests; with it linux-x64 338/338 under v1 and v2).

## [0.15.0] - 2026-11-07

Released with amlc 0.15.0. Core's loops use `until`, which older compilers
don't have.
