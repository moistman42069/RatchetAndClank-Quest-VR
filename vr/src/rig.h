// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 the Ratchet & Clank Quest VR contributors
//
// The player's body in the game's world: plain math on what OpenXR reports, apart from OpenXR and
// the game so that tests/rig_test.cpp can run it on any machine.
//
// Three spaces:
//
// - tracking: OpenXR's LOCAL space, metres, right-handed, Y up, looking down -Z at start;
// - frame: the game's world turned so that it is Y up: X is the world's X, Y the world's Z, Z minus
//   the world's Y. Right-handed like tracking, in game units (frontend/views.cpp reads a world pose
//   in this frame). The frame's -Z is the world's +Y;
// - world: the game's own, Z up. A yaw is the angle of (cos yaw, sin yaw) in its XY plane.
//
// The rig joins them: `yaw` turns tracking about Y, `scale` is game units a metre, and the point of
// tracking called `origin` (the head at the last recentre) lands on the anchor in the frame.

#pragma once

#include <algorithm>
#include <cmath>

namespace quest {

inline constexpr float kPi = 3.14159265358979f;
inline constexpr float kTwoPi = 2.0f * kPi;

struct Vec3 {
    float x = 0, y = 0, z = 0;
};

struct Quat {
    float x = 0, y = 0, z = 0, w = 1;
};

inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 operator*(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }

inline float length(Vec3 a) { return std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z); }

inline Quat operator*(Quat a, Quat b) {
    return {
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
    };
}

inline Quat conjugate(Quat q) { return {-q.x, -q.y, -q.z, q.w}; }

inline Vec3 rotate(Quat q, Vec3 v) {
    const Vec3 u{q.x, q.y, q.z};
    const Vec3 t{2 * (u.y * v.z - u.z * v.y), 2 * (u.z * v.x - u.x * v.z), 2 * (u.x * v.y - u.y * v.x)};
    return {
        v.x + q.w * t.x + (u.y * t.z - u.z * t.y),
        v.y + q.w * t.y + (u.z * t.x - u.x * t.z),
        v.z + q.w * t.z + (u.x * t.y - u.y * t.x),
    };
}

// A turn of `angle` radians about Y (counter-clockwise seen from above).
inline Quat yaw_quat(float angle) {
    return {0, std::sin(angle * 0.5f), 0, std::cos(angle * 0.5f)};
}

// -pi..pi.
inline float wrap_angle(float a) {
    while (a > kPi) {
        a -= kTwoPi;
    }
    while (a < -kPi) {
        a += kTwoPi;
    }
    return a;
}

// Between the spaces (a direction or a position in units already the frame's).
inline Vec3 world_to_frame(Vec3 w) { return {w.x, w.z, -w.y}; }
inline Vec3 frame_to_world(Vec3 f) { return {f.x, -f.z, f.y}; }

// The world yaw a direction in the frame points along (its height is left out).
inline float frame_direction_yaw(Vec3 f) { return std::atan2(-f.z, f.x); }

// The turn about Y that takes tracking's -Z to the direction a pose looks along.
inline float tracking_yaw(Quat q) {
    const Vec3 forward = rotate(q, {0, 0, -1});
    return std::atan2(-forward.x, -forward.z);
}

struct Pose {
    Vec3 position;
    Quat orientation;
};

// The rotation that takes tracking's -Z to the unit direction `forward`, Y staying up.
inline Quat looking_along(Vec3 forward) {
    const float pitch = std::asin(std::clamp(forward.y, -1.0f, 1.0f));
    return yaw_quat(std::atan2(-forward.x, -forward.z))
           * Quat{std::sin(pitch * 0.5f), 0, 0, std::cos(pitch * 0.5f)};
}

// The world yaw of tracking's rotation `q` carried by a rig turned by `rig_yaw`: tracking's -Z with
// no turn is the world's +Y.
inline float world_yaw(float rig_yaw, Quat q) {
    return wrap_angle(rig_yaw + tracking_yaw(q) + kPi * 0.5f);
}

struct Rig {
    float yaw = 0;       // tracking's turn into the frame
    float scale = 1;     // game units a metre
    Vec3 origin;         // tracking's point that lands on the anchor (the head at the recentre)

