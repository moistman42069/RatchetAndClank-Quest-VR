#!/usr/bin/env bash
# Sourced in WSL: the APK's real ARM64 programs under qemu-user with Android's own linker and libc,
# laid out as the launcher lays them out on the headset (Tools.java).
S=/mnt/c/RACQuest/b/sysroot/root
Q=/mnt/c/RACQuest/toolchain/qemu/x/usr/bin/qemu-aarch64-static
T=/mnt/c/RACQuest/b/armtest
LIB=$T/lib
ROOT=$T/files/openrac
arm() {
    "$Q" -L "$S" \
        -E LD_LIBRARY_PATH="$LIB:/system/lib64" \
        -E PYTHONHOME="$ROOT/python" -E PYTHONDONTWRITEBYTECODE=1 -E PYTHONUNBUFFERED=1 \
        -E HOME="$T/files" -E TMPDIR="$T/cache" -E XDG_DATA_HOME="$T/files/share" \
        -E XDG_CONFIG_HOME="$T/files/config" -E OPENRAC_NATIVE_LIBRARY_DIR="$LIB" \
        -E ANDROID_ROOT=/system -E ANDROID_DATA=/data \
        "$@"
}
