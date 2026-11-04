# Headset checklist

What the PC tests cannot reach, to try on the Quest. Each line: where, what to do, and the log line that shows
it worked (the log is in Download/RatchetQuestVR). PC-proven features are in STATUS.md and are not repeated here.

## First, every session

- **Walking with the left stick.** Confirmed working on 0.1.2 on the Quest. If it ever stops, the log's
  `[openxr] left hand:` line should name a controller, not `(none)`, with `hands 11` and `sticks 11`.
- **The launcher's update.** Worked on a Quest from 0.1.2 to 0.1.3, then the game would not start ("cannot move
  .../openrac aside": what earlier updates left behind), fixed in 0.1.4. Each update: **Check now** on the Play tab,
  and the game must start after it, set-up and saves kept.
- **How far you see.** 0.1.4 lost every moby behind the game's camera (crates, plants, the ship, enemies), and they
  popped in close; its particles and stars went too. On 0.1.5 turn round on the first level, look across it, walk:
  nothing should pop in near you, smoke and sparks should stay, Rilgar's stars should fill the sky, and an enemy seen
  with your back to the game's camera should move and take hits.
- **Max draw distance (0.1.7, on by default).** Walk the first level and look far across it: no crate, bolt, plant or
  enemy should appear as you approach, far toads should hop about rather than stand still, and after the opening scene
  the toads and bolts should be there at once. Then switch it off in the launcher's Graphics tab and compare: things
  should pop in again at 0.1.6's distances. Note the `[vr-perf]` lines with it on and off.
- **Guns line up with the controller (0.1.8).** Up to 0.1.7 the guns followed the controller's handle, and a tester had
  to point at the floor to shoot ahead. With the Blaster and the Pyrocitor, point the controller at something and fire:
  the shot should go along the controller, the line the menu pointer takes. Point it level and the log's
  `[vr-aim] class 168 aimed by the hand: forward (x y z)` should have z near 0. If shots still go a little high or low,
  try VR Hands' Gun angle (pause, Options, VR Hands; "down" lowers the barrel) and say which value suits you. Then
  Gun size 70 percent: the gun smaller in the hand, its shots still leaving its muzzle; and Hand size 70 and 130
  percent: Ratchet's hands and a glove smaller and bigger, still on the controllers. The wrench, the gloves, the
  Swingshot and the PDA should otherwise look as in 0.1.7. The log's start says `[vr-hand] guns point where the
  controller points: gun angle 0, gun size 100 percent, hand size 100 percent`.
- **No line between the hands (0.1.9).** In 0.1.8 a thin orange line ran from one of Ratchet's arms to the other in
  cutscenes seen through his eyes. Watch the opening scene on Novalis and look down at his arms as the scene moves
  them: there should be nothing between them. In play, hold both hands out in front of you: nothing between them
  either. With Player body on Cutscenes or Always, his body should look as in 0.1.8.
- **Odd views in scenes seen through his eyes.** Your eyes follow the scene's own Ratchet even in shots where the game's
  camera looks elsewhere, so a moment can be seen from under the ground or the wrong place (on PC, a Novalis scene
  started out of its place in the story). Note the planet and the scene with your log. Press X: the scene goes on In VR,
  from the game's own camera, X again On a screen, X again back through his eyes (`[vr-scene] X in a scene: cutscenes
  ... now`). When the scene ends you should play in your View as before, first person by default, and X should open the
  map again. VR Options' Cutscenes row (after View) shows what X left, and sets it too.
- **A tester's list (0.1.10).** Each item a tester reported on 0.1.7 to 0.1.9:
  - The ship's landing on Novalis (a new game, or leave for another planet and come back): the view is Ratchet's eyes,
    turned the way he faces, as the ship flies in and he climbs out; never at his feet nor inside the ground or the
    ship, his hands not twisted. Then the gun in the hand at its usual size, and the Bomb Glove steady (no flicker),
    after the landing and after loading a save.
  - The Bomb Glove: hold a bomb in it and wave the hand: the bomb stays in the glove. The Pyrocitor: its flame along
    the barrel wherever the hand points, not where Ratchet's body faces.
  - Hand size 70 and 130 percent (VR Hands): the wrench and the guns in the palm at both, smaller and bigger with
    Items match hand size On, the game's size with it Off.
  - The Pyrocitor and the RYNO held two-handed: the left hand on the barrel where Ratchet's own left hand goes. The
    log's `[vr-hand] item 16's foregrip at (...) from the grip (Rerac-vr's table: ...)` gives both points: say if the
    first looks wrong in the headset.
  - Left-handed: Akimbo on, the Pyrocitor in the left hand. Squeeze the right grip on its barrel: both hands hold it,
    and no wrench comes out. With Mirror left-hand guns On its tank and handle sit as on a left-handed gun. The ammo
    box shows the gun fired last (Left gun ammo): the left's once it fires (`[vr-akimbo] the ammo box shows item 16's,
    the left gun's`), the right's again once that fires (`[vr-akimbo] the ammo box goes back to the held gun's`), each
    change sliding the box out and back in as a weapon change does, never flickering between the two.
  - HUD size and HUD height (VR Hands): the HUD grows, shrinks, rises and drops at once and keeps its place after a
    restart; the launcher's HUD size, changed after, wins.
  - A vendor: the bolts at the top of the view whole and readable, not at its very edge.
  - VR Options' View to Third person and back to First person, several times, on Novalis and on another planet: no
    crash. A line `[vr-hand] a wall test not asked` may show once; that is the fix at work.
  - Water: walk up to Novalis's pond from its bank. Not reproduced on PC: if the water shows only once Ratchet is in
    it, a video and the log.
