#!/bin/bash
# port_shot.sh TAG LEVEL [FRAMES] [EVERY]: the game alone, no mod loaded (--no-mods), started on a level and left to
# stand, a picture of its window every EVERY-th frame (the port's own OPENRAC_SHOT), into C:/RACQuest/pc/out/port-TAG/.
# What the port draws by itself is the reference the VR build's picture is held against (0.1.19: the city's plants
# went missing in 0.1.18 and no test compared the scenery with anything).
# QUEST_GAME: another build of the game. QUEST_INI: more lines for the settings file. QUEST_LEVELS: other level data.
set -u
TAG=$1
LEVEL=$2
FRAMES=${3:-1500}
EVERY=${4:-300}
GAME=${QUEST_GAME:-/c/RACQuest/RatchetQuestVR/third_party/OpenRAC/port/build/release/openrac-rac1-pal.exe}
DATA=C:/RACQuest/pc/data
LEVELS=${QUEST_LEVELS:-$DATA/port/pal}
OUT=C:/RACQuest/pc/out/port-$TAG
rm -rf "$OUT"
mkdir -p "$OUT"
cat > "$DATA/port-settings.ini" <<INI
vsync = off
frame_rate = 60
aspect = 4:3
hud = centre
menu_bar = off
show_fps = off
render_scale = 100
msaa = 2
texture_filter = normal
anisotropy = 4
skip_any_movie = true
volume = 0
${QUEST_INI:-}
INI
CARDS=C:/RACQuest/pc/cards/port
rm -rf "$CARDS"
mkdir -p "$CARDS"
OPENRAC_DIRECT=1 OPENRAC_SHOT="$EVERY:$OUT/f" timeout "${QUEST_TIMEOUT:-200}" "$GAME" --data "$DATA" --window \
  --levels "$LEVELS" --cards "$CARDS" --no-mods --settings "$DATA/port-settings.ini" --level "$LEVEL" \
  --frames "$FRAMES" > "$OUT/stdout.txt" 2>&1
echo "exit $?"
ls "$OUT" | grep -c png
