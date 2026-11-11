// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 the Ratchet & Clank Quest VR contributors
//
// RAC-Board in a headset (0.1.18, the owner's ask; github.com/mateuszklysz/rac-board, MIT): its board is taken from
// the back, the riding controls are the hands', and the view goes with the board through its spins and flips.
//
// - The board on and off: a hand reached behind the back with its stick clicked, as the board would be pulled from
//   there. Either hand. The hand feels the place when it gets there. The click is the board's alone: it opens no
//   weapon wheel. The mod's own button (its "Board on and off" setting) is pressed for it, a few frames.
// - Riding: the right trigger the mod's spins and flips, the right grip its grind (A, the jump, with it onto a rail),
//   the left trigger or grip its grab; A jumps. Each goes to the button the player chose in the mod's settings. In
//   first person the stick is about the board's heading (pad.h's jet_yaw): forward keeps on its way, a side turns it.
// - The view: it turns with the board's heading as he carves, and with his body through the spins and flips the mod
//   poses him in (his pelvis's turn from how he stands on it), about his middle, back level as he lands.
//
// Plain math, apart from the game (tests/rig_pad_test.cpp).

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

#include "hand.h"
#include "pad.h"
#include "support.h"

namespace quest {

// ---- Behind the back ----

// In the head's heading (tracking space, its yaw alone): a hand at least kBackBehind behind the head's middle, no lower
// than kBackLow under it, within kBackWide of the spine. Once in, the place is kBackLeave larger every way, so a hand at
// its edge does not come and go.
inline constexpr float kBackBehind = 0.12f;
inline constexpr float kBackLow = 0.80f;
inline constexpr float kBackWide = 0.45f;
inline constexpr float kBackLeave = 0.04f;

inline bool hand_behind_back(const Pose& head, Vec3 hand, bool was_in) {
    // The heading: where the face points, flat; looking straight down or up, where the top of the head points.
    Vec3 ahead = rotate(head.orientation, {0, 0, -1});
    ahead.y = 0;
    if (length(ahead) < 0.2f) {
        const Vec3 top = rotate(head.orientation, {0, 1, 0});
        ahead = {top.x, 0, top.z};
        if (rotate(head.orientation, {0, 0, -1}).y > 0) {
            ahead = ahead * -1.0f;  // looking up: the top of the head points back
        }
    }
    ahead = normalized(ahead);
    if (length(ahead) < 0.5f) {
        return false;
    }
    const Vec3 side{-ahead.z, 0, ahead.x};
    const Vec3 d = hand - head.position;
    const float grow = was_in ? kBackLeave : 0.0f;
    const float front = d.x * ahead.x + d.z * ahead.z;
    const float across = d.x * side.x + d.z * side.z;
    return front < -(kBackBehind - grow) && d.y > -(kBackLow + grow) && std::abs(across) < kBackWide + grow;
}

// ---- The mod's buttons ----

// Its "Board on and off" setting (board_button): up, down, l2r2, r2. Unknown: D-pad Up, its default.
inline std::uint16_t board_toggle_bits(const char* value) {
    if (std::strcmp(value, "down") == 0) {
        return kPadDown;
    }
    if (std::strcmp(value, "l2r2") == 0) {
        return kPadL2 | kPadR2;
    }
    if (std::strcmp(value, "r2") == 0) {
        return kPadR2;
    }
    return kPadUp;
}

// Its "Spins and flips" (trick_button): r1r2, r1, l2, r2, l1. One of them is pressed for it: R1 for "R1 or R2".
inline std::uint16_t board_trick_bits(const char* value) {
    if (std::strcmp(value, "l2") == 0) {
        return kPadL2;
    }
    if (std::strcmp(value, "r2") == 0) {
        return kPadR2;
    }
    if (std::strcmp(value, "l1") == 0) {
        return kPadL1;
    }
    return kPadR1;
}

// Its grind (grind_button): r1r2x, r1x (held with Cross, the jump onto a rail: A gives the Cross), circle, square, l2,
// r2. For "R1 or R2" R2, so that with a trick on R1 the grip is not a trick too.
inline std::uint16_t board_grind_bits(const char* value) {
    if (std::strcmp(value, "r1x") == 0) {
        return kPadR1;
    }
    if (std::strcmp(value, "circle") == 0) {
        return kPadCircle;
    }
    if (std::strcmp(value, "square") == 0) {
        return kPadSquare;
    }
    if (std::strcmp(value, "l2") == 0) {
        return kPadL2;
    }
    if (std::strcmp(value, "r2") == 0) {
        return kPadR2;
    }
    return kPadR2;
}

// Its grab (grab_button): l2, r2, l1, r3, none.
inline std::uint16_t board_grab_bits(const char* value) {
    if (std::strcmp(value, "l2") == 0) {
        return kPadL2;
    }
    if (std::strcmp(value, "r2") == 0) {
        return kPadR2;
    }
    if (std::strcmp(value, "r3") == 0) {
        return kPadR3;
    }
    if (std::strcmp(value, "none") == 0) {
        return 0;
    }
    return kPadL1;
}

struct BoardButtons {
    std::uint16_t toggle = kPadUp;
    std::uint16_t trick = kPadR1;
    std::uint16_t grind = kPadR2;
    std::uint16_t grab = kPadL1;
};

// ---- The view with the board ----

// How the board turns him, eased: a follow of `target` (the turn of his body from how he stands on the board), a third
// of the way left after kBoardViewEase, never more than kBoardViewFastest a second. His pose comes at the game's tick, the
// headset's frames faster: the ease takes the tick's steps out, short enough that a flip is seen as it goes (a lag of a
// few degrees, far from the half turn where the short way would be the other one). Back to none when the board lets him go.
inline constexpr float kBoardViewEase = 0.035f;
inline constexpr float kBoardViewFastest = 40.0f;  // radians a second

struct BoardView {
    Quat turn{};
    bool on = false;

