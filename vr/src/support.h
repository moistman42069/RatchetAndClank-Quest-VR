// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 the Ratchet & Clank Quest VR contributors
//
// A two-handed weapon (Rerac-vr's blaster.rs SupportGrip, npiriou, ISC). The game says which hand
// items Ratchet holds with both arms (an item definition's +0x18 is 2, copied to the hero when the
// item is made). Held in one hand such a weapon is heavy: its turn and position follow the hand with
// a lag, within limits, and it droops and sags. The left hand squeezed on its foregrip holds it too:
// then it follows both hands exactly, pointing from the right hand to the left. Plain math, in
// OpenXR's tracking space (metres; -Z ahead of the fist, Y up out of the thumb, X to the right)
// (tests/rig_pad_test.cpp).

#pragma once

#include "rig.h"

namespace quest {

inline Vec3 cross(Vec3 a, Vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }

inline Vec3 normalized(Vec3 v) {
    const float l = length(v);
    return l > 1e-6f ? v * (1.0f / l) : Vec3{};
}

inline float quat_dot(Quat a, Quat b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }

inline Quat normalized(Quat q) {
    const float l = std::sqrt(quat_dot(q, q));
    return l > 1e-6f ? Quat{q.x / l, q.y / l, q.z / l, q.w / l} : Quat{};
}

// From `a` (t 0) to `b` (t 1), the short way.
inline Quat slerp(Quat a, Quat b, float t) {
    float d = quat_dot(a, b);
    if (d < 0) {
        b = {-b.x, -b.y, -b.z, -b.w};
        d = -d;
    }
    if (d > 0.9995f) {
        return normalized(Quat{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t,
                               a.w + (b.w - a.w) * t});
    }
    const float theta = std::acos(d);
    const float s = std::sin(theta);
    const float wa = std::sin((1 - t) * theta) / s, wb = std::sin(t * theta) / s;
    return {a.x * wa + b.x * wb, a.y * wa + b.y * wb, a.z * wa + b.z * wb, a.w * wa + b.w * wb};
}

// The angle (radians) of the turn from `a` to `b`.
inline float angle_between(Quat a, Quat b) {
    return 2.0f * std::acos(std::min(1.0f, std::fabs(quat_dot(a, b))));
}

// The shortest turn taking the direction `from` to `to` (both of length 1).
inline Quat rotation_arc(Vec3 from, Vec3 to) {
    const float d = dot(from, to);
    if (d < -0.999999f) {
        Vec3 axis = cross({1, 0, 0}, from);
        if (length(axis) < 1e-3f) {
            axis = cross({0, 1, 0}, from);
        }
        axis = normalized(axis);
        return {axis.x, axis.y, axis.z, 0};
    }
    const Vec3 c = cross(from, to);
    return normalized(Quat{c.x, c.y, c.z, 1 + d});
}

// A turn of `angle` radians about X.
inline Quat pitch_quat(float angle) { return {std::sin(angle * 0.5f), 0, 0, std::cos(angle * 0.5f)}; }

// Held in one hand: the lag of turn and position (seconds), their limits (radians, metres), the
// droop (radians of pitch down) and the sag (metres).
inline constexpr float kHeavyTurn = 0.22f;
inline constexpr float kHeavyFollow = 0.12f;
inline constexpr float kHeavyMaxTurn = 0.70f;
inline constexpr float kHeavyMaxLag = 0.10f;
inline constexpr float kHeavyDroop = 0.10f;
inline constexpr float kHeavySag = 0.015f;
// The left hand takes the foregrip squeezed past kSupportTake within kSupportReach of it, and lets
// go below kSupportLet.
inline constexpr float kSupportReach = 0.25f;
inline constexpr float kSupportTake = 0.6f;
inline constexpr float kSupportLet = 0.35f;
// The Hydrodisplacer is not heavy: plugging it into a tap wants the hand's exact pose.
inline constexpr int kHydrodisplacerItem = 22;

// Where the left hand holds item `item`, from the right hand's grip (right, up, back).
inline Vec3 foregrip(int item) {
    switch (item) {
    case 11: return {-0.27f, 0.09f, -0.36f};  // Devastator
    case 13: return {-0.27f, 0.05f, -0.22f};  // Visibomb
    case 16: return {0.0f, -0.10f, -0.49f};   // Pyrocitor
    case 23: return {0.0f, -0.06f, -0.35f};   // RYNO
    case 9: return {0.0f, -0.01f, -0.40f};    // Suck Cannon
    default: return {0.0f, 0.0f, -0.32f};
    }
}

struct Support {
    bool held = false;        // the left hand holds the foregrip
    bool two_handed = false;  // the item in the hand is held with both arms
    int weapon = 0;
    bool settled = false;     // `rotation` and `position` are the weapon's
    Quat rotation;            // the weapon's turn and grip, lagging behind the right hand
    Vec3 position;
    Vec3 lag;                 // `position` from where the right hand puts it

