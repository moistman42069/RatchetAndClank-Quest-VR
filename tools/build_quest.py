#!/usr/bin/env python3
"""
Builds the Quest APK: OpenRAC's port for both discs, the VR mod, OpenRAC's tools with their Python,
the launcher.

  python tools/build_quest.py [--no-port] [--no-apk] [--release]

It needs the toolchain of toolchain/setup.sh (source env.sh first). The output is
build/RatchetQuestVR-<version>.apk beside android/, and every step stops the build on a failure.
"""
import argparse
import hashlib
import os
import re
import shutil
import subprocess
import sys
import urllib.request
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OPENRAC = ROOT / "third_party/OpenRAC"
# OpenRAC 0.10.1 (develop, 2026-10-30), with patches/*.patch on top.
OPENRAC_PIN = "293cb5cc05bcbffe06d3c59685488efc806d1bff"
PATCHES = ROOT / "patches"
BUILD_ROOT = Path(os.environ.get("QUEST_BUILD_ROOT", "C:/RACQuest/b"))
PORT_BUILD = BUILD_ROOT / "port"
VR_BUILD = BUILD_ROOT / "vr"
STAGE = ROOT / "android/build/stage"
GAMES = ("rac1-pal", "rac1-ntsc")

OPENXR_VERSION = "1.1.65"
OPENXR_SHA1 = "b81c1049f63302428fbf473d1b310de51c32b969"
OPENXR_URL = ("https://repo1.maven.org/maven2/org/khronos/openxr/openxr_loader_for_android/"
              f"{OPENXR_VERSION}/openxr_loader_for_android-{OPENXR_VERSION}.aar")

sys.path.insert(0, str(OPENRAC / "tools"))
import build_android as openrac_android  # noqa: E402  (its pinned Python for Android)


def run(command, cwd=None, env=None):
    print("$", " ".join(str(c) for c in command), flush=True)
    subprocess.run([str(c) for c in command], cwd=cwd, env=env, check=True)


def check_openrac() -> None:
    """OpenRAC at the pin with every patch applied, or no build: a game without the Quest patches
    builds, and fails only in the headset."""
    def git(*command):
        return subprocess.run(["git", "-C", OPENRAC, *command], capture_output=True, text=True)

    head = git("rev-parse", "HEAD").stdout.strip()
    if head != OPENRAC_PIN:
        sys.exit(f"third_party/OpenRAC is at {head or '(no commit)'}, not the pinned {OPENRAC_PIN}")
    patches = sorted(PATCHES.glob("*.patch"))
    if not patches:
        sys.exit(f"{PATCHES} has no patches")
    for patch in patches:
        if git("apply", "--reverse", "--check", patch).returncode == 0:
            print("applied:", patch.name, flush=True)
        elif git("apply", "--check", patch).returncode == 0:
            run(["git", "-C", OPENRAC, "apply", patch])
        else:
            sys.exit(f"{patch.name} is neither applied to third_party/OpenRAC nor applies to it cleanly")
    # The launcher takes the level format from editor/port.py (android/app/build.gradle); the game
    # reads kManifestFormat. Apart, every set-up would be refused or never asked for again.
    tool = re.search(r"^FORMAT = (\d+)$", (OPENRAC / "editor/port.py").read_text(encoding="utf8"), re.M)
    game = re.search(r"kManifestFormat = (\d+);", (OPENRAC / "port/scene/level.h").read_text(encoding="utf8"))
    if not tool or not game or tool.group(1) != game.group(1):
        sys.exit("OpenRAC's level formats differ: editor/port.py FORMAT and port/scene/level.h kManifestFormat")
    print("level format", tool.group(1), flush=True)


def ndk() -> Path:
    home = os.environ.get("ANDROID_NDK_HOME")
    if not home or not Path(home).is_dir():
        sys.exit("ANDROID_NDK_HOME is not set: source env.sh first")
    return Path(home)


def openxr_loader() -> Path:
    folder = BUILD_ROOT / "openxr"
    if not (folder / "jni/arm64-v8a/libopenxr_loader.so").is_file():
        print("downloading", OPENXR_URL, flush=True)
        with urllib.request.urlopen(OPENXR_URL) as response:
            data = response.read()
        got = hashlib.sha1(data).hexdigest()
        if got != OPENXR_SHA1:
            sys.exit(f"the OpenXR loader's SHA-1 is {got}, not {OPENXR_SHA1}")
        folder.mkdir(parents=True, exist_ok=True)
        archive = folder / "loader.aar"
        archive.write_bytes(data)
        with zipfile.ZipFile(archive) as z:
            z.extractall(folder)
    return folder


def toolchain_file() -> Path:
    return ndk() / "build/cmake/android.toolchain.cmake"


