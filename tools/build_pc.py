#!/usr/bin/env python3
"""
Builds the PC side: OpenRAC's game for Windows, the VR mod for Windows (OpenXR for PC VR, and the
simulated headset vr/tests/pc_run.sh drives, vr/src/xr_sim.cpp), OpenRAC's mods for Windows. With
--package, the PC release too: the launcher (pc/launcher, the APK's launcher for a PC) as
RatchetClankVR.exe with its own Java runtime, the game for both discs, OpenRAC's set-up tools with
their Python, the VR mod and the bundled mods, zipped as pc/build/RatchetQuestVR-<version>-windows.zip.

  python tools/build_pc.py [--no-game] [--package]

Needs Visual Studio's C++ tools, LLVM (clang-cl) in C:\\Program Files\\LLVM, CMake and Ninja; for
--package, the JDK of env.sh (JAVA_HOME: javac, jlink, jpackage).
"""
import argparse
import hashlib
import os
import re
import shutil
import stat
import subprocess
import sys
import tarfile
import urllib.request
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OPENRAC = ROOT / "third_party/OpenRAC"
BUILD_ROOT = Path(os.environ.get("QUEST_BUILD_ROOT", "C:/RACQuest/b"))
LLVM = Path(os.environ.get("LLVM_BIN", "C:/Program Files/LLVM/bin"))

sys.path.insert(0, str(OPENRAC / "tools"))
import build_port as openrac_port  # noqa: E402  (OpenRAC's own PC build and its Visual Studio lookup)


# The Khronos OpenXR loader for the PC VR build (0.1.14): the release's headers, its 64-bit loader and its
# license, checked against the SHA-256 the Halo CE Quest VR port's Windows build pins for the same file.
OPENXR_PC_VERSION = "1.1.63"
OPENXR_PC_URL = ("https://github.com/KhronosGroup/OpenXR-SDK-Source/releases/download/"
                 f"release-{OPENXR_PC_VERSION}/openxr_loader_windows-{OPENXR_PC_VERSION}.zip")
OPENXR_PC_SHA256 = "01c631aeabbfe0879540f77ef833416c532a20746285b494630160c23588b771"
OPENXR_PC_DIR = BUILD_ROOT / f"openxr-windows-{OPENXR_PC_VERSION}"
OPENXR_PC_KEEP = ("include/openxr/", "x64/bin/openxr_loader.dll", "share/doc/openxr/LICENSE")


def openxr_windows() -> Path:
    """The loader package once: kept under BUILD_ROOT, the zip itself not."""
    if (OPENXR_PC_DIR / "x64/bin/openxr_loader.dll").is_file():
        return OPENXR_PC_DIR
    archive = BUILD_ROOT / f"openxr_loader_windows-{OPENXR_PC_VERSION}.zip"
    print("downloading", OPENXR_PC_URL, flush=True)
    with urllib.request.urlopen(OPENXR_PC_URL) as response, open(archive, "wb") as f:
        shutil.copyfileobj(response, f)
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    if digest != OPENXR_PC_SHA256:
        archive.unlink()
        sys.exit(f"the OpenXR loader package's SHA-256 is {digest}, not {OPENXR_PC_SHA256}")
    with zipfile.ZipFile(archive) as z:
        for member in z.namelist():
            if member.startswith(OPENXR_PC_KEEP) and not member.endswith("/"):
                target = OPENXR_PC_DIR / member
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(z.read(member))
    archive.unlink()
    if not (OPENXR_PC_DIR / "x64/bin/openxr_loader.dll").is_file():
        sys.exit(f"{OPENXR_PC_URL} has no x64/bin/openxr_loader.dll")
    return OPENXR_PC_DIR

def run(command, cwd=None, env=None):
    print("$", " ".join(str(c) for c in command), flush=True)
    subprocess.run([str(c) for c in command], cwd=cwd, env=env, check=True)


# ---- The PC release (--package) ----

# The Python the set-up's tools run with: the very archive OpenRAC's own Windows launcher carries
# (third_party/OpenRAC/.github/workflows/launcher.yml: python-build-standalone, its release, version and SHA-256).
PYTHON_RELEASE = "20261009"
PYTHON_VERSION = "3.12.15"
PYTHON_FILE = f"cpython-{PYTHON_VERSION}+{PYTHON_RELEASE}-x86_64-pc-windows-msvc-install_only_stripped.tar.gz"
PYTHON_URL = f"https://github.com/astral-sh/python-build-standalone/releases/download/{PYTHON_RELEASE}/{PYTHON_FILE}"
PYTHON_SHA256 = "11ed9f8a6ac64fb1b5a55e40e305a6744a3f77a70863c8f8197a6e4349abc64a"
PYTHON_DIR = BUILD_ROOT / f"python-windows-{PYTHON_VERSION}+{PYTHON_RELEASE}"

