**A test build of Ratchet & Clank in VR: standalone on Meta Quest, and now on PC VR.** It is a Quest port of [npiriou's Rerac-vr](https://github.com/npiriou/Rerac-vr), running on [OpenRAC](https://github.com/OpenRAC/OpenRAC)'s native port of the PS2 game. You need your own disc image. No game files are included.

> **This is a test build, and we need your logs.** **Expect bugs.** Every log helps, including from runs that worked: **Send your log** below says how.

## New in 0.1.15

**FIXED THAT DARN THUMB!** Sorry about that one. 0.1.14 turned your thumb the wrong way, back toward your wrist, and it got out because we did not test it thoroughly enough before the release. Our apologies for the hiccup. Our old test pictures looked along the arm, from too far away to show it, and we did not catch it until the owner's video.

- **It now moves like a thumb**, in both hands, on Quest and on PC, with **Fingers** on Tracked (VR Hands). Lifted off the controller, it rests open beside your hand. On a button, the stick or the thumb rest, it comes in along your fingers. Close your hand and a lifted thumb is a thumbs-up, and a thumb on the controller folds over your fingers.
- **A hand on a weapon** keeps the game's own hold, and **Fingers** on Static is as it was.
- **Can't find the new options?** A player could not, so the launcher's **Controls** tab now starts with a **Menus** section: how the menus work and where every option is. The options themselves have not moved. They are in the game, not the launcher: in a level, press **Y**, then **Options**, **VR Options**, **VR Controls**, **More controls**, **Moves**. A page shows eight rows at a time and nothing on it says there are more: hold your hand on the bottom row and the list moves on. VR Controls and More controls are both below the first eight rows.

**Tested for 0.1.15 on PC only** (the PAL disc and a simulated headset, not yet on a Quest, a real PC headset or the US disc): pictures of each hand from its palm side, its back and above, and of both hands raised before the eyes, looked at for each state above; and the whole way to **Moves** by the hand, each page moved on and back as the Menus section says.

## Also new: all of 0.1.14, which came out just before this, in case you missed it

**Now on PC VR too.** Beside the APK, this release has `RatchetQuestVR-0.1.15-windows.zip`: the Quest version on a Windows PC, with the same launcher (`RatchetClankVR.exe`: Play, Graphics, VR, Game, Controls, Logs and Credits, the disc check and set-up, the mods, the updates), the same options and the same game. **Install on PC** below says how. Rerac-vr is the original PC VR mod, and the one this port follows; this is for players who want the Quest version's launcher, options and mods on a PC.

- **Any OpenXR PC VR software:** SteamVR, Meta Horizon Link (Link and Air Link), Virtual Desktop, Pico Connect, Windows Mixed Reality and others. If the one set as your PC's OpenXR runtime cannot draw this game (it needs OpenGL, which Windows Mixed Reality's lacks), the game uses SteamVR's instead, by itself, when SteamVR is installed; Windows' setting is not changed.
- **Your controllers, by themselves:** the game gives its bindings for Touch (Quest, Rift S, Quest Pro, Touch Plus), Index, Vive and Vive Cosmos, Vive Focus 3, Pico 4 and Neo 3, HP Reverb G2 and Windows Mixed Reality, so each runtime puts them on your controllers. Others, such as **PlayStation VR2**, the **Steam Frame** and **DPVR** headsets, come through SteamVR, which maps them to one of those (change it in SteamVR's **Manage controller bindings** if you like). The launcher's **Controls** tab says what each button is on each, and your log names the controllers it got (`[openxr] left hand:`).
- **Untested on real PC headsets.** It has run on SteamVR's simulated headset only, which has no controllers: there it started, drew the title's menus and drew Kerwan, and nothing was played. Please send a log, with your headset and PC VR software, whatever happens.

**Moves: a new page** (in play: VR Options, VR Controls, More controls, **Moves**, its first row). **Swingshot anywhere** has moved here, beside what goes with it. Everything on it starts as it was: **Off**, **Normal**, **By itself**.

- **Wall run and jump** (new, **Off** to start; first person, on foot; a tester: "Im zipping to walls and just kinda hitting them then falling straight down"). In the air by a wall:
  - **Along it:** the stick along the wall, or his own speed along it, runs him along the wall, level for most of two seconds and then sinking into a fall. What he met it with goes into the run (a zip with the stick pushed becomes a fast run); the stick the other way brakes and turns him; the run follows bends and goes round inside corners. The stick straight away drops him off.
  - **Up it:** the stick into the wall runs him up it, once a wall, and over the top if it ends.
  - **On it:** a zip of Swingshot anywhere that ends at a wall holds him there for a second and a half, then he slides, and is off at two and a half seconds: time for the stick, the jump button or the next hook.
  - **Off it:** the jump button in the air by a wall, running on it or not, just past it, or pressed just before he reaches it, leaps him **where your head looks**: the level way is your look's, turned only as far as it must be to leave the wall. His speed is kept and goes that way; looking up trades way for height. His air control is off until you let go of the stick or really move it, so a stick still held along the wall has no say. Each wall gives one run and one jump until he lands or fires a hook, so walls and hooks chain and one wall cannot be climbed for ever. A jump from the ground beside a wall is still a jump.
- **Swingshot reach** (new, **Normal** to start; the same tester: "can ypu please extend the hookshot distance?"): **Normal** is the game's, **Longer** is twice that, **No limit** reaches any surface you can aim at, right across a level. A far hook gets there in well under a second, and a far zip goes up to 70 units a second while the way is clear, then comes down to the game's own arrival. A left hand's shot past 32 units zips instead of swinging: on a rope that long he would only fall. With Swingshot anywhere off it is how far the game's own targets can be hooked from.
- **Wrench throw distance** (new, **Normal** to start; a player's ask, "throwing really far with an arch"): **Far**, **Very far** and **Super far** go 2, 4 and 8 times as far, in an arc. It works on any throw, by hand or the game's own.
- **Wrench comes back** (new, **By itself** to start; the same player, "physically called back to your hand"): **When you call it**, it flies on, arcing, until it meets something, and stays there with its whirl stopped. A fresh squeeze of the right grip, or the trigger, calls it: the game's own whirl starts and it comes in about a second, your hand buzzing harder as it nears. It comes by itself if you press for the ring (the game opens no ring while the wrench is out), change item, grab a ledge or need your hands some other way, or after six seconds' flight with nothing met, so it can never be lost. Third person calls it the same way. Akimbo's left Swingshot or Walloper leaves it where it is until they are done.
- **A thump when you catch the wrench**, always on: harder after a call or a far throw.

**From a tester's notes and videos of Swingshot anywhere on 0.1.13**

- **The fling.** Yank the hand that fired, hard, **away from where you want to go**: on a swing it throws you on, on a zip it leaps you off. A slow hand does nothing. Off with Swingshot anywhere.
- **Zips chain in the air.** The right zip only worked once in a chain: the next one was refused until he landed.
- **The hook no longer cancels the Heli-Pack's glide.** He stays up while it flies, and with jump held the glide comes back. **Not run: the Thruster-Pack's hover.**
- **A press aimed at nothing keeps trying** while you hold it, instead of being lost.
- **Each hand's Swingshot has its own hook.** With one in each hand they shared one: the hook lagged behind or hung in the air, and firing one fired the other. Always on.
- **The Swingshot lies along your aim,** however you hold the controller: the housing and the way the hook goes now agree. Always on. **If you set a Swingshot angle before (VR Hands), put it back to Default.**

**From the owner's notes and video of 0.1.13 on a Quest 3**

- **Keep the refresh rate** (new, Graphics, **On** to start). The headset stays at the rate you chose, such as 120 Hz, so wherever the game keeps up it is that smooth. In 0.1.13, where the game could not hold the rate for twenty seconds the headset dropped to 72 Hz for the rest of the run. **Off** brings that back. The game also no longer holds its frames to the window's refresh rate: the headset paces them (on a PC with a 60 Hz monitor, a 90 Hz headset got 60).
- **The left grip only closes your hand**, on foot in first person, with **Physical crouch** on *or off*: in 0.1.13, with it off, the grip and the trigger both crouched. The **left trigger** crouches and works the packs (and lowering your head, with Physical crouch on). With **Akimbo**'s gun in the left hand (whose trigger fires it), in third person and in vehicles, the grip still crouches.
- **Empty hand angle starts at 65 down** (VR Hands), the owner's pick. A settings file still at 0.1.13's start, 0, moves to 65 down once; an angle you set yourself stays. **Gun angle** is unchanged.
- **The thumb** was meant to lie along the controller, and instead turned back toward the wrist: 0.1.15 fixes it.
- **No slide forward after walking backwards.** Letting go of the stick after walking backwards or aside, Ratchet slid about a metre forward: the game carries what speed is left along his facing. It now goes the way he was going, as it already did after walking forward.
- **No warped pictures at the start.** After the intro movie, the copyright lines stood still in front of you and warped as you moved your head (the game draws them once and holds them); and as a level's first scene began, the save select's picture showed for a moment. A picture held for more than a quarter second, or from another place, is no longer shown: the title's lines are on its panel.
- **Anti-aliasing** says it is cheap "for the graphics chip", the Quest's or the PC's.

**Tested for 0.1.14 on PC only** (the PAL disc and a simulated headset; none of it has run on a Quest, a real PC headset or the US disc):
- **The PC release:** the packaged launcher's 177 self-checks pass. Through real OpenXR on SteamVR's simulated headset, the exe started on OpenGL 4.6 and drew the title's menus, and the game drawn from the same build, started directly, drew Kerwan in first person with no errors. That headset has no controllers, so nothing was played through it, and its head is not where a player's is: the view in Kerwan was up against his boot, most likely for that reason.
- **Wall run and jump:** a zip at a wall held him there, then he slid. Leaps went the way the look asked, turned only as far as they had to be to leave the wall, and landed 0.0 degrees off that way, with the stick still held along the wall too. He ran up a wall, turned a corner smoothly, and chained a corner run, a zip in the air and another run with no stuck state. Off, the wall moves do nothing.
- **Swingshot reach:** No limit hooked a building 370 units away across Kerwan and pulled him there at up to 70 units a second, in about 7 seconds. Longer took one of the game's own targets from 45 units, and Normal refused it. A left hand's shot from 370 units zipped.
- **The wrench:** Far and Super far turned 9.6 and 38 units out. **When you call it:** thrown by hand or by the game's own throw, it rested at what it met, and came back to the grip, the trigger or the press for the ring; in third person, to the grip and the trigger. Ratchet dying called it, and it threw normally after he came back. Akimbo's left Swingshot left it where it was, and it could be called afterwards. Normal and By itself flew as before, with the thump on the catch.
- **Everything on at once:** ledges, the game's own ledges, swimming, the Heli-Pack, the hoverboard, the wrench swing and Arm moves on Buttons ran with no wall move taking over and no errors, the ledge and wrench runs' lines matching the same runs with it off.
- **Not shown:** the feel of any of it, a ledge grabbed during a wall run (from the code it should work, and the headset checklist asks for it), a long stick-held run along a long straight wall, a full loop of the swing, the Thruster-Pack's hover. The whirl and the thump were logged, not heard or felt.

## New in 0.1.13

The owner's list after playing 0.1.12 on a Quest 3, a tester's notes and logs on 0.1.12 (the US disc), and a player's message asking for buttons in place of the swings, built on what 0.1.12 has. Thanks for the logs.

**From the tester's notes**

- **Shimmy the way you reach.** Hanging from a ledge, hold your hands out to the right and Ratchet shimmies right, to the left and he goes left, for as long as you hold them out; bring them back in front of you and he stops. The tester found 0.1.12's way backwards: there the hands slid along the ledge, so they had to be drawn left to go right. **Ledge shimmy** (More controls) is **Reach** to start; **Pull** gives 0.1.12's way back.
- **Shimmy with the left stick too** (the tester's ask). Pushed along the ledge, the stick shimmies that way, whatever the hands do. Pushed toward or away from it, it does nothing, so a climb or a drop is still the hands' and A's.
- **No gun over your hands on a ledge.** A hand holding the ledge puts its gun away (Akimbo's left gun too) until it lets go, and gets it back then. In 0.1.12 the gun stayed drawn above the hands, and the Bomb Glove looked like a third hand floating there.
- **The ammo box after switching weapons.** After switching between two weapons, such as the Pyrocitor and the Bomb Glove with the other in the left hand, the box could stay on the left gun's ammo for the rest of the session. Each weapon change passes through an empty hand for a tick, and that tick made the box take the left gun's; the box now changes only after a moment of the hand empty, and a box still showing the left gun's with another weapon drawn is slid off, so the game's own comes back.
- **Hide black bars** (new, More controls, **On** to start; the tester's ask for third person). The game's black bars at the top and bottom in its short scenes during play, such as the start of Oltanis's grind race, are not drawn, in first person as in third. The HUD still hides there, as in the game. **Off** gives the bars back. 2D mode keeps them.
- **Point with either hand in menus** (new, More controls, **Menu pointer**: **Both hands** to start; the tester's ask). The right hand points, as before, until you pull the left trigger with the left hand aimed at the menu: then the left points, until you pull the right trigger aimed at it. That pull only moves the pointer, it picks nothing. While the left hand points, its trigger picks and its grip is Square, and the right grip and trigger turn the pages (L1 and R1). **Right hand** or **Left hand** keeps the pointer on that one. The vendor on foot still takes the right hand.
- **The map's missions** (the tester's "show missions" played the infobot). On the map, A and the trigger each pressed both of the game's buttons there: the missions page's and the infobot's, and the infobot won. Now A shows the missions and the trigger plays the infobot, as their icons on the map say. Every other menu is as before.
- **Climbing out of the ship** (the tester's "sinking through the floor right before exiting the ship"). In the landing scene Ratchet ducks in the cockpit before he jumps out, and seen through his eyes the view went down through the hull to under the pad (on Aridia, 3.5 units). Over the cockpit his eyes now go no lower than the seat; once out, beside the ship, they are his again.
- **VR Controls' last rows can be reached.** The port shows at most 12 rows of a page, and VR Controls had 16 in 0.1.12: its last four (Sparkles on the bolts and the three before it) could not be reached. The page now ends with **More controls**, which holds them and the new rows: Swingshot anywhere, Ledge shimmy, Hide black bars, Menu pointer, Arm moves. A page with more rows than the port shows now says so in the log.
- **Not fixed: someone vanishing in a scene through Ratchet's eyes.** In the tester's scene on Aridia (Skid's), he vanishes for a moment and comes back. On PC the game itself does not draw him in some of the scene's shots, the ones its own camera aims elsewhere; seen from Ratchet's eyes he is in plain view then. That is the game's own drawing, not the view, and changing it reaches every moby drawn, so it is left for a later version. **Cutscenes** (VR Options) **In VR, around you** or **On a screen** shows the scene from the game's own camera, as it was made to be seen. The camera under the level and the sinking at the end were not seen on PC (the PAL disc).

**From a player's message**

- **Arm moves** (new, More controls, **For real** to start; for a player whose wrists and hands hurt to swing). On **Buttons**, nothing in first person asks you to swing or punch. Squeeze the right grip and the wrench swings, the game's own swing: squeeze again for the next blow, and in the air for its jump attack. Pull the trigger and the Walloper punches. As Giant Clank the right grip punches. Gloves are thrown by the trigger, the game's way; ledges are caught for you, as the game does it; the left stick swims. Your hands still aim the guns. The wrench shows in your hand as it swings, but it does not move with a swing your hand does not make. **Swing the wrench for real**, **Throw by hand**, **Grab ledges with your hands** and **Swim with your arms** keep what you set, for when you go back to **For real**.

**From the owner's list**

- **Swingshot anywhere** (new, More controls' first row: pause, VR Options, VR Controls, More controls; **Off** to start; testers' ideas). Where no Swingshot target of the game's is in reach, the whole level is one:
  - **In your right hand** the Swingshot zips you to whatever wall or ground you aim it at, up to the 30 units the game's own pull reaches, as at a yellow target.
  - **In your left hand** (Akimbo) it swings you from any point above your head, up to the game's 16 units, as at a green target.
  - **Hold the trigger** to zip or swing, and **let go** to drop off. A zip let go midway now keeps its speed, as a swing let go does: the game would brake it at once.
  - **Lean in your room** during a swing to push it that way, as the left stick does in the game's own swings.
  - **One Swingshot in each hand** chains them: swing, let go, zip, and on.

  The game does the moves itself: its hook, its pull, its swing and its rope, with a hidden target of the mod's put where you aim. So its rules hold: a blocked way drops you, as at its targets. The game's own targets come first, and **with the switch off the Swingshot is exactly the game's**. It does nothing on **Novalis**, the one level whose game code has no Swingshot moves (the game never fires it there). A pull or swing that stalls is let go after a moment, whatever the level, so it can never hold you in place.
- **What's new, in the launcher.** The first time you open a new version, a box lists what changed, the biggest first. **What's new** on the Updates card shows it again.
- **"Hide Ratchet's body" is gone from the launcher.** Switched off, it drew his whole model around your eyes in first person: the view inside or above his head, the gun floating, your hands no longer his (a tester's report on 0.1.12). Now his body is always fitted to you in first person, and how much of it you see is set by **Player body** and **Body parts** (VR Body). If you had switched it off, there is nothing to do: 0.1.13 no longer reads it. Your log now also starts with a `[vr-settings]` line naming the launcher's settings, and which of its switches are off, so a report shows what was set.
- **Akimbo is on for everyone.** **Akimbo** (VR Options) now starts **On**, and a settings file from 0.1.12 or before that never had Akimbo changed switches to On once. If you had switched it off yourself, switch it off again: 0.1.12's files cannot tell a choice of Off from the start it was then. Nothing changes until you want it: the left hand stays empty, and your left trigger and grip do what they did, until you click the left stick and pick a gun from the left hand's ring. Pick the same gun again there to empty the hand. With Akimbo on, the right hand's weapon ring sits at the right edge of the HUD, to leave room for the left one, and its pages go from your own quick select layout to all you own, eight at a time, keeping your layout as it is.
- **Empty hand angle goes to 65 down.** VR Hands' **Empty hand angle** now reaches 65 down (it stopped at 45), for hands that still feel turned too high with nothing in them (a tester's ask). It still goes to 45 up, in steps of 5, and your setting stays as it was. **Gun angle** is unchanged.
- **An angle for each weapon, the Swingshot too.** VR Hands has a new last row named after the item you last drew, such as **Swingshot angle** or **Blaster angle**: 60 down to 60 up, in steps of 5, for that item alone, in either hand, on top of **Gun angle**. For a gun it turns how the gun sits in your hand and where it fires, together. For the **Swingshot** it turns only the housing on your wrist: the hook still flies where your controller points. So if the housing looks tilted away from where the hook goes (a tester found it "a bit wild"), turn it until the two line up. **Gun angle** could not do that: it turns where the hook flies, but not the housing. Every item starts at **Default**, so nothing changes until you set one. The wrench and the gloves have no angle of their own.
- **The whole body is the start.** **Body parts** (VR Body) now starts on **Everything**. **A settings file from 0.1.12 or before that never had Body parts changed switches to Everything once.** Any other choice you made stays. If you had picked **Arms only** yourself, pick it again: 0.1.12's files cannot tell a choice of Arms only from the start it was then. As Clank, Everything shows his body too: walking, it can come into the bottom of your view and cover his left hand. **Arms only** shows his hands alone, as 0.1.12 did. Giant Clank looks the same either way.
- **Fingers in the shadow.** With **Shadow matches what you see** (Graphics, on), each of Ratchet's fingers and his thumb now casts its own shadow, bent as you bend yours, with the palm between them and the wrist. The hand was one rounded block before. The shadow is still the game's: its shapes along his bones, not his exact outline.
- **His head in the shadow.** In first person the head is still not drawn, so it never gets in your view, but it now casts its shadow, turned as you turn and tilt your head. It shows with the torso: Everything, Body and legs, Body and arms.
- **The left grip is free with Physical crouch.** With **Physical crouch** on, in first person and on foot, the left grip no longer crouches or works the packs: your left hand only closes, for a fist or a grab. Lower your head to crouch. The **left trigger** still does all the grip did: crouch, long and high jumps, the packs. With **Akimbo** and a gun in the left hand, the trigger fires that gun, so the grip stays the crouch there. With Physical crouch off, nothing changes.
- **Climb a ledge by pulling yourself up.** Hanging on, draw a holding hand down toward your chest at an easy pace, the left, the right or both, and Ratchet climbs. Before, only a fast push down with both hands at once counted, so a slower or one-handed pull did nothing and A was the only way up (the owner's logs). The fast push and A still work. A climb asked for while you slide along the ledge now waits for the slide to end, instead of being lost.
- **Shooting on rails and cables, checked.** Grinding, the right hand's gun fires (the game allows it) and, with **Akimbo**, the left hand's too. On a cable the right hand holds the wrench on the cable, as in the game; with Akimbo the left gun fires all the way along. Nothing had to change for this.
- **The wrench stays on the right grip.** A grab from the chest or the hip was looked at. The wrench is Rerac-vr's: held only while the grip is squeezed, thrown by letting go mid-swing, put away by letting go slowly. A second way to take it out would clash with all three, and with the two-handed hold and the ledges on the same grip, so it is left as it is.

**Tested for 0.1.13 on PC only** (the PAL disc and a simulated headset; none of it has run on a Quest yet):
- **Ledges:** the pull up climbed with both hands, with the left hand alone, and in the middle of a slide, where it waited and then climbed. The fast push, A and letting go to drop still work, and the earlier ledge runs give what they did.
- **Rails and cables:** on Oltanis's rail both guns fired while he ground, on both stretches. On Kerwan's cable the left gun fired all the way along.
- **The left grip:** with Physical crouch on, holding it gave no crouch, and the trigger still crouched and high-jumped.
- **Settings files:** files from 0.1.12 switched to Everything once; other choices stayed.
- **The shadow:** with **Shadow matches what you see** on, it showed the fingers, and the head turning with the view. With it off, the game's own shadow is as before.
- **The tester's notes:** hanging on a ledge, hands held out to the right shimmied him right, to the left left; **Pull** shimmied the 0.1.12 way; the left stick shimmied both ways. With Akimbo, the left hand on the ledge put its gun away. With the Pyrocitor in the left hand, switching the right hand's Bomb Glove to the wrench and back twice, the ammo box ended on the glove's each time. On the map, A opened the missions page with no infobot, and the trigger played the infobot. **Menu pointer:** the left trigger at the menu moved the pointer to the left hand and picked nothing; on **Left hand**, its trigger opened Options, then VR Options. Landing on Aridia and Kerwan, the view stayed at the seat's height. **Hide black bars:** at the start of Oltanis's grind race the game's bars were not drawn, in first person and in third; **Off**, they were, in both.
- **Arm moves on Buttons:** by a crate, the grip swung the wrench twice and broke it; at the top of a jump the grip gave the jump attack; fast swings with the grip open did nothing; the trigger brought the Blaster back, then fired it. The Walloper's trigger lunged and broke a crate, and a real punch did nothing. As Giant Clank the grip gave the game's punch. The Bomb Glove threw one bomb a pull; a fall by a ledge was caught with the hands doing nothing; at a bolt the grip seated the wrench. No swing, throw, ledge grab or stroke was taken from the hands. Not run on PC: swimming with the left stick on Buttons, and little Clank's punch.
- **Everything else:** the Akimbo, wrench, Walloper, throw, swimming, Giant Clank, Swingshot, ledge, scene, menu, map, VR Hands and vendor runs gave what they did before, with the new lines above.

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

- **Quest:** a **Meta Quest 3 or 3S** in Developer Mode, and **about 6 GB free** on it. Quest 2 and Quest Pro may run it, slower; they are untested.
- **PC:** **Windows 10 or 11**, 64-bit, a PC and graphics card that run PC VR, a PC VR headset and its PC VR software, and about 2 GB free beside your `.iso` (6 GB if the `.iso` is on another drive, as it is then copied).
- **Your own `.iso` of exactly one of these discs.** Greatest Hits, Platinum, other regions and the sequels are not supported, and the launcher says exactly why a disc is refused.

| Disc | Serial | Version | Size in bytes | SHA-1 |
| --- | --- | --- | --- | --- |
| PAL (Europe), best supported | SCES-50916 | v2.00 | 4,214,784,000 | `ef304c641c7cd426ba492707dda5974679befbc0` |
| NTSC-U (North America) | SCUS-97199 | v1.00 | 4,214,095,872 | `aee9275384f739c9b24cd5de6f7e200292db7b01` |

**NTSC-U is barely tested:** one tester's run on 0.1.5 showed the VR mod did not load on it (fixed in 0.1.6). It is built, and its addresses and the mod's hooks are checked against the disc's tables, but all other testing was on PAL. Reports from that disc are especially welcome.

## Install

1. Put your Quest in **Developer Mode** (in the Meta Horizon app on your phone).
2. Get `RatchetQuestVR-0.1.15.apk` onto the headset and install it. Either:
   - download it in the Quest's browser (or email it to yourself and download it there) and open it from the **Files** app, or
   - sideload it with SideQuest or `adb install -r RatchetQuestVR-0.1.15.apk`.
3. Open **Ratchet & Clank Quest VR** from your Library's **Unknown Sources**.
4. Copy your `.iso` to the headset's **Download** folder and choose it in the launcher. The first set-up copies the disc and prepares the levels: **up to half an hour, once. Keep the headset on charge.**
5. Press **PLAY IN VR**. **Play in 2D** puts the game on a big flat screen, played with the Touch controllers as a PlayStation pad: if VR fails, try it and tell us, as it separates a game problem from a VR problem.

**Updating:** install a new APK over the old one, or use the launcher's Play tab, which checks this page every six hours (**Check now** to do it at once; you can switch it off). **Do not uninstall or clear the app's data:** your set-up and saves go with it.

## Install on PC

1. Download `RatchetQuestVR-0.1.15-windows.zip` and unzip it anywhere you can write: your Desktop, Documents or a games drive. Keep the `RatchetClankVR` folder together. Nothing is installed into Windows.
2. Start your headset's PC VR software (SteamVR, Meta Horizon Link, Virtual Desktop, Pico Connect, ...).
3. Open `RatchetClankVR.exe`. Windows may warn about an app from the internet it does not know: **More info**, then **Run anyway**.
4. **Choose your .iso file** and wait for the set-up, once: it checks the disc, then prepares the levels.
5. Press **PLAY IN VR**. **Play in 2D** puts the game on a big flat screen in the headset, played with your VR controllers as a PlayStation pad: if VR fails, try it and tell us, as it separates a game problem from a VR problem.

Your set-up, saves and settings are in `%LOCALAPPDATA%\RatchetQuestVR`, and each run's log in `Downloads\RatchetQuestVR`. **Updating:** the launcher's Updates card downloads a newer release's zip, checks it against GitHub's SHA-256 and size, closes, puts it over its folder and opens again, keeping all of that. Or unzip a new release over the old folder yourself.

## Send your log

1. Play, then **quit and open the launcher again.** That is what writes the run's log.
2. In **Download/RatchetQuestVR** on the headset, the newest `run_<date>.txt` is your last run. The launcher's **Logs** tab shows a summary. **On PC** the launcher waits for the game and writes the log as it ends, in **Downloads\RatchetQuestVR**; the log also names your PC VR software and the controllers it bound.
3. Attach it to a [**headset report**](https://github.com/moistman42069/RatchetAndClank-Quest-VR/issues/new?template=headset-report.yml). You can open that page in the Quest's browser, or plug the headset into a PC and copy the file from there. Or DM **@MeWhenINameMyself** on Discord.

The log holds your headset's model and Android version, the game's own log, why VR stopped if it did, and the app's Android log. Tell us your headset, disc, planet, what you did, and any settings you changed. A video helps.

**Most useful right now:**

- **PC VR (new in 0.1.14):** any log at all, with your headset, its PC VR software and your controllers: whether the game opens in the headset, whether the buttons do what the Controls tab says, and how smooth it is.
- **0.1.15's thumb:** how it looks in your headset, lifted, on a button or the stick, and in a closed hand: a video helps most.
- **0.1.14's changes:** the refresh rate held at 90 or 120 Hz (and how smooth it is where the game cannot keep up); the left grip with Physical crouch off; walking backwards and stopping; the screens after the intro movie.
- **0.1.13's changes:** whether an easy pull up a ledge climbs when you mean it, and only then; the left grip with Physical crouch; the whole body as the start (as Ratchet and as Clank); the shadow's fingers and head.
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

The full list of what to try is in [docs/HEADSET_CHECKLIST.md](https://github.com/moistman42069/RatchetAndClank-Quest-VR/blob/v0.1.15/docs/HEADSET_CHECKLIST.md).

## What is in it

Rerac-vr's design, rebuilt on OpenRAC so that a headset can run it:

- **First person** through Ratchet's eyes at his height, or **third person** from the game's camera. Switch at any time in **VR Options**.
- **Your hands are his hands.** Real wrench swings, throws and Hyper Strikes; guns that aim where your hand points; thrown bombs, mines and Decoys; a real punch for the Walloper; two-handed heavy weapons; **Akimbo** (on by default from 0.1.13). His arms bend to your controllers, his fingers follow yours, and all of it stops at the world's walls.
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
| Left grip or trigger | Crouch, and the packs (with A: long and high jumps, flips). Lowering your head crouches too; with **Physical crouch** on, the trigger alone (the grip is your hand's) |
| A grip by an edge | Grab a ledge; pull yourself up (a holding hand, or both, drawn down toward you), push both hands down fast or press A to climb; let go of both to drop |
| Right stick click | Talk; hold for quick select, move your right hand towards a weapon, let go |
| Left stick click | With Akimbo (on by default): the left hand's own quick select |
| Left menu button | Recentre (the headset's own recentre too) |

**Menus:** point with your right hand and pull the trigger or press A. B goes back. A page shows eight rows at a time: hold your hand on the bottom row and the list moves on, on the top row and it moves back. **Hoverboard:** left stick steers, left trigger boosts, right trigger shoots, A jumps, right stick for tricks. The whole list is in the launcher's **Controls** tab.

## VR Options

In the game: pause, **Options**, **VR Options** (or **VR Options** on the title's main menu): Helmet, Boots, Camera flip, Direction (Gaze by default, or Body), Player body, Akimbo, Play position (Standing or Seated), **View** (First or Third person), **Cutscenes** (Through his eyes, In VR or On a screen; X does the same during one) and, in play, **Cheats**, **VR Body** and **VR Controls**. **VR Hands** comes after it: Gun angle, Gun size, Hand size, Empty hand angle, Items match hand size, Mirror left-hand guns, Left gun ammo, HUD size, HUD height, Fingers, Fingers bend around the world. Cheats, VR Body and VR Controls are below the first eight rows, and **More controls** is VR Controls' last row; its first row is **Moves**: Swingshot anywhere, Wall run and jump, Swingshot reach, Wrench throw distance, Wrench comes back. The launcher's **Controls** tab lists every page.

## Tested, and not tested

**On a real Quest (a Quest 3):** on 0.1.2 both controllers bind, walking works, and the first level plays, at about 60 to 70 frames a second against the headset's 72 Hz. The launcher's update has worked from 0.1.2 to 0.1.3, and 0.1.4 starts after an update. Most of the game has yet to be played on one.

**On PC, with the PAL disc and a simulated headset:** menus, saving and loading, vendors, weapons, wrench, ledges, swimming, cranks, turrets, Clank and Giant Clank, scenes, the hoverboard on Rilgar and Kalebo III, grind rails, Kerwan's cable, and flying the ships of Pokitaru and the fleet. The full table is in [docs/STATUS.md](https://github.com/moistman42069/RatchetAndClank-Quest-VR/blob/v0.1.14/docs/STATUS.md). A simulated headset proves that a feature runs, not how it feels: aim angles, comfort and haptics need real hands.

**On a real Quest with the NTSC-U disc:** one tester's run on 0.1.5: the game started, the VR mod did not load (fixed in 0.1.6, checked against the disc's tables, not yet played).

**Never run anywhere:** Magneboots floors, the orders ring for Clank's little robots, Gemlik's ship, Clank's ledges, catching a higher rung from a hang, and the hoverboard's weapon on Kalebo III.

## Known issues

- **Frame rate:** the first level ran at about 60 to 70 frames a second on 0.1.4, and about 57 on 0.1.6, under the headset's 72 Hz, so it may not look perfectly smooth. 0.1.7's Max draw distance draws more again and has not yet been measured on a Quest: switch it off in the launcher's Graphics tab if a planet gets less smooth. 120 Hz is tried at the start and falls back to 72 Hz. Improving this is next. A tester's log on the US disc showed **47 to 60 frames a second on Aridia, 48 on average**. The log's `[vr-perf]` lines now also say how many of the game's line tests your hands, fingers and the menu pointer ask each tick, and how long they take (on PC about 8 a tick, 0.03 ms, while pushing against the world): please send a log either way.
- **Feet slide when you walk around your room:** the steps move Ratchet without his walk, so his feet slide along the ground.
- **Cutscenes seen through Ratchet's eyes can show an odd view for a moment.** With **Cutscenes** on **Through his eyes** (the default), your eyes ride on the scene's own Ratchet, even in a shot where the game's camera is looking somewhere else. The game never shows him in those shots, so he can be standing below the ground or off to one side, and for that moment you see the level from underneath or from the wrong place. It is only the view: the scene plays and ends as normal. We saw it once on PC, in a Novalis scene that the test started out of its place in the story. If it happens, press **X**: the scene goes on **In VR**, from the game's own camera, and X again puts it **On a screen** (On a screen was checked in that scene on PC). VR Options' **Cutscenes** row and the launcher's **VR** tab set it too, and Y skips the scene.
- **Sat down after starting? Recentre.** **Physical crouch** measures from your height when you last recentred. Sit down after that and Ratchet crouches each time you stand still, as if you were crouching (a tester's log: 66 cm below standing). Press the **left menu button** once you are seated, or play with Physical crouch off (VR Controls).
- **A gun held against the crates you hang from breaks them.** Its first shot (or the Pyrocitor's flame) breaks the crate under your hand, and you fall with it, as the game's crates go. Hold the gun out, away from what you hang from: held above the stack on PC, he hung on through 33 shots, until the test let go. Nothing in 0.1.13 changed this.
- **A full list of things can still stop the game** at a place in the game other than the scenes fixed here, if the list fills there. The fixes in this release keep it from filling on Kerwan; tell us where it happens, with your log.
- VR Options' rows sit close together (about 10 cm on the menu), closer than in Rerac-vr. Tell us if the pointer picks the wrong row.
- A crooked hoverboard landing is straightened instead of throwing you off, as in Rerac-vr, but how much that helps is unproven.
- In third person the pause menu is a flat panel, not in the world.
- The picture may rarely show one turned frame when you turn your head fast in a menu.
- Direction defaults to Gaze here; Rerac-vr's default is Body. Please try wall jumps on both.

## If you play on PC

**Play [Rerac-vr](https://github.com/npiriou/Rerac-vr).** It is the complete experience, and the one this port follows. It runs on [ReRAC](https://github.com/re-rac/rerac), a Rust rewrite of the game that a headset's processor cannot yet carry at frame rate, which is why this port runs OpenRAC's recompiled game instead.

## Credits and licence

Built on **[Rerac-vr](https://github.com/npiriou/Rerac-vr)** by npiriou, the main base of this project and its VR design (ISC), and on **[OpenRAC](https://github.com/OpenRAC/OpenRAC)** and [rac1-mods](https://github.com/OpenRAC/rac1-mods) (GPL-3.0-or-later). Thanks also to the [ReRAC](https://github.com/re-rac/rerac) team. Full [credits](https://github.com/moistman42069/RatchetAndClank-Quest-VR/blob/v0.1.13/CREDITS.md) and [third-party notices](https://github.com/moistman42069/RatchetAndClank-Quest-VR/blob/v0.1.13/THIRD_PARTY_NOTICES.txt).

Please **report bugs in this port here**, not to Rerac-vr, ReRAC or OpenRAC.

This is free software under GPL-3.0-or-later, without any warranty. The source of this release is its tag: this repository, OpenRAC at the submodule's commit (`293cb5cc`), and `patches/`, which carries OpenRAC's later fixes named above. It is an unofficial community project, not affiliated with or endorsed by Sony Interactive Entertainment or Insomniac Games. Ratchet & Clank is their property: the licence gives no rights in it, and no game files are distributed.

## Download

| File | SHA-256 |
| --- | --- |
| `RatchetQuestVR-0.1.15.apk` (version code 16, 48 MB) | `88696122faa0bba0ac50ade3f026bce6143153f4f1d284a27b6af0a956439e2f` |
| `RatchetQuestVR-0.1.15-windows.zip` (65 MB) | `1c93737d3fc0ee9fb8d11a24d5d26a58033cf58049a2d69e2f84efdfb132a9e0` |
