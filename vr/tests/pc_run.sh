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
cp /c/RACQuest/b/vrpc/package/quest_vr/lib/windows-x86_64/quest_vr.dll "$DATA/plugins/quest-vr/lib/windows-x86_64/"
cp /c/RACQuest/b/vrpc/package/quest_vr/openrac-mod.json "$DATA/plugins/quest-vr/"
cat > "$DATA/plugins/plugins.json" <<JSON
{"format": 1, "mods": [{"id": "quest-vr", "enabled": true, "settings": ${QUEST_SETTINGS:-{\}}}]}
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
original_lod = true
INI
export QUEST_SIM_SCRIPT="C:/RACQuest/RatchetQuestVR/vr/tests/$NAME.sim" QUEST_SIM_OUT="$OUT" QUEST_VR_STATUS="$OUT/status.txt"
timeout "${QUEST_TIMEOUT:-300}" "$GAME" --data "$DATA" --window --levels "$DATA/port/pal" \
  --plugins "$DATA/plugins/plugins.json" --settings "$DATA/quest-settings.ini" "$@" > "$OUT/stdout.txt" 2>&1
echo "exit $?"
ls "$OUT" | head -30
