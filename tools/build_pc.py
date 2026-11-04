#!/usr/bin/env python3
"""
Builds what the PC test runs (vr/tests/pc_run.sh): OpenRAC's game for Windows, and the VR mod for
Windows with its simulated headset (vr/src/xr_sim.cpp). Nothing of it goes in the APK.

  python tools/build_pc.py [--no-game]

Needs Visual Studio's C++ tools, LLVM (clang-cl) in C:\\Program Files\\LLVM, CMake and Ninja.
"""
import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OPENRAC = ROOT / "third_party/OpenRAC"
BUILD_ROOT = Path(os.environ.get("QUEST_BUILD_ROOT", "C:/RACQuest/b"))
LLVM = Path(os.environ.get("LLVM_BIN", "C:/Program Files/LLVM/bin"))

sys.path.insert(0, str(OPENRAC / "tools"))
import build_port as openrac_port  # noqa: E402  (OpenRAC's own PC build and its Visual Studio lookup)


def run(command, cwd=None, env=None):
    print("$", " ".join(str(c) for c in command), flush=True)
    subprocess.run([str(c) for c in command], cwd=cwd, env=env, check=True)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--no-game", action="store_true", help="the mod only")
    args = parser.parse_args()
    if not args.no_game:
        # Port/build/release/openrac-rac1-pal.exe, in OpenRAC's tree as its own launcher expects.
        run([sys.executable, "tools/build_port.py", "rac1-pal"], cwd=OPENRAC)
    env = openrac_port.msvc_environment(openrac_port.find_visual_studio())
    env["PATH"] = str(LLVM) + os.pathsep + env.get("PATH", env.get("Path", ""))
    out = BUILD_ROOT / "vrpc"
    run(["cmake", "-S", ROOT / "vr", "-B", out, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=RelWithDebInfo",
         "-DCMAKE_C_COMPILER=clang-cl", "-DCMAKE_CXX_COMPILER=clang-cl",
         f"-DCMAKE_MAKE_PROGRAM={shutil.which('ninja')}", "-Wno-dev"], env=env)
    run(["cmake", "--build", out], env=env)
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
    return 0


if __name__ == "__main__":
    sys.exit(main())
