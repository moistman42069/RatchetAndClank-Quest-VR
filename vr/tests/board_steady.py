# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 the Ratchet & Clank Quest VR contributors
"""How steady RAC-Board rides under a still gaze in first person (0.1.26), from a run's stdout.txt with
QUEST_JET_TICK_TRACE=1 ("[vr-jetboard] steer at tick N: heading H, frame F, head G, stick turned T, stick X Y, turn U,
way W").

His path is W, the way of the board's velocity (or, in a line without it, F, the flat way from the game's camera to him,
which follows him and trails his path). For each stretch with the head held still and the stick pushed:
  - how many ticks his path took to come within 0.1 rad of the gaze (from the stretch's first tick);
  - from then, plus half a second: how far the path strays from the gaze, and the board's nose (its heading), most and
    root mean square, and the nose from the path (its carve), root mean square;
  - how many times a second the nose's turn changes way, and how many of those are sharp: the turn rate changing side by
    more than 2.1 rad/s (0.035 rad a tick) from one tick to the next, which alone swings the board's lean to the other side
    by more than a fifth of its turn frames, where Jak's board plays its "board-turn" sound (target_board_anim_trans).

board_steady.py <stdout.txt>
"""
import math
import re
import sys

LINE = re.compile(
    r"steer at tick (\d+): heading (-?[\d.]+), frame (-?[\d.]+), head (-?[\d.]+), stick turned (-?[\d.]+), stick (-?[\d.]+) (-?[\d.]+)"
    r"(?:, turn -?[\d.]+, way (-?[\d.]+))?"
)
TICKS_PER_SECOND = 60.0
SETTLE_TICKS = 30
COME_ROUND = 0.1
SHARP = 0.035


def wrap(a):
    return (a + math.pi) % (2.0 * math.pi) - math.pi


def rms(values):
    return math.sqrt(sum(v * v for v in values) / len(values))


def main(path):
    rows = []
    with open(path, "rb") as f:
        for raw in f:
            m = LINE.search(raw.decode("utf-8", "replace"))
            if m:
                tick, heading, frame, head, _, x, y, way = m.groups()
                # His path: the board's velocity's way where the line has it (0.1.26), else the camera's way to him.
                path = float(way) if way is not None else float(frame)
                rows.append((int(tick), float(heading), path, float(head), float(x), float(y)))
    if not rows:
        print("board steady: no steer lines (QUEST_JET_TICK_TRACE=1)")
        return
    # Stretches: the head the same (to a thousandth) and the stick pushed, ticks one after another.
    stretches, current = [], []
    for r in rows:
        held = math.hypot(r[4], r[5]) > 0.5
        if current and (not held or abs(wrap(r[3] - current[-1][3])) > 1e-3 or r[0] != current[-1][0] + 1):
            stretches.append(current)
            current = []
        if held:
            current.append(r)
    if current:
        stretches.append(current)
    for s in stretches:
        if len(s) < 60 + SETTLE_TICKS:
            continue
        met = next((i for i, r in enumerate(s) if abs(wrap(r[2] - r[3])) < COME_ROUND), None)
        if met is None or len(s) - met - SETTLE_TICKS < 60:
            when = "not" if met is None else f"in {met} ticks, too late to measure (a second at least after)"
            print(
                f"board steady: head {s[0][3]:.2f}, ticks {s[0][0]}-{s[-1][0]}: the path came within {COME_ROUND} of the gaze "
                f"{when}; at the end {wrap(s[-1][2] - s[-1][3]):+.3f}"
            )
            continue
        part = s[met + SETTLE_TICKS :]
        path_off = [wrap(r[2] - r[3]) for r in part]
        nose_off = [wrap(r[1] - r[3]) for r in part]
        carve = [wrap(r[1] - r[2]) for r in part]
        turns = [wrap(b[1] - a[1]) for a, b in zip(part, part[1:])]
        ways = [t for t in turns if abs(t) > 1e-4]
        changes = sum(1 for a, b in zip(ways, ways[1:]) if a * b < 0)
        sharp = sum(1 for a, b in zip(turns, turns[1:]) if a * b < 0 and abs(b - a) > SHARP)
        seconds = len(part) / TICKS_PER_SECOND
        print(
            f"board steady: head {part[0][3]:.2f}, ticks {part[0][0]}-{part[-1][0]}: the path came round in {met} ticks; "
            f"path off the gaze at most {max(abs(o) for o in path_off):.3f}, rms {rms(path_off):.3f}; nose at most "
            f"{max(abs(o) for o in nose_off):.3f}, rms {rms(nose_off):.3f} (from the path {rms(carve):.3f}); its turn changed way {changes / seconds:.1f} times a "
            f"second, sharply {sharp / seconds:.1f}"
        )


if __name__ == "__main__":
    main(sys.argv[1])
