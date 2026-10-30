# Source this: the Quest build's tools, all under C:\RACQuest\toolchain.
T=/c/RACQuest/toolchain
export JAVA_HOME=$T/jdk17 ANDROID_HOME=$T/android ANDROID_SDK_ROOT=$T/android
export ANDROID_NDK_HOME=$T/android/ndk/27.2.12479018 NDK_HOME=$T/android/ndk/27.2.12479018
export RUSTUP_HOME=$T/rustup CARGO_HOME=$T/cargo GRADLE_USER_HOME=$T/gradle
export PATH="$T/cargo/bin:$JAVA_HOME/bin:$ANDROID_HOME/platform-tools:$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/windows-x86_64/bin:$PATH"
