# Wine for running windows-x64 / windows-x86 builds from a Mac/Linux host. amd64 only
# (runs emulated on Apple Silicon).
#   docker build --platform linux/amd64 -t wine-amlang tools/wine
# Console programs / tests:
#   docker run --rm --platform linux/amd64 -v "$PWD":/w -w /w wine-amlang wine app.exe
# Windowed (SDL2 + Direct3D 9 via Wine's wined3d on Mesa's software GL), on a
# virtual X display; am-gl's AMGL_DUMP writes a frame to check:
#   docker run --rm --platform linux/amd64 -v "$PWD":/w -w /w -e AMGL_DUMP=frame.bmp \
#       wine-amlang xvfb-run -a -s "-screen 0 1280x1024x24" wine app.exe
FROM debian:bookworm-slim
# i386 multiarch for wine32: windows-x86 (XP / ReactOS) builds are 32-bit.
RUN dpkg --add-architecture i386 && apt-get update && apt-get install -y --no-install-recommends \
        wine wine64 wine32 xvfb xauth libgl1-mesa-dri libglx-mesa0 \
        libgl1-mesa-dri:i386 libglx-mesa0:i386 \
    && rm -rf /var/lib/apt/lists/*
ENV WINEDEBUG=-all
# Create the prefix once at build time, so each run starts quickly.
RUN wineboot --init 2>/dev/null; true