    // Recentre: the head as it is now is the origin, and looks along the world yaw `facing`.
    void recentre(const Pose& head, float facing) {
        origin = head.position;
        yaw = wrap_angle(facing - kPi * 0.5f - tracking_yaw(head.orientation));
    }

    // A tracking pose in the frame, with `anchor` (a frame position) where the origin lands.
    Pose to_frame(const Pose& pose, Vec3 anchor) const {
        const Quat turn = yaw_quat(yaw);
        return {anchor + rotate(turn, (pose.position - origin) * scale), turn * pose.orientation};
    }

    // The tracking point of a frame position, `to_frame` backwards.
    Vec3 from_frame(Vec3 position, Vec3 anchor) const {
        return origin + rotate(yaw_quat(-yaw), (position - anchor) * (1.0f / scale));
    }

    // A tracking pose relative to a game camera, for a view given as relative to it: the head at
    // the recentre looks along the camera. `recentre_yaw` is the head's tracking yaw then.
    Pose to_camera(const Pose& pose, float recentre_yaw) const {
        const Quat turn = yaw_quat(-recentre_yaw);
        return {rotate(turn, (pose.position - origin) * scale), turn * pose.orientation};
    }

    // What a smooth or snap turn does: `angle` to the left about `pivot`, a tracking point (the
    // head), so that the head stays where it is in the frame.
    void turn(float angle, Vec3 pivot) {
        const Vec3 before = rotate(yaw_quat(yaw), pivot - origin);
        yaw = wrap_angle(yaw + angle);
        const Vec3 after = rotate(yaw_quat(yaw), pivot - origin);
        slide_origin(after - before);
    }

    // Moves the origin by `delta`, in the frame's direction but metres: what shifts the view when
    // the head has walked from the anchor.
    void slide_origin(Vec3 delta) { origin = origin + rotate(yaw_quat(-yaw), delta); }
};

// Rerac-vr's gravity_frame, the view's up on Magneboots (quest_vr.cpp's follow_surface): `turn`, the view's turn about
// the anchor after the rig's, taken on by the least arc that brings its up to `up` (a unit vector, y up), a frame at a
// time. A half turn has no least arc: then it waits a frame.
inline Quat follow_up(Quat turn, Vec3 up) {
    const Vec3 old = rotate(turn, {0, 1, 0});
    const Vec3 axis{old.y * up.z - old.z * up.y, old.z * up.x - old.x * up.z, old.x * up.y - old.y * up.x};
    const float w = 1.0f + old.x * up.x + old.y * up.y + old.z * up.z;
    if (w <= 0.001f) {
        return turn;
    }
    const float size = std::sqrt(axis.x * axis.x + axis.y * axis.y + axis.z * axis.z + w * w);
    turn = Quat{axis.x / size, axis.y / size, axis.z / size, w / size} * turn;
    const float n = std::sqrt(turn.x * turn.x + turn.y * turn.y + turn.z * turn.z + turn.w * turn.w);
    return {turn.x / n, turn.y / n, turn.z / n, turn.w / n};
}

// Off the surface the view's up is the world's again, but the arcs that took it along the surface and back can leave it
// turned about up, by as much as the area its up went round (round a wall's corner and off: a quarter turn). The view
// showed the world so turned while the stick, the throws, the punches and the room's steps went by the rig's yaw, off
// by that much until the game was restarted (a PCVR tester on 0.1.17: "magnetic surface controls break the walking
// directions even after you fall of the walk area"). The rig takes that turn on: the same view, and the head's yaw is
// the view's again. Any tilt left (a half turn the arcs could not undo) goes too. Returns the turn the rig took, radians,
// left positive as yaw_quat's.
inline float give_up_turn_to_rig(Quat& turn, float& rig_yaw) {
    if (turn.x == 0.0f && turn.y == 0.0f && turn.z == 0.0f) {
        return 0.0f;
    }
    Vec3 along{0, 0, -1};
    Vec3 now = rotate(turn, along);
    if (now.x * now.x + now.z * now.z < 0.09f) {  // looking up or down: the turn of a level direction across instead
        along = {1, 0, 0};
        now = rotate(turn, along);
    }
    const float yaw = wrap_angle(std::atan2(-now.x, -now.z) - std::atan2(-along.x, -along.z));
    rig_yaw = wrap_angle(rig_yaw + yaw);
    turn = Quat{};
    return yaw;
}

}  // namespace quest
