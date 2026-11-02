// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 the Ratchet & Clank Quest VR contributors
//
// What this mod and the port's frontend/views.cpp agree on beyond openrac_mod.h: the fork's meanings
// of openrac_view.reserved and one more view kind. Both sides name these; this is the mod's side.

#pragma once

#include <cstdint>

namespace quest {

// openrac_view.kind: the window's picture as it stands (world and 2D), scaled to the view, and the
// frame's movie or boot still when reserved[1] says the frame shows one.
inline constexpr std::uint32_t kViewKindWindow = 3;

// openrac_view.reserved[0], set by the mod: position and orientation are the eye's in the world, in
// the Y-up frame of rig.h, and not relative to the game's camera.
inline constexpr std::uint32_t kViewFlagWorldPose = 1;
// openrac_view.reserved[0] bit 2, on an eye: the game's camera is far from it (the Visibomb's missile), so the port
// draws what is near the eye too, and the game's last sky when it draws none.
inline constexpr std::uint32_t kViewFlagNearEye = 2;
inline constexpr std::uint32_t kViewFlagNoOwnCameras = 4;
// openrac_view.reserved[0] bit 8, on an eye: a head turns it away from the game's camera, and the game draws no moby
// behind its camera, so the port keeps what is within draw distance of the eye too.
inline constexpr std::uint32_t kViewFlagHeadTurns = 8;

// views[0].reserved[1], set by the port before begin: this frame shows a picture, not the scene.
inline constexpr int kViewHintPicture = 1;
// views[0].reserved[2], set by the port before begin: the scene's blend from the last tick to the
// kept one, the bytes of a float from 0 to 1.
inline constexpr int kViewHintAlpha = 2;

}  // namespace quest
