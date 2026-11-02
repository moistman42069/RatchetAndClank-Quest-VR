# Headset checklist

What the PC tests cannot reach, to try on the Quest. Each line: where, what to do, and the log line that shows
it worked (the log is in Download/RatchetQuestVR). PC-proven features are in STATUS.md and are not repeated here.

## First, every session

- **Walking with the left stick.** Confirmed working on 0.1.2 on the Quest. If it ever stops, the log's
  `[openxr] left hand:` line should name a controller, not `(none)`, with `hands 11` and `sticks 11`.
- **The launcher's update.** From 0.1.2, the Play tab offers 0.1.3 (or **Check now**); set-up and saves stay.
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
