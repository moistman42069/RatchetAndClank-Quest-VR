// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 the Ratchet & Clank Quest VR contributors
//
// A manned turret turned toward where the player points (Rerac-vr's vr/turret.rs and its game side,
// creature/turn.rs aim_speed and units/hoven_turret.rs; npiriou, ISC). All three turrets, Gaspar's
// cannons, Hoven's turret and Batalia's anti-aircraft turret, turn at Hoven's speeds, so that each
// handles alike: a turn speed that eases into the aim and lands without overshooting. Plain math
// (tests/rig_pad_test.cpp).

#pragma once

#include <algorithm>
#include <cmath>

#include "rig.h"

namespace quest {

enum class TurretKind { kGaspar, kHoven, kBatalia };

// Hoven's rates, degrees a second and degrees a second squared.
inline constexpr float kTurretYawSpeed = 150.0f;
inline constexpr float kTurretPitchSpeed = 70.0f;
inline constexpr float kTurretYawAccel = 240.0f;
inline constexpr float kTurretPitchAccel = 180.0f;
inline constexpr float kTurretYawBrake = 360.0f;
inline constexpr float kTurretPitchBrake = 720.0f;
inline constexpr float kDegree = kPi / 180.0f;

// The stick pushed this far steers the turret as the game does; the hand waits.
inline constexpr float kTurretStick = 0.3f;

// `v` (radians a tick) toward an aim `error` radians away: never faster than `most` nor than it can
// still brake from at `brake`, easing over the last ticks.
inline void aim_speed(float& v, float error, float most, float accel, float brake) {
    const float size = std::abs(error);
    const float want = std::copysign(std::min({most, std::sqrt(2.0f * brake * size), size * 0.25f}), error);
    const float rate = want * v > 0.0f && std::abs(want) > std::abs(v) ? accel : brake;
    const float gap = want - v;
    v = std::abs(gap) <= rate ? want : v + std::copysign(rate, gap);
    if (error * v >= 0.0f && std::abs(v) >= size) {
        v = error;
    }
}

inline void turret_yaw_speed(float& v, float error, float tick) {
    aim_speed(v, error, kTurretYawSpeed * kDegree * tick, kTurretYawAccel * kDegree * tick * tick,
              kTurretYawBrake * kDegree * tick * tick);
}

inline void turret_pitch_speed(float& v, float error, float tick) {
    aim_speed(v, error, kTurretPitchSpeed * kDegree * tick, kTurretPitchAccel * kDegree * tick * tick,
              kTurretPitchBrake * kDegree * tick * tick);
}

// The yaw a turret's barrels point along, from its moby's: the models of Gaspar's and Hoven's look
// along their +Y, Batalia's along its +X.
inline float turret_heading(TurretKind kind, float rot_z) {
    return kind == TurretKind::kBatalia ? rot_z : wrap_angle(rot_z + kPi * 0.5f);
}

// Where the eye sits, in the moby's axes. Hoven's is Rerac-vr's. Batalia's is the game's own camera
// (Rerac-vr's, two units further back, is inside the turret's hull as this port draws it). On Gaspar
// and Batalia the game keeps this in its script camera's target, which is read first (seat_now).
inline Vec3 turret_seat(TurretKind kind, Vec3 position, float rot_z) {
    const Vec3 local = kind == TurretKind::kBatalia ? Vec3{4.074f, 0.0f, 6.5f} : Vec3{0.0f, 0.45f, 3.0f};
    const float c = std::cos(rot_z);
    const float s = std::sin(rot_z);
    return {position.x + local.x * c - local.y * s, position.y + local.x * s + local.y * c, position.z + local.z};
}

}  // namespace quest
