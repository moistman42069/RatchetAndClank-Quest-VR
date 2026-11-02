**The first public test build of Ratchet & Clank in standalone VR on Meta Quest.** It is a Quest port of [npiriou's Rerac-vr](https://github.com/npiriou/Rerac-vr), running on [OpenRAC](https://github.com/OpenRAC/OpenRAC)'s native port of the PS2 game. You need your own disc image. No game files are included.

> **This is a test build, and we need your logs.** Only the very first build ever ran on a real Quest, and walking did not work on it. Everything since has been tested on a PC with a simulated headset. **Expect bugs, and walking on the headset is still unproven.** Every log helps, including from runs that worked: [how to send one](#send-your-log).

## What you need

- **Meta Quest 3 or 3S** in Developer Mode. Quest 2 and Quest Pro may run it, slower; they are untested.
- **About 6 GB free** on the headset.
- **Your own `.iso` of exactly one of these discs.** Greatest Hits, Platinum, other regions and the sequels are not supported, and the launcher says exactly why a disc is refused.

| Disc | Serial | Version | Size in bytes | SHA-1 |
| --- | --- | --- | --- | --- |
| PAL (Europe), best supported | SCES-50916 | v2.00 | 4,214,784,000 | `ef304c641c7cd426ba492707dda5974679befbc0` |
| NTSC-U (North America) | SCUS-97199 | v1.00 | 4,214,095,872 | `aee9275384f739c9b24cd5de6f7e200292db7b01` |

**NTSC-U has never been run:** it is built and its addresses are checked against the disc's tables, but all testing was on PAL. Reports from that disc are especially welcome.

## Install

1. Put your Quest in **Developer Mode** (in the Meta Horizon app on your phone).
2. Get `RatchetQuestVR-0.1.2.apk` onto the headset and install it. Either:
   - download it in the Quest's browser (or email it to yourself and download it there) and open it from the **Files** app, or
   - sideload it with SideQuest or `adb install -r RatchetQuestVR-0.1.2.apk`.
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

- **Did walking with the left stick work?** The log's `[openxr] left hand:` line should name a controller, not `(none)`, with `hands 11` and `sticks 11` after it. If a bar in the headset says a controller is missing, tell us.
- Does it hold its frame rate, on which planets, at which settings?
- Anything that crashes, freezes or leaves you stuck: the log says why far more often than not.

The full list of what to try is in [docs/HEADSET_CHECKLIST.md](https://github.com/moistman42069/RatchetAndClank-Quest-VR/blob/v0.1.2/docs/HEADSET_CHECKLIST.md).

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

**On a real Quest:** only 0.1.0, on a Quest 3, where it ran at 72 Hz but could not save, held tools at the wrong angles, and could not walk. Saving and the tools were fixed on PC; the walking fix has never been seen working on a headset. Performance on a headset is unknown for everything added since.

**On PC, with the PAL disc and a simulated headset:** menus, saving and loading, vendors, weapons, wrench, ledges, swimming, cranks, turrets, Clank and Giant Clank, scenes, the hoverboard on Rilgar and Kalebo III, grind rails, Kerwan's cable, and flying the ships of Pokitaru and the fleet. The full table is in [docs/STATUS.md](https://github.com/moistman42069/RatchetAndClank-Quest-VR/blob/v0.1.2/docs/STATUS.md). A simulated headset proves that a feature runs, not how it feels: aim angles, comfort and haptics need real hands.

**Never run anywhere:** the NTSC-U disc, Magneboots floors, the orders ring for Clank's little robots, Gemlik's ship, Clank's ledges, catching a higher rung from a hang, and the hoverboard's weapon on Kalebo III.

## Known issues

- VR Options' rows sit close together (about 10 cm on the menu), closer than in Rerac-vr. Tell us if the pointer picks the wrong row.
- A crooked hoverboard landing is straightened instead of throwing you off, as in Rerac-vr, but how much that helps is unproven.
- In third person the pause menu is a flat panel, not in the world.
- The picture may rarely show one turned frame when you turn your head fast in a menu.
- Direction defaults to Gaze here; Rerac-vr's default is Body. Please try wall jumps on both.

## If you play on PC

**Play [Rerac-vr](https://github.com/npiriou/Rerac-vr).** It is the complete experience, and the one this port follows. It runs on [ReRAC](https://github.com/re-rac/rerac), a Rust rewrite of the game that a headset's processor cannot yet carry at frame rate, which is why this port runs OpenRAC's recompiled game instead.

## Credits and licence

Built on **[Rerac-vr](https://github.com/npiriou/Rerac-vr)** by npiriou, the main base of this project and its VR design (ISC), and on **[OpenRAC](https://github.com/OpenRAC/OpenRAC)** and [rac1-mods](https://github.com/OpenRAC/rac1-mods) (GPL-3.0-or-later). Thanks also to the [ReRAC](https://github.com/re-rac/rerac) team. Full [credits](https://github.com/moistman42069/RatchetAndClank-Quest-VR/blob/v0.1.2/CREDITS.md) and [third-party notices](https://github.com/moistman42069/RatchetAndClank-Quest-VR/blob/v0.1.2/THIRD_PARTY_NOTICES.txt).

Please **report bugs in this port here**, not to Rerac-vr, ReRAC or OpenRAC.

This is free software under GPL-3.0-or-later, without any warranty. The source of this release is its tag: this repository, OpenRAC at the submodule's commit (`293cb5cc`), and `patches/`. It is an unofficial community project, not affiliated with or endorsed by Sony Interactive Entertainment or Insomniac Games. Ratchet & Clank is their property: the licence gives no rights in it, and no game files are distributed.

## Download

| File | SHA-256 |
| --- | --- |
| `RatchetQuestVR-0.1.2.apk` (version code 3, 47 MB) | `57e1bc261fb2e58aa41c425a2457c6c0673264ea20ed7eb3bd49ab94a6bb0213` |
