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
  # Kerwan's start: its sims were written with Ratchet facing along y (1.57), where 0.1.17's play began. From 0.1.18 play
  # keeps the arrival scene's heading and he turns to it (0.67), so every aim written for that start went 52 degrees to
  # the side. A Kerwan run with no place of its own is put back at the start facing along y; "[sim] put at" says so.
  # And play begins recentred on his facing as in 0.1.17 (QUEST_TEST_NO_HANDOVER=1): with the put alone the view kept
  # the scene's heading for a tick, the Gaze turn took him to 1.53 before the recentre, and every aim went 2 degrees to
  # the side (anywhere-zips' third pull found nothing; with this its three pulls are 0.1.17's to the tenth).
  if [ "$level" = 3 ] && [[ " ${envs[*]} " != *" QUEST_SIM_AT="* && " ${envs[*]} " != *" QUEST_SIM_NEAR="* && " ${envs[*]} " != *" QUEST_SIM_RAIL="* ]]; then
    envs+=(QUEST_SIM_AT=263.98,101.89,54.5,90 QUEST_TEST_NO_HANDOVER=1)
  fi
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
    # The game's code the port has no C for, called in this run: it did nothing (an Orxon barrier undrawn on 0.1.17's US disc).
    grep -a -o '[A-Za-z0-9_]* has no C in the port yet' "$log" | sort | uniq -c | sed 's/^ */port: /' | head -10
    grep -a -E '\[(vr-[a-z]+|sim)\]' "$log" | grep -a -v -E 'vr-perf|vr-state] (tick|phase)|vr-input|vr-glyphs|vr-body] hidden|sim] hittable|vr-hand] class|vr-aim] near the head|vr-aim] class .* (also holds|aimed by)|\[sim\] (frame|shot|pointer:|smoother frames)|haptic|vr-sound|centred text|not a list' \
      | sed -e 's/.*\[mod quest-vr\] //' -e 's/^\[sim\] stick tick [0-9]*:/[sim] stick:/' | cut -c1-170 \
      | awk '!seen[$0]++ || /^\[sim\] hero state/' | head -150
  # (The stick's trace on the boots' surface comes every 8 ticks: without its tick, the same line is kept once and the
  # climb's own lines stay under the 150.)
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
# 0.1.13: the ammo box through the wrench and back (a tester's Pyrocitor and Bomb Glove: the left gun's box stayed).
run akimbobox akimbobox 1 $G=16,10 $H=10 $L=16 QUEST_AMMO_TRACE=1 KEEP=1
run akimboglove-decoy akimboglove 1 $G=15,25 $H=15 $L=25
run akimbolead akimbolead 1 $G=15,18 $H=15 $L=18
run akimboring akimboring 1 $G=15,16,19,21 $H=16 $L=0 'QUEST_VR_OPTIONS=wheel_reach 1'
run hands-21 hands 1 $G=15,16,21 $H=21
run hands-16 hands 1 $G=15,16,21 $H=16
run inputs inputs 1 $G=15,16,21
run quick quick 1 $G=10,15,16,11,13,9,18,19,20,17,21,23 'QUEST_VR_OPTIONS=wheel_reach 1'
# 0.1.13: Akimbo on for everyone ([vr-options] says it). The ring as 0.1.12 had it, with Akimbo chosen off; a file of
# 0.1.12's with Off (its start) switches on once, one with On stays on, Off chosen on the page stays off.
run quick-akimbooff quick 1 $G=10,15,16,11,13,9,18,19,20,17,21,23 'QUEST_VR_OPTIONS=akimbo 0;akimbo_chosen 1;wheel_reach 1'
# 0.1.17: the weapon wheel. Wheel pick's Point (a turn of the hand goes round the icons, the default) and its slow motion (the
# default, with sounds); the same with the slow motion's other settings; Reach by the same turns picks nothing; third person's
# wheel slows too, its stick the game's.
run wheelpoint wheelpoint 1 $G=10,15,16,11,13,9,18,19 KEEP=1
run wheelcentre wheelcentre 1 $G=10,15,16,11,13,9,18,19 'QUEST_VR_OPTIONS=akimbo 0;akimbo_chosen 1' KEEP=1
# 0.1.19: let go in the wheel's middle (a tester's "releasing in the middle of the weapon wheel should go back to fists"):
# the right hand's Blaster put away and brought back by the trigger, a quick pass through the middle keeps the weapon, and
# the left hand's Pyrocitor put away.
run wheelmiddle wheelmiddle 1 $G=10,15,16,11,13,9,18,19 $H=15 $L=16 KEEP=1
run wheelquiet wheelpoint 1 $G=10,15,16,11,13,9,18,19 'QUEST_VR_OPTIONS=wheel_slow 1'
run wheeloff wheelpoint 1 $G=10,15,16,11,13,9,18,19 'QUEST_VR_OPTIONS=wheel_slow 2;wheel_reach 1'
run wheelthird wheelpoint 1 $G=10,15,16,11,13,9,18,19 'QUEST_SETTINGS={"view":"third","third_person_controls":"vr"}'
# 0.1.17: Feet on the floor (VR Body, off to start): the view lifted to the head's real height over the floor.
run feetfloor hands 1 $G=15,16,21 $H=21 'QUEST_VR_OPTIONS=feet_on_floor 1' KEEP=1
# 0.1.17: the water from right beside it, where the owner stood: whole, whichever way the head turns.
run waternear waternear 1 QUEST_SIM_AT=172.7,91.0,62.4,147 KEEP=1
# 0.1.17: a video in the middle of a scene, not skipped, and what is on the HUD's panel after it (a tester's black box).
run clankvideo clankvideo 0 QUEST_SKIP_ANY_MOVIE=false QUEST_VIEW_TRACE=1 QUEST_SIM_AT=165.67,272.68,29.23,-15 KEEP=1
run akimbo-oldoff t2 1 'QUEST_VR_OPTIONS=akimbo 0;body 2'
run akimbo-oldon t2 1 'QUEST_VR_OPTIONS=akimbo 1;body 2'
run akimbo-chosenoff t2 1 'QUEST_VR_OPTIONS=akimbo 0;akimbo_chosen 1;body 2'
# 0.1.13: Empty hand angle reaches 65 down (a tester's ask; 45 before), the guns' Gun angle still 45. vrhands.sim's
# step of the row (5 up) is the page's side of it.
run emptyhand emptyhand 3
run emptyhand-down emptyhand 3 'QUEST_VR_OPTIONS=empty_hand_angle -65' KEEP=1
# 0.1.14: the start is 65 down (the owner's pick). A file of 0.1.13's 0, the start then, becomes 65 down once ("[vr-hand]
# Empty hand angle: 0 was the start before 0.1.14"); 0 set on the page stays; another angle of an old file stays.
run emptyhand-old emptyhand 3 'QUEST_VR_OPTIONS=empty_hand_angle 0'
run emptyhand-zero emptyhand 3 'QUEST_VR_OPTIONS=empty_hand_angle 0;empty_hand_chosen 1'
run emptyhand-oldchosen emptyhand 3 'QUEST_VR_OPTIONS=empty_hand_angle -30'
# 0.1.15: the thumb on and off the controller, from around each hand and before the eyes (KEEP=1: look at it), at the start and at 0.
run thumb thumb 3 KEEP=1
run thumb-zero thumb 3 'QUEST_VR_OPTIONS=empty_hand_angle 0;empty_hand_chosen 1' KEEP=1
# 0.1.14: walking straight back does not slide him ahead as the stick is let go ("[vr-move] the stick let go", ahead
# stays below 0 to the stop); the run forward and back, the strafe let go.
run backwalk backwalk 3 QUEST_WALK_TRACE=1 QUEST_STATE_TRACE=1
# 0.1.14: the first screens (the owner's "frame warping in the first screen ... like a credits area"): the copyright
# lines and the title stand still in the room as the head turns and moves.
run titlelook titlelook - QUEST_VIEW_TRACE=1 KEEP=1
# 0.1.14: on foot in first person the left grip never crouches, Physical crouch off or on; the left trigger does
# ("[sim] hero state" 4 only after the trigger).
run gripcrouch-off gripcrouch 3 QUEST_STATE_TRACE=1 'QUEST_VR_OPTIONS=physical_crouch 0'
run gripcrouch-on gripcrouch 3 QUEST_STATE_TRACE=1
# 0.1.13: an item's own angle (VR Hands' last row; a tester's Swingshot was "a bit wild"): set on the page, and from
# the file, the Swingshot's housing drawn 30 down while its hook still pulls; the Blaster's 20 up fires 20 up.
run itemangle itemangle 3 $G=12 $H=12 KEEP=1
run swing-right-angle swing 3 $G=12 $H=12 $N=758 $A=10,1,180 QUEST_STATE_TRACE=1 'QUEST_VR_OPTIONS=item_angle_12 -30' KEEP=1
run aim-itemangle aim 1 $G=15 $H=15 'QUEST_VR_OPTIONS=item_angle_15 20'
# 0.1.13: Swingshot anywhere (More controls, off; testers' ideas): on Kerwan, at the start (a tunnel), no target near.
# The right hand's Swingshot pulls him to the floor it aims at; the left's (Akimbo, leading) swings him from the arch
# (a point over his head; the floor: "a swing needs a point above his head"); one in each hand chains a swing into a
# pull; a zip let go midway keeps its speed; a lean in the room pushes a swing. Off, nothing is done; at Kerwan's target
# the game's own pull, with it on. On Novalis (level 1), whose game code has no Swingshot states, nothing is given; given
# there all the same (the PC's QUEST_ANYWHERE_ANY_LEVEL), the pull stalls in 0x25 and the watchdog lets him go. The
# More controls page's row sets it, by the sticks (anywherepage, which steps Hide black bars too).
run anywhere anywhere 3 $G=12 $H=12 QUEST_STATE_TRACE=1 'QUEST_VR_OPTIONS=swing_anywhere 1' KEEP=1
run anywhere-off anywhere 3 $G=12 $H=12 QUEST_STATE_TRACE=1
run anywhere-left anywhereleft 3 $G=15,12 $H=15 $L=12 QUEST_STATE_TRACE=1 'QUEST_VR_OPTIONS=swing_anywhere 1' KEEP=1
run anywhere-chain anywherechain 3 $G=12 $H=12 $L=12 QUEST_STATE_TRACE=1 'QUEST_VR_OPTIONS=swing_anywhere 1' KEEP=1
run anywhere-letgo anywhereletgo 3 $G=12 $H=12 QUEST_STATE_TRACE=1 'QUEST_VR_OPTIONS=swing_anywhere 1' KEEP=1
run anywhere-lean anywherelean 3 $G=15,12 $H=15 $L=12 QUEST_STATE_TRACE=1 'QUEST_VR_OPTIONS=swing_anywhere 1'
run anywhere-zips anywherezips 3 $G=12 $H=12 QUEST_STATE_TRACE=1 'QUEST_VR_OPTIONS=swing_anywhere 1'
run anywhere-zips2 anywherezips2 3 $G=12 $H=12 $L=12 QUEST_STATE_TRACE=1 'QUEST_VR_OPTIONS=swing_anywhere 1' KEEP=1
run anywhere-zips3 anywherezips3 3 $G=12 $H=12 QUEST_STATE_TRACE=1 'QUEST_VR_OPTIONS=swing_anywhere 1'
run anywhere-fling anywherefling 3 $G=12 $H=12 QUEST_STATE_TRACE=1 'QUEST_VR_OPTIONS=swing_anywhere 1'
run anywhere-fling-swing anywhereflingswing 3 $G=12 $H=12 $L=12 QUEST_STATE_TRACE=1 'QUEST_VR_OPTIONS=swing_anywhere 1'
run anywhere-hover anywherehover 3 $G=2,12 $H=12 QUEST_STATE_TRACE=1 'QUEST_VR_OPTIONS=swing_anywhere 1'
run swing-hold swinghold 3 $G=12 $H=12 $L=12 QUEST_SIM_GRIP_TILT=50 KEEP=1
run anywhere-slow anywhereslow 3 $G=12 $H=12 QUEST_STATE_TRACE=1 'QUEST_VR_OPTIONS=swing_anywhere 1'
# Wall run and jump, Swingshot reach, the wrench thrown far and called (More controls, Moves; 0.1.14).
run wallzip wallzip 3 $G=12 $H=12 QUEST_STATE_TRACE=1 'QUEST_VR_OPTIONS=swing_anywhere 1;wall_moves 1' KEEP=1
run wallzip-off wallzip 3 $G=12 $H=12 QUEST_STATE_TRACE=1 'QUEST_VR_OPTIONS=swing_anywhere 1'
run wallrun wallrun 3 QUEST_STATE_TRACE=1 'QUEST_VR_OPTIONS=wall_moves 1'
run wallrun-off wallrun 3 QUEST_STATE_TRACE=1
run wallup wallup 3 QUEST_STATE_TRACE=1 'QUEST_VR_OPTIONS=wall_moves 1'
run wallchain wallchain 3 $G=12 $H=12 QUEST_STATE_TRACE=1 'QUEST_VR_OPTIONS=swing_anywhere 1;wall_moves 1'
run swingfar swingfar 3 $G=12 $H=12 QUEST_STATE_TRACE=1 'QUEST_VR_OPTIONS=swing_anywhere 1;swing_reach 2;wall_moves 1' KEEP=1
run swingfar-normal swingfar 3 $G=12 $H=12 QUEST_STATE_TRACE=1 'QUEST_VR_OPTIONS=swing_anywhere 1'
run swingfarleft swingfarleft 3 $G=15,12 $H=15 $L=12 QUEST_STATE_TRACE=1 'QUEST_VR_OPTIONS=swing_anywhere 1;swing_reach 2'
run swingtarget swingtarget 3 $G=12 $H=12 $N=758 $A=45,6,0 QUEST_STATE_TRACE=1 'QUEST_VR_OPTIONS=swing_reach 1'
run swingtarget-normal swingtarget 3 $G=12 $H=12 $N=758 $A=45,6,0 QUEST_STATE_TRACE=1
run wrenchfar wrenchfar 3 $G=15 $H=15 'QUEST_VR_OPTIONS=wrench_far 3'
run wrenchfar-normal wrenchfar 3 $G=15 $H=15
run wrenchcall wrenchcall 3 $G=15,10,16 $H=15 'QUEST_VR_OPTIONS=wrench_far 2;wrench_stay 1' KEEP=1
run wrenchbuttons wrenchbuttons 3 $G=15 $H=15 'QUEST_VR_OPTIONS=button_moves 1;wrench_far 2;wrench_stay 1' QUEST_STATE_TRACE=1
run wrenchthird wrenchthird 3 $G=15 $H=15 'QUEST_VR_OPTIONS=wrench_far 2;wrench_stay 1' 'QUEST_SETTINGS={"view":"third","third_person_controls":"vr"}' QUEST_STATE_TRACE=1
run wrenchlead wrenchlead 3 $G=15,12 $H=15 $L=12 'QUEST_VR_OPTIONS=swing_anywhere 1;wrench_far 2;wrench_stay 1' QUEST_STATE_TRACE=1
run wrenchdeath wrenchdeath 3 $G=15 $H=15 QUEST_SIM_KILL=2300 'QUEST_VR_OPTIONS=wrench_far 2;wrench_stay 1' QUEST_STATE_TRACE=1
run wallrun-third wallrun 3 QUEST_STATE_TRACE=1 'QUEST_VR_OPTIONS=wall_moves 1' 'QUEST_SETTINGS={"view":"third","third_person_controls":"vr"}'
# All of it on against what it could take the hands or buttons from: ledges by hand, a pull up, the shimmy, swimming, the Heli-Pack, grinding.
OPTS_ALL="swing_anywhere 1;wall_moves 1;swing_reach 2;wrench_far 3;wrench_stay 1"
run on-ledge ledge 9 $N=500 $A=0.95,2.6 "QUEST_VR_OPTIONS=$OPTS_ALL"
run on-ledgeearly ledgeearly 1 $N=501 $A=0.95,1.7,90 QUEST_SIM_GRIP=1 "QUEST_VR_OPTIONS=$OPTS_ALL"
run on-ledgepull ledgepull 1 $N=501 $A=0.95,1.7,90 "QUEST_VR_OPTIONS=$OPTS_ALL"
run on-ledgeshimmy ledgeshimmy 1 $N=501 $A=0.95,1.7,90 "QUEST_VR_OPTIONS=$OPTS_ALL"
run on-ledgegame ledgegame 1 $N=501 $A=0.95,1.7,90 'QUEST_SETTINGS={"grab_ledges":false}' "QUEST_VR_OPTIONS=$OPTS_ALL"
run on-ledgeakimbo ledgeakimbo 1 $G=15,10 $H=15 $L=10 $N=501 $A=0.95,1.7,90 "QUEST_VR_OPTIONS=$OPTS_ALL"
run on-swim swim 1 $N=695 $A=2.0,0 QUEST_STATE_TRACE=1 "QUEST_VR_OPTIONS=$OPTS_ALL"
run on-heli kerwanheli 3 $G=2 "QUEST_VR_OPTIONS=$OPTS_ALL"
run on-hover anywherehover 3 $G=2,12 $H=12 QUEST_STATE_TRACE=1 "QUEST_VR_OPTIONS=$OPTS_ALL"
run on-boardjump boardjump 5 $N=439 QUEST_STATE_TRACE=1 QUEST_BOARD_TWIST=1 "QUEST_VR_OPTIONS=$OPTS_ALL"
run on-cablefire cablefire 3 $G=15,16 $H=15 $L=16 QUEST_SIM_RAIL=0,-1.3,0 QUEST_SIM_ALIGN=2620 QUEST_STATE_TRACE=1 "QUEST_VR_OPTIONS=$OPTS_ALL"
run on-wrench wrench 1 $G=10 $N=511 "QUEST_VR_OPTIONS=$OPTS_ALL"
run on-wrenchthrow wrenchthrow 1 $G=15 $H=15 "QUEST_VR_OPTIONS=$OPTS_ALL"
run on-unequip unequip 1 $G=10 $H=10 "QUEST_VR_OPTIONS=$OPTS_ALL"
run on-wrenchwall wrenchwall 1 "QUEST_VR_OPTIONS=$OPTS_ALL"
run on-wrenchlong wrenchlong 1 $G=10 "QUEST_VR_OPTIONS=$OPTS_ALL"
run on-armbuttons armbuttons 1 $G=15 $H=15 $N=511 "QUEST_VR_OPTIONS=button_moves 1;$OPTS_ALL" QUEST_STATE_TRACE=1
run on-akimbolead akimbolead 1 $G=15,18 $H=15 $L=18 "QUEST_VR_OPTIONS=$OPTS_ALL"
run anywhere-novalis anywhere 1 $G=12 $H=12 QUEST_STATE_TRACE=1 'QUEST_VR_OPTIONS=swing_anywhere 1'
run anywhere-stall anywhere 1 $G=12 $H=12 QUEST_STATE_TRACE=1 QUEST_ANYWHERE_ANY_LEVEL=1 'QUEST_VR_OPTIONS=swing_anywhere 1'
run swing-right-anywhere swing 3 $G=12 $H=12 $N=758 $A=10,1,180 QUEST_STATE_TRACE=1 'QUEST_VR_OPTIONS=swing_anywhere 1'
run anywherepage anywherepage 1 KEEP=1
# 0.1.15: the way to Moves by the hand, and a page of eight rows moved on and back by the beam (KEEP=1: look at it).
run menuscroll menuscroll 1 KEEP=1
run throw throw 1 $G=10,17,20,25 $H=10
run throw-doom throw 1 $G=20 $H=20
run glovehold glovehold 1 $G=10 $H=10
run twohand twohand 1 $G=23 $H=23 'QUEST_VR_OPTIONS=hand_punch 1'
# A left hand holding an item takes no foregrip (Rerac-vr's): no "holds the foregrip" line.
run twohandleft twohand 1 $G=23,15 $H=23 $L=15
run twohand-pyro twohand 1 $G=16 $H=16
run supportleft supportleft 1 $G=15,16 $L=16
run visibomb visibomb 1 $G=13 $H=13 QUEST_MISSILE_TRACE=1 KEEP=1
run visibombgone visibombgone 1 $G=13 $H=13 KEEP=1
# The same flame twice, its sprites turned to the eyes and left to the game's camera: compare the shots.
run spritesface spritesface 1 $G=16 $H=16 KEEP=1
run spritesfaceoff spritesfaceoff 1 $G=16 $H=16 QUEST_SPRITES_OFF=1 KEEP=1
run walloper walloper 1 $G=18 $H=18 $N=500
run walloperchain walloperchain 1 $G=18 $H=18 QUEST_STATE_TRACE=1
run wrench wrench 1 $G=10 $N=511 KEEP=1
run wrench70 wrench 1 $G=10 $N=511 'QUEST_VR_OPTIONS=hand_size 70' KEEP=1
run wrenchthrow wrenchthrow 1 $G=15 $H=15
# 0.1.17: the grip eased in a fast swing keeps the wrench (no throw), opened it throws; seated and standing (a tester on
# 0.1.16, seated: "you can't do the wrench it jus disconnects").
run wrenchease wrenchease 1 $G=15 $H=15
run wrenchease-seated wrenchease 1 $G=15 $H=15 'QUEST_VR_OPTIONS=seated 1'
run unequip unequip 1 $G=10 $H=10 KEEP=1
run wrenchwall wrenchwall 1
run hyper hyper 1
run hypercrate hypercrate 1 $N=500 $A=1.2,0 QUEST_POINTER_TRACE=1
# 0.1.13: Arm moves on Buttons (More controls; a player's ask: wrists and hands that hurt to swing). The grip is the
# game's swing, the trigger the Walloper's punch; the throws, ledges, strokes, cranks and Giant Clank's fists the game's.
run armbuttons armbuttons 1 $G=15 $H=15 $N=511 'QUEST_VR_OPTIONS=button_moves 1' QUEST_STATE_TRACE=1 KEEP=1
run walloperbuttons walloperbuttons 1 $G=18 $H=18 $N=500 'QUEST_VR_OPTIONS=button_moves 1' QUEST_STATE_TRACE=1
run throw-buttons throw 1 $G=10,17,20,25 $H=10 'QUEST_VR_OPTIONS=button_moves 1'
run ledge-buttons ledge 9 $N=500 $A=0.95,2.6 'QUEST_VR_OPTIONS=button_moves 1' QUEST_STATE_TRACE=1
run swim-buttons swim 1 $N=695 $A=2.0,0 'QUEST_VR_OPTIONS=button_moves 1' QUEST_STATE_TRACE=1
run crankstick-buttons crankstick 1 $N=280 $A=1.2,0 'QUEST_VR_OPTIONS=button_moves 1' QUEST_STATE_TRACE=1
run giant-buttons giant 15 QUEST_SIM_NEAR=1451 QUEST_SIM_NEAR_AT=0,0 QUEST_STATE_TRACE=1 'QUEST_VR_OPTIONS=button_moves 1' KEEP=1
run solidhand solidhand 1 KEEP=1
run disguise disguise 1 $G=31 $H=31
run swing-right swing 3 $G=12 $H=12 $N=758 $A=10,1,180 QUEST_STATE_TRACE=1
run swing-left swing 3 $G=15,12 $H=15 $L=12 $N=758 $A=10,1,180 QUEST_STATE_TRACE=1
# Ledges, cranks, sockets, water
run ledge ledge 9 $N=500 $A=0.95,2.6
run ledgenofire ledgenofire 9 $G=15 $H=15 $N=500 $A=0.95,2.6
run ledgeshoot ledgeshoot 9 $G=15 $H=15 $N=500 $A=0.95,2.6
run ledgeshootleft ledgeshootleft 9 $G=15,16 $H=16 $L=15 $N=500 $A=0.95,2.6
# 0.1.13: the gun held above the stack he hangs from (ledgeshoot's breaks it): 25 rounds while he hangs, dropped by the grip
run ledgeshoothigh ledgeshoothigh 9 $G=15 $H=15 $N=500 $A=0.95,2.6
run ledgeearly ledgeearly 1 $N=501 $A=0.95,1.7,90 QUEST_SIM_GRIP=1
run ledgereach ledgereach 9 $N=500 $A=0.95,0
run wrenchlong wrenchlong 1 $G=10 KEEP=1
run ledgedrop ledgedrop 1 $N=501 $A=0.95,1.7,90
run ledgeopen ledgeopen 1 $N=501 $A=0.95,1.7,90
run ledgeshimmy ledgeshimmy 1 $N=501 $A=0.95,1.7,90
# 0.1.13: Ledge shimmy's Pull, 0.1.12's way kept as a choice; the left stick shimmies too (testers' asks).
run ledgeshimmy-pull ledgeshimmy 1 $N=501 $A=0.95,1.7,90 'QUEST_VR_OPTIONS=shimmy_pull 1'
run ledgestick ledgestick 1 $N=501 $A=0.95,1.7,90
run ledgeakimbo ledgeakimbo 1 $G=15,10 $H=15 $L=10 $N=501 $A=0.95,1.7,90 KEEP=1
# 0.1.13: the pull up, with both hands, with one, and during a shimmy
run ledgepull ledgepull 1 $N=501 $A=0.95,1.7,90
run ledgepullone ledgepullone 1 $N=501 $A=0.95,1.7,90
run ledgepullshimmy ledgepullshimmy 1 $N=501 $A=0.95,1.7,90
run ledgegame ledgegame 1 $N=501 $A=0.95,1.7,90 'QUEST_SETTINGS={"grab_ledges":false}'
run crank crank 1 $N=280 $A=1.2,0
run crankstick crankstick 1 $N=280 $A=1.2,0
run crankleft crankleft 1 $G=15 $L=15 $N=280 $A=1.2,0
run crankdeath crankdeath 1 $N=280 $A=1.2,0 QUEST_SIM_KILL=680
# 0.1.17: seated the crank goes as standing (to 0.1.16 Seated lifted the view and the hands 45 cm over the bolt).
run crank-seated crank 1 $N=280 $A=1.2,0 'QUEST_VR_OPTIONS=seated 1'
run socket socket 2 $G=26 $H=26 $N=615 $A=2.2,0
run puzzle puzzle 2 $G=26 $H=26 $N=615 $A=2.2,0 KEEP=1
# 0.1.17: the same in third person, the game's own way (a tester could not see the rings on PC VR): on the floor pad, the fire button.
run puzzlethird puzzlethird 2 $G=26 $H=26 $N=615 $A=0.1,0 'QUEST_SETTINGS={"view":"third","third_person_controls":"vr"}' KEEP=1
run sockethydro sockethydro 5 $G=22 $H=22 $N=341 $A=1.3,0
run swim swim 1 $N=695 $A=2.0,0 QUEST_STATE_TRACE=1
# 0.1.17: the left trigger dives at the surface (a tester: "how do you dive?"); under, the head looks down and up with the stick.
run dive dive 1 $N=695 $A=2.0,0 QUEST_STATE_TRACE=1 QUEST_SWIM_TRACE=1
# 0.1.17: a tester on 0.1.16 could not dive at all. The hands pushed up edge-on dive, then a stroke looking down; seated
# (to 0.1.16 Seated lifted the hands 45 cm, out of the water) and standing; the left trigger's dive in third person too.
run divehands divehands 1 $N=695 $A=2.0,0 QUEST_STATE_TRACE=1 QUEST_SWIM_TRACE=1
run divehands-seated divehands 1 $N=695 $A=2.0,0 QUEST_STATE_TRACE=1 QUEST_SWIM_TRACE=1 'QUEST_VR_OPTIONS=seated 1'
run dive-third dive 1 $N=695 $A=2.0,0 QUEST_STATE_TRACE=1 QUEST_SWIM_TRACE=1 'QUEST_SETTINGS={"view":"third","third_person_controls":"vr"}'
run water water 1 $N=695 $A=10,0 KEEP=1
# The body, the view, scenes, turrets, ships
run body body 10
run bodyflip bodyflip 1 'QUEST_VR_OPTIONS=camera_flip 1;body 2'
run bodyroll bodyroll 1 'QUEST_VR_OPTIONS=body 2'
# 0.1.13: a tester's 0.1.12 videos (Kerwan, the Heli-Pack): his body hidden and fitted in first person, and the same with
# a settings file that still says hide_body false (the launcher's gone "Hide Ratchet's body" Off), which nothing reads
run kerwanheli kerwanheli 3 $G=2 KEEP=1
run kerwanheli-oldhide kerwanheli 3 $G=2 'QUEST_SETTINGS={"hide_body":false}' KEEP=1
# 0.1.13: the shadow's fingers and head (OPENRAC_DEBUG: the proxies' count)
run fingershadow fingershadow 1 OPENRAC_DEBUG=1 QUEST_STATE_TRACE=1 KEEP=1
run headshadow headshadow 1 OPENRAC_DEBUG=1 KEEP=1
run death death 1 QUEST_SIM_KILL=150
run room room 1 $N=500 $A=1.6,0
run scene scene 1
# 0.1.13: Hide black bars (More controls, on; a tester's ask for third person). The game's bars (draw_letterbox_bars,
# Rerac-vr's DrawScreenFade) come with its director's short cutaways, such as Oltanis's grind race start, not in talks or
# the arrival scenes: on the rail in third person, "[vr-view] the game's black bars were wanted (height 1): hidden" and no
# opaque black strips on the panel shots; Off, none of that line and opaque black strips top and bottom on r2 to r5.
run bars rail 14 $G=29 QUEST_SIM_RAIL=2,0.3,3 QUEST_SIM_ALIGN=2620 'QUEST_SETTINGS={"view":"third","third_person_controls":"vr"}' KEEP=1
run bars-off rail 14 $G=29 QUEST_SIM_RAIL=2,0.3,3 QUEST_SIM_ALIGN=2620 'QUEST_SETTINGS={"view":"third","third_person_controls":"vr"}' 'QUEST_VR_OPTIONS=hide_bars 0' KEEP=1
# The same in first person (the owner's word: the bars show there too): the same line and shots.
run bars-first rail 14 $G=29 QUEST_SIM_RAIL=2,0.3,3 QUEST_SIM_ALIGN=2620 KEEP=1
run bars-first-off rail 14 $G=29 QUEST_SIM_RAIL=2,0.3,3 QUEST_SIM_ALIGN=2620 'QUEST_VR_OPTIONS=hide_bars 0' KEEP=1
# 0.1.9: looking down in a cutscene through his eyes, no thin line from one elbow to the other
run scenearms scenearms 1 KEEP=1
run landing landing 1 $G=10 $H=10 QUEST_SHIP_TRACE=1 KEEP=1
# 0.1.13: Aridia's landing ("[vr-scene] his eyes ducked in the ship ...: held at the seat's height", a tester's sinking
# before he climbs out), then Skid's scene where the tester had it (QUEST_SCENE_TRACE: his drawn flag 0x80 comes and
# goes as the game's camera cuts; not fixed, the scene's own draw test)
run aridia aridiascene 2 QUEST_SIM_AT=250.8,270.8,25.5,-20 QUEST_SHIP_TRACE=1 QUEST_SCENE_TRACE=1 KEEP=1
# 0.1.19: Aridia's landing climbed out of in first person (the owner's "clips through the ship before exiting"): the view
# in the seat through his duck, eased out as he springs out.
run aridialand aridialand 2 QUEST_SHIP_TRACE=1 QUEST_SCENE_EYE_TRACE=1
# 0.1.19: the seas wherever the head turns (wide_view.c: the liquid grid's cull opened under a headset; the owner's 0.1.18
# video). Aridia's sand sea and channel with the head turned from the game's camera, each liquid level's start swept
# round, and the first level walked with Max draw distance.
run aridiaback aridiaback 2 QUEST_SIM_AT=210.41,170.35,25.34,-48 KEEP=1
run aridiachannel aridiachannel 2 QUEST_SIM_AT=221.5,158.0,26.0,-138 KEEP=1
run aridiawater aridiawater 2
run watersweep-aridia watersweep 2
run watersweep-kerwan watersweep 3
run watersweep-rilgar watersweep 5
run watersweep-umbris watersweep 7
run watersweep-batalia watersweep 8
run watersweep-gaspar watersweep 9
run watersweep-oltanis watersweep 14
run maxdraw maxdraw 1
run screen screen 1
# 0.1.17: the left menu button held a second switches first and third person (a tester's ask); a tap only recentres.
run viewhold viewhold 1 KEEP=1
# 0.1.20: the view starts as the launcher says whatever vr-options.txt keeps (a tester's every start went third person),
# and the notices fade after a couple of seconds (viewnotice.sim says what to expect).
run viewnotice viewnotice 1 QUEST_STATE_TRACE=1 'QUEST_VR_OPTIONS=first_person 0;launcher_first 1'
# Third person with Gamepad, its default from 0.1.19: an Xbox controller, the D-pad by the head (RAC-Board on D-pad Up),
# Start as the menu button is let go, the hold back to first person, the Xbox mod's icons pointed for each view.
run thirdpad thirdpad 1 QUEST_RACBOARD=1 QUEST_JET_TRACE=1 QUEST_STATE_TRACE=1 'QUEST_SETTINGS={"view":"third"}' 'QUEST_MODS=, {"id": "xbox-icons", "enabled": true}' KEEP=1
# 0.1.21: the launcher's Third person buttons on "Like first person": the right grip the wrench, X the map, the menus Xbox.
run thirdlayout thirdlayout 1 QUEST_STATE_TRACE=1 'QUEST_SETTINGS={"view":"third","pad_cross":"a","pad_circle":"right_trigger","pad_square":"right_grip","pad_triangle":"right_stick","pad_r1":"left_trigger","pad_r2":"b","pad_l1":"left_grip","pad_l2":"y","pad_select":"x","pad_r3":"left_stick","pad_l3":"none"}' KEEP=1
run t1 t1 1
run t2 t2 1
# How far the eyes see, the head turned from the game's camera: the ship must show in turned and turned2. QUEST_LOD=true,
# or QUEST_HEAD_TURNS_OFF=1, to compare. Veldin's intro gives control at frame 8056.
run far far 0 QUEST_SIM_ALIGN=2620 QUEST_DRAWN_TRACE=1 KEEP=1
# A bolt's twinkle (the glints) in the world, not on the panel: no 2D strip of tbp 0x316c or 0x3158 in the UI
# residual, and horizon40's panel has none (0.1.10's had one there, behind the ship). sparkleview: the glint at
# about x 747 in level, 800 and 700 with the head 6 degrees each way (QUEST_SPARKLE_TRACE logs each glint's numbers);
# QUEST_SETTINGS='{"sparkles_in_world": false}' for the game's own, on the panel, to compare.
run sparkles sparkles 0 QUEST_SIM_ALIGN=2620 OPENRAC_UI_RESIDUAL=1 KEEP=1
run sparkleview sparkleview 0 QUEST_SIM_ALIGN=2620 OPENRAC_UI_RESIDUAL=1 QUEST_SPARKLE_TRACE=1 KEEP=1
run particles particles 1 $G=16 $H=16 QUEST_SIM_ALIGN=2620 OPENRAC_DRAW_COVERAGE=C:/RACQuest/logs/regress/particles.json
run board board 5 $N=439 QUEST_STATE_TRACE=1 QUEST_BOARD_TRACE=1
run board-kalebo board 16 $N=439 QUEST_STATE_TRACE=1 QUEST_BOARD_TRACE=1
run boardjump boardjump 5 $N=439 QUEST_STATE_TRACE=1 QUEST_BOARD_TWIST=1
# 0.1.17: Hoverboard view (Moves; a tester: "The board was not aligned and kept going sideways"): Ahead by default, the
# trace's view along the board; Side-on, 0.1.16's, about 0.8 off it and swaying; seated as standing.
run board-sideon board 5 $N=439 QUEST_STATE_TRACE=1 QUEST_BOARD_TRACE=1 'QUEST_VR_OPTIONS=board_side_on 1'
run board-seated board 5 $N=439 QUEST_STATE_TRACE=1 QUEST_BOARD_TRACE=1 'QUEST_VR_OPTIONS=seated 1'
run rail rail 14 $G=29 QUEST_SIM_RAIL=2,0.3,3 QUEST_SIM_ALIGN=2620 QUEST_STATE_TRACE=1 KEEP=1
run cable cable 3 QUEST_SIM_RAIL=0,-1.3,0 QUEST_SIM_ALIGN=2620 QUEST_STATE_TRACE=1 KEEP=1
# 0.1.13: the guns on a rail and on a cable, Akimbo's left too ([vr-akimbo] the left gun fired, [sim] item 15's rounds)
run railfire railfire 14 $G=29,15,16 $H=15 $L=16 QUEST_SIM_RAIL=2,0.3,3 QUEST_SIM_ALIGN=2620 QUEST_STATE_TRACE=1 KEEP=1
run cablefire cablefire 3 $G=15,16 $H=15 $L=16 QUEST_SIM_RAIL=0,-1.3,0 QUEST_SIM_ALIGN=2620 QUEST_STATE_TRACE=1 KEEP=1
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
# 0.1.13: the map's A (its missions, Cross) and trigger (the planet's infobot, Circle) apart; both had pressed both
run mapmissions mapmissions 1 KEEP=1
# 0.1.13: Menu pointer (More controls; a tester's ask): Both hands, the default, a trigger gives its hand the pointer
# (that pull no press); Left hand, the left trigger presses from the start.
run menupointer menupointer 1 KEEP=1
run menuleft menuleft 1 'QUEST_VR_OPTIONS=menu_hand 2' KEEP=1
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
# 0.1.18: the boots on bent legs, standing and crouched, and the Grindboots on a grind rail, the legs bent to them
run bootsbend bootsbend 1 $G=28 QUEST_BOOTS_TRACE=1 KEEP=1
run railboots railboots 14 $G=29 QUEST_SIM_RAIL=2,0.3,3 QUEST_SIM_ALIGN=2620 QUEST_STATE_TRACE=1 QUEST_BOOTS_TRACE=1 KEEP=1
run shop shop 1 $G=16,17,18,19 $N=11 QUEST_SIM_BOLTS=9000 QUEST_POINTER_TRACE=1
run shopcontrol shopcontrol 1 $N=11 QUEST_STATE_TRACE=1
run shopsweep shopsweep 1 $G=16,17,18,19 $N=11 QUEST_SIM_BOLTS=9000 QUEST_POINTER_TRACE=1
run shopbuy shopbuy 1 $G=16,17,18,19 $N=11 QUEST_SIM_BOLTS=9000 QUEST_POINTER_TRACE=1
# Eudora's landing gives control at a varying frame (3933, 4318): the script is aligned to the set-up (QUEST_SIM_ALIGN).
run shopbuy-eudora shopbuy-eudora 4 $G=16,17,18,19 $N=11 $A=1.1,0,103.4 QUEST_SIM_BOLTS=9000 QUEST_POINTER_TRACE=1 QUEST_SIM_ALIGN=2620
run shoppda shoppda 1 $G=16,17,18,19 QUEST_SIM_PDA=1 QUEST_SIM_BOLTS=9000 QUEST_POINTER_TRACE=1
run shoppda-eudora shoppda 4 $G=16,17,18,19 QUEST_SIM_PDA=1 QUEST_SIM_BOLTS=9000 QUEST_POINTER_TRACE=1 QUEST_SIM_ALIGN=2620
# 0.1.21: the PDA in the hand, its vendor opened by the trigger: the press the game keeps asks it open again inside the
# shop's pass, refused ("[vr-shop] the game asked the vendor open again"), no crash; with Akimbo's left gun and without.
run pda-held pdaakimbo 1 $G=32 $H=32 QUEST_SIM_BOLTS=9000
run pda-akimbo pdaakimbo 1 $G=15,32 $H=32 $L=15 QUEST_SIM_BOLTS=9000
# After 0.1.21 (more PDA reports on 0.1.19): opened three times, closed each, a walk between; three refusals, no crash.
run pdaagain pdaagain 1 $G=32 $H=32 QUEST_SIM_BOLTS=9000
run worldmenu worldmenu 1 QUEST_POINTER_TRACE=1
run worldmenuwall worldmenuwall 1 QUEST_PANEL_FAR=0
run giant giant 15 QUEST_SIM_NEAR=1451 QUEST_SIM_NEAR_AT=0,0 QUEST_STATE_TRACE=1 KEEP=1
run skipscene skipscene 1 QUEST_STATE_TRACE=1
run tworings tworings 1 $G=15,16,19,21 $H=16 $L=15 'QUEST_VR_OPTIONS=wheel_reach 1'
run tworings2 tworings2 1 $G=15,16,19,21 $H=16 $L=15 'QUEST_VR_OPTIONS=wheel_reach 1'
run tworings3 tworings3 1 $G=10,15,16,11,13,9,18,19,20,17,21,23,14,24,25,12,22,26,27 $H=16 $L=15 'QUEST_VR_OPTIONS=wheel_reach 1'
# 0.1.19: the two rings with Pick by pointing (the wheel's default): two lasers, one from each hand ("[sim] pointer: ...
# second beam 1 dot 1" while both are open), and no bomb in the right palm after its page turn (ring_pull).
run tworings-point tworings 1 $G=15,16,19,21 $H=16 $L=15 KEEP=1
# 0.1.17: pointing at every slot of the left hand's wheel; the Trespasser's pickup with Help Desk forced on; Help Desk turned by the hand
# through the real menu (on to start from 0.1.17, so the presses turn it off); a real new game from the title with Help Desk on
# ("voice 1 text 1"); the title's scene staying behind the main menu and Load Game.
run wheelleft wheelleft 1 $G=10,15,16,11,13,9,18,19 $H=16 $L=15 KEEP=1
run trespickup trespickup 2 $N=1005 $A=0.2,0 QUEST_HELP_TRACE=3 KEEP=1
run helpdeskon helpdeskon 1 QUEST_HELP_TRACE=1 QUEST_POINTER_TRACE=1 KEEP=1
run newgamehelp newgamehelp - QUEST_HELP_TRACE=1
run titleratchet titleratchet - --cards C:/RACQuest/pc/cards/regress-p17 KEEP=1
# 0.1.17: Smoother frames (Application SpaceWarp, the owner's ask): the eyes' motion images ([sim] motion lines, the shots'
# -motion.png): still nearly nothing, turning across, a walk spreading, the hand alone, a cut none, looking up down.
run appsw appsw 1 KEEP=1
# 0.1.17: World scale in VR Body (the owner's ask): stepped to 110 percent on the page and taken in play; from the file,
# 120 kept while the launcher's is as it was, the launcher's own when it has changed since.
run worldscale worldscale 1 KEEP=1
run worldscale-saved hands 1 $G=15,16,21 $H=21 'QUEST_VR_OPTIONS=world_scale 120;launcher_world_scale 100'
run worldscale-launcher hands 1 $G=15,16,21 $H=21 'QUEST_VR_OPTIONS=world_scale 120;launcher_world_scale 90'
# 0.1.17: Seated, the head going down does not crouch him; standing it does (crouch.sim's own).
run crouch crouch 1 QUEST_STATE_TRACE=1
run crouch-seated crouch 1 QUEST_STATE_TRACE=1 'QUEST_VR_OPTIONS=seated 1'
# 0.1.18: Ratchet's hand on the gun's grip (the owner's Blaster pictures), still on the Pyrocitor's while walking, and the left
# hand on its own gun as the right on its own, mirrored (a PCVR tester: "the left hand gun is slightly offset").
run gungrip gungrip 3 $G=15,16 $H=15 KEEP=1
run gungrip-pyro gungrip 3 $G=15,16 $H=16 QUEST_GRIP_TRACE=1 KEEP=1
run gungrip-akimbo gungrip 1 $G=15 $H=15 $L=15 KEEP=1
run akimbohands akimbohands 1 $G=15 $H=15 $L=15 KEEP=1
# 0.1.18: the left Suck Cannon fills and fires (QUEST_AKIMBO_TRACE: "1 held", then "0 held").
run akimbosuck akimbosuck 1 $G=15,9 $H=15 $L=9 $N=577 $A=5,0 QUEST_AKIMBO_TRACE=1 KEEP=1
# 0.1.18: Akimbo while running at the place of the owner's Gaspar crash: no crash.
run akimborun akimborun 9 $G=15,11 $H=15 $L=11 QUEST_SIM_AT=218.2,308.9,26.03,98
# 0.1.18: a tester's two crashes on Aridia with Akimbo (0.1.17, Quest 3S): the left Swingshot's lead pressed again and again
# with nothing to swing from, and a hand item the game made while the left hand led. No crash, the right item back each time.
run akimbo-leadspam akimboleadspam 2 $G=12 $H=12 $L=12
run akimbo-leadswitch akimboleadswitch 2 $G=15,12,16 $H=15 $L=12 QUEST_TEST_LEAD_SWITCH=16
# 0.1.20: a tester's crash on Umbris (0.1.19 PC): drowned with the left Swingshot leading. Two deaths with the left
# trigger held: each respawn drops the lead, never "the hand's slot held moby ...: deleted" (akimboleaddeath.sim).
run akimboleaddeath akimboleaddeath 1 $G=15,12 $H=15 $L=12 QUEST_SIM_KILL=400
# 0.1.18: the vendor's trigger is the shop's alone; the vendor looked round; Pokitaru's sky whole all the way round.
run shoptrig shoptrig 3 $G=10,16 $H=10 $N=11 QUEST_SIM_BOLTS=9000 QUEST_POINTER_TRACE=1
run shoplook shoplook 3 $G=10,16 $H=10 $N=11 QUEST_SIM_BOLTS=9000 KEEP=1
run resortsky resortsky 11 KEEP=1
# 0.1.18: the menus a frame at a time as they close and change pages: no dark frame of the game's backdrop.
run menuclose menuclose 3 QUEST_VIEW_TRACE=1 KEEP=1
run menuswitch menuswitch 3 QUEST_VIEW_TRACE=1 KEEP=1
# 0.1.18: the ship from Novalis to Kerwan: the flight between them in the eyes, and the glide out of his eyes into play.
run takeoff takeoff 1 QUEST_VIEW_TRACE=1 QUEST_SHIP_TRACE=1 QUEST_SCENE_EYE_TRACE=1 QUEST_SIM_MENU=14 QUEST_SIM_PLANETS=1,3,5,7 KEEP=1
# 0.1.18: X in a scene goes between his eyes and in VR only, never to the screen.
run sceneview sceneview 1
# 0.1.18: the weapon wheel opened during a Swingshot pull keeps the pull (a tester: "using the weapon selector cancels the hookshot").
run swingwheel swingwheel 3 $G=12 $H=12 $N=758 $A=10,1,180 QUEST_STATE_TRACE=1
# 0.1.18: Punch with your hands (Moves, off to start: the owner's call): turned on, the left hand free, the right with
# the Blaster; and as it starts, off.
PUNCH='QUEST_VR_OPTIONS=hand_punch 1'
run handpunch handpunch 1 $G=15 $H=15 $N=500 $A=1.2,0 "$PUNCH"
run handpunch-off handpunch 1 $G=15 $H=15 $N=500 $A=1.2,0
# 0.1.19: the right jab with the Swingshot in the hand (a tester on 0.1.18: weapon melee "just cant do it when you have the
# hookshot equipped"): "Ratchet's right punch" and the crate hit, as with the Blaster.
run handpunch-swingshot handpunch 1 $G=12 $H=12 $N=500 $A=1.2,0 "$PUNCH"
# 0.1.18: Swingshot grabs enemies and its Grab moves (Moves, each off to start: the owner's call). GRAB turns them all
# on; a row named after it is that one off.
GRAB='swing_grab 1;grab_fling 1;grab_stun 1;grab_launch 1;grab_slam 1;grab_zip 1;grab_hazards 1'
# A Novalis critter caught, yanked in and thrown; and as it starts, nothing caught.
run swinggrab swinggrab 1 $G=12 $H=12 $N=577 $A=6,0,180 "QUEST_VR_OPTIONS=$GRAB" KEEP=1
run swinggrab-off swinggrab 1 $G=12 $H=12 $N=577 $A=6,0,180
# 0.1.19: the same with Punch with your hands on too: the Swingshot's jab is a punch only with its hook home, so the catch,
# the yank and the throw are the grab's, with no [vr-fist] punch while it holds the critter.
run swinggrab-punch swinggrab 1 $G=12 $H=12 $N=577 $A=6,0,180 "QUEST_VR_OPTIONS=$GRAB;hand_punch 1"
# Grab moves, each on, and each off. From the critter's far side and early, as swinggrab:
# from OpenRAC 91b67c67 the critters are on him by about 2790 some runs, and hurt, he cannot fire (grabfling's pull at
# 2900 from the near side caught one in one run and nothing in the next; grabslam's throw up met another of them).
run grabfling grabfling 1 $G=12 $H=12 $N=577 $A=12,0,180 "QUEST_VR_OPTIONS=$GRAB"
run grabfling-off grabfling 1 $G=12 $H=12 $N=577 $A=12,0,180 "QUEST_VR_OPTIONS=$GRAB;grab_fling 0"
run grabyank grabyank 1 $G=12 $H=12 $N=577 $A=12,0,180 "QUEST_VR_OPTIONS=$GRAB"
run grabyank-off grabyank 1 $G=12 $H=12 $N=577 $A=12,0,180 "QUEST_VR_OPTIONS=$GRAB;grab_fling 0"
run grabstun grabstun 1 $G=12 $H=12 $N=577 $A=6,0 "QUEST_VR_OPTIONS=$GRAB"
run grabstun-off grabstun 1 $G=12 $H=12 $N=577 $A=6,0 "QUEST_VR_OPTIONS=$GRAB;grab_stun 0"
# The wrench's hit on the stunned one, on Eudora's Blarg trooper, which stands alone: on Novalis the critter beside the
# stunned one takes the sweep in most runs.
run grabstun-trooper grabstuntrooper 4 $G=12 $H=12 $N=427 $A=6,0 "QUEST_VR_OPTIONS=$GRAB"
run grabslam grabslam 1 $G=12 $H=12 $N=577 $A=6,0,180 "QUEST_VR_OPTIONS=$GRAB"
run grabslam-off grabslam 1 $G=12 $H=12 $N=577 $A=6,0,180 "QUEST_VR_OPTIONS=$GRAB;grab_launch 0;grab_slam 0"
# The same on Eudora's Blarg trooper, which stands alone: on Novalis the critter thrown up can meet the one beside it.
run grabslam-trooper grabslamtrooper 4 $G=12 $H=12 $N=427 $A=6,0 "QUEST_VR_OPTIONS=$GRAB"
run grabslam-trooper-off grabslamtrooper 4 $G=12 $H=12 $N=427 $A=6,0 "QUEST_VR_OPTIONS=$GRAB;grab_launch 0;grab_slam 0"
run grabzip grabzip 4 $G=12 $H=12 $N=427 $A=6,0 "QUEST_VR_OPTIONS=$GRAB"
run grabzip-off grabzip 4 $G=12 $H=12 $N=427 $A=6,0 "QUEST_VR_OPTIONS=$GRAB;grab_zip 0"
# Swingshot grabs enemies on alone, as a player who turns only that row on finds it: a catch and a plain throw, no
# Grab move (grabfling's flick while it comes in does nothing).
run grabplain grabfling 1 $G=12 $H=12 $N=577 $A=12,0,180 'QUEST_VR_OPTIONS=swing_grab 1'
# 0.1.18: VR in a multiplayer party (the owner: "vr swinging and shooting should work also with flat players"). The
# Multiplayer mod on (QUEST_MODS; it goes online only from its menu, so no party): the wrench's and the fist's hits go
# through openrac_api.moby_hit_as_game and still land; the Blaster in each hand still fires. QUEST_PARTY_ROLE=2 (a
# member of a flat host, PC tests only): "[vr-party] ... member", the Swingshot catches nothing, no wheel slow motion,
# and the hits still land.
MP='QUEST_MODS=, {"id": "multiplayer", "enabled": true, "settings": {}}'
run wrench-mp wrench 1 $G=10 $N=511 "$MP"
run handpunch-mp handpunch 1 $G=15 $H=15 $N=500 $A=1.2,0 "$MP" "$PUNCH"
run akimbo-mp akimbo 1 $G=15,16 $H=16 $L=15 "$MP"
run handpunch-member handpunch 1 $G=15 $H=15 $N=500 $A=1.2,0 "$MP" "$PUNCH" QUEST_PARTY_ROLE=2
run swinggrab-member swinggrab 1 $G=12 $H=12 $N=577 $A=6,0,180 "QUEST_VR_OPTIONS=$GRAB" QUEST_PARTY_ROLE=2
run wheel-member wheelpoint 1 $G=10,15,16,11,13,9,18,19 QUEST_PARTY_ROLE=2
# 0.1.18: with the Multiplayer mod on, it loads beside the VR mod (the fork's caps: 32 pages, 3 pause items; it was
# refused at 16 pages), and the pause menu's Options page has VR Options, VR Hands and Multiplayer, the way to VR Options
# as options.sim goes.
run options-mp options 1 "$MP" KEEP=1
# 0.1.18: Turning bolts, Trigger or hand (Moves, the default): the trigger held turns it, Ratchet still; the hand round it.
run cranktrigger cranktrigger 1 $N=280 $A=1.2,0
run cranktrigger-walk cranktrigger 1 $N=280 $A=1.2,0 'QUEST_VR_OPTIONS=bolt_walk 1'
run crankhand crankhand 1 $N=280 $A=1.2,0
# 0.1.23: a party member's bolt turn in the host's game (membercrank.sim): this game the host, a member's word put through
# as Multiplayer delivers it; the host's bolt turns with it and the game's own finish runs, the host's hero lent a tick.
run membercrank membercrank 1 $N=280 $A=1.2,0 QUEST_PARTY_ROLE=1 QUEST_FAKE_MEMBER_CRANK=1
# 0.1.18: what is worn on a hand meets the forearm: the four throwing gloves, the Walloper, the Swingshot's housing.
run glovefit-a glovefit 1 $G=10,17 $H=10 $L=17 KEEP=1
run glovefit-b glovefit 1 $G=20,25 $H=20 $L=25 KEEP=1
run glovefit-c glovefit 1 $G=18,12 $H=18 $L=12 KEEP=1
# 0.1.18: a two-handed gun walked with: the front hand keeps its place and turn on it (the Pyrocitor, the Devastator).
run holdwalk holdwalk 1 $G=16 $H=16 QUEST_SWING_TRACE=1 KEEP=1
run holdwalk23 holdwalk 1 $G=23 $H=23 QUEST_SWING_TRACE=1
# 0.1.18: Batalia's magnet ramp and wall (a PCVR tester's "magnet boots don't work"), walked, steered and stopped.
run magnet magnet 8 $G=28 QUEST_SIM_AT=279.0,274.6,37.2,-163 QUEST_STATE_TRACE=1
run magnetturn magnetturn 8 $G=28 QUEST_SIM_AT=279.0,274.6,37.2,-163 QUEST_STATE_TRACE=1 QUEST_WALK_TRACE=1
run magnetsteer magnetsteer 8 $G=28 QUEST_SIM_AT=279.0,274.6,37.2,-163 QUEST_STATE_TRACE=1 QUEST_WALK_TRACE=1
run magnetstop magnetstop 8 $G=28 QUEST_SIM_AT=279.0,274.6,37.2,-163 QUEST_STATE_TRACE=1 QUEST_WALK_TRACE=1
run magnetstop-flat magnetstopflat 8 $G=28 QUEST_SIM_AT=279.0,274.6,37.2,-163 QUEST_STATE_TRACE=1 QUEST_WALK_TRACE=1 'QUEST_SETTINGS={"view": "screen"}'
# 0.1.18: off the boots' surface the stick, throws and steps go where the view looks (a PCVR tester's 0.1.17: walking
# directions broken after falling off the walk area): [vr-boots] off the surface, the turn the rig took.
# From 0.1.19 with Stay on magnet paths off: on, the guide would take him along the strip's lean instead of off it.
run magnetoff magnetoff 8 $G=28 QUEST_SIM_AT=279.0,274.6,37.2,-163 QUEST_STATE_TRACE=1 QUEST_WALK_TRACE=1 'QUEST_VR_OPTIONS=magnet_paths 0'
# What the tester saw: the turn left as 0.1.17 left it (a PC test switch), the walk trace's "view" off its "aim".
run magnetoff-before magnetoff 8 $G=28 QUEST_SIM_AT=279.0,274.6,37.2,-163 QUEST_STATE_TRACE=1 QUEST_WALK_TRACE=1 QUEST_TEST_NO_BOOTS_FOLD=1 'QUEST_VR_OPTIONS=magnet_paths 0'
# 0.1.18: the stick turned in the boots' surface's own plane (stick_camera_yaw): Batalia's wall stood on still while the
# game's camera settles, then climbed on; and the PCVR tester's own Orxon wall walked end to end in first person.
run magnetwait magnetwait 8 $G=28 QUEST_SIM_AT=279.0,274.6,37.2,-163 QUEST_STATE_TRACE=1 QUEST_WALK_TRACE=1
run magnetorxon magnetorxon 10 QUEST_SIM_OWN=6 $G=28 QUEST_SIM_AT=270.7,323.0,33.6,78 QUEST_STATE_TRACE=1 QUEST_WALK_TRACE=1 QUEST_TIMEOUT=300 KEEP=1
# 0.1.19: a tester on 0.1.18, first person: "when you get to where the ramp turns its like the magnet shuts off". The same
# wall turned on the right stick (smooth, as the launcher has it) a little short of the head's turns, as a player does:
# with Stay on magnet paths (on to start) he is taken round the bend; off, he steps off the strip's top edge at the bend
# (0x6, then 0x7C), what the tester saw. And the stick alone held ahead from the foot, nothing turned.
run magnetorxon-turn magnetorxon-turn 10 QUEST_SIM_OWN=6 $G=28 QUEST_SIM_AT=270.7,323.0,33.6,78 QUEST_STATE_TRACE=1 QUEST_WALK_TRACE=1 QUEST_TIMEOUT=300 KEEP=1
run magnetorxon-turn-off magnetorxon-turn 10 QUEST_SIM_OWN=6 $G=28 QUEST_SIM_AT=270.7,323.0,33.6,78 QUEST_STATE_TRACE=1 QUEST_WALK_TRACE=1 QUEST_TIMEOUT=300 'QUEST_VR_OPTIONS=magnet_paths 0'
run magnetorxon-ahead magnetorxon-ahead 10 QUEST_SIM_OWN=6 $G=28 QUEST_SIM_AT=270.7,323.0,33.6,78 QUEST_STATE_TRACE=1 QUEST_WALK_TRACE=1 QUEST_TIMEOUT=300 KEEP=1
# 0.1.19: Weapons on the board (RAC-Board's setting, on to start; the owner: "make the board compatible with
# weapons/gadgets as well, so weapons can be used/equipped while riding. including wrench"). First person: the Blaster
# fired, the quick select opened from the board, a trick, the wrench swung by hand; third person (the controllers a
# pad): X swings the board's wrench at a crate, B fires; and the Swingshot from the board at Kerwan's first target
# (swingwheel.sim's place): the pull, set down back on the board and held still there; a swing, back on as it lets go.
# Weapons on the board off: racboardweapons-off.sim (the wrench in his hand as before 0.1.19, the grip taking nothing
# from him, the trigger the board's spins, nothing fired).
run racboardweapons racboardweapons 1 $G=15,12,16,10 $H=15 $N=511 QUEST_RACBOARD=1 QUEST_JET_TRACE=1 QUEST_STATE_TRACE=1 'QUEST_RACBOARD_SETTINGS={"trace": true}' KEEP=1
run racboardweapons-off racboardweapons-off 1 $G=15,12,16,10 $H=15 $N=511 QUEST_RACBOARD=1 QUEST_JET_TRACE=1 QUEST_STATE_TRACE=1 'QUEST_RACBOARD_SETTINGS={"trace": true, "weapons": false}'
run racboardwrench3 racboardwrench3 1 $G=15 $H=15 $N=511 QUEST_RACBOARD=1 QUEST_JET_TRACE=1 QUEST_STATE_TRACE=1 'QUEST_RACBOARD_SETTINGS={"trace": true}' 'QUEST_SETTINGS={"view":"third"}' KEEP=1
run racboardswing racboardswing 3 $G=12 $H=12 $N=758 $A=10,1,180 QUEST_RACBOARD=1 QUEST_JET_TRACE=1 QUEST_STATE_TRACE=1 'QUEST_RACBOARD_SETTINGS={"trace": true}' KEEP=1
run racboardswing2 racboardswing2 3 $G=12 $H=12 $N=803 $A=11.3,-5.6,-95 QUEST_RACBOARD=1 QUEST_JET_TRACE=1 QUEST_STATE_TRACE=1 'QUEST_RACBOARD_SETTINGS={"trace": true}' KEEP=1
# 0.1.19: the Weapon wheel page (More controls' last row): Swap at full speed (on to start: wheelpoint's pick comes out
# with the wheel up, "[vr-ring] Swap at full speed"), off; Slow motion speed 10 times slower ("time comes back: N ticks
# in S s", against wheelswap-off's at 3 times: both with Swap at full speed off, whose full-speed moments add ticks);
# and Left hand wheel, Its own slots (wheelleftown.sim), as it starts and with eight set.
run wheelswap-off wheelpoint 1 $G=10,15,16,11,13,9,18,19 'QUEST_VR_OPTIONS=wheel_swap_fast 0'
run wheelslow10 wheelpoint 1 $G=10,15,16,11,13,9,18,19 'QUEST_VR_OPTIONS=wheel_speed 2;wheel_swap_fast 0'
run wheelleftown wheelleftown 1 $G=10,15,16,11,13,9,18,19 $H=16 $L=15 'QUEST_VR_OPTIONS=left_own_slots 1;left_slot1 19;left_slot2 18;left_slot3 9;left_slot4 11;left_slot5 16;left_slot6 10;left_slot7 15;left_slot8 0' KEEP=1
run wheelleftsame wheelleftown 1 $G=10,15,16,11,13,9,18,19 $H=16 $L=15
# 0.1.19: Throw strength Light and Throw follows your swing off (Grab moves): swinggrab's throw slower than Full's.
run swinggrab-light swinggrab 1 $G=12 $H=12 $N=577 $A=6,0,180 "QUEST_VR_OPTIONS=$GRAB;grab_throw_strength 3"
run swinggrab-nofollow swinggrab 1 $G=12 $H=12 $N=577 $A=6,0,180 "QUEST_VR_OPTIONS=$GRAB;grab_throw_follow 0"
# 0.1.18: scenery ahead of a flown ship on Pokitaru (a PCVR tester's building that popped in), and without the level's
# own visibility data to compare.
run shipfly shipfly 11 $G=7 $N=1242 $A=2,0 QUEST_STATE_TRACE=1 KEEP=1
run shipfly-nopvs shipfly 11 $G=7 $N=1242 $A=2,0 OPENRAC_PVS_OFF=1 KEEP=1
# 0.1.18: RAC-Board in the headset (bundled, off): got out by reaching behind the back, ridden, flipped, put away.
run racboard racboard 1 QUEST_RACBOARD=1 QUEST_JET_TRACE=1 QUEST_STATE_TRACE=1 'QUEST_RACBOARD_SETTINGS={"trace": true}'
# 0.1.19: RAC-Board in first person steered to the gaze (the owner's "make sure it follows head direction"): after each
# turn of the head the heading meets its yaw and stays met.
run racboardhead racboardhead 1 QUEST_RACBOARD=1 QUEST_JET_TRACE=1 'QUEST_RACBOARD_SETTINGS={"trace": true}'
# 0.1.19: racboard's ride with 0.1.18's right trigger, which with Weapons on is the Bomb Glove's: his own bomb's hit is
# not taken on the board ("his own weapon's hit: not taken on the board", no target-board-hit).
run racboardbomb racboardbomb 1 QUEST_RACBOARD=1 QUEST_JET_TRACE=1 QUEST_STATE_TRACE=1 'QUEST_RACBOARD_SETTINGS={"trace": true}'
# 0.1.19: the Swingshot's grab while riding RAC-Board (the owner: "fling people while staying on the board with the
# swingshot"): an enemy hooked, yanked in and thrown, and the ride goes on to the left hand's reach.
run racboardgrab racboardgrab 1 $G=12 $H=12 $N=577 $A=6,0,180 "QUEST_VR_OPTIONS=$GRAB" QUEST_RACBOARD=1 QUEST_JET_TRACE=1 QUEST_STATE_TRACE=1 'QUEST_RACBOARD_SETTINGS={"trace": true}'
# 0.1.23: RAC-Board loaded beside Multiplayer (the owner: "rac board doesnt seem to work in multiplayer"): until then the
# port gave on_moby_hit to one mod, Multiplayer took it first, and RAC-Board did not load. racboard's and racboardbomb's
# rides with Multiplayer on, as they go without it.
run racboard-mp racboard 1 QUEST_RACBOARD=1 QUEST_JET_TRACE=1 QUEST_STATE_TRACE=1 'QUEST_RACBOARD_SETTINGS={"trace": true}' "$MP"
run racboardbomb-mp racboardbomb 1 QUEST_RACBOARD=1 QUEST_JET_TRACE=1 QUEST_STATE_TRACE=1 'QUEST_RACBOARD_SETTINGS={"trace": true}' "$MP"
# 0.1.18: the wrench thrown where the controller pointed as the hand opened, 15 degrees up, and along the swing. (From
# 0.1.23 the start is where the hand threw: wrenchthrow-angle throws that way, 15 degrees up.)
run wrenchthrow-angle wrenchthrow 1 $G=15 $H=15 'QUEST_VR_OPTIONS=wrench_angle 15'
run wrenchthrow-swing wrenchthrow 1 $G=15 $H=15 'QUEST_VR_OPTIONS=wrench_aim 1'
# 0.1.23: the owner's veering throw (wrenchflick.sim: the wrist cocked up 40 degrees as the grip opens): where the hand
# threw, near level; where the controller pointed, up with the wrist (0.1.22's start); along the swing. And aim help
# (wrenchhelp.sim: Novalis's first crates, the throw passing 0.24 rad wide of a class 501 crate 5.5 units off): "aim help
# to class 501", "its own side" (a crate's sides stopped the line to its middle until 0.1.23's fix), that crate "broken",
# the other three "whole"; without, no help and all four "whole".
run wrenchflick wrenchflick 1 $G=15 $H=15
run wrenchflick-point wrenchflick 1 $G=15 $H=15 'QUEST_VR_OPTIONS=wrench_aim 2'
run wrenchflick-swing wrenchflick 1 $G=15 $H=15 'QUEST_VR_OPTIONS=wrench_aim 1'
run wrenchhelp wrenchhelp 1 $G=15 $H=15 $N=500 $A=6,0.5,60 QUEST_WRENCH_TRACE=1
run wrenchhelp-off wrenchhelp 1 $G=15 $H=15 $N=500 $A=6,0.5,60 QUEST_WRENCH_TRACE=1 'QUEST_VR_OPTIONS=wrench_help 0'
# 0.1.18: Orxon's electric barrier (a PCVR tester on the US disc: undrawn, it killed them twice), faced from 12 units:
# its six "class 1067" rows, no "port:" line, and its strands in the "ahead" eyes.
run orxonwall orxonwall 10 QUEST_SIM_OWN=6 QUEST_SIM_AT=224.0,289.0,35.2,-132 QUEST_ROWS_TRACE=1067 KEEP=1
# 0.1.18: the same barrier with the head turned 70 degrees from his heading (the game's camera keeps it): its strands in
# "turned70" too (before the eye's view counted in the game's updates, only the post's glow; [vr-drawn] head -2.27, camera -1.03).
run orxonwallturn orxonwallturn 10 QUEST_SIM_OWN=6 QUEST_SIM_AT=224.0,289.0,35.2,-62 QUEST_STATE_TRACE=1 QUEST_DRAWN_TRACE=1 KEEP=1
# 0.1.18: the Mine Glove's mines thrown several ways (a tester's "one of my mines became an oval"): no "UNEVEN" row.
run mines mines 1 $G=17 $H=17 QUEST_ROWS_TRACE=74
# The same thrown by the left hand (Akimbo: a second glove the game does not know of), and one lobbed by Eudora's trooper.
run minesleft minesleft 1 $G=15,17 $H=15 QUEST_SIM_LEFT=17 QUEST_ROWS_TRACE=74
run mineseek mineseek 4 $G=17 $H=17 $N=427 $A=6,0 QUEST_ROWS_TRACE=74
echo regress-done
