#!/usr/bin/env bash
# Installs the Quest build toolchain under C:\RACQuest\toolchain (nothing system-wide).
set -euo pipefail
T=/c/RACQuest/toolchain; DL=$T/dl
export RUSTUP_HOME=$T/rustup CARGO_HOME=$T/cargo
echo "[rust]"
[ -f $DL/rustup-init.exe ] || curl -fsSL -o $DL/rustup-init.exe https://static.rust-lang.org/rustup/dist/x86_64-pc-windows-gnu/rustup-init.exe
"$DL/rustup-init.exe" -y --no-modify-path --profile minimal --default-host x86_64-pc-windows-gnu --default-toolchain stable -t aarch64-linux-android
"$T/cargo/bin/cargo.exe" install cargo-ndk --locked
echo "[jdk]"
if [ ! -d $T/jdk17 ]; then
  curl -fsSL -o $DL/jdk17.zip "https://api.adoptium.net/v3/binary/latest/17/ga/windows/x64/jdk/hotspot/normal/eclipse"
  mkdir -p $T/jdk.tmp && unzip -q $DL/jdk17.zip -d $T/jdk.tmp && mv $T/jdk.tmp/* $T/jdk17 && rmdir $T/jdk.tmp
fi
export JAVA_HOME=$T/jdk17
echo "[android sdk]"
if [ ! -d $T/android/cmdline-tools/latest ]; then
  curl -fsSL -o $DL/cmdline-tools.zip https://dl.google.com/android/repository/commandlinetools-win-13114758_latest.zip
  mkdir -p $T/android/cmdline-tools && unzip -q $DL/cmdline-tools.zip -d $T/android/cmdline-tools && mv $T/android/cmdline-tools/cmdline-tools $T/android/cmdline-tools/latest
fi
SM=$T/android/cmdline-tools/latest/bin/sdkmanager.bat
yes | "$SM" --sdk_root="$(cygpath -w $T/android)" --licenses >/dev/null || true
"$SM" --sdk_root="$(cygpath -w $T/android)" "platform-tools" "platforms;android-34" "build-tools;34.0.0" "ndk;27.2.12479018"
echo "[done]"
"$T/cargo/bin/rustc.exe" -V; "$T/cargo/bin/cargo.exe" ndk --version; "$JAVA_HOME/bin/java" -version 2>&1 | head -1; ls $T/android/ndk
