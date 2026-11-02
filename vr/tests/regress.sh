#!/usr/bin/env bash
# The regression: every sim in vr/tests with its own settings, one after another. Each run's summary goes to
# C:/RACQuest/logs/regress/<name>.txt; shots are deleted unless KEEP=1. It takes about two hours and needs a PAL .iso set up.
# regress.sh [names...]: only those.
set -u
cd /c/RACQuest/RatchetQuestVR
OUTS=/c/RACQuest/logs/regress
mkdir -p "$OUTS"
ONLY=" ${*:-} "
run() { # name sim level|- [NAME=value...] [the sim's own arguments, such as --cards <folder>]
  local name=$1 sim=$2 level=$3; shift 3
  if [ "$ONLY" != "  " ] && [[ "$ONLY" != *" $name "* ]]; then return; fi
  local start=$SECONDS
  local envs=() extra=() a keep=${KEEP:-}
  for a in "$@"; do
    if [ "$a" = KEEP=1 ]; then keep=1; continue; fi  # this entry's shots are to be looked at
    if [[ "$a" =~ ^[A-Za-z_][A-Za-z0-9_]*= ]]; then envs+=("$a"); else extra+=("$a"); fi
  done
  if [ "$level" = "-" ]; then
    env OPENRAC_DIRECT=1 QUEST_TIMEOUT=480 "${envs[@]}" bash vr/tests/pc_run.sh "$sim" "${extra[@]}" > "$OUTS/$name.run" 2>&1
  else
    env OPENRAC_DIRECT=1 QUEST_TIMEOUT=480 "${envs[@]}" bash vr/tests/pc_run.sh "$sim" --level "$level" "${extra[@]}" > "$OUTS/$name.run" 2>&1
  fi
  local log=/c/RACQuest/pc/out/$sim/stdout.txt
  {
    echo "== $name ($sim, level $level, $*) in $((SECONDS - start)) s: $(head -1 "$OUTS/$name.run")"
    grep -a -c 'not loaded' "$log" | sed 's/^/mod not loaded lines: /'
    grep -a -i -E 'crash|exception|assert|fatal' "$log" | grep -a -v 'settings from' | head -3
    grep -a -E '\[(vr-[a-z]+|sim)\]' "$log" | grep -a -v -E 'vr-perf|vr-state] (tick|phase)|vr-input|vr-glyphs|vr-body] hidden|sim] hittable|vr-hand] class|vr-aim] near the head|vr-aim] class .* (also holds|aimed by)|\[sim\] (frame|shot|pointer:)|haptic|vr-sound|centred text|not a list' \
      | sed 's/.*\[mod quest-vr\] //' | cut -c1-170 | awk '!seen[$0]++ || /^\[sim\] hero state/' | head -150
  } > "$OUTS/$name.txt"
  [ -n "$keep" ] || rm -f /c/RACQuest/pc/out/"$sim"/*.png
  echo "$name done in $((SECONDS - start)) s"
}
# The menu sims aim by fixed hand angles: the panel stays at its whole distance for them. worldmenuwall sets 0.
export QUEST_PANEL_FAR=1
G=QUEST_SIM_GIVE; H=QUEST_SIM_HOLD; N=QUEST_SIM_NEAR; A=QUEST_SIM_NEAR_AT; L=QUEST_SIM_LEFT
# Hands, aim, weapons
run aim aim 1 $G=15,16,21,9,11,19,23,13,14 $H=23
run aim-pyro aim 1 $G=16 $H=16
run aim-tesla aim 1 $G=19 $H=19
run aim-morph aim 1 $G=21 $H=21
run aim-suck aim 1 $G=9 $H=9
# 0.1.8: the sim's grip 40 degrees above its aim, as on a Quest: pointed level, the guns fire level ([vr-aim] z 0)
run aim-tilt aim 1 $G=15,16 $H=15 QUEST_SIM_GRIP_TILT=40
run aim-tilt-pyro aim 1 $G=16 $H=16 QUEST_SIM_GRIP_TILT=40
run aim-sizes aim 1 $G=15,10 $H=15 'QUEST_VR_OPTIONS=gun_size 50;hand_size 60' KEEP=1
run akimbo-blaster akimbo 1 $G=15,16 $H=16 $L=15
run akimbo-taunter akimbo 1 $G=15,14 $H=15 $L=14
run akimbo-suck akimbo 1 $G=15,9 $H=15 $L=9
run akimbo2 akimbo2 1 $G=15,21 $H=21 $L=21
run akimboglove akimboglove 1 $G=15,10 $H=15 $L=10
run akimboglove-decoy akimboglove 1 $G=15,25 $H=15 $L=25
run akimbolead akimbolead 1 $G=15,18 $H=15 $L=18
run akimboring akimboring 1 $G=15,16,19,21 $H=16 $L=0
run hands-21 hands 1 $G=15,16,21 $H=21
run hands-16 hands 1 $G=15,16,21 $H=16
run inputs inputs 1 $G=15,16,21
run quick quick 1 $G=10,15,16,11,13,9,18,19,20,17,21,23
run throw throw 1 $G=10,17,20,25 $H=10
run throw-doom throw 1 $G=20 $H=20
run twohand twohand 1 $G=23 $H=23
# A left hand holding an item takes no foregrip (Rerac-vr's): no "holds the foregrip" line.
run twohandleft twohand 1 $G=23,15 $H=23 $L=15
run visibomb visibomb 1 $G=13 $H=13 QUEST_MISSILE_TRACE=1 KEEP=1
run visibombgone visibombgone 1 $G=13 $H=13 KEEP=1
# The same flame twice, its sprites turned to the eyes and left to the game's camera: compare the shots.
run spritesface spritesface 1 $G=16 $H=16 KEEP=1
run spritesfaceoff spritesfaceoff 1 $G=16 $H=16 QUEST_SPRITES_OFF=1 KEEP=1
run walloper walloper 1 $G=18 $H=18 $N=500
run walloperchain walloperchain 1 $G=18 $H=18 QUEST_STATE_TRACE=1
run wrench wrench 1 $G=10 $N=511 KEEP=1
run wrenchthrow wrenchthrow 1 $G=15 $H=15
run unequip unequip 1 $G=10 $H=10 KEEP=1
run wrenchwall wrenchwall 1
run hyper hyper 1
run hypercrate hypercrate 1 $N=500 $A=1.2,0 QUEST_POINTER_TRACE=1
run solidhand solidhand 1 KEEP=1
run disguise disguise 1 $G=31 $H=31
run swing-right swing 3 $G=12 $H=12 $N=758 $A=10,1,180 QUEST_STATE_TRACE=1
run swing-left swing 3 $G=15,12 $H=15 $L=12 $N=758 $A=10,1,180 QUEST_STATE_TRACE=1
# Ledges, cranks, sockets, water
run ledge ledge 9 $N=500 $A=0.95,2.6
run ledgenofire ledgenofire 9 $G=15 $H=15 $N=500 $A=0.95,2.6
run ledgeshoot ledgeshoot 9 $G=15 $H=15 $N=500 $A=0.95,2.6
run ledgeshootleft ledgeshootleft 9 $G=15,16 $H=16 $L=15 $N=500 $A=0.95,2.6
run ledgeearly ledgeearly 1 $N=501 $A=0.95,1.7,90 QUEST_SIM_GRIP=1
run ledgereach ledgereach 9 $N=500 $A=0.95,0
run wrenchlong wrenchlong 1 $G=10 KEEP=1
run ledgedrop ledgedrop 1 $N=501 $A=0.95,1.7,90
run ledgeopen ledgeopen 1 $N=501 $A=0.95,1.7,90
run ledgeshimmy ledgeshimmy 1 $N=501 $A=0.95,1.7,90
run ledgegame ledgegame 1 $N=501 $A=0.95,1.7,90 'QUEST_SETTINGS={"grab_ledges":false}'
run crank crank 1 $N=280 $A=1.2,0
run crankstick crankstick 1 $N=280 $A=1.2,0
run crankleft crankleft 1 $G=15 $L=15 $N=280 $A=1.2,0
run crankdeath crankdeath 1 $N=280 $A=1.2,0 QUEST_SIM_KILL=680
run socket socket 2 $G=26 $H=26 $N=615 $A=2.2,0
run puzzle puzzle 2 $G=26 $H=26 $N=615 $A=2.2,0
run sockethydro sockethydro 5 $G=22 $H=22 $N=341 $A=1.3,0
run swim swim 1 $N=695 $A=2.0,0 QUEST_STATE_TRACE=1
# The body, the view, scenes, turrets, ships
run body body 10
run bodyflip bodyflip 1 'QUEST_VR_OPTIONS=camera_flip 1;body 2'
run bodyroll bodyroll 1 'QUEST_VR_OPTIONS=body 2'
run death death 1 QUEST_SIM_KILL=150
run room room 1 $N=500 $A=1.6,0
run scene scene 1
# 0.1.9: looking down in a cutscene through his eyes, no thin line from one elbow to the other
run scenearms scenearms 1 KEEP=1
run screen screen 1
run t1 t1 1
run t2 t2 1
# How far the eyes see, the head turned from the game's camera: the ship must show in turned and turned2. QUEST_LOD=true,
# or QUEST_HEAD_TURNS_OFF=1, to compare. Veldin's intro gives control at frame 8056.
run far far 0 QUEST_SIM_ALIGN=2620 QUEST_DRAWN_TRACE=1 KEEP=1
run particles particles 1 $G=16 $H=16 QUEST_SIM_ALIGN=2620 OPENRAC_DRAW_COVERAGE=C:/RACQuest/logs/regress/particles.json
run board board 5 $N=439 QUEST_STATE_TRACE=1 QUEST_BOARD_TRACE=1
run board-kalebo board 16 $N=439 QUEST_STATE_TRACE=1 QUEST_BOARD_TRACE=1
run boardjump boardjump 5 $N=439 QUEST_STATE_TRACE=1 QUEST_BOARD_TWIST=1
run rail rail 14 $G=29 QUEST_SIM_RAIL=2,0.3,3 QUEST_SIM_ALIGN=2620 QUEST_STATE_TRACE=1 KEEP=1
run cable cable 3 QUEST_SIM_RAIL=0,-1.3,0 QUEST_SIM_ALIGN=2620 QUEST_STATE_TRACE=1 KEEP=1
run shipboard shipboard 11 $G=7 $N=1242 $A=2,0 QUEST_STATE_TRACE=1 KEEP=1
run shipboard-fleet shipboard 17 $G=7 $N=1379 $A=2,0 QUEST_STATE_TRACE=1
run ship ship 17 $N=1379 $A=0.5,1.0
run turret-hoven turret 12 $N=1267 $A=0.2,0.3
run turret-gaspar turret 9 $N=1201 $A=0,1.5
run turret-batalia turret 8 $N=440 $A=0.2,1.5 QUEST_SIM_DONE=1
# Menus, pointer, saves, vendor
run mainmenu mainmenu - KEEP=1
run exitbox exitbox - KEEP=1
run save save -
run menus menus 1
run menusticks menusticks 1 KEEP=1
run mapsticks mapsticks 1 KEEP=1
run thirdperson thirdperson 1 KEEP=1
run options options 1
run vrhands vrhands 1
run vrhands2 vrhands2 1 $G=15 $H=15 KEEP=1
run pointer pointer 1
run pointer2 pointer2 1
run pointer3 pointer3 1
run pointer4 pointer4 1 $G=15,16,19,21,23 $H=15
run pointer5 pointer5 1
run pointer6 pointer6 1
run pointer7 pointer7 1
run pointer8 pointer8 1
run pointer9 pointer9 1
run pointer10 pointer10 1 QUEST_SIM_MENU=14 QUEST_SIM_PLANETS=1,3,5,7
run pointer11 pointer11 1 QUEST_SIM_MENU=10 QUEST_SIM_PLANETS=1,3,5,7
run pointer12 pointer12 1 QUEST_SIM_FREEZE=2
run pointer13 pointer13 1 QUEST_SIM_FREEZE=0
rm -rf /c/RACQuest/pc/cards/regress-p14; mkdir -p /c/RACQuest/pc/cards/regress-p14
run pointer14 pointer14 1 --cards C:/RACQuest/pc/cards/regress-p14
[[ "$ONLY" == "  " || "$ONLY" == *" pointer14 "* ]] && find /c/RACQuest/pc/cards/regress-p14 -type f -printf 'card file %P %s bytes\n' >> "$OUTS/pointer14.txt"
run pointer15 pointer15 1 $G=10,11,12,13,14,15,16,17,18,19,20
run pointer16 pointer16 1
# The title's Load Game from pointer14's card when it ran here, else the kept one.
rm -rf /c/RACQuest/pc/cards/regress-p17
if [ -n "$(find /c/RACQuest/pc/cards/regress-p14 -type f 2>/dev/null)" ]; then
  cp -r /c/RACQuest/pc/cards/regress-p14 /c/RACQuest/pc/cards/regress-p17
else
  cp -r /c/RACQuest/pc/cards/pointer14 /c/RACQuest/pc/cards/regress-p17
fi
run pointer17 pointer17 - --cards C:/RACQuest/pc/cards/regress-p17
run pointer18 pointer18 1 $G=3 QUEST_STATE_TRACE=1
run pointer20 pointer20 1 $G=6,28
run vendor vendor 1 $N=11 QUEST_SIM_BOLTS=9000 'QUEST_SETTINGS={"shop_on_foot":false}'
run vendor3 vendor3 1 $G=16,17,18,19 $N=11 QUEST_SIM_BOLTS=9000 QUEST_POINTER_TRACE=1 'QUEST_SETTINGS={"shop_on_foot":false}'
run vendor4 vendor4 1 $G=10,11,12,13,14,15,16,17,18,19,20 $N=11 QUEST_SIM_BOLTS=9000 QUEST_POINTER_TRACE=1 'QUEST_SETTINGS={"shop_on_foot":false}'
run vendor5 vendor5 1 $N=11 QUEST_SIM_BOLTS=9000 QUEST_POINTER_TRACE=1 'QUEST_SETTINGS={"shop_on_foot":false}'
# 2026-11-01: on foot, the PDA's, the world menu, the two rings, the boots
run boots boots 1 $G=28 QUEST_BOOTS_TRACE=1
run shop shop 1 $G=16,17,18,19 $N=11 QUEST_SIM_BOLTS=9000 QUEST_POINTER_TRACE=1
run shopcontrol shopcontrol 1 $N=11 QUEST_STATE_TRACE=1
run shopsweep shopsweep 1 $G=16,17,18,19 $N=11 QUEST_SIM_BOLTS=9000 QUEST_POINTER_TRACE=1
run shopbuy shopbuy 1 $G=16,17,18,19 $N=11 QUEST_SIM_BOLTS=9000 QUEST_POINTER_TRACE=1
# Eudora's landing gives control at a varying frame (3933, 4318): the script is aligned to the set-up (QUEST_SIM_ALIGN).
run shopbuy-eudora shopbuy-eudora 4 $G=16,17,18,19 $N=11 $A=1.1,0,103.4 QUEST_SIM_BOLTS=9000 QUEST_POINTER_TRACE=1 QUEST_SIM_ALIGN=2620
run shoppda shoppda 1 $G=16,17,18,19 QUEST_SIM_PDA=1 QUEST_SIM_BOLTS=9000 QUEST_POINTER_TRACE=1
run shoppda-eudora shoppda 4 $G=16,17,18,19 QUEST_SIM_PDA=1 QUEST_SIM_BOLTS=9000 QUEST_POINTER_TRACE=1 QUEST_SIM_ALIGN=2620
run worldmenu worldmenu 1 QUEST_POINTER_TRACE=1
run worldmenuwall worldmenuwall 1 QUEST_PANEL_FAR=0
run giant giant 15 QUEST_SIM_NEAR=1451 QUEST_SIM_NEAR_AT=0,0 QUEST_STATE_TRACE=1 KEEP=1
run skipscene skipscene 1 QUEST_STATE_TRACE=1
run tworings tworings 1 $G=15,16,19,21 $H=16 $L=15
run tworings2 tworings2 1 $G=15,16,19,21 $H=16 $L=15
run tworings3 tworings3 1 $G=10,15,16,11,13,9,18,19,20,17,21,23,14,24,25,12,22,26,27 $H=16 $L=15
echo regress-done
