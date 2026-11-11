"""scenery_compare.py SHOTS OUT LEVEL...: for each level, port-ref-LEVEL/fN.png beside port-now-LEVEL/fN.png on a sheet
OUT/level-LEVEL.jpg, with the mean difference and the share of pixels more than 40 apart: the frames from 1200 on that
both builds took (the port skips a shot now and then while a level streams in, the same one in both builds, so a fixed
list left most levels a frame short). A level is flagged when its frames differ by more than FLAG percent (env,
default 6), or fewer than two frames are in both: the hero and the enemies move a little differently between builds,
the scenery should not."""
import os
import sys
from pathlib import Path

from PIL import Image, ImageChops

shots, out = Path(sys.argv[1]), Path(sys.argv[2])
flag = float(os.environ.get("FLAG", "6"))
FIRST = 1200
W = 640
flagged = []
for level in sys.argv[3:]:
    rows, notes = [], []
    worst = 0.0
    took = [{int(p.stem[1:]) for p in (shots / f"port-{build}-{level}").glob("f*.png") if p.stem[1:].isdigit()}
            for build in ("ref", "now")]
    frames = sorted(f for f in took[0] & took[1] if f >= FIRST)
    for frame in sorted((took[0] ^ took[1]) - {f for f in took[0] | took[1] if f < FIRST}):
        notes.append(f"f{frame} in one build only")
    for frame in frames:
        pair = [shots / f"port-{build}-{level}" / f"f{frame}.png" for build in ("ref", "now")]
        a, b = (Image.open(p).convert("RGB") for p in pair)
        if a.size != b.size:
            b = b.resize(a.size)
        hist = ImageChops.difference(a, b).convert("L").histogram()
        total = sum(hist)
        mean = sum(i * c for i, c in enumerate(hist)) / total
        far = 100 * sum(hist[41:]) / total
        worst = max(worst, far)
        notes.append(f"f{frame} mean {mean:4.1f} far {far:4.1f}%")
        rows.append([im.resize((W, int(im.height * W / im.width))) for im in (a, b)])
    if rows:
        h = rows[0][0].height
        sheet = Image.new("RGB", (2 * W + 4, len(rows) * (h + 4)))
        for r, row in enumerate(rows):
            for c, im in enumerate(row):
                sheet.paste(im, (c * (W + 4), r * (h + 4)))
        sheet.save(out / f"level-{level}.jpg", quality=80)
    mark = "  <-- look" if worst > flag or len(rows) < 2 else ""
    if mark:
        flagged.append(level)
    print(f"level {level:>2}: " + ", ".join(notes) + mark)
print("flagged: " + (" ".join(flagged) if flagged else "none"))
