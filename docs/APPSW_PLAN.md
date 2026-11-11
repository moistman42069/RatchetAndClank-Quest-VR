# Application SpaceWarp (AppSW) on the Quest: how 0.1.17 does it

The owner's ask (7 November 2026, during 0.1.17): AppSW in the launcher, to smooth the frame rate, in this release.
It is in 0.1.17 as **Smoother frames (AppSW)**, on the launcher's Graphics tab, **on** to start (the rule for the
port's new switches). Only a Quest can show how it looks: everything below was checked in the PC sim's headset.

## What AppSW is

Meta's `XR_FB_space_warp`. The game draws every other frame (36 a second at 72 Hz, 45 at 90, 60 at 120) and hands
the headset, beside each eye's picture, two more images: how far each pixel moved since the last drawn frame (the
motion vectors) and its depth. The headset makes the frames in between. At 120 Hz the game draws 60 a second, its
own tick rate, with twice the time for each picture. From 0.1.20 smoother frames always run the headset at 120 Hz
(xr_openxr.cpp kSmootherHz): a tester's micro stutters at 72 Hz (36 pictures for 60 ticks) were gone at 120, and every
launcher preset had set 72.

What Meta's guide asks for ([Application SpaceWarp Developer Guide](https://developers.meta.com/horizon/documentation/native/android/mobile-asw/)),
and how it is met:

- Motion vectors in NDC, this frame's position minus the last frame's, each through its own frame's view-projection
  and its own object pose: `GL_RGBA16F` (0x881A), at the size `XrSystemSpaceWarpPropertiesFB` recommends.
- Depth of the same pass, `GL_DEPTH24_STENCIL8` (0x88F0), the same size. The port's eyes use reversed depth
  (cleared to 0, `GEQUAL`), so the layer says `nearZ > farZ`: `nearZ` the distance at depth 0 (the far plane,
  2000 units) and `farZ` the distance at depth 1 (the near plane, 0.05), both in metres (units over the eyes'
  separation in units a metre).
- `XrCompositionLayerSpaceWarpInfoFB` chained on each eye's `XrCompositionLayerProjectionView`.
  `appSpaceDeltaPose` stays identity: the motion vectors are made from the eyes' own view-projections, which
  already hold Ratchet's movement and the stick's turn.
- The HUD and the panels sit on quad layers of their own, which AppSW does not touch.

## The port: the eyes' motion pass (patches/0001)

`port/game/common/frontend/views.cpp`, drawn after each world eye view (`draw_view_motion`):

1. **Depth:** the level's terrain, ties and shrubs into the motion depth, colour off, as the eye drew them.
2. **The camera's motion:** a full-screen pass that turns each depth back into a world point with this frame's
   inverse view-projection and takes it through the last frame's. The sky (depth 0) moves by direction only.
3. **Mobys with their own motion:** each drawn moby again (`LevelScene::draw_motion`) with the shader of the FSR
   velocity pass (`motion.vert`, its water hook included), this frame's matrix and joint palette against the last
   frame's, matched by the scene's temporal id. Glows and blended draws are left out. One with no last pose counts as
   new (no motion of its own).
4. The last frame's view-projection, objects and palettes are kept for the next (`end_view_motion`,
   `keep_view_motion_scene`, from `upload_instances`).

The pass draws nothing when the eye did not move on smoothly: a level change, a jump of 4 units or more, a turn of
more than about 25 degrees between frames. The SDK's view source has a new call for it (`view_motion`), which old
mods leave empty.

## The mod and OpenXR (vr/src)

- `quest_vr.cpp` (`views_begin`, `views_end`): asks for the motion targets each eye frame, not in a menu in the
  world, the title's world, the vendor, a recentre or a snap of the rig past 10 degrees; marks each eye drawn; logs
  `[vr-view] smoother frames: the eyes' motion drawn (W x H), depth ... to ... m` once.
- `xr_openxr.cpp` (Android only): enables `XR_FB_space_warp` when the runtime lists it and the option is on; reads
  `XrSystemSpaceWarpPropertiesFB`; a motion and a depth swapchain an eye; chains the space warp info when both eyes'
  motion was drawn this frame and neither eye is a kept picture. Logs `[openxr] smoother frames (Application
  SpaceWarp): on, motion W x H an eye` or why it is off, and the frame statistics add `smoother frames on N% of them`.
  Keep the refresh rate's check takes half the rate as the goal when most frames are warped.
- On PC nothing is enabled: SteamVR has no `XR_FB_space_warp`, and its own Motion Smoothing, Meta's ASW over Link and
  Virtual Desktop's frame synthesis already make frames for any app.

## Tests

- `vr/tests/appsw.sim` (regress `appsw`, KEEP=1): the sim's headset makes motion targets a quarter of the eye's
  size and saves the left eye's as `<shot>-motion.png` (grey still, red right, green up), with `[sim] motion` numbers.
  On the PC build: standing still, 0% moving; the head turning left a degree a frame, the level moving right
  (across +0.022 of NDC a frame) with the hands still; a walk spreading from the middle, the sky still; the right hand
  raised alone, only it moving, up; a jump of the head, a cut (no motion given); the head looking up, the level moving
  down (up -0.020).
- On a Quest (HEADSET_CHECKLIST.md): at 72, 90 and 120 Hz, the log's `[openxr] smoother frames` line, the
  statistics' share, and how the view, the hands and enemies look when turning with the stick and walking.

Spec: [XrCompositionLayerSpaceWarpInfoFB](https://registry.khronos.org/OpenXR/specs/1.1/man/html/XrCompositionLayerSpaceWarpInfoFB.html),
[XrSystemSpaceWarpPropertiesFB](https://registry.khronos.org/OpenXR/specs/1.0/man/html/XrSystemSpaceWarpPropertiesFB.html).
