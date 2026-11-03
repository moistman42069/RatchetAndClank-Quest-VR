**A test build of Ratchet & Clank in standalone VR on Meta Quest.** It is a Quest port of [npiriou's Rerac-vr](https://github.com/npiriou/Rerac-vr), running on [OpenRAC](https://github.com/OpenRAC/OpenRAC)'s native port of the PS2 game. You need your own disc image. No game files are included.

> **This is a test build, and we need your logs.** **Expect bugs.** Every log helps, including from runs that worked: **Send your log** below says how.

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
2. Get `RatchetQuestVR-0.1.10.apk` onto the headset and install it. Either:
   - download it in the Quest's browser (or email it to yourself and download it there) and open it from the **Files** app, or
   - sideload it with SideQuest or `adb install -r RatchetQuestVR-0.1.10.apk`.
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

- **With the US disc (NTSC-U):** does VR start now? Any log from that disc.
- **How far you can see:** turn round, look across a level and walk, with **Max draw distance** on. Tell us if anything still pops in, and how smooth it stays.
- **Did the launcher's update work?** It has worked on a Quest from 0.1.2 to 0.1.3, and the game starts after updating to 0.1.4.
- **Frame rate:** which planets stay smooth, which do not, and at which settings.
- If a bar in the headset says a controller is missing, tell us.
- Anything that crashes, freezes or leaves you stuck: the log says why far more often than not.

The full list of what to try is in [docs/HEADSET_CHECKLIST.md](https://github.com/moistman42069/RatchetAndClank-Quest-VR/blob/v0.1.10/docs/HEADSET_CHECKLIST.md).

## What is in it

Rerac-vr's design, rebuilt on OpenRAC so that a headset can run it:

- **First person** through Ratchet's eyes at his height, or **third person** from the game's camera. Switch at any time in **VR Options**.
- **Your hands are his hands.** Real wrench swings, throws and Hyper Strikes; guns that aim where your hand points; thrown bombs, mines and Decoys; a real punch for the Walloper; two-handed heavy weapons; **Akimbo** (off by default).
- **Moving with your body.** Grab ledges with your hands and climb, swim with your arms, turn bolt cranks by walking round them, plug in the Hydrodisplacer and Trespasser by pushing them in, walk around your room.
- **Levels and vehicles.** Hand-aimed turrets, the hoverboard ridden side-on and steered about its own heading, grind rails with a smooth view, flown ships from the cockpit, cable slides.
- **Clank and Giant Clank** in first person with their own hands and fists, and the Hologuise.
- **Menus in the world.** The title's 3D scene around you, the pause menu standing in the level with its 3D models in stereo; point and pull the trigger. Vendors on their own monitors, shopping as you walk. The quick select by reaching for a weapon, two rings with Akimbo.
- **Cutscenes** through Ratchet's eyes, in VR around you, or on a screen. Y skips them.
- **Feel.** Vibration for each weapon, hits, packs, ledges and more. On-screen hints show your controller's buttons. The Visibomb's picture on the weapon's own screen.
- **Settings.** Graphics presets, 72 to 120 Hz, resolution, MSAA, smooth or snap turning, and a toggle for nearly everything.

## Controls

| Control | Action |
| --- | --- |
| Left stick | Walk, towards where you look |
| Right stick | Turn (smooth by default; snap in the launcher). In third person, the game's camera |
| A | Jump; twice for a double jump |
| B | Hover and strafe with Clank's packs, Hydro-Pack boost |
| X | Map |
| Y | Pause; skips a cutscene |
| Right trigger | Fire; with the wrench in hand, takes your last gun back out |
| Right grip | Takes the wrench out; swing your hand for real to hit, let go mid-swing to throw |
| Left grip or trigger | Crouch, and the packs (with A: long and high jumps, flips) |
| A grip by an edge | Grab a ledge; push both hands down or press A to climb; let go of both to drop |
| Right stick click | Talk; hold for quick select, move your right hand towards a weapon, let go |
| Left stick click | With Akimbo on: the left hand's own quick select |
| Left menu button | Recentre (the headset's own recentre too) |

**Menus:** point with your right hand and pull the trigger or press A. B goes back. **Hoverboard:** left stick steers, left trigger boosts, right trigger shoots, A jumps, right stick for tricks. The whole list is in the launcher's **Controls** tab.

## VR Options

In the game: pause, **Options**, **VR Options** (or **VR Options** on the title's main menu): Helmet, Boots, Camera flip, Direction (Gaze by default, or Body), Player body, Akimbo, Play position (Standing or Seated) and **View** (First or Third person).

## Tested, and not tested

**On a real Quest (a Quest 3):** on 0.1.2 both controllers bind, walking works, and the first level plays, at about 60 to 70 frames a second against the headset's 72 Hz. The launcher's update has worked from 0.1.2 to 0.1.3, and 0.1.4 starts after an update. Most of the game has yet to be played on one.

**On PC, with the PAL disc and a simulated headset:** menus, saving and loading, vendors, weapons, wrench, ledges, swimming, cranks, turrets, Clank and Giant Clank, scenes, the hoverboard on Rilgar and Kalebo III, grind rails, Kerwan's cable, and flying the ships of Pokitaru and the fleet. The full table is in [docs/STATUS.md](https://github.com/moistman42069/RatchetAndClank-Quest-VR/blob/v0.1.10/docs/STATUS.md). A simulated headset proves that a feature runs, not how it feels: aim angles, comfort and haptics need real hands.

**On a real Quest with the NTSC-U disc:** one tester's run on 0.1.5: the game started, the VR mod did not load (fixed in 0.1.6, checked against the disc's tables, not yet played).

**Never run anywhere:** Magneboots floors, the orders ring for Clank's little robots, Gemlik's ship, Clank's ledges, catching a higher rung from a hang, and the hoverboard's weapon on Kalebo III.

## Known issues

- **Frame rate:** the first level ran at about 60 to 70 frames a second on 0.1.4, and about 57 on 0.1.6, under the headset's 72 Hz, so it may not look perfectly smooth. 0.1.7's Max draw distance draws more again and has not yet been measured on a Quest: switch it off in the launcher's Graphics tab if a planet gets less smooth. 120 Hz is tried at the start and falls back to 72 Hz. Improving this is next.
- VR Options' rows sit close together (about 10 cm on the menu), closer than in Rerac-vr. Tell us if the pointer picks the wrong row.
- A crooked hoverboard landing is straightened instead of throwing you off, as in Rerac-vr, but how much that helps is unproven.
- In third person the pause menu is a flat panel, not in the world.
- The picture may rarely show one turned frame when you turn your head fast in a menu.
- Direction defaults to Gaze here; Rerac-vr's default is Body. Please try wall jumps on both.

## If you play on PC

**Play [Rerac-vr](https://github.com/npiriou/Rerac-vr).** It is the complete experience, and the one this port follows. It runs on [ReRAC](https://github.com/re-rac/rerac), a Rust rewrite of the game that a headset's processor cannot yet carry at frame rate, which is why this port runs OpenRAC's recompiled game instead.

## Credits and licence

Built on **[Rerac-vr](https://github.com/npiriou/Rerac-vr)** by npiriou, the main base of this project and its VR design (ISC), and on **[OpenRAC](https://github.com/OpenRAC/OpenRAC)** and [rac1-mods](https://github.com/OpenRAC/rac1-mods) (GPL-3.0-or-later). Thanks also to the [ReRAC](https://github.com/re-rac/rerac) team. Full [credits](https://github.com/moistman42069/RatchetAndClank-Quest-VR/blob/v0.1.10/CREDITS.md) and [third-party notices](https://github.com/moistman42069/RatchetAndClank-Quest-VR/blob/v0.1.10/THIRD_PARTY_NOTICES.txt).

Please **report bugs in this port here**, not to Rerac-vr, ReRAC or OpenRAC.

This is free software under GPL-3.0-or-later, without any warranty. The source of this release is its tag: this repository, OpenRAC at the submodule's commit (`293cb5cc`), and `patches/`. It is an unofficial community project, not affiliated with or endorsed by Sony Interactive Entertainment or Insomniac Games. Ratchet & Clank is their property: the licence gives no rights in it, and no game files are distributed.

## Download

| File | SHA-256 |
| --- | --- |
| `RatchetQuestVR-0.1.10.apk` (version code 11, 47 MB) | `275a85d28e59661c442cc6599f26429e3c1bbfd6f0e3f561147f6d01f81fedde` |
