# Android NDK port — feasibility & plan

Estimate for adding an Android (NDK / Bionic) build target to AmLang, by analogy to the
existing `nintendo-ppc` (GameCube) and `linux-arm64v8` ports.

## Why it's tractable
`libc` is already an **abstract parent platform** with a dozen concrete children
(`linux-x64`, `linux-arm64v8`, `linux-ppc32/64`, `macos-arm`, musl variants, `amigaos`,
`morphos-ppc`, `nintendo-ppc`, `aros-x86-64`). Android's NDK is just another libc (Bionic)
on aarch64 — the **same shape** as the GameCube port: add a platform child + a build target.
No new build mechanism is required. `linux-arm64v8` is a near-identical starting point
(same ISA; only the toolchain/sysroot and a few Bionic quirks differ).

## Short answer
- **Headless / CLI binary running on a device via `adb`: easy** — ~1 day, low risk.
  Arguably *easier* than GameCube: Bionic is a real POSIX libc, stdout works over adb,
  no "must not return from `main`", no invisible logging, no emulator-config rabbit hole.
- **A real Android app (APK with a window + graphics): meaningfully more** — several days,
  medium complexity. The extra work is all Android plumbing (APK packaging, `NativeActivity`,
  EGL/GLES surface, activity lifecycle), not AmLang.

## The "same as GameCube" config (easy tier)
Directly analogous to the `nintendo-ppc` block — a platform pointing at the NDK clang,
plus a docker/ssh build target:

```yaml
platforms:
  - id: "android-arm64"
    extends: libc
    gccCommand: "aarch64-linux-android24-clang"   # NDK wrapper sets --sysroot/--target itself
    gccAdditionalOptions: [ -DAM_PLATFORM_ANDROID, -fPIC ]
    gccAdditionalLibraries: [ log, m ]
buildTargets:
  - id: "android-ndk-docker"
    platform: "android-arm64"
    commands:
      - docker run --rm -v ${build_dir}:/src <ndk-image> /entrypoint.sh -f android-arm64.makefile all
```

Add more ABIs (`armeabi-v7a`, `x86_64` for the emulator) later as extra children — trivial
once one works. NDK docker images exist, so the existing `dockerBuild` pattern applies.

## Where the real work is
1. **Runtime fixups (low risk, ~1 day).** `native/libc/*.c` already compiles for musl and
   several libcs, so most just works. Expect a handful of Bionic-specific edits:
   - `backtrace()` is absent in Bionic.
   - `pthread_setname_np` differs (16-char name limit).
   - no `execvpe`; some locale / `getpwuid` gaps.
   - (`socketpair`/`fork` are a non-issue — Bionic has both.)
   Shake-out, not redesign.
2. **Run path — pick your scope:**
   - *CLI:* `adb push` the PIE executable, run in `adb shell`. Done.
   - *App:* entry point becomes `android_main` (via `native_app_glue`), not `main`; package
     a `.so` into an APK and sign it. This is the analog of the libogc GX plumbing and is the
     bulk of the effort.
3. **Graphics (optional, on top of the app tier):** slot an **EGL + GLES2** backend into the
   existing `am-gl` package (`Am.Gl.GlContext` already abstracts macOS/MorphOS/etc.) — moderate,
   and it reuses the abstraction already built.

## Caveats
- The NDK **compile** can be verified end-to-end in a docker image. **On-device** behavior
  needs a device/emulator (adb makes this far less painful than the Dolphin case).
- APK/graphics-tier estimates are soft; the CLI tier is predictable.

## Suggested first step
Prototype the headless tier: add the `android-arm64` platform, wire an NDK docker build target,
and get a hello-world AmLang example to compile to an arm64 PIE executable → a pushable binary.
