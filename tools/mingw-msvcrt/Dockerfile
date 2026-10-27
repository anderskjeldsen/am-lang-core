# 32-bit Windows (XP-era / ReactOS) cross compiler. Debian's mingw-w64 links
# against the old msvcrt.dll, which every Windows since 95 and ReactOS ship;
# Homebrew's mingw-w64 targets UCRT (Windows 10+). Native arm64 on Apple Silicon.
#   docker build -t mingw-msvcrt tools/mingw-msvcrt
FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y --no-install-recommends \
      gcc-mingw-w64-i686-posix g++-mingw-w64-i686-posix make \
    && rm -rf /var/lib/apt/lists/*