- **Two testers' lists (0.1.12).** Everything here ran on PC with the PAL disc only.
  - The Swingshot, on Kerwan: its ribbed shell on the back of the wrist, the hook ahead of the hand, the rope leaving
    the hook. Swap it for the wrench and back many times: never a giant hook, the shell never see-through as the hand
    moves about or out of view. Swing on a real target, let go, swap it, bring it back: `[vr-feel] item 12 runs` only
    while the rope is out, and no buzz in the hand after. The giant hook was not reproduced on PC.
  - **The US disc:** buy the Heli-Pack from Big Al on Kerwan. The game must go on, with no `moby grid:` line in the log.
    Send the log whatever happens. On PC (PAL) the cause was found: Max draw distance ran hidden spawners, whose rail cars
    piled up in one grid cell; `[vr-grid]` is a PC aid only, so the log's word is the absence of `moby grid:`.
  - The US disc: load a save with Gun size 50 and Hand size 70. The gun at its size and both hands right from the first
    moment, not only after the wrench. Also after leaving the ship.
  - The Pyrocitor's pilot flame from the front, at Gun size 50 and 100: in the gun, in proportion.
  - The RYNO or the Pyrocitor held with both hands while walking: steady, not swaying with his steps. The left hand's
    spot where it was when you took hold.
  - Empty hand angle (VR Hands): the hands with nothing in them tilt, the wrench and the guns do not. VR Hands has nine
    rows in a window of eight: the last row reachable.
  - Pause on Novalis, Kerwan, Rilgar and a river or lava level, on both discs: the level, the hands and the seas, lava
    or mud stay; closing it does not stop the game. Then a death and a swim: fades still cover the view.
  - Scenes: at Big Al's counter on Kerwan, at his place, not the ship's seat, starting along your gaze. The ship's take-off
    and landing still from the ship (`[vr-scene] ... his ship's scene`).
  - The foot of Veldin's trees: no hard lines round the low plants as the head moves. Other levels' low plants look right.
  - Ratchet's shadow with Player body Off and Always: just the hands, or the whole body.
  - Cheats (VR Options, in play): Invincible against a real hit, Big heads with Third person, Trippy contrails.
