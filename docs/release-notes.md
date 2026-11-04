**A test build of Ratchet & Clank in standalone VR on Meta Quest.** It is a Quest port of [npiriou's Rerac-vr](https://github.com/npiriou/Rerac-vr), running on [OpenRAC](https://github.com/OpenRAC/OpenRAC)'s native port of the PS2 game. You need your own disc image. No game files are included.

> **This is a test build, and we need your logs.** **Expect bugs.** Every log helps, including from runs that worked: **Send your log** below says how.

## New in 0.1.12

Two testers' lists and the owner's own, worked through, a body and hands of your own, and **mod support**: OpenRAC's mods in the launcher, five of them built in, and more from a file. Thanks for the videos and the logs.

**A body and hands of your own** (all of it can be changed in the game)

- **Arms with hands are the new start.** Ratchet's arms now run from his shoulders to your controllers and bend at the elbows, with a shadow to match. **A settings file from before changes once:** hands alone become arms with hands, and a **Player body** you had on becomes the whole of him.
- **New page, VR Body** (VR Options, in play): **Player body**; **Body parts**: Everything, Arms and legs, Arms only, Legs only, Body and legs, Body and arms; **Tail**; **Arms bend to your hands**; **Legs bend**; **Body turns with your head**; **Body height**; **Body forward**; **Arm length**; **View height**. The shadow shows the parts you chose, where they are bent to.
- **The whole body** stands under your head: your hips follow once you have turned well past them, your spine takes the rest, and your knees bend as your head goes down. His feet are where the game puts them, running included. When you walk around your room they slide along the ground, because his walk is not played for those steps. **Seated** and **View height** lift only your view: his body stays on its feet and his arms reach up to your hands. Neither the game nor Rerac-vr has this. Clank, Giant Clank and the Hologuise keep their own hands; only Ratchet's body bends.
- **Your fingers follow yours** (VR Hands, **Fingers**: Tracked by default, or Static for the hands as they were). From the controllers' touch sensors: your first finger off the trigger points, your thumb off the buttons is a thumbs-up, the grip closes the other three together. Clank's hands too. A controller has no sensor for each of those three fingers, so they move as one.
- **Fingers bend around the world** (VR Hands, on): they stop on what they meet, and bend back on it or curl round it, smoothly.
- **World collision** (VR Controls, on): your hands and everything you hold, wrench, guns and gloves in either hand, stop at walls and the ground.
- **World vibration** (VR Controls, on): a tap on touching, then a buzz that grows the further your hand is pushed in, fading out once you rest it there.
- **Physical crouch** (VR Controls, on): lower your head and Ratchet crouches. **Crouch when head drops** sets how far, 10 to 60 cm (25 to start). Looking down is not a crouch: it is measured at your neck. A jump from low is the game's high jump. The game's crouch cannot walk, so the stick walks him with your head still low, and he crouches again when you stop. The left grip's crouch works as before. Rerac-vr has only that grip.
- **New page, VR Controls** (VR Options, in play): Turning, Snap angle, Smooth turn speed, **Physical crouch** and **Crouch when head drops**, then the launcher's switches for your hands and body: Vibration, Aim with the right hand, Grab ledges with your hands, Swim with your arms, Swing the wrench for real, Throw by hand, Two-handed weapons, World collision, World vibration, Walk around your room and Sparkles on the bolts. A change here is saved. If you later change the same switch in the launcher, the launcher's wins.
- **Hand size and Empty hand angle work with the bending arms.** The arm reaches the hand at any size; the forearm grows from your elbow to the hand's size at the wrist, and takes half of the hand's roll, so a wrist turned over stays whole.

**Mods** (new: the launcher's Game tab)

- **Mod support.** The launcher's new **Game** tab runs [OpenRAC's mods](https://github.com/OpenRAC/rac1-mods), built for the headset. Each one is turned on there, with its own settings and presets, and counts from the game's next start.
- **Five come with it:** **Difficulty**, **Item randomizer**, **Xbox button icons** and **Button swap** (off until you turn them on) and **Ammo at the most** (on). A save made with the Item randomizer belongs to its seed.
- **Add a mod file** takes another OpenRAC mod's .zip: a script mod, a data pack, or a native mod with a build for the headset (android-arm64), of mod interface 1.11 or older. The launcher says why if it cannot take one, and **Remove this mod** takes an added one out again. The game runs what it can load and says what it refused. Mods are programs: add only ones you trust. Some, made for a flat screen, may not suit VR.
- **In VR:** the game's button pictures are already the Quest's, so **Xbox button icons** changes nothing you see in the headset. **Button swap** swaps the game's buttons after the Quest's are given: with Cross and Square swapped (its default), A swings the wrench instead of jumping, and A and the trigger no longer pick in the game's menus; the pictures stay as they were.

**The Swingshot**

- **Fixed: it faced the wrong way.** It was held rolled half a turn from Rerac-vr's. Its shell now sits on the back of your wrist, with the hook ahead of your hand.
- **Fixed: its shell going see-through.** The game keeps a marker for each thing's place, to know when it is out of sight, and it set the one for a held item at Ratchet's own hand, not yours. Once that marker fell out of your view the shell was no longer drawn, and only its shine and the hook were left. The marker now follows your hand, for everything you hold.
- **Fixed: a giant Swingshot, as far as we can tell.** The hook could be given the size meant for the wrench, about 18 times too big, when it took the wrench's place in the game's list. That write is gone, and the hook is now drawn at the shell's size. The cause comes from the code: we did not manage to make it happen here, so tell us if you still see it.
- **Fixed: a rumble that would not stop.** After a swing, each time the Swingshot came back to your hand the controller buzzed as if its rope were out. It now buzzes only while the rope is out.

**Crashes**

- **Fixed: buying the Heli-Pack from Big Al on Kerwan could stop the game** (a tester on the US disc, the moment it was bought). **The fault was this port's:** **Max draw distance** (0.1.7) also kept alive some hidden makers of things, far past where the game lets them run. On Kerwan their rail cars froze and piled up in one square of the game's table of what stands where, until that square's count ran over, and the game stops itself when that table looks wrong. Left long enough, they could fill the list of things too, and then the shop's scene crashed reading something missing. Now only things with a shape to draw are kept running, so the hidden makers are left to the game, as in Rerac-vr, which keeps running only what the headset draws; a square that is too full no longer stops the game; and the shop's scene is safe with a full list (OpenRAC's fix). On PC with the European disc the pile-up is gone (45 in the fullest square, was 142), and buying the Heli-Pack works. **Not yet run on the US disc or on a Quest:** if you get there on the US disc, please send your log, even if all goes well.
- **Closing the pause menu could stop the game** on some planets, in the same test of lines that 0.1.10 fixed elsewhere. Fixed.

**Your hands and what they hold**

- **Fixed: a smaller gun and a twisted left hand after loading a save, or after the ship** (normal again after the wrench). Sizes were read back from a moby that had already been resized; they now come from the game's own table. We did not manage to reproduce this one on PC either, so tell us if it is gone.
- **Fixed: a giant gun when paused, and the let-go wrench coming back in the pause.** Your guns, left hand's too, stay as they are in play.
- **Fixed: the Pyrocitor's pilot flame outside the gun.** It now moves with the gun and is in proportion at any Gun size.
- **Fixed: a shaky two-handed hold.** The spot for your other hand followed Ratchet's animated palm, which sways up to several centimetres as he walks, and that swung the whole gun. The spot is now fixed once you take hold.
- **New in VR Hands: Empty hand angle** (45 down to 45 up, in steps of 5). It tilts your hands when they hold nothing, if they feel turned too high. It starts at Default. **VR Hands now scrolls:** nine rows, eight at a time, like VR Options.
- **New in VR Options: Cheats** (in play only). Invincible, Infinite ammo, Infinite bolts, Big heads, Trippy contrails, Give all weapons, Give all gadgets, Unlock all planets. All off until you turn them on, and saved.

**What you see**

- **Fixed: scenes seen from inside the ship.** A scene elsewhere on a planet, such as at Big Al's counter, could put you in the ship's seat, wherever the ship was. Only a scene at the ship, such as its take-off, landing or flight away, is now seen from it, as in Rerac-vr. Other scenes also start along your gaze, not Ratchet's body. A scene with no one to see through, such as a flyover or an infobot's message, is watched from the scene's camera.
- **New: change how a cutscene is seen while it plays.** Press **X** during a cutscene to step through **Through his eyes**, **In VR** (the game's own camera, in 3D around you) and **On a screen**. **VR Options** now has the same **Cutscenes** row, after **View**, and the choice is kept for the scenes after it. It changes only cutscenes: when a scene ends, you play in the **View** you had, first or third person, and X is the map again.
- **Fixed: the world vanishing behind the pause menu.** With fades on, the level round the menu could be lost, with your hands, and seas, lava and mud could go missing. They all stay now.
- **New in the launcher's Graphics tab, all on by default:**
  - **Shadow matches what you see:** Ratchet's shadow shows only what is drawn of him. Hands only, it is the hands'; with **Player body** on, it is the whole of him.
  - **Fades fill your view:** the game's fades to black and its other full-screen colours cover the eyes, not just a flat picture.
  - **Smooth plant edges:** the hard lines round low plants, such as the ring at the foot of Veldin's trees, are gone. It is a plant, not a shadow as we first thought.
- Clank's antenna light and the Hologuise's glows are not drawn in front of your eyes in first person, as in Rerac-vr.

**The launcher and the US disc**

- **New Game tab in the launcher:** **Skip any movie** (on) and **Game volume**, beside the mods above.
- **OpenRAC's fixes since our version, taken one by one:** the European disc's freeze after picking Quartu and drowning there; the landing and the vendor's scene with a full list of things; a grey Pyrocitor on the vendor's screen; screen fills reaching the edges; leaving out what the level's own visibility data says cannot be seen; Clank's shadow; the flight between planets; two small errors in the game's line tests; and the Difficulty and Item randomizer mods' own fixes.
- **US disc:** a small maths function in the game did nothing on that disc, so some of the game's tests on things' joints read numbers never written. It now works.
- **Ledges:** opening both hands drops you. Shorter logs too.

Tested on PC with the European disc on Kerwan and other planets: the Swingshot's facing, a pull and a real swing with its rumble before and after; losing the shell when its marker is moved away; buying the Heli-Pack; the two-handed hold walking; Empty hand angle at 0 and 30 each way; scenes at Big Al's and the ship's take-off and landing; the pause on five planets. Also on PC: **Physical crouch** (looking down and a small dip do nothing, a real crouch does, and you can walk low and jump from it); the whole body and each **Body parts** choice, with **Seated** and **View height**, and their shadows; the fingers and their bend around walls; **World collision** and **World vibration** with a gun in each hand pushed into a wall and the ground, at about 8 line tests a game tick and 0.03 ms; walking around your room with the whole body; the weapon wheel; the VR Body, VR Controls and Cheats pages; X switching a cutscene's view three times over in the Novalis arrival, with play after it still in first person, and the new Cutscenes row; saving and loading with a gun in your hand; the level's own visibility data on Veldin, Novalis, Kerwan and Pokitaru, which leaves out nothing your head can see; the five bundled mods all on and loading beside the VR mod; the OpenRAC tests that this release's changes reach; and the launcher's tests. **Not yet tried on a Quest**, and nothing here has yet been run on the US disc on PC, because we only have the European one to test with here. Please tell us how all of it feels.

## New in 0.1.11

- **Fixed: a twinkle that hung in front of your eyes.** Now and then a small sparkle showed up in your view, tied to nothing, and stayed there as you looked around. It was the glint the game puts on a bolt, or a gold bolt, to catch your eye. The game draws it flat on the screen, so in the headset it landed on the HUD's layer, which moves with your head, and it showed through walls and the ship. It is now drawn in the world, on the bolt: as big as the game makes it at that distance, spinning and fading as it does, and hidden by whatever stands in front of the bolt, as on the PS2.
- **New in the launcher: Sparkles on the bolts** (On by default). Off, the twinkle is drawn as the game draws it, flat on your view, as in 0.1.10.
- Also checked, with nothing to change: the stars in the sky (drawn in the world, they stay put as you turn), the glow of lights, and the game's lens flare and queued glows, which no planet in the game turns on.

Tested on PC on Veldin, the first level, where the twinkle was reported: the one 0.1.10 put on the HUD is gone from it; a bolt's twinkle shows at the bolt and stays there as the head turns 6 degrees each way; one behind the ship is hidden by the ship. With **Sparkles on the bolts** Off, the 0.1.10 twinkle is back. The tests of far scenery, a gun's flame, the pause menu in the world and the Visibomb's screen run as before. **Not yet tried on a Quest:** tell us if you still see a twinkle that does not belong to anything.

## New in 0.1.10

A tester's list, worked through. Thanks for the feedback and the logs.

- **Fixed: the ship's take-off and landing seen from the wrong place.** In those scenes the view sat at Ratchet's feet, side-on to where he went, and could clip into the ground and the ship; seen from there his hands looked twisted or upside down. The view is now his eyes, turned the way he faces, as in Rerac-vr: as the ship flies in, as he climbs out, as he stands by it.
- **Fixed: a tiny gun after a trip in the ship.** After you changed planets, a size meant for the wrench on the planet you had left could be put on whatever took its place, such as your gun. It is now forgotten when the planet changes. The same mistake could make the Bomb Glove flicker after leaving the ship or loading a save, and should be gone too: tell us if you still see it.
- **Fixed: the Bomb Glove's bomb and the Pyrocitor's flame wandering.** A bomb, a mine or a Decoy held in a glove now stays in it as you move your hand, and the Pyrocitor's flame now comes out where the gun points, not where Ratchet's body faces.
- **Your hand stays on what it holds, at every Hand size.** With a Hand size other than 100 percent, the wrench and the guns sat off Ratchet's hand. His palm is now put where it goes on the item, at whatever size each is drawn.
- **Two-handed guns: the off-hand's spot is where its hand is drawn.** Once you squeezed a big gun such as the Pyrocitor with your other hand, that hand was drawn where Ratchet holds the gun (its side handle, up and to the left of the trigger hand), but the spot you had to reach, and the line the gun aimed along, came from a fixed table that put it under the barrel. The spot is now read from Ratchet's own hold, so the hand you see, the spot you squeeze and the aim all agree. It moves with the gun's size.
- **Left-handed: two hands on the left gun.** With a gun in the left hand (Akimbo) held with both arms, such as the Pyrocitor, squeeze the right grip on its foregrip to hold it in both hands, the same way the left hand holds a right-hand gun. The spot is Ratchet's own hold mirrored: the gun's side handle, which a mirrored gun has on its right. While the right hand holds it, the right grip does not bring out or swing the wrench.
- **New in VR Hands** (pause, Options, VR Hands), all saved:
  - **Items match hand size** (On by default): the wrench and the guns follow your **Hand size** too, so a smaller hand holds a smaller wrench. Off, only the hands and the gloves change size, as in 0.1.9.
  - **Mirror left-hand guns** (On by default): a gun in the left hand is drawn as a left-handed version of itself, its handle, side grip and tank on the other side. Off, only its grip is turned round, as in 0.1.9.
  - **Left gun ammo** (On by default): the game's ammo box shows the gun you fired last: the left hand's once it fires, the right hand's again once that one fires, and the left's whenever the right hand holds nothing with ammo. At each change the box slides out and back in with the other gun's, as when you change weapons.
  - **HUD size**, from 50 to 150 percent. It starts at the launcher's HUD size, which still wins if you change it there afterwards.
  - **HUD height**, from 30 cm down to 30 cm up: where the HUD hangs, if it sits in the way or out of sight.
- **The store's bolt count is easier to read.** The counters in its top corners, your bolts among them, were cut off at the edge of your view. They are now drawn bigger and moved in from the edge.
- **Fixed: a crash switching from third person back to first.** For a moment as the view moved back, the game was asked about lines far from Ratchet, and on some planets it stopped there. Those lines are no longer asked.
- **Water before you are in it:** we could not reproduce this one yet. Ponds and lakes show from their banks here, on PC. If you still see water appear only once you are in it, a short video, with the planet and the log, would help a lot.

Tested on PC on the first level: the landing seen from Ratchet's eyes; a bomb kept in the glove as the hand moves and turns; the Pyrocitor's flame from its muzzle with the hand level, up and turned aside; a smaller hand still on the wrench and the Blaster; the off-hand on the RYNO's and the Pyrocitor's side handle; the right hand holding the left hand's Pyrocitor with no wrench coming out; the ammo box following the gun fired last, its numbers matching each gun's rounds; the store's counters whole; third person and back with no crash; every new VR Hands row stepped and saved. Not reproduced on PC: the tiny gun after the ship (its cause was found in the code), the glove's flicker (likely the same cause), and the water. **Not yet tried on a Quest:** tell us how each of these feels in the headset.

## New in 0.1.9

- **Fixed: a thin orange line between Ratchet's hands.** In cutscenes seen through his eyes, a thin line ran from one of his arms to the other. In first person the game draws Ratchet as just his forearms and hands, and folds the rest of him away at his elbows; a sliver of his fur, stretched from one elbow to the other, was still drawn. It is now left out. The same goes in play, where his hands follow the controllers. His hands, his forearms and everything else look as before.

Tested on PC in the Novalis arrival scene, looking down at his arms: the line is gone and nothing else in the picture changed, with OpenGL and with OpenGL ES (what the Quest draws with). His hands in play, and his whole body with **Player body** set to Cutscenes or Always, look as before. **Not yet tried on a Quest:** tell us if you still see a line.

## New in 0.1.8

- **Fixed: guns aimed high.** The Blaster, Pyrocitor and every other gun followed the controller's handle, which on Quest controllers slopes up well above where the controller points. To shoot straight you had to point the controller at the floor. Guns now point where the controller points, the same direction as the menu pointer, for everyone, with nothing to set. The wrench and the gloves were already right, and they stay as they were.
- **New: VR Hands**, in the pause menu's Options, after VR Options. **Gun angle** turns the guns up or down from where the controller points, in steps of 5 degrees, up to 45 each way. Use it if your shots still go a little high (choose a "down" value) or low (choose an "up" value). **Gun size** draws the guns smaller, from 100 percent (the game's own size) down to 50 percent. **Hand size** draws Ratchet's hands and forearms, and the gloves, from 50 to 150 percent of their size. All three are saved. The two sizes only change what you see: a smaller gun still fires from its muzzle.
- The same angle applies to the left hand's gun (Akimbo), to turrets aimed by the hand, and to the Visibomb's screen on the gun. The Swingshot's hook, and a glove's throw when **Throw by hand** is off, also aim where the controller points now.

Tested on PC with the controller's handle sloped 40 degrees above its pointer, as on a Quest: shots now go where the hand points, and pointing level fires level. **Not yet tried on a Quest:** tell us if the guns now line up with your controller, and which Gun angle you prefer if not Default.

## New in 0.1.7

- **Max draw distance: no more pop-in.** A new switch in the launcher's **Graphics** tab, **on by default**. Everything the game places is drawn as far as you can see: enemies, crates, bolts, plants, your ship, smoke and sparks. Nothing appears any more as you walk up to it.
- **Far enemies move.** What you can see far off now counts as seen, so the game keeps it running instead of leaving it standing still until you come near. Enemies and bolts the game hides during a scene come back when the scene ends, not when you reach them.
- **It costs frame rate.** If a planet gets less smooth, switch it off: you are back to 0.1.6's distances. The **Performance** preset switches it off, and **Original draw distances** On overrides it. In the game's own Options, **Scenery detail** now goes Original, Full, Max.

Tested on PC, on the first level, walking and looking round: hundreds of things appeared in the distance with it off, none with it on. Something the game only creates as you come near (a spawned enemy) can still appear then: there is nothing to draw before. **Not yet tried on a Quest:** tell us how smooth it stays, and if anything still pops in.

## New in 0.1.6

- **Fixed: a black headset with the US disc (NTSC-U, SCUS-97199).** On that disc the VR part of the game never loaded, in every release from 0.1.2 to 0.1.5. The game started behind it, but the headset stayed black, and the log said `mod quest-vr not loaded: Hud_Frame: the hook point's signature is another`. One game function the VR mod hooks was declared with a slightly different type for the US disc than for the European one, and the game refuses a mod whose hooks do not match exactly. Both discs now match, and every build now stops if any hook the mod takes differs on either disc, so this cannot ship again. Thanks to the tester who sent their log.
- **The launcher's Logs tab now shows why the VR mod did not load**, if it ever does not.
- Nothing changes on the European disc (PAL).

**The US disc is still the less tested one:** this fix is checked against its tables, not yet played. If you have that disc, a log from 0.1.6 is especially welcome, whether it works or not.

## New in 0.1.5

- **Fixed: the short view distance.** The game only draws what is in front of its own camera, and that camera sits behind Ratchet and does not turn with your head. Looking away from it, crates, plants, your ship and enemies more than a few metres off were simply not there, and popped in as you walked up. Smoke, sparks, flying traffic and the stars went the same way. They are now drawn for where you look, in first and third person, as in Rerac-vr.
- **What you can see is awake.** The same camera decided what the game runs: an enemy behind it could stand frozen in plain sight. What you can see now counts as seen, and that goes for fires, the orbs on nanotech crates and Kerwan's traffic too. This is tested on a PC, not yet in a real fight on a headset: if anything stands still while you look at it, tell us.
- **All the scenery is drawn by default.** **Original draw distances** is a low-end option that had been left on. It is now off for new installs and on Balanced, and an existing install is moved to Off once, on its first start after this update. Only the **Performance** preset still turns it on. It stays in the launcher's Graphics tab.

More on screen costs some frame rate: if a planet got less smooth, tell us which.

## New in 0.1.4

- **Fixed: "The game could not be started: cannot move .../openrac aside".** Updating the app, by the launcher or by installing over it, could leave old files behind that stopped the next update from starting the game. **If you have that message, you do not need to reinstall:** open the launcher's Play tab, press **Check now**, and install 0.1.4. Your set-up and saves are not touched.

## New in 0.1.3

- **You can put a gun or a glove down.** Squeeze the right grip for the wrench and let go slowly: your hand is now empty, as in Rerac-vr. In 0.1.2 the last item came straight back, so a Bomb Glove could not be put away. The right trigger brings it back when you want it; that pull fires nothing, the next one does.
- **Left grip:** it can no longer send a stray crouch in the moment the left hand takes a ledge. Crouching on the left grip itself is by design, as in Rerac-vr; it stays off while that hand holds a ledge or a big weapon's barrel.

**Already installed?** Open the launcher's Play tab: it offers this update by itself, or press **Check now**. Your set-up and saves are kept.

## What you need

- **Meta Quest 3 or 3S** in Developer Mode. Quest 2 and Quest Pro may run it, slower; they are untested.
- **About 6 GB free** on the headset.
- **Your own `.iso` of exactly one of these discs.** Greatest Hits, Platinum, other regions and the sequels are not supported, and the launcher says exactly why a disc is refused.

| Disc | Serial | Version | Size in bytes | SHA-1 |
| --- | --- | --- | --- | --- |
| PAL (Europe), best supported | SCES-50916 | v2.00 | 4,214,784,000 | `ef304c641c7cd426ba492707dda5974679befbc0` |
| NTSC-U (North America) | SCUS-97199 | v1.00 | 4,214,095,872 | `aee9275384f739c9b24cd5de6f7e200292db7b01` |

**NTSC-U is barely tested:** one tester's run on 0.1.5 showed the VR mod did not load on it (fixed in 0.1.6). It is built, and its addresses and the mod's hooks are checked against the disc's tables, but all other testing was on PAL. Reports from that disc are especially welcome.

## Install

1. Put your Quest in **Developer Mode** (in the Meta Horizon app on your phone).
2. Get `RatchetQuestVR-0.1.12.apk` onto the headset and install it. Either:
   - download it in the Quest's browser (or email it to yourself and download it there) and open it from the **Files** app, or
   - sideload it with SideQuest or `adb install -r RatchetQuestVR-0.1.12.apk`.
3. Open **Ratchet & Clank Quest VR** from your Library's **Unknown Sources**.
4. Copy your `.iso` to the headset's **Download** folder and choose it in the launcher. The first set-up copies the disc and prepares the levels: **up to half an hour, once. Keep the headset on charge.**
5. Press **PLAY IN VR**. **Play in 2D** puts the game on a big flat screen, played with the Touch controllers as a PlayStation pad: if VR fails, try it and tell us, as it separates a game problem from a VR problem.

**Updating:** install a new APK over the old one, or use the launcher's Play tab, which checks this page every six hours (**Check now** to do it at once; you can switch it off). **Do not uninstall or clear the app's data:** your set-up and saves go with it.

## Send your log

1. Play, then **quit and open the launcher again.** That is what writes the run's log.
2. In **Download/RatchetQuestVR** on the headset, the newest `run_<date>.txt` is your last run. The launcher's **Logs** tab shows a summary.
3. Attach it to a [**headset report**](https://github.com/moistman42069/RatchetAndClank-Quest-VR/issues/new?template=headset-report.yml). You can open that page in the Quest's browser, or plug the headset into a PC and copy the file from there. Or DM **@MeWhenINameMyself** on Discord.

The log holds your headset's model and Android version, the game's own log, why VR stopped if it did, and the app's Android log. Tell us your headset, disc, planet, what you did, and any settings you changed. A video helps.

**Most useful right now:**

- **With the US disc (NTSC-U):** buying the Heli-Pack from Big Al on Kerwan, loading a save, and any log from that disc.
- **The Swingshot:** which way it faces, its size after swaps, any buzz that will not stop.
- **Two-handed guns while walking**, and whether **Empty hand angle** gets your hands right.
- **Your fingers and the world:** a point, a thumbs-up, a fist; a hand pushed into a wall; how strong **World vibration** feels; whether the game stays smooth with **World collision** on.
- **Physical crouch:** whether it comes on when you want it and not otherwise, and which **Crouch when head drops** suits you.
- **The arms and the whole body:** elbows and shoulders that look wrong, **Arm length** and **Body height** values that suit you.
- **How far you can see:** turn round, look across a level and walk, with **Max draw distance** on. Tell us if anything still pops in, and how smooth it stays.
- **Did the launcher's update work?** It has worked on a Quest from 0.1.2 to 0.1.3, and the game starts after updating to 0.1.4.
- **Frame rate:** which planets stay smooth, which do not, and at which settings.
- If a bar in the headset says a controller is missing, tell us.
- Anything that crashes, freezes or leaves you stuck: the log says why far more often than not.

The full list of what to try is in [docs/HEADSET_CHECKLIST.md](https://github.com/moistman42069/RatchetAndClank-Quest-VR/blob/v0.1.12/docs/HEADSET_CHECKLIST.md).

## What is in it

Rerac-vr's design, rebuilt on OpenRAC so that a headset can run it:

- **First person** through Ratchet's eyes at his height, or **third person** from the game's camera. Switch at any time in **VR Options**.
- **Your hands are his hands.** Real wrench swings, throws and Hyper Strikes; guns that aim where your hand points; thrown bombs, mines and Decoys; a real punch for the Walloper; two-handed heavy weapons; **Akimbo** (off by default). His arms bend to your controllers, his fingers follow yours, and all of it stops at the world's walls.
- **Moving with your body.** Grab ledges with your hands and climb, swim with your arms, turn bolt cranks by walking round them, plug in the Hydrodisplacer and Trespasser by pushing them in, walk around your room, crouch by crouching.
- **Levels and vehicles.** Hand-aimed turrets, the hoverboard ridden side-on and steered about its own heading, grind rails with a smooth view, flown ships from the cockpit, cable slides.
- **Clank and Giant Clank** in first person with their own hands and fists, and the Hologuise.
- **Menus in the world.** The title's 3D scene around you, the pause menu standing in the level with its 3D models in stereo; point and pull the trigger. Vendors on their own monitors, shopping as you walk. The quick select by reaching for a weapon, two rings with Akimbo.
- **Cutscenes** through Ratchet's eyes (the default), in VR around you, or on a screen, chosen in the launcher's **VR** tab or VR Options, or with X as one plays. Y skips them.
- **Feel.** Vibration for each weapon, hits, packs, ledges and more. On-screen hints show your controller's buttons. The Visibomb's picture on the weapon's own screen.
- **Settings.** Graphics presets, 72 to 120 Hz, resolution, MSAA, smooth or snap turning, and a toggle for nearly everything.
- **Mods**, new to this port: OpenRAC's mods in the launcher's **Game** tab, with **Difficulty**, **Item randomizer**, **Xbox button icons**, **Button swap** and **Ammo at the most** built in, and more added from a file.

## Controls

| Control | Action |
| --- | --- |
| Left stick | Walk, towards where you look |
| Right stick | Turn (smooth by default; snap in the launcher). In third person, the game's camera |
| A | Jump; twice for a double jump |
| B | Hover and strafe with Clank's packs, Hydro-Pack boost |
| X | Map; in a cutscene, how it is seen (through his eyes, in VR, on a screen) |
| Y | Pause; skips a cutscene |
| Right trigger | Fire; with the wrench in hand, takes your last gun back out |
| Right grip | Takes the wrench out; swing your hand for real to hit, let go mid-swing to throw |
| Left grip or trigger | Crouch, and the packs (with A: long and high jumps, flips). Lowering your head crouches too |
| A grip by an edge | Grab a ledge; push both hands down or press A to climb; let go of both to drop |
| Right stick click | Talk; hold for quick select, move your right hand towards a weapon, let go |
| Left stick click | With Akimbo on: the left hand's own quick select |
| Left menu button | Recentre (the headset's own recentre too) |

**Menus:** point with your right hand and pull the trigger or press A. B goes back. **Hoverboard:** left stick steers, left trigger boosts, right trigger shoots, A jumps, right stick for tricks. The whole list is in the launcher's **Controls** tab.

## VR Options

In the game: pause, **Options**, **VR Options** (or **VR Options** on the title's main menu): Helmet, Boots, Camera flip, Direction (Gaze by default, or Body), Player body, Akimbo, Play position (Standing or Seated), **View** (First or Third person), **Cutscenes** (Through his eyes, In VR or On a screen; X does the same during one) and, in play, **Cheats**, **VR Body** and **VR Controls**. **VR Hands** comes after it: Gun angle, Gun size, Hand size, Empty hand angle, Items match hand size, Mirror left-hand guns, Left gun ammo, HUD size, HUD height, Fingers, Fingers bend around the world.

## Tested, and not tested

**On a real Quest (a Quest 3):** on 0.1.2 both controllers bind, walking works, and the first level plays, at about 60 to 70 frames a second against the headset's 72 Hz. The launcher's update has worked from 0.1.2 to 0.1.3, and 0.1.4 starts after an update. Most of the game has yet to be played on one.

**On PC, with the PAL disc and a simulated headset:** menus, saving and loading, vendors, weapons, wrench, ledges, swimming, cranks, turrets, Clank and Giant Clank, scenes, the hoverboard on Rilgar and Kalebo III, grind rails, Kerwan's cable, and flying the ships of Pokitaru and the fleet. The full table is in [docs/STATUS.md](https://github.com/moistman42069/RatchetAndClank-Quest-VR/blob/v0.1.12/docs/STATUS.md). A simulated headset proves that a feature runs, not how it feels: aim angles, comfort and haptics need real hands.

**On a real Quest with the NTSC-U disc:** one tester's run on 0.1.5: the game started, the VR mod did not load (fixed in 0.1.6, checked against the disc's tables, not yet played).

**Never run anywhere:** Magneboots floors, the orders ring for Clank's little robots, Gemlik's ship, Clank's ledges, catching a higher rung from a hang, and the hoverboard's weapon on Kalebo III.

## Known issues

- **Frame rate:** the first level ran at about 60 to 70 frames a second on 0.1.4, and about 57 on 0.1.6, under the headset's 72 Hz, so it may not look perfectly smooth. 0.1.7's Max draw distance draws more again and has not yet been measured on a Quest: switch it off in the launcher's Graphics tab if a planet gets less smooth. 120 Hz is tried at the start and falls back to 72 Hz. Improving this is next. A tester's log on the US disc showed **47 to 60 frames a second on Aridia, 48 on average**. The log's `[vr-perf]` lines now also say how many of the game's line tests your hands, fingers and the menu pointer ask each tick, and how long they take (on PC about 8 a tick, 0.03 ms, while pushing against the world): please send a log either way.
- **Feet slide when you walk around your room:** the steps move Ratchet without his walk, so his feet slide along the ground.
- **Cutscenes seen through Ratchet's eyes can show an odd view for a moment.** With **Cutscenes** on **Through his eyes** (the default), your eyes ride on the scene's own Ratchet, even in a shot where the game's camera is looking somewhere else. The game never shows him in those shots, so he can be standing below the ground or off to one side, and for that moment you see the level from underneath or from the wrong place. It is only the view: the scene plays and ends as normal. We saw it once on PC, in a Novalis scene that the test started out of its place in the story. If it happens, press **X**: the scene goes on **In VR**, from the game's own camera, and X again puts it **On a screen** (On a screen was checked in that scene on PC). VR Options' **Cutscenes** row and the launcher's **VR** tab set it too, and Y skips the scene.
- **A full list of things can still stop the game** at a place in the game other than the scenes fixed here, if the list fills there. The fixes in this release keep it from filling on Kerwan; tell us where it happens, with your log.
- VR Options' rows sit close together (about 10 cm on the menu), closer than in Rerac-vr. Tell us if the pointer picks the wrong row.
- A crooked hoverboard landing is straightened instead of throwing you off, as in Rerac-vr, but how much that helps is unproven.
- In third person the pause menu is a flat panel, not in the world.
- The picture may rarely show one turned frame when you turn your head fast in a menu.
- Direction defaults to Gaze here; Rerac-vr's default is Body. Please try wall jumps on both.

## If you play on PC

**Play [Rerac-vr](https://github.com/npiriou/Rerac-vr).** It is the complete experience, and the one this port follows. It runs on [ReRAC](https://github.com/re-rac/rerac), a Rust rewrite of the game that a headset's processor cannot yet carry at frame rate, which is why this port runs OpenRAC's recompiled game instead.

## Credits and licence

Built on **[Rerac-vr](https://github.com/npiriou/Rerac-vr)** by npiriou, the main base of this project and its VR design (ISC), and on **[OpenRAC](https://github.com/OpenRAC/OpenRAC)** and [rac1-mods](https://github.com/OpenRAC/rac1-mods) (GPL-3.0-or-later). Thanks also to the [ReRAC](https://github.com/re-rac/rerac) team. Full [credits](https://github.com/moistman42069/RatchetAndClank-Quest-VR/blob/v0.1.12/CREDITS.md) and [third-party notices](https://github.com/moistman42069/RatchetAndClank-Quest-VR/blob/v0.1.12/THIRD_PARTY_NOTICES.txt).

Please **report bugs in this port here**, not to Rerac-vr, ReRAC or OpenRAC.

This is free software under GPL-3.0-or-later, without any warranty. The source of this release is its tag: this repository, OpenRAC at the submodule's commit (`293cb5cc`), and `patches/`, which carries OpenRAC's later fixes named above. It is an unofficial community project, not affiliated with or endorsed by Sony Interactive Entertainment or Insomniac Games. Ratchet & Clank is their property: the licence gives no rights in it, and no game files are distributed.

## Download

| File | SHA-256 |
| --- | --- |
| `RatchetQuestVR-0.1.12.apk` (version code 13, 48 MB) | `2f61bd7448ea622259a62085a6581df7c5f380c5c49fdb34bef001d32515c3ff` |
