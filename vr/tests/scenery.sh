#!/bin/bash
# scenery.sh [LEVELS...]: the scenery check. 0.1.18 shipped with the leaves of every plant missing (a shader value our
# patch set for skinned meshes alone) and no test compared the world with a build known to draw it. This runs the game
# alone (no mod, port_shot.sh) on each level twice, with a reference build (REF_GAME, by default 0.1.17 unpacked in
# C:/RACQuest/old117) and with this one, then scenery_compare.py puts the pictures side by side and flags a level whose
# picture moved further than the game logic explains. Look at the sheets: a flag is a question, not an answer.
# Output: C:/RACQuest/logs/scenery/ (sheets, scenery.txt). Levels 0 to 18 when none are named.
set -u
REF_GAME=${REF_GAME:-/c/RACQuest/old117/RatchetClankVR/openrac/port/build/release/openrac-rac1-pal.exe}
OUT=/c/RACQuest/logs/scenery
mkdir -p "$OUT"
LEVELS=("$@")
if [ ${#LEVELS[@]} -eq 0 ]; then
  LEVELS=(0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18)
fi
HERE=$(cd "$(dirname "$0")" && pwd)
for level in "${LEVELS[@]}"; do
  QUEST_GAME="$REF_GAME" bash "$HERE/port_shot.sh" "ref-$level" "$level" 3100 600 > "$OUT/ref-$level.run" 2>&1
  bash "$HERE/port_shot.sh" "now-$level" "$level" 3100 600 > "$OUT/now-$level.run" 2>&1
  echo "level $level: ref $(head -1 "$OUT/ref-$level.run"), now $(head -1 "$OUT/now-$level.run")"
done
python "$HERE/scenery_compare.py" C:/RACQuest/pc/out "$OUT" "${LEVELS[@]}" | tee "$OUT/scenery.txt"
