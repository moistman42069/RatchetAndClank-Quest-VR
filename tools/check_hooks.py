#!/usr/bin/env python3
"""
Every hook point the VR mod takes is on every disc with the signature the SDK gives it, or no build. A disc
whose table has another signature, or none, refuses the whole mod at load and the headset shows black: 0.1.5
on the US disc, whose C had Hud_Frame's flags unsigned.

  python tools/check_hooks.py hooks.c [hooks.c ...]

Each hooks.c is a disc's generated table (<port build>/games/<game>/gen/hooks.c). The hook points taken are
the SDK's (sdk/include/openrac/games/rac1.hpp) that the mod's sources name, directly or through its
templates, and those its manifest lists.
"""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SDK = ROOT / "third_party/OpenRAC/sdk/include/openrac/games/rac1.hpp"
MANIFEST = ROOT / "vr/openrac-mod.json"
SOURCES = ROOT / "vr/src"

STRUCT = re.compile(r"^struct (\w+) \{\n(.*?)^\};", re.M | re.S)
FIELD = re.compile(r'static constexpr const char\* (id|name|signature) = "([^"]+)";')
# {"Hud_Frame", "func_L00_0023BAB8", "void(u32,i32,i32,i32,i32,i32)", &slot, original},
ROW = re.compile(r'^\s*\{"(\w+)", "[^"]+", "([^"]+)",', re.M)


def sdk_points() -> dict[str, dict[str, str]]:
    """name -> its id and signature, for each hook point the SDK declares."""
    out = {}
    for m in STRUCT.finditer(SDK.read_text(encoding="utf-8")):
        fields = dict(FIELD.findall(m.group(2)))
        if {"id", "name", "signature"} <= fields.keys():
            out[fields["name"]] = fields
    return out


def taken(points: dict[str, dict[str, str]], problems: list[str]) -> set[str]:
    names = set()
    for source in sorted(SOURCES.iterdir()):
        if source.suffix in (".cpp", ".h"):
            names |= {n for n in re.findall(r"\brac1::(\w+)", source.read_text(encoding="utf-8")) if n in points}
    by_id = {p["id"]: name for name, p in points.items()}
    for hook_id in json.loads(MANIFEST.read_text(encoding="utf-8")).get("hooks", []):
        if hook_id in by_id:
            names.add(by_id[hook_id])
        else:
            problems.append(f"{MANIFEST.name} lists {hook_id}, which the SDK has no hook point for")
    return names


def main(tables: list[str]) -> int:
    if not tables:
        sys.exit(__doc__)
    problems: list[str] = []
    points = sdk_points()
    if len(points) < 100:
        sys.exit(f"check_hooks: read only {len(points)} hook points from {SDK}: its format changed")
    names = taken(points, problems)
    for table in map(Path, tables):
        disc = table.parent.parent.name
        rows = dict(ROW.findall(table.read_text(encoding="utf-8")))
        if len(rows) < 100:
            problems.append(f"{table}: only {len(rows)} hook points read: its format changed")
            continue
        for name in sorted(names):
            want = points[name]["signature"]
            if name not in rows:
                problems.append(f"{disc}: no hook point {name}")
            elif rows[name] != want:
                problems.append(f"{disc}: {name} is {rows[name]} here, the SDK's (and the mod's) {want}")
    if problems:
        print("check_hooks: the mod would not load:", *problems, sep="\n  ", file=sys.stderr)
        return 1
    print(f"check_hooks: the {len(names)} hook points the mod takes have the SDK's signature on "
          f"{', '.join(Path(t).parent.parent.name for t in tables)}", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