    void update(Quat target, bool riding, float seconds) {
        if (!riding) {
            target = Quat{};
        }
        if (seconds <= 0.0f) {
            return;
        }
        const float share = 1.0f - std::exp(-seconds / kBoardViewEase);
        const float apart = angle_between(turn, target);
        float t = share;
        if (apart > 1e-5f && apart * share > kBoardViewFastest * seconds) {
            t = kBoardViewFastest * seconds / apart;
        }
        turn = normalized(slerp(turn, target, std::clamp(t, 0.0f, 1.0f)));
        on = riding || angle_between(turn, Quat{}) > 1e-3f;
        if (!on) {
            turn = Quat{};
        }
    }
};

// The turn taking the axes x, y, z (1 0 0, 0 1 0, 0 0 1) to `x`, `y`, `z`: three of length 1, at right angles, right-handed.
inline Quat frame_quat(Vec3 x, Vec3 y, Vec3 z) {
    const float trace = x.x + y.y + z.z;
    Quat q;
    if (trace > 0.0f) {
        const float s = std::sqrt(trace + 1.0f) * 2.0f;
        q = {(y.z - z.y) / s, (z.x - x.z) / s, (x.y - y.x) / s, 0.25f * s};
    } else if (x.x > y.y && x.x > z.z) {
        const float s = std::sqrt(1.0f + x.x - y.y - z.z) * 2.0f;
        q = {0.25f * s, (y.x + x.y) / s, (z.x + x.z) / s, (y.z - z.y) / s};
    } else if (y.y > z.z) {
        const float s = std::sqrt(1.0f + y.y - x.x - z.z) * 2.0f;
        q = {(y.x + x.y) / s, 0.25f * s, (z.y + y.z) / s, (z.x - x.z) / s};
    } else {
        const float s = std::sqrt(1.0f + z.z - x.x - y.y) * 2.0f;
        q = {(z.x + x.z) / s, (z.y + y.z) / s, 0.25f * s, (x.y - y.x) / s};
    }
    return normalized(q);
}

// A joint's turn from two of its axes as drawn (any length, not quite at right angles): the first kept, the second made
// square to it, the third theirs.
inline Quat axes_quat(Vec3 a, Vec3 b) {
    const Vec3 x = normalized(a);
    const Vec3 y = normalized(b - x * dot(b, x));
    if (length(x) < 0.5f || length(y) < 0.5f) {
        return Quat{};
    }
    return frame_quat(x, y, cross(x, y));
}

// The part of `q` about up (the frame's y; a yaw, left positive as yaw_quat's), in radians -pi..pi; none when q is a half
// turn about a level axis, where it has none.
inline float yaw_part(Quat q) {
    if (std::fabs(q.y) + std::fabs(q.w) < 1e-6f) {
        return 0.0f;
    }
    return wrap_angle(2.0f * std::atan2(q.y, q.w));
}

// What the view shows of his body's turn: its yaw, and its tilt but for the sway of riding (his lean into a carve, the
// bob of a bump, a crouch: in the sim to a third of a radian, a rocking horizon in a headset), none of it under
// kBoardSwayFrom, all of it past kBoardSwayTo (a flip goes on to a half turn and over), eased between.
inline constexpr float kBoardSwayFrom = 0.15f;
inline constexpr float kBoardSwayTo = 0.8f;

inline Quat board_view_turn(Quat turn) {
    const float yaw = yaw_part(turn);
    const Quat tilt = normalized(yaw_quat(-yaw) * turn);
    const float t = std::clamp((angle_between(tilt, Quat{}) - kBoardSwayFrom) / (kBoardSwayTo - kBoardSwayFrom), 0.0f, 1.0f);
    return normalized(yaw_quat(yaw) * slerp(Quat{}, tilt, t * t * (3.0f - 2.0f * t)));
}

// How the view turns with RAC-Board, from how it poses him, a frame at a time (quest_vr.cpp's follow_jet_board; all in
// the frame's axes, y up):
// - `heading`, his heading as the mod turns him (it turns him by it, the spins and flips by his joints). A carve turns
//   nothing (0.1.19: the stick steers the board to where the player looks, pad.h; a view turned with the carve would keep
//   the gaze off the heading and the board turning after it; to 0.1.18 the rig turned with him). A change of more than
//   kBoardHeadingJump at once is the mod's own at a spin's end (its heading jumps to the board's, his body drawn on as it
//   was): no turn of the view.
// - `pelvis`, his pelvis's turn as drawn: its turn from `rest` (how he stands on the board, in his heading's frame, taken
//   as the ride starts and eased to a new stance while he is near it) is the view's `turn` (the spins and flips).
// - After a heading jump (a spin's end, a turn-around, a bounce), the yaw left in that turn goes to the rig a little at a
//   time once he is near level, the view the same: so `turn` is back to none, the view along the board's new way, and the
//   next trick is measured from his stance.
// - Any other yaw in it is his stance or the ride's sway, never the rig's (0.1.19): getting on (the first
//   kBoardMountSettle), all of it goes into `rest` at once; then, while under kBoardNearStance, eased into it. To 0.1.18
//   every yaw went to the rig: getting on, RAC-Board turns his pelvis about 55 degrees from his standing pose to its
//   stance in a second (racboardhead.sim), and the view faced that far off the board's nose, the stick, about the board,
//   taking it 55 degrees from where the player looked; and the pelvis's yaw jitters a tenth of a radian a frame as he
//   rides, which walked the view round a little at a time.
inline constexpr float kBoardHeadingJump = 0.6f;     // radians in a frame: no carve turns him that fast
inline constexpr float kBoardYawToRig = 0.25f;       // seconds: the yaw left in the turn, to a third
inline constexpr float kBoardNearLevel = 0.30f;      // radians of tilt
inline constexpr float kBoardNearStance = 0.35f;     // radians from the stance: a new stance, not a trick
inline constexpr float kBoardStanceEase = 0.6f;      // seconds: a new stance, to a third
inline constexpr float kBoardMountSettle = 1.2f;     // seconds from his first pose on the board: getting on

struct BoardFollow {
    bool started = false;
    float heading = 0;
    Quat rest{};
    float riding = 0;        // seconds since his first pose on the board (kBoardMountSettle)
    bool trick_end = false;  // a heading jump's: the yaw in the turn is the rig's until it is gone

