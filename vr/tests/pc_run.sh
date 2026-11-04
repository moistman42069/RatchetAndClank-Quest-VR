#!/usr/bin/env bash
# The real game on this PC with the mod and the simulated headset: tests/<script>.sim, its shots in
# C:/RACQuest/pc/out/<script>. pc_run.sh <script> [extra game arguments]
set -u
NAME=$1; shift
TESTS=/c/RACQuest/RatchetQuestVR/vr/tests
DATA=C:/RACQuest/pc/data
GAME=/c/RACQuest/RatchetQuestVR/third_party/OpenRAC/port/build/release/openrac-rac1-pal.exe
OUT=C:/RACQuest/pc/out/$NAME
mkdir -p "$OUT" "$DATA/plugins/quest-vr/lib/windows-x86_64"
# QUEST_MOD_PACKAGE: another build of the mod, for a before and after.
MOD_PACKAGE=${QUEST_MOD_PACKAGE:-/c/RACQuest/b/vrpc/package/quest_vr}
cp "$MOD_PACKAGE/lib/windows-x86_64/quest_vr.dll" "$DATA/plugins/quest-vr/lib/windows-x86_64/"
cp "$MOD_PACKAGE/openrac-mod.json" "$DATA/plugins/quest-vr/"
# The bundled mods (tools/build_pc.py builds them) beside it, for a test that turns some on: QUEST_MODS is their
# plugins.json entries, ', {"id": "difficulty", "enabled": true, "settings": {...}}' each, after quest-vr's.
MODS_PC=/c/RACQuest/b/rac1-mods-pc/package
for pair in difficulty:difficulty item-randomizer:item_randomizer ammo-max:ammo_max xbox-icons:xbox_icons; do
  rm -rf "$DATA/plugins/${pair%%:*}"
  if [ -n "${QUEST_MODS:-}" ] && [ -d "$MODS_PC/${pair#*:}" ]; then
    cp -r "$MODS_PC/${pair#*:}" "$DATA/plugins/${pair%%:*}"
  fi
done
rm -rf "$DATA/plugins/button-swap"
if [ -n "${QUEST_MODS:-}" ]; then
  cp -r /c/RACQuest/upstream/rac1-mods/button-swap "$DATA/plugins/button-swap"
fi
cat > "$DATA/plugins/plugins.json" <<JSON
{"format": 1, "mods": [{"id": "quest-vr", "enabled": true, "settings": ${QUEST_SETTINGS:-{\}}}${QUEST_MODS:-}]}
JSON
cat > "$DATA/quest-settings.ini" <<INI
vsync = off
frame_rate = display
aspect = 4:3
hud = centre
menu_bar = off
show_fps = off
render_scale = 100
msaa = ${QUEST_MSAA:-2}
texture_filter = normal
anisotropy = 4
original_lod = ${QUEST_LOD:-false}
max_draw_distance = ${QUEST_MAX_DRAW:-true}
matching_shadow = ${QUEST_MATCHING_SHADOW:-true}
fades_in_view = ${QUEST_FADES_IN_VIEW:-true}
smooth_plant_edges = ${QUEST_SMOOTH_PLANTS:-true}
skip_any_movie = ${QUEST_SKIP_ANY_MOVIE:-true}
volume = ${QUEST_VOLUME:-100}
INI
export QUEST_SIM_SCRIPT="C:/RACQuest/RatchetQuestVR/vr/tests/$NAME.sim" QUEST_SIM_OUT="$OUT" QUEST_VR_STATUS="$OUT/status.txt"
# VR Options start at their defaults each run: the file a run saves beside its status is not the next run's. A test
# that needs others names them: QUEST_VR_OPTIONS="body 2;camera_flip 1" (vr-options.txt's lines, ; between them).
rm -f "$OUT/vr-options.txt"
if [ -n "${QUEST_VR_OPTIONS:-}" ]; then
  printf '%s\n' "$QUEST_VR_OPTIONS" | tr ';' '\n' > "$OUT/vr-options.txt"
fi
# A fresh empty memory card each run, never the PC's own (%LOCALAPPDATA%/openrac/memcard, the game's default);
# a test that needs a card of its own passes --cards after the script's name, which comes later and wins.
CARDS=C:/RACQuest/pc/cards/run
rm -rf "$CARDS"
mkdir -p "$CARDS"
timeout "${QUEST_TIMEOUT:-300}" "$GAME" --data "$DATA" --window --levels "$DATA/port/pal" --cards "$CARDS" \
  --plugins "$DATA/plugins/plugins.json" --settings "$DATA/quest-settings.ini" "$@" > "$OUT/stdout.txt" 2>&1
echo "exit $?"
if grep -a "not loaded" "$OUT/stdout.txt"; then
  echo "THE MOD DID NOT LOAD: this run tested nothing"
fi
ls "$OUT" | head -30
