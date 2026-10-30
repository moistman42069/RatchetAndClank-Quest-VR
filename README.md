<p align="center"><img src=".github/launcher-play.png" alt="The Ratchet & Clank Quest VR launcher" width="640" /></p>

<h1 align="center">Ratchet & Clank — Quest VR</h1>

<p align="center">An unofficial port of Ratchet & Clank (2002, PlayStation 2) to standalone VR on Meta Quest, bringing <a href="https://github.com/npiriou/Rerac-vr"><b>npiriou's Rerac-vr</b></a> to the headset on OpenRAC's native port. You supply your own disc image; no game files are included.</p>

## Built on Rerac-vr

**This project is a Quest port of [Rerac-vr](https://github.com/npiriou/Rerac-vr) by [npiriou](https://github.com/npiriou), the PC VR mod for Ratchet & Clank, and his work is its main base.** Rerac-vr worked out how Ratchet & Clank should play in VR; this port carries that design to a standalone headset. From Rerac-vr come:

- **The VR design:** first person through Ratchet's eyes, Ratchet facing and shooting where your hand points, the weapon held in your hand, and quick select by reaching for a weapon.
- **The controls:** the Touch controller layout, button for button, and the comfort choices (eye height, snap turning, recentring).
- **The game knowledge:** his notes on the hero's attached items (which mobys are Clank, his pack and the item in the hand) are how this port hides Ratchet's body and Clank in first person and keeps the weapon.
- **The roadmap:** every feature still to come here (real wrench swings, thrown bombs, two-handed weapons, ledge grabbing, swimming, bolt cranks, the hoverboard, a spatial pause menu and more) is one Rerac-vr already has on PC: [docs/STATUS.md](docs/STATUS.md) tracks them one by one.

Rerac-vr runs on [ReRAC](https://github.com/re-rac/rerac), a Rust rewrite of the game that a headset's processor cannot yet carry at frame rate, so this port runs [OpenRAC](https://github.com/OpenRAC/OpenRAC)'s recompiled game instead and rebuilds Rerac-vr's features on it in its own VR mod (`vr/`). **If you play on PC, play [Rerac-vr](https://github.com/npiriou/Rerac-vr)**: it is the complete experience, and the one this port follows.

**Latest:** no public release yet. Builds will be published on the [Releases page](https://github.com/moistman42069/RatchetAndClank-Quest-VR/releases) once they have been tested on a headset; the launcher then updates itself from there.

> **Experimental / WIP:** the game is OpenRAC's native port, not an emulator, with a VR mod of its own. It has run on PC with your disc and a simulated headset; it needs Quest reports. Gloves and gadgets in the hand, physical wrench swings, thrown bombs, two-handed weapons, ledge grabbing, swimming strokes and a spatial pause menu are not in yet: [docs/STATUS.md](docs/STATUS.md) lists every feature and how far it has been tested.

## Downloads

| Platform | Download |
| --- | --- |
| Meta Quest 3 / 3S — standalone VR | `RatchetQuestVR-<version>.apk` on the [latest release](https://github.com/moistman42069/RatchetAndClank-Quest-VR/releases/latest) (none yet) |

Quest 2 and Quest Pro may run it, slower; they are untested. The APK holds no game data.

## What you need

- A Quest 3 or 3S in Developer Mode, for sideloading.
- About 6 GB free on the headset.
- **Your own `.iso` of exactly one of these discs:**

| Disc | Serial | Version | Size in bytes | SHA-1 |
| --- | --- | --- | --- | --- |
| PAL (Europe), best supported | SCES-50916 | v2.00 | 4,214,784,000 | `ef304c641c7cd426ba492707dda5974679befbc0` |
| NTSC-U (North America) | SCUS-97199 | v1.00 | 4,214,095,872 | `aee9275384f739c9b24cd5de6f7e200292db7b01` |

Greatest Hits, Platinum, other regions and the sequels are not supported. The launcher checks the disc and says exactly why one is refused.

## Installing and updating

- **Install:** sideload the APK with SideQuest or `adb install -r`, then open **Ratchet & Clank Quest VR** from Unknown Sources.
- **Set up:** put your `.iso` where the headset's file picker can see it (the Download folder), and choose it in the launcher. The first set-up copies the disc and prepares the levels: up to half an hour, once. Keep the headset on charge.
- **Play:** press **PLAY IN VR**. **Run the 2D test** plays the game flat, with a gamepad, to tell a game problem from a VR problem.
- **Updates:** the launcher's Play tab checks this repository's releases (automatically every six hours, or with **Check now**; switch it off there). An update is downloaded, checked against GitHub's SHA-256, its size, the app's package, a newer version and the app's signing key, then handed to Android's installer, which asks you. Your set-up, saves and settings are kept. Installing a new APK over the old one by hand keeps them too; **do not uninstall or clear the app's data**.
- **Level updates:** when a new version reads its levels in a newer form, the Play tab offers **Update the levels**, made again from the disc already on the headset.

## Highlights

- **Native, not emulated:** OpenRAC's recompiled game draws every object of a level whatever the game culled, so turning your head never pops the world in.
- **First person** through Ratchet's eyes, his body, Clank and the pack hidden, the weapon in your right hand; or **third person** from the game's camera with your head looking around.
- **Smooth turning by default**, snap turning (30–90°) as an option; walking towards where you look; Ratchet faces where your right hand points.
- **Cutscenes in VR:** the game's in-engine scenes in 3D around you from its camera, your head free to look round; or, in the VR tab, **on a screen** in the game's own framing. Movies play on the screen.
- **HUD and menus on panels.**
- **Vibration** on shots and hits; quick select by reaching towards a weapon.
- **Graphics:** presets, 72–120 Hz (back to 72 Hz when a higher rate is not held), eye resolution, MSAA, texture filtering, draw distances.
- **Everything can be switched in the launcher**, and is on by default.
- **Launcher:** disc requirements and checks, set-up progress, settings, controls, logs, credits, updates.

## Controls

| Control | Action |
| --- | --- |
| Left stick | Walk, towards where you look |
| Right stick | Turn (smooth by default; snap in the VR tab). In third person, the game's camera |
| A | Jump; twice for a double jump |
| B | Crouch, or Clank's packs in the air |
| X | Map |
| Y | Pause; skips a cutscene |
| Right trigger | Fire |
| Right grip | Swing the wrench |
| Left trigger or grip | Crouch |
| Right stick click | Talk; hold for quick select, move your right hand towards a weapon, let go |
| Left stick click | The game's first-person look |
| Both sticks clicked | Recentre |

In menus either stick moves, A or the trigger takes, B goes back. Long jump, high jump and flips are as in the game: Crouch + A. The layout follows Rerac-vr, the PC VR mod.

## Support and project

Please report both failures and successful runs: headset and its OS version, the app's version, your disc (PAL or NTSC-U), the planet, any settings you changed, what you did, and the log of that run.

| Where | Each run's log |
| --- | --- |
| Quest | **Download/RatchetQuestVR**: the newest file is the last run (or set-up), with the game's own log, why VR stopped if it did, and the app's Android log. The launcher's **Logs** tab shows a summary of the last run. |

Report in [GitHub Issues](https://github.com/moistman42069/RatchetAndClank-Quest-VR/issues), or DM **@MeWhenINameMyself** on Discord.

This is an unofficial community project, not affiliated with or endorsed by Sony Interactive Entertainment or Insomniac Games. No game files are distributed. See [credits](CREDITS.md) and [third-party notices](THIRD_PARTY_NOTICES.txt).

## How it fits together

| Part | Where | What it is |
| --- | --- | --- |
| The game | `third_party/OpenRAC` | [OpenRAC](https://github.com/OpenRAC/OpenRAC) 0.10.1 as a submodule (`293cb5cc`), with the Quest's changes to its frame path in [`patches/`](patches) |
| The VR mod | `vr/` | A native OpenRAC mod: OpenXR on the Quest, the head and hands into the game's views, Touch controllers into its pad |
| The launcher | `android/` | The APK: disc import and set-up, settings, controls, logs, credits, updates |
| The build | `tools/build_quest.py` | Everything into one APK; `tools/build_pc.py` for the PC test |
| Status | [`docs/STATUS.md`](docs/STATUS.md) | Every feature, and the strongest test each has had |

Why OpenRAC and not ReRAC, which the PC VR mod is built on: Rerac-vr's own measurements (51–59 fps near spawn, 14–25 under Blaster fire on a Ryzen 5 5600 and RTX 3090) rule it out on a headset's CPU; Rerac-vr's design is ported into `vr/` instead. More in [docs/STATUS.md](docs/STATUS.md).

## Building

On Windows with Git Bash (the tested set-up), in a folder without spaces:

```sh
git clone --recursive https://github.com/moistman42069/RatchetAndClank-Quest-VR.git C:/RACQuest/RatchetQuestVR
cd C:/RACQuest/RatchetQuestVR
bash tools/setup_toolchain.sh        # JDK 17, Android SDK 34, NDK 27.2, Rust, under C:\RACQuest\toolchain
source env.sh
python tools/build_quest.py --release
```

It needs Python 3.13 or later, CMake 3.24 or later and Ninja on the PATH. The build stops unless OpenRAC is at the pinned commit with `patches/` applied (it applies them to a clean checkout), runs the launcher's tests, and opens the finished APK to check it. A release must be signed with the project's key (see [docs/RELEASING.md](docs/RELEASING.md)); without it the APK is signed with your debug key, and the launcher's updater will not install over it.

## Licence

This project is free software under the GNU General Public License, version 3 or (at your option) any later version (`GPL-3.0-or-later`), without any warranty; the full text is in [LICENSE](LICENSE).

- **Why:** the game it runs is OpenRAC (GPL-3.0-or-later), and the walking and facing hooks come from OpenRAC's rac1-mods (GPL-3.0-or-later).
- **Source:** each release's source is its tag: this repository, OpenRAC at the submodule's commit, and `patches/`.
- **Other parts:** third-party code keeps its own licence, listed in [THIRD_PARTY_NOTICES.txt](THIRD_PARTY_NOTICES.txt); Rerac-vr's design is used under its ISC License.
- **Ratchet & Clank is not covered:** the licence covers this project's and its upstreams' work, and gives no rights in Ratchet & Clank, a trademark of Sony Interactive Entertainment, or anything else Sony or Insomniac Games own.