- **A body and hands of your own (0.1.12).** New, and PC-run with a simulated headset only.
  - **Arms by default:** a new install, and a settings file from 0.1.11 with Player body Off, starts with arms from the
    shoulders to the controllers. Reach out, up, to the side, across, down: the elbow stays natural, the hand on the
    controller. VR Body's Arm length and Body height: note the values that suit you.
  - **Fingers (Tracked):** first finger off the trigger points; thumb off the buttons, stick and rest is a thumbs-up; grip
    closes the rest. The touch sensors have never run on a headset. The log's `[vr-hand] tracked fingers on class 0`
    says the path ran, and class 87 as Clank. Static brings the old hands back.
  - **World collision and World vibration:** push a hand, the wrench, a gun and a glove into a wall and the ground: each
    stops there, the buzz grows with the push and fades when rested. Nothing buzzes when nothing is touched (a tester had
    a stuck rumble). Note the frame rate with World collision and Fingers bend around the world on and off: the cost of
    their line tests has not been measured on a Quest. Each `[vr-perf]` line ends with it, `line tests N a tick, X ms a
    tick` (on PC, pushing two guns into the ground: 6 to 8 a tick, 0.02 to 0.03 ms; none with World collision off).
  - **Physical crouch:** lower your head about 25 cm and he crouches (`[vr-body] physical crouch on`), rise and he stands.
    Looking at the floor must not do it; try Crouch when head drops 15 and 40. A jump from low is a high jump. The stick
    walks him with your head low. The view must not drop twice. Seated too, after a recentre.
  - **Room scale with the whole body:** Player body Always, Body parts Everything: step about, lean, turn on the spot, walk
    up to a wall. The body stays under your head; his feet slide while you step, as his walk is not played for room steps.
  - **Mods:** the launcher's Game tab: Difficulty, Item randomizer, Xbox button icons, Button swap, then Add a mod file with
    a mod of interface 1.11; a newer one is refused and said so. On PC: with Button swap's Cross and Square, A swings
    the wrench (hero state 0x13, not the jump's 0x7) and A or the trigger no longer picks in the game's menus; Xbox button
    icons changes nothing in the headset, as the Quest's pictures are painted either way (VR Options' hint, the same to
    the pixel). All six mods load (plugins/status.json).