PC = ROOT / "pc"
PACKAGE_BUILD = BUILD_ROOT / "pc-package"
# The folder in the zip: unzipped over an earlier release, it lands on it.
PACKAGE_NAME = "RatchetClankVR"
EXE = "RatchetClankVR"
GAMES = ("rac1-pal", "rac1-ntsc")
# The Java modules the launcher uses: Swing (java.desktop), and TLS's elliptic curves for GitHub (jdk.crypto.ec).
JAVA_MODULES = "java.base,java.desktop,jdk.crypto.ec"
# The launcher's sources: the APK's own option table, discs and update rules, then the PC's.
SHARED_JAVA = ("Options.java", "Discs.java", "UpdatePolicy.java")
ANDROID_JAVA = ROOT / "android/app/src/main/java/dev/ratchetquest/vr"


def python_windows() -> Path:
    """python-build-standalone for Windows once, checked against the SHA-256 OpenRAC pins: its python/ folder."""
    home = PYTHON_DIR / "python"
    if (home / "python.exe").is_file():
        return home
    archive = BUILD_ROOT / PYTHON_FILE
    print("downloading", PYTHON_URL, flush=True)
    with urllib.request.urlopen(PYTHON_URL) as response, open(archive, "wb") as f:
        shutil.copyfileobj(response, f)
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    if digest != PYTHON_SHA256:
        archive.unlink()
        sys.exit(f"the Python archive's SHA-256 is {digest}, not {PYTHON_SHA256}")
    shutil.rmtree(PYTHON_DIR, ignore_errors=True)
    with tarfile.open(archive) as tar:
        tar.extractall(PYTHON_DIR, filter="data")
    archive.unlink()
    if not (home / "python.exe").is_file():
        sys.exit(f"{PYTHON_FILE} has no python/python.exe")
    return home


def version() -> str:
    text = (ROOT / "android/app/build.gradle").read_text(encoding="utf8")
    return re.search(r'versionName "([^"]+)"', text).group(1)


def level_format() -> int:
    text = (OPENRAC / "editor/port.py").read_text(encoding="utf8")
    return int(re.search(r"^FORMAT = (\d+)$", text, re.MULTILINE).group(1))


def java_tool(name: str) -> Path:
    home = os.environ.get("JAVA_HOME")
    tool = Path(home) / "bin" / f"{name}.exe" if home else None
    if not tool or not tool.is_file():
        sys.exit(f"JAVA_HOME has no {name}: source env.sh first")
    return tool


