#!/usr/bin/env sh
set -eu

if [ -n "${SDL3_PREFIX:-}" ]; then
    PKG_CONFIG_PATH="${SDL3_PREFIX}/lib64/pkgconfig:${SDL3_PREFIX}/lib/pkgconfig:${PKG_CONFIG_PATH:-}"
    export PKG_CONFIG_PATH
fi

if pkg-config --exists sdl3; then
    SDL3_CFLAGS=$(pkg-config --cflags sdl3)
    SDL3_LIBS=$(pkg-config --libs sdl3)
elif pkg-config --exists SDL3; then
    SDL3_CFLAGS=$(pkg-config --cflags SDL3)
    SDL3_LIBS=$(pkg-config --libs SDL3)
else
    echo "SDL3 development files not found. Install SDL3 or set SDL3_PREFIX." >&2
    exit 1
fi

mkdir -p build
cc -std=c99 -Wall -Wextra -Werror -pedantic -Iinclude $SDL3_CFLAGS \
    src/component.c \
    src/ecsvm.c \
    src/main.c \
    src/pong.c \
    src/system_renderer.c \
    src/system_window.c \
    -o build/ecsvm \
    $SDL3_LIBS -lm