- **Sparkles on the bolts (0.1.11).** On 0.1.10 a small twinkle now and then seemed to hang in front of the eyes,
  tied to nothing (an owner's report, Veldin, a Quest 3): the glint the game puts on a bolt, drawn flat on the HUD's
  layer. Stand on Veldin's start and look round at the bolts lying about: now and then one twinkles, on the bolt;
  turn the head while it does and the twinkle stays on the bolt. A bolt behind a ledge or the ship twinkles only where
  it shows. No twinkle anywhere else, nor in front of the HUD. The log says `[vr-sparkle] a glint in the world` once.
  With the launcher's "Sparkles on the bolts" Off, the 0.1.10 twinkle comes back, to compare.
- **The US disc (NTSC-U).** 0.1.2 to 0.1.5 never loaded the VR mod on it: a black headset, and
  `mod quest-vr not loaded: Hud_Frame: the hook point's signature is another` in the log. On 0.1.6 or later with that
  disc the log must say `mod quest-vr 0.1.7 loaded` (or the version run), and the headset show the game. Fixed and
  checked by the disc's hook table; no NTSC run was possible here.
- **Frame rate.** The `[vr-perf]` lines. The first level ran at 60 to 70 frames a second against 72 Hz on 0.1.2 and
  0.1.4, and about 57 on 0.1.6; 0.1.7's Max draw distance draws and runs more, so compare it on and off.
- **Recentring.** Press the left controller's menu button: `[vr-rig] recentred: the left menu button`. Hold the
  headset's own button (the Meta button) to recentre: `[vr-rig] recentred: the headset's own recentre`. Either
  puts the view back where Ratchet stands, facing where you look.
- **Feel.** Every vibration is checked by log only on the PC: shots and each gun's own, a hit on you (two
  beats), bolts picked up (a light tap), the wrench's hits and its knock on walls, Hyper Strike's start and
  landing, ledge catches and the climb, the Walloper's lunge and hits, the Suck Cannon's gulp and its vacuum
  loop, the Taunter's whistle, the Swingshot's hook, the Tesla Claw and Morph-o-Ray while they fire, the packs
  (Heli rotor, Thruster jet, Hydro-Pack), a bomb in the palm, each item's click as it comes out, Clank's and
  Giant Clank's fists. Say which feel wrong, too strong or missing.
- **Wall jumps on Gaze.** VR Options' Direction is Gaze by default (the owner's choice; Rerac-vr's is Body):
  try a few wall jumps, then the same with Direction on Body, and say which works better.

## The menus and screens

| Where | Try | Look for |
|---|---|---|
| Any level | Pause (Y) | The menu stands in the world before you, level, the stopped level around it, readable; point and pull the trigger |
| Any level | Pause by a wall, and looking down | The menu comes nearer and smaller instead of into the wall (`[vr-menu] a wall before the menu`) |
| Any level | Turn your head fast in a menu or a box | Rarely, one frame may show the picture before it, turned (the eyes keep an older image when the port draws none that frame): say if it flickers |
| A vendor | Walk up to it | Its screens on its monitors, walking while shopping, the trigger on Buy |
| The PDA (Gadgetron) | Open its vendor | It stands 3.8 m ahead and stays until you leave it |
| Visibomb levels | Fire a Visibomb | Its picture on the gun's own screen in your hand, readable; the left stick flies it (`[vr-missile] the picture on the weapon's screen`) |
| Any level | A flame, a smoke, Clank's antenna light in a scene | Sprites and glows face you, not edge-on |
| The title | Look around, then point at the menus | The space scene is around you, the logo and menus stand still in the room; say if the moving camera is uncomfortable |
| Weapons, Gadgets pages | Lean left and right | Ratchet and the items stand out of their tiles with real depth |
| Ladders and rungs (Aridia) | Hang by one hand, grip a higher rung with the other | `[vr-ledge] a higher ledge in reach` |
| Eudora's vendor | Buy on foot | Bolts paid, the item received |
| VR Options | Point at each row and pull the trigger | All eight rows show at once with no scrolling. Rows are about 10 cm apart on the menu, tighter than Rerac-vr's stretched box: say if the pointer picks the wrong one |
| VR Hands (pause, Options, after VR Options) | Point at Gun angle, Gun size and Hand size, pull the trigger, push the stick left and right | Each row takes the pointer; the value steps both ways (`[vr-hand] VR Hands: gun angle ..., gun size ... percent, hand size ... percent`), the gun and hands change at once back in play, and the values are kept after a restart |
| VR Options | Set View to Third person, play, set it back | His whole body from the game's camera, the horizon level, the left stick walking where you look; say if the camera moves too much for comfort |
| Any menu page with rows | Push the sticks, hand on or off the menu | The cursor stays put (the pointer is the selection). With the right controller off, the sticks move it. The Select map still pans |

## Gear and levels the PC tests cannot start in

| Where | Try | Look for |
|---|---|---|
| Rilgar, Kalebo III (hoverboard races) | Ride the board: view side-on, left trigger boosts, right stick holds a trick, Kalebo's weapon on the right trigger | Left and right on the stick turn the board, always its own left and right; say how it feels at speed |
| Rilgar, Kalebo III | Land a jump crooked, after a spin | He rides on (`[vr-board] a crooked landing straightened along the board`); a trick left unfinished still throws him |
| Rilgar, Kalebo III | Ride a second time, or fall and respawn | The view keeps the first ride's posture; after a recentre it takes the new one |
| Grind rails (Oltanis, Kalebo) | Grind with the Grindboots | Taking a rail and the view on it are PC-proven: say if it is comfortable through curves; a soft hum while riding and a pulse as you land on it |
| Magneboot floors (Orxon) | Walk up the magnetic surfaces | The view's up follows the surface; a pulse as the boots take hold |
| Where the Metal Detector finds buried bolts | Detect and dig | A pulse at each dig, a beat while it searches |
| A moving carrier (Kerwan's transports, a hover car, a ferry) | Ride one and walk about the room | Room steps do not push you off it |
| Gemlik's ship, ship flights | Fly each ship | The cockpit view and the lasers on the trigger (boarding and flying are PC-proven on Pokitaru and the fleet) |
| Quartu (Giant Clank) | Punch with each hand, fire the missiles, turn your head | Enemies hurt by the punch's ground sweep (`[vr-fist]` lines), missiles from your hands and going where you look |
| Orxon (Clank) | Clank's ledges, his own hands | Clank grabs a ledge with the grips |
| Clank's scenes | Watch a scene as Clank | The eyes sit in Clank's head |
| Clank's little robots | Open the orders ring | The ring takes the right hand, the stick still walks |
| Kerwan cable slides | Ride a cable | The view clears the cable and wrench above (the grab and the slide are PC-proven; say how it feels) |
| The O2 Mask / pilot's helmet | Wear it | `[vr-helmet] class ... worn` and the rim around the view (seen on PC) |
| Magneboots worn | Look down at your feet | The boots are drawn |
| Swing targets (Swingshot) | Swing from a swing target, not a pull target | The swing (pull targets are PC-proven) |
| The Trespasser | Plug it into a lock | The rings hang over the tool, about 0.6 m from you (placement PC-proven, look not) |
| A bolt crank | Die with the wrench on it | It lets go (`[vr-crank] let go: he is down`) |

## Known differences from Rerac-vr

- VR Options shows all its rows in the game's Sound frame, not in a stretched box as Rerac-vr's: the rows are tighter.
- Direction is Gaze by default (the owner's choice), Rerac-vr's Body.