def build_port(python_target: Path) -> None:
    run(["cmake", "-S", OPENRAC / "port", "-B", PORT_BUILD, "-G", "Ninja",
         f"-DCMAKE_TOOLCHAIN_FILE={toolchain_file()}", "-DCMAKE_BUILD_TYPE=RelWithDebInfo",
         "-DANDROID_ABI=arm64-v8a", "-DANDROID_PLATFORM=android-29", "-DANDROID_STL=c++_shared",
         "-DOPENRAC_PORT_LTO=ON", "-DOPENRAC_ANDROID_ADRENOTOOLS=OFF", "-DOPENRAC_PROBE_BAKER=OFF",
         f"-DOPENRAC_ANDROID_PYTHON={python_target}", f"-DCMAKE_MAKE_PROGRAM={shutil.which('ninja')}",
         f"-DPython3_EXECUTABLE={sys.executable}", "-Wno-dev", "--no-warn-unused-cli"])
    targets = [t for game in GAMES for t in (f"openrac-{game}", f"openrac-{game}-cli")]
    run(["cmake", "--build", PORT_BUILD, "--target", *targets, "openrac-python", "SDL3-shared"])


def check_hooks() -> None:
    """Every hook point the mod takes, with the SDK's signature, in each disc's table (tools/check_hooks.py):
    a disc with another refuses the mod at load, and the headset shows black."""
    run([sys.executable, ROOT / "tools/check_hooks.py",
         *(PORT_BUILD / "games" / game / "gen/hooks.c" for game in GAMES)])


def build_mod(openxr: Path) -> None:
    run(["cmake", "-S", ROOT / "vr", "-B", VR_BUILD, "-G", "Ninja",
         f"-DCMAKE_TOOLCHAIN_FILE={toolchain_file()}", "-DCMAKE_BUILD_TYPE=RelWithDebInfo",
         "-DANDROID_ABI=arm64-v8a", "-DANDROID_PLATFORM=android-29", "-DANDROID_STL=c++_shared",
         f"-DCMAKE_MAKE_PROGRAM={shutil.which('ninja')}", f"-DOPENXR_DIR={openxr.as_posix()}", "-Wno-dev"])
    run(["cmake", "--build", VR_BUILD])


def strip_tool() -> Path:
    found = sorted(ndk().glob("toolchains/llvm/prebuilt/*/bin/llvm-strip*"))
    if not found:
        sys.exit("the NDK has no llvm-strip")
    return found[0]


def stage_libraries(python_target: Path, openxr: Path) -> None:
    dest = STAGE / "jniLibs/arm64-v8a"
    shutil.rmtree(dest, ignore_errors=True)
    dest.mkdir(parents=True)
    wanted = [PORT_BUILD / f"libopenrac-{game}.so" for game in GAMES]
    wanted += [PORT_BUILD / f"libopenrac-{game}-cli.so" for game in GAMES]
    wanted.append(PORT_BUILD / "libopenrac-python.so")
    found = sorted(PORT_BUILD.rglob("libSDL3.so"))
    if not found:
        sys.exit("the port's build has no libSDL3.so")
    wanted.append(found[0])
    wanted += sorted(ndk().glob("toolchains/llvm/prebuilt/*/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so"))
    wanted.append(openxr / "jni/arm64-v8a/libopenxr_loader.so")
    wanted += sorted((python_target / "jniLibs/arm64-v8a").glob("*.so"))
    for path in wanted:
        if not path.is_file():
            sys.exit(f"{path} was not built")
        shutil.copy2(path, dest / path.name)
        run([strip_tool(), "--strip-debug", dest / path.name])
    # Android packs only lib*.so: the mod is copied under its own name when the game starts.
    shutil.copy2(VR_BUILD / "quest_vr.so", dest / "libquest_vr.so")
    run([strip_tool(), "--strip-debug", dest / "libquest_vr.so"])


def stage_java() -> None:
    java = STAGE / "java"
    shutil.rmtree(java, ignore_errors=True)
    sources = sorted(PORT_BUILD.rglob("android-project/app/src/main/java/org/libsdl"))
    if not sources:
        sys.exit("the port's build has no SDL Java sources")
    shutil.copytree(sources[0], java / "org/libsdl")
    shutil.copytree(OPENRAC / "port/android/java/dev", java / "dev")


def stage_tools(python_home: Path) -> None:
    out = BUILD_ROOT / "bundle"
    shutil.rmtree(out, ignore_errors=True)
    run([sys.executable, OPENRAC / "tools/bundle_standalone.py", out, "--android", "--python", python_home],
        cwd=OPENRAC)
    assets = STAGE / "assets"
    assets.mkdir(parents=True, exist_ok=True)
    archive = assets / "openrac.zip"
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as z:
        for path in sorted(out.rglob("*")):
            if path.is_file():
                z.write(path, path.relative_to(out).as_posix())
    print(f"{archive}: {archive.stat().st_size / 1e6:.0f} MB", flush=True)


def stage_texts() -> None:
    assets = STAGE / "assets"
    assets.mkdir(parents=True, exist_ok=True)
    manifest = sorted((VR_BUILD / "package").rglob("openrac-mod.json"))
    if not manifest:
        sys.exit("the mod's build has no packaged manifest")
    shutil.copy2(manifest[0], assets / "openrac-mod.json")
    shutil.copy2(ROOT / "CREDITS.txt", assets / "credits.txt")
    with (assets / "third_party_notices.txt").open("w", encoding="utf8", newline="\n") as out:
        for source in (ROOT / "THIRD_PARTY_NOTICES.txt", OPENRAC / "THIRD_PARTY_NOTICES.md"):
            out.write(source.read_text(encoding="utf8"))
            out.write("\n\n")


