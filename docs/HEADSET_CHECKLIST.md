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