    void release() {
        settled = false;
        lag = {};
    }

    // The foregrip with the right hand at `right`.
    Vec3 point(const Pose& right) const { return right.position + rotate(right.orientation, foregrip(weapon)); }

    // The left hand at `left` holds, or would take, the foregrip.
    bool reaches(const Pose& right, const Pose& left) const {
        return held || (two_handed && length(left.position - point(right)) < kSupportReach);
    }
};

// Both hands on it: the right hand turned so the foregrip points at the left.
inline Pose physical_hand(Pose right, const Pose* left, const Support& s) {
    if (s.held && left != nullptr) {
        const Vec3 direction = left->position - right.position;
        if (length(direction) > 0.12f) {
            const Vec3 current = normalized(rotate(right.orientation, foregrip(s.weapon)));
            right.orientation = normalized(rotation_arc(current, normalized(direction)) * right.orientation);
        }
    }
    return right;
}

// Where the weapon's grip is, for the right hand at `right`.
inline Pose weapon_hand(const Pose& right, const Pose* left, const Support& s) {
    Pose pose = physical_hand(right, left, s);
    if (s.two_handed && !s.held && s.settled) {
        pose.orientation = s.rotation;
        pose.position = pose.position + s.lag;
    }
    return pose;
}

// Where the left hand is drawn: on the foregrip while it holds it.
inline Pose support_hand(const Pose& right, const Pose& left, const Support& s) {
    if (!s.held) {
        return left;
    }
    const Pose weapon = weapon_hand(right, &left, s);
    return {s.point(weapon), left.orientation};
}

// `dt` seconds on: item `item` in the right hand (two-handed or not), the hands, the left grip.
inline void step(Support& s, int item, bool two_handed, const Pose& right, const Pose* left, float left_grip, float dt) {
    if (!two_handed || s.weapon != item || left_grip < kSupportLet || left == nullptr) {
        if (s.weapon != item) {
            s.release();
        }
        s.held = false;
    }
    s.weapon = item;
    if (two_handed && left_grip > kSupportTake && left != nullptr
        && length(left->position - s.point(right)) < kSupportReach) {
        s.held = true;
    }
    s.two_handed = two_handed;
    const bool one_handed = two_handed && !s.held && item != kHydrodisplacerItem;
    const Pose physical = physical_hand(right, left, s);
    Quat desired = physical.orientation;
    Vec3 target = physical.position;
    if (one_handed) {
        desired = normalized(physical.orientation * pitch_quat(-kHeavyDroop));
        target.y -= kHeavySag;
    }
    const Quat old = s.settled ? s.rotation : desired;
    const Vec3 previous = s.settled ? s.position : target;
    const float turn = one_handed ? 1.0f - std::exp(-dt / kHeavyTurn) : 1.0f;
    const float follow = one_handed ? 1.0f - std::exp(-dt / kHeavyFollow) : 1.0f;
    Quat weighted = slerp(old, desired, turn);
    const float behind = angle_between(weighted, desired);
    if (behind > kHeavyMaxTurn) {
        weighted = slerp(weighted, desired, 1.0f - kHeavyMaxTurn / behind);
    }
    s.rotation = weighted;
    Vec3 offset = (previous + (target - previous) * follow) - target;
    const float far = length(offset);
    if (far > kHeavyMaxLag) {
        offset = offset * (kHeavyMaxLag / far);
    }
    s.position = target + offset;
    s.lag = s.position - physical.position;
    s.settled = true;
}

}  // namespace quest