def build_apk(release: bool) -> Path:
    gradle = ROOT / "android/gradlew.bat" if os.name == "nt" else ROOT / "android/gradlew"
    task = "assembleRelease" if release else "assembleDebug"
    # The launcher's tests first: they read what was just staged (the mod's settings among it).
    run([gradle, "--no-daemon", "testDebugUnitTest", task], cwd=ROOT / "android")
    kind = "release" if release else "debug"
    apk = ROOT / f"android/app/build/outputs/apk/{kind}/app-{kind}.apk"
    gradle_text = (ROOT / "android/app/build.gradle").read_text(encoding="utf8")
    version = re.search(r'versionName "([^"]+)"', gradle_text).group(1)
    out = ROOT / f"android/build/RatchetQuestVR-{version}.apk"
    shutil.copy2(apk, out)
    print(f"{out}: {out.stat().st_size / 1e6:.0f} MB", flush=True)
    return out


def verify_apk(apk: Path) -> None:
    """Opens the finished APK: everything the game needs must really be in it."""
    with zipfile.ZipFile(apk) as z:
        names = {info.filename: info for info in z.infolist()}
    wanted = [f"lib/arm64-v8a/lib{name}.so" for name in (
        "openrac-rac1-pal", "openrac-rac1-ntsc", "openrac-rac1-pal-cli", "openrac-rac1-ntsc-cli",
        "openrac-python", "python3.13", "SDL3", "c++_shared", "openxr_loader", "quest_vr")]
    wanted += [f"assets/{name}" for name in (
        "openrac.zip", "openrac-mod.json", "credits.txt", "controls.txt", "third_party_notices.txt")]
    missing = [name for name in wanted if name not in names]
    if missing:
        sys.exit(f"{apk.name} lacks: {missing}")
    # Programs run from the native library folder, so the libraries must be unpacked at install.
    stored = [n for n in wanted if n.startswith("lib/") and names[n].compress_type == zipfile.ZIP_STORED]
    build_tools = Path(os.environ["ANDROID_HOME"]) / "build-tools/34.0.0"
    aapt = next(iter(sorted(build_tools.glob("aapt2*"))))
    badging = subprocess.run([aapt, "dump", "badging", apk], capture_output=True, text=True).stdout
    tree = subprocess.run([aapt, "dump", "xmltree", apk, "--file", "AndroidManifest.xml"],
                          capture_output=True, text=True).stdout
    for needle in ("com.oculus.intent.category.VR", "org.khronos.openxr.intent.category.IMMERSIVE_HMD",
                   "com.oculus.vr.focusaware", "android.hardware.vr.headtracking",
                   "org.khronos.openxr.permission.OPENXR", ".QuestGameActivity"):
        if needle not in tree and needle not in badging:
            sys.exit(f"{apk.name}: the manifest lacks {needle}")
    if "extractNativeLibs" not in tree:
        sys.exit(f"{apk.name}: the manifest lacks extractNativeLibs")
    print(f"verified {apk.name}: {len(wanted)} files, {len(stored)} libraries stored uncompressed, manifest checked",
          flush=True)


def verify_signer(apk: Path) -> None:
    """A release is signed with the project's key (tools/release-cert.sha256): else it installs
    over no earlier release and the launcher's updater refuses it."""
    build_tools = Path(os.environ["ANDROID_HOME"]) / "build-tools/34.0.0"
    signer = next(iter(sorted(build_tools.glob("apksigner*"))))
    out = subprocess.run([signer, "verify", "--print-certs", apk], capture_output=True, text=True)
    found = re.search(r"certificate SHA-256 digest: ([0-9a-f]{64})", out.stdout)
    expected = (ROOT / "tools/release-cert.sha256").read_text(encoding="utf8").strip()
    if out.returncode != 0 or not found or found.group(1) != expected:
        sys.exit(f"{apk.name} is not signed with the release key (tools/release-cert.sha256); "
                 "set RACQUEST_SIGNING to its properties file")
    print(f"signed with the release key {expected[:16]}...", flush=True)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--no-port", action="store_true", help="use the port's last build")
    parser.add_argument("--no-apk", action="store_true", help="stop before Gradle")
    parser.add_argument("--release", action="store_true")
    args = parser.parse_args()

    check_openrac()
    python_target, python_home = openrac_android.python()
    openxr = openxr_loader()
    if not args.no_port:
        build_port(python_target)
    check_hooks()
    build_mod(openxr)
    stage_libraries(python_target, openxr)
    stage_java()
    stage_tools(python_home)
    stage_texts()
    if not args.no_apk:
        apk = build_apk(args.release)
        verify_apk(apk)
        if args.release:
            verify_signer(apk)
    return 0


if __name__ == "__main__":
    sys.exit(main())