def build_launcher(out: Path, ver: str) -> Path:
    """The launcher's jar, with PcBuild (the version, the level format) written for it, and its icon."""
    shutil.rmtree(out, ignore_errors=True)
    generated = out / "gen/dev/ratchetquest/vr"
    generated.mkdir(parents=True)
    (generated / "PcBuild.java").write_text(
        "// Written by tools/build_pc.py --package.\npackage dev.ratchetquest.vr;\n\n"
        f"final class PcBuild {{\n    static final String VERSION = \"{ver}\";\n"
        f"    static final int LEVEL_FORMAT = {level_format()};\n\n    private PcBuild() {{}}\n}}\n",
        encoding="utf8", newline="\n")
    sources = [ANDROID_JAVA / name for name in SHARED_JAVA]
    sources += sorted((PC / "launcher/src").rglob("*.java")) + [generated / "PcBuild.java"]
    classes = out / "classes"
    run([java_tool("javac"), "--release", "17", "-encoding", "UTF-8", "-Xlint:all", "-Xlint:-serial", "-Werror",
         "-d", classes, *sources])
    jar = out / "input/launcher.jar"
    jar.parent.mkdir(parents=True)
    with zipfile.ZipFile(jar, "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr("META-INF/MANIFEST.MF", "Manifest-Version: 1.0\r\nMain-Class: dev.ratchetquest.vr.PcLauncher\r\n\r\n")
        for path in sorted(classes.rglob("*.class")):
            z.write(path, path.relative_to(classes).as_posix())
    run([java_tool("java"), "-cp", classes, "dev.ratchetquest.vr.PcLauncher", "--write-icon", out / "bolt.ico"])
    return jar


def vc_runtime() -> list[Path]:
    """The Visual C++ runtime the game and the mods were built against (vcvars64's redistributable folder)."""
    env = openrac_port.msvc_environment(openrac_port.find_visual_studio())
    redist = Path(env.get("VCTOOLSREDISTDIR", ""))  # parse_environment upper-cases the names
    found = sorted(redist.glob("x64/Microsoft.VC*.CRT"))
    if not found:
        sys.exit(f"Visual Studio's redistributable folder ({redist}) has no x64 C runtime")
    dlls = sorted(p for p in found[-1].glob("*.dll") if p.name.startswith(("msvcp140", "vcruntime140", "concrt140")))
    for wanted in ("msvcp140.dll", "msvcp140_2.dll", "vcruntime140.dll", "vcruntime140_1.dll"):
        if not any(p.name.lower() == wanted for p in dlls):
            sys.exit(f"{found[-1]} has no {wanted}")
    return dlls


def remove_tree(path: Path) -> None:
    """A folder and all in it, its read-only files too (jpackage leaves the exe read-only)."""
    def writable(function, name, _error):
        os.chmod(name, stat.S_IWRITE)
        function(name)
    if path.exists():
        shutil.rmtree(path, onexc=writable)


def package(ver: str) -> Path:
    """The PC release's folder and zip."""
    build_quest_module = sys.modules["build_quest"]
    for game in GAMES:
        if not (OPENRAC / f"port/build/release/openrac-{game}.exe").is_file():
            sys.exit(f"the game for {game} was not built (tools/build_port.py {game})")
    work = PACKAGE_BUILD
    remove_tree(work)
    work.mkdir(parents=True)
    jar = build_launcher(work / "launcher", ver)
    runtime = work / "runtime"
    run([java_tool("jlink"), "--add-modules", JAVA_MODULES, "--strip-debug", "--no-header-files", "--no-man-pages",
         "--compress=2", "--output", runtime])
    run([java_tool("jpackage"), "--type", "app-image", "--name", EXE, "--input", jar.parent, "--main-jar", jar.name,
         "--main-class", "dev.ratchetquest.vr.PcLauncher", "--runtime-image", runtime,
         "--icon", work / "launcher/bolt.ico", "--app-version", ver,
         "--vendor", "Ratchet & Clank Quest VR contributors", "--description", "Ratchet & Clank VR",
         "--copyright", "GPL-3.0-or-later", "--dest", work / "image"])
    folder = work / "image" / EXE
    if not (folder / f"{EXE}.exe").is_file():
        sys.exit(f"jpackage made no {EXE}.exe")

    # OpenRAC's copy: its tools and editor, the games for both discs with the C runtime beside them, the Python.
    openrac = folder / "openrac"
    run([sys.executable, OPENRAC / "tools/bundle_standalone.py", openrac, "--game-build", OPENRAC / "port/build/release",
         "--python", python_windows()], cwd=OPENRAC)
    games = openrac / "port/build/release"
    for test in games.glob("mod_loader_test*.dll"):
        test.unlink()
    for dll in vc_runtime():
        shutil.copy2(dll, games / dll.name)

    # The VR mod as its build packaged it, with the OpenXR loader.
    shutil.copytree(BUILD_ROOT / "vrpc/package/quest_vr", folder / "vr")
    for needed in ("lib/windows-x86_64/quest_vr.dll", "lib/windows-x86_64/openxr_loader.dll", "openrac-mod.json"):
        if not (folder / "vr" / needed).is_file():
            sys.exit(f"the VR mod's package has no {needed}")

    # The bundled mods, each folder named by its id, as the APK's assets/mods.
    mods = folder / "mods"
    for mod_id, target in build_quest_module.BUNDLED_MODS.items():
        packaged = BUILD_ROOT / "rac1-mods-pc/package" / target
        if not (packaged / f"lib/windows-x86_64/{target}.dll").is_file():
            sys.exit(f"{packaged} has no Windows build")
        shutil.copytree(packaged, mods / mod_id)
    for mod_id, files in build_quest_module.BUNDLED_SCRIPT_MODS.items():
        (mods / mod_id).mkdir(parents=True)
        for file in files:
            shutil.copy2(build_quest_module.RAC1_MODS / mod_id / file, mods / mod_id / file)

    # The launcher's texts: the APK's, and the PC's own.
    texts = folder / "texts"
    texts.mkdir()
    assets = ROOT / "android/app/src/main/assets"
    for name in ("whats_new.txt", "controls.txt"):
        shutil.copy2(assets / name, texts / name)
    shutil.copy2(PC / "launcher/texts/pc_controls.txt", texts / "pc_controls.txt")
    shutil.copy2(ROOT / "CREDITS.txt", texts / "credits.txt")
    with (texts / "third_party_notices.txt").open("w", encoding="utf8", newline="\n") as out:
        for source in (PC / "launcher/texts/third_party_notices_pc.txt", ROOT / "THIRD_PARTY_NOTICES.txt",
                       OPENRAC / "THIRD_PARTY_NOTICES.md"):
            out.write(source.read_text(encoding="utf8"))
            out.write("\n\n")
    licenses = folder / "licenses"
    licenses.mkdir()
    shutil.copy2(OPENXR_PC_DIR / "share/doc/openxr/LICENSE", licenses / "openxr-loader-LICENSE.txt")
    shutil.copy2(ROOT / "LICENSE", folder / "LICENSE.txt")
    shutil.copy2(PC / "README.txt", folder / "README.txt")
    # Nothing read-only (jpackage's exe is): an update's robocopy and the player's own unzip write over all of it.
    for path in folder.rglob("*"):
        if path.is_file():
            os.chmod(path, stat.S_IREAD | stat.S_IWRITE)

    # The launcher's own checks on what was just put together.
    env = dict(os.environ)
    env.pop("RATCHETQUESTVR_HOME", None)
    env["RATCHETQUESTVR_INSTALL"] = str(folder)
    run([runtime / "bin/java.exe", "-cp", folder / "app" / jar.name, "dev.ratchetquest.vr.PcLauncher",
         "--self-test"], env=env)

    out = PC / f"build/RatchetQuestVR-{ver}-windows.zip"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.unlink(missing_ok=True)
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for path in sorted(folder.rglob("*")):
            if path.is_file():
                z.write(path, f"{PACKAGE_NAME}/{path.relative_to(folder).as_posix()}")
    digest = hashlib.sha256(out.read_bytes()).hexdigest()
    print(f"{out}: {out.stat().st_size / 1e6:.0f} MB, SHA-256 {digest}", flush=True)
    return out


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--no-game", action="store_true", help="the mod only")
    parser.add_argument("--package", action="store_true", help="the PC release's zip too")
    args = parser.parse_args()
    if not args.no_game:
        # Port/build/release/openrac-rac1-<version>.exe, in OpenRAC's tree as its own launcher expects; the release
        # carries both discs' games, as the APK does.
        for game in GAMES if args.package else GAMES[:1]:
            run([sys.executable, "tools/build_port.py", game], cwd=OPENRAC)
    env = openrac_port.msvc_environment(openrac_port.find_visual_studio())
    env["PATH"] = str(LLVM) + os.pathsep + env.get("PATH", env.get("Path", ""))
    out = BUILD_ROOT / "vrpc"
    openxr = openxr_windows()
    run(["cmake", "-S", ROOT / "vr", "-B", out, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=RelWithDebInfo",
         "-DCMAKE_C_COMPILER=clang-cl", "-DCMAKE_CXX_COMPILER=clang-cl",
         f"-DCMAKE_MAKE_PROGRAM={shutil.which('ninja')}", f"-DOPENXR_PC_DIR={openxr.as_posix()}", "-Wno-dev"], env=env)
    run(["cmake", "--build", out], env=env)
    # The loader beside the mod, where xr_openxr.cpp looks for it first (the PC release ships it there).
    shutil.copy2(openxr / "x64/bin/openxr_loader.dll", out / "package/quest_vr/lib/windows-x86_64/openxr_loader.dll")
    # The rig's, the pad's and the hands' math (vr/tests/rig_pad_test.cpp), each build.
    test = out / "rig_pad_test.exe"
    run([LLVM / "clang-cl.exe", "/nologo", "/std:c++20", "/EHsc", "/O1", f"/I{ROOT / 'vr/src'}",
         ROOT / "vr/tests/rig_pad_test.cpp", f"/Fe{test}", f"/Fo{out / 'rig_pad_test.obj'}"], env=env)
    run([test], env=env)
    print(f"{out / 'package/quest_vr'}: the mod for vr/tests/pc_run.sh", flush=True)
    # The mods the APK bundles (build_quest.py BUNDLED_MODS), for pc_run.sh's QUEST_MODS.
    sys.path.insert(0, str(ROOT / "tools"))
    import build_quest  # noqa: E402
    build_quest.check_rac1_mods()
    mods = BUILD_ROOT / "rac1-mods-pc"
    run(["cmake", "-S", build_quest.RAC1_MODS, "-B", mods, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=RelWithDebInfo",
         "-DCMAKE_C_COMPILER=clang-cl", "-DCMAKE_CXX_COMPILER=clang-cl", f"-DCMAKE_MAKE_PROGRAM={shutil.which('ninja')}",
         f"-DOPENRAC_SDK={(OPENRAC / 'sdk').as_posix()}", "-DBUILD_TESTING=OFF", "-Wno-dev"], env=env)
    run(["cmake", "--build", mods, "--target", *build_quest.BUNDLED_MODS.values()], env=env)
    print(f"{mods / 'package'}: the bundled mods for vr/tests/pc_run.sh", flush=True)
    if args.package:
        package(version())
    return 0


if __name__ == "__main__":
    sys.exit(main())
