# Ratchet & Clank Quest VR: release notes of 0.1.3 to 0.1.11

As each release gave them. Since 0.1.19 the release page carries 0.1.12 onwards (`docs/release-notes.md`): GitHub limits how long a release's notes can be.

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
