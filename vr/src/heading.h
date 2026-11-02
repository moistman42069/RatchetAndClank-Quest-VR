// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 the Ratchet & Clank Quest VR contributors
//
// Where the player's body faces, from the head and both controllers (Rerac-vr's vr/orientation.rs,
// npiriou, ISC): a weighted mean of the head's heading, the shoulder line, the hands' midpoint ahead of
// the neck and where the controllers point. It follows past 15 degrees and settles within 2, so a glance
// does not swing it, and the head never looks more than 90 degrees from it. Plain math, in OpenXR's
// tracking space (tests/rig_pad_test.cpp). A heading is as tracking_yaw's.

#pragma once

#include <algorithm>
#include <cmath>

#include "rig.h"

namespace quest {

inline constexpr float kBodyHeadWeight = 0.75f;
inline constexpr float kBodyShoulderWeight = 2.0f;
inline constexpr float kBodyReachWeight = 1.5f;
inline constexpr float kBodyPointWeight = 0.5f;
inline constexpr float kBodyStart = 0.26f;    // radians off before it follows
inline constexpr float kBodySettle = 0.035f;  // and where it stops
inline constexpr float kBodyFollowSeconds = 0.15f;
inline constexpr float kBodyNeckBack = 0.10f;

inline float smooth_step(float a, float b, float x) {
    const float t = std::clamp((x - a) / (b - a), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

inline float heading_of(Vec3 v) { return std::atan2(-v.x, -v.z); }

inline Vec3 flat_unit(Vec3 v) {
    v.y = 0;
    const float size = length(v);
    return size > 0.0f ? v * (1.0f / size) : Vec3{};
}

// The head's heading, proof against its pitch: square to its right axis.
inline Vec3 head_forward(const Pose& head) {
    const Vec3 right = rotate(head.orientation, {1, 0, 0});
    Vec3 h{right.z, 0, -right.x};
    if (h.x * h.x + h.z * h.z < 0.04f) {
        h = rotate(head.orientation, {0, 0, -1});
        h.y = 0;
    }
    const Vec3 unit = flat_unit(h);
    return length(unit) > 0.0f ? unit : Vec3{0, 0, -1};
}

// The body's heading this instant; `left` and `right` null when a controller is lost: then the head's.
inline float body_target(const Pose& head, const Pose* left, const Pose* right) {
    const Vec3 h = head_forward(head);
    if (left == nullptr || right == nullptr) {
        return heading_of(h);
    }
    Vec3 sum = h * kBodyHeadWeight;
    // The shoulder line: forward is square to left to right (not when the hands cross).
    Vec3 across = right->position - left->position;
    across.y = 0;
    const Vec3 shoulder = flat_unit({-across.z, 0, across.x}) * 1.0f;
    if (shoulder.x * h.x + shoulder.z * h.z > -0.3f) {
        sum = sum + shoulder * (kBodyShoulderWeight * smooth_step(0.2f, 0.4f, length(across)));
    }
    // The hands' midpoint ahead of the neck (a head turning in place swings the headset about it).
    const Vec3 neck = head.position - h * kBodyNeckBack;
    Vec3 reach = (left->position + right->position) * 0.5f - neck;
    reach.y = 0;
    const Vec3 ahead = flat_unit(reach);
    if (ahead.x * h.x + ahead.z * h.z > 0.0f) {
        sum = sum + ahead * (kBodyReachWeight * smooth_step(0.08f, 0.25f, length(reach)));
    }
    for (const Pose* hand : {left, right}) {
        Vec3 pointing = rotate(hand->orientation, {0, 0, -1});
        pointing.y = 0;
        sum = sum + pointing * kBodyPointWeight;
    }
    return sum.x * sum.x + sum.z * sum.z > 1e-6f ? heading_of(sum) : heading_of(h);
}

class BodyHeading {
public:
    float update(const Pose& head, const Pose* left, const Pose* right, float seconds) {
        const float target = body_target(head, left, right);
        const float head_yaw = heading_of(head_forward(head));
        if (!m_known) {
            m_yaw = target;
            m_known = true;
        }
        const float gap = wrap_angle(target - m_yaw);
        if (std::abs(gap) > kBodyStart) {
            m_turning = true;
        } else if (std::abs(gap) < kBodySettle) {
            m_turning = false;
        }
        if (m_turning) {
            m_yaw = wrap_angle(m_yaw + gap * (1.0f - std::exp(-seconds / kBodyFollowSeconds)));
        }
        const float neck = wrap_angle(head_yaw - m_yaw);
        if (std::abs(neck) > kPi * 0.5f) {
            m_yaw = wrap_angle(head_yaw - std::copysign(kPi * 0.5f, neck));
        }
        return m_yaw;
    }

    void reset() {
        m_known = false;
        m_turning = false;
    }

private:
    bool m_known = false;
    bool m_turning = false;
    float m_yaw = 0;
};

}  // namespace quest
