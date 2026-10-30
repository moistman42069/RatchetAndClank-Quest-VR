# Status

What this port does and does not do yet. "Proven" names the strongest test each item has had.

## Why OpenRAC is the engine

| | ReRAC + Rerac-vr | OpenRAC |
|---|---|---|
| Measured by its authors | Ryzen 5 5600 and RTX 3090: 51-59 fps near spawn, 14-25 fps in sustained Blaster fire | Runs on phones with OpenGL ES 3.2 |
| The game's code | Rewritten in Rust on Bevy, about 5,000 entities an eye | The original code, recompiled |
| VR | Complete gameplay, PC only | The view and OpenXR hooks, no VR game |
| Discs | SCUS-97199 only | SCES-50916 v2.00 and SCUS-97199 v1.00 |

A Quest 3's CPU is roughly two to three times slower than that desktop chip, so Rerac-vr's own numbers
rule Bevy out on the headset. Rerac-vr's design is ported into `vr/` feature by feature instead.
`rac1-mods` holds no VR code; the VR groundwork is OpenRAC's mod interface (`on_views`, `setup_vulkan`).

## The engine's version

OpenRAC 0.10.1 (`develop` at `293cb5cc`, 2026-10-30): 0.10.0's Android mods, Exit item and texture
fixes, and 0.10.1's Android full screen, lower input lag (late camera latch, low-latency frames) and
rac1 fixes (Batalia's ice, the red planes after a flight, fires at the sides of a wide view).
`patches/0001-quest-vr-frame-path.patch` is the Quest's change to it: the eyes drawn without a third
picture for the window, movies in the headset, the window's swap left to OpenXR, an absolute eye pose,
MSAA for the eyes, `on_before_capture` for the weapon in the hand, and the game's view tests opened
while a headset draws (`picture_wider` 18, some 85 degrees a side): the mobys beside the player stay
"drawn" for the game, so active and making their effects, and the seas send their side blocks.
Sideways only, as upstream's widening is: what lies well above or below the game camera's view is
still out of it for the game (needs a headset test). Checked against 0.10.1: the new
"keep the last picture" frames go through the same patched path; the camera latch reads only a real
gamepad, which the headset does not have; low-latency frames do nothing on OpenGL ES with no window
swap. `tools/build_quest.py` stops unless OpenRAC is at the pin with the patch applied.

## Base features

| Feature | State |
|---|---|
| Launcher: themed, tabs, disc requirements, disc import and set-up | Built |
| Levels made again from the kept disc when an update changes their format | Built, unit tested |
| Updates from this repository's releases in the launcher (checked SHA-256, size, package, version, signing key), automatic checks on by default | Built, unit tested; no release yet to update to, so not run against GitHub |
| Releases signed with the project's own key (`tools/release-cert.sha256`, checked by the build) | Built |
| Graphics options: preset, refresh, resolution, MSAA, filtering, draw distance | Built; every value checked against the port's parser (unit test) |
| VR options: view, turning, world scale, HUD size, aiming, body, vibration | Built |
| Credits and licences in the launcher | Built |
| Logs in `Download/RatchetQuestVR`, summary on the Logs tab | Built |
| Test without VR, on a flat panel | Built |
| Stereo, head tracking, first and third person | Built; the real game on PC with a simulated headset (Novalis, PAL) |
| In first person, Ratchet, Clank and the pack hidden before each frame; the weapon kept | Built; PC run (vr/tests/t2.sim): classes 0, 607, 601, 1204 hidden in play, shown in pause and scenes; looking down shows no body |
| Weapon in the right hand | Built; PC run: the Blaster in the hand, aimed and fired |
| Smooth turning, on by default; snap as an option | Built |
| Touch controllers to the game's pad | Built, unit tested |
| Head-relative walking | Built, unit tested |
| HUD panel, menu panel, movies on a panel | Built |
| Vibration on shots and damage | Built |
| Refresh rate falls back to 72 Hz when not held | Built |

## Rerac-vr features to port

Each needs the game's hooks and a test against the real game.

| Feature | State |
|---|---|
| Guns aim where the hand points | Facing follows the hand; pitch not yet |
| Held weapon and gloves in the hand | Weapon built (above); gloves not yet |
| Real wrench swing, throw, Hyper Strike | Not yet (grip presses the game's swing) |
| Bombs, mines and Decoys thrown by hand | Not yet |
| Two-handed heavy weapons | Not yet |
| Akimbo | Not yet |
| Grabbing ledges with the hands | Not yet |
| Swimming with the arms | Not yet |
| Bolt cranks turned for real | Not yet |
| Plugging in the Hydrodisplacer and Trespasser | Not yet |
| Walloper punch, Clank's fists | Not yet |
| Hoverboard side-on, rails with a smooth view | Not yet |
| Room-scale walking through the game's collision | View only: the body catches up when you move the stick |
| Cockpit view for ships, first-person cutscenes | Not yet (stereo from the game's camera) |
| Spatial pause menu with a pointer | Not yet (menus on a panel, driven by the sticks) |
| Quick select by reaching | Built, needs a headset test |
| Controller glyphs in on-screen hints | Not yet |
| Per-weapon vibration | Not yet (one kind of shot) |
| Helmet and boots, player body options | Not yet |