    // This frame's: how much to turn the rig (left positive) and the view's turn after it (`turn`, applied about his
    // middle, on top of the rig; left as it was while his pose is not seen).
    float update(float heading_now, Quat pelvis, bool pelvis_seen, float seconds, Quat& turn) {
        float rig_turn = 0;
        const Quat facing = yaw_quat(heading_now);
        if (!started) {
            if (!pelvis_seen) {
                return 0;  // from his first pose seen
            }
            started = true;
            heading = heading_now;
            rest = conjugate(facing) * pelvis;
        }
        const float change = wrap_angle(heading_now - heading);
        heading = heading_now;
        if (std::fabs(change) > kBoardHeadingJump) {
            rest = yaw_quat(-change) * rest;  // the stance in the world kept where it was: the view does not jump
            trick_end = true;
        }
        if (!pelvis_seen) {
            return rig_turn;  // `turn` as it was
        }
        riding += std::max(seconds, 0.0f);
        turn = normalized(pelvis * conjugate(rest) * conjugate(facing));
        float yaw = yaw_part(turn);
        if (riding < kBoardMountSettle && !trick_end) {
            // Getting on: his stance, all of it.
            rest = yaw_quat(yaw) * rest;
            turn = normalized(pelvis * conjugate(rest) * conjugate(facing));
            yaw = yaw_part(turn);
        }
        const Quat tilt = yaw_quat(-yaw) * turn;
        if (trick_end) {
            if (seconds > 0.0f && angle_between(tilt, Quat{}) < kBoardNearLevel && std::fabs(yaw) > 1e-4f) {
                const float step = yaw * (1.0f - std::exp(-seconds / kBoardYawToRig));
                rig_turn += step;
                rest = yaw_quat(step) * rest;
                turn = normalized(pelvis * conjugate(rest) * conjugate(facing));
            }
            trick_end = std::fabs(yaw_part(turn)) >= 0.02f;
        } else if (seconds > 0.0f && std::fabs(yaw) < kBoardNearStance) {
            // His stance or the ride's sway: into the rest, the rig not turned.
            const float step = yaw * (1.0f - std::exp(-seconds / kBoardStanceEase));
            rest = yaw_quat(step) * rest;
            turn = normalized(pelvis * conjugate(rest) * conjugate(facing));
        }
        // (Once the yaw is the rig's: the stance's ease would turn the view where the rig does not.)
        if (seconds > 0.0f && angle_between(turn, Quat{}) < kBoardNearStance && std::fabs(yaw_part(turn)) < 0.02f) {
            const Quat stance = conjugate(facing) * pelvis;
            rest = normalized(slerp(rest, stance, 1.0f - std::exp(-seconds / kBoardStanceEase)));
            turn = normalized(pelvis * conjugate(rest) * conjugate(facing));
        }
        return rig_turn;
    }
};

}  // namespace quest
