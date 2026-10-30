// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 the Ratchet & Clank Quest VR contributors
//
// Touch controllers as the game's pad: what each control means in each phase of play, and the pad
// report it makes. Plain data, apart from OpenXR and the game (tests/pad_test.cpp).
//
// The layout is Rerac-vr's (npiriou), so a player of the PC VR mod finds the same buttons:
//
//   left stick   walk (head-relative)      right stick  turn, up/down looks in a menu
//   A            jump (Cross)              B            crouch or Clank's packs (R1)
//   X            map (Select)              Y            pause (Start)
//   right trigger  fire (Circle)           right grip   wrench (Square)
//   left trigger   crouch (R1)             left grip    crouch (R1)
//   right stick click  talk, quick select (Triangle), the right hand picking the slot
//   left stick click   first-person look (L1)
//   both sticks clicked  recentre

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "rig.h"

namespace quest {

// The pad's buttons, set while held (openrac_mod.h's OPENRAC_PAD_*).
enum PadBit : std::uint16_t {
    kPadSelect = 0x0001,
    kPadL3 = 0x0002,
    kPadR3 = 0x0004,
    kPadStart = 0x0008,
    kPadUp = 0x0010,
    kPadRight = 0x0020,
    kPadDown = 0x0040,
    kPadLeft = 0x0080,
    kPadL2 = 0x0100,
    kPadR2 = 0x0200,
    kPadL1 = 0x0400,
    kPadR1 = 0x0800,
    kPadTriangle = 0x1000,
    kPadCircle = 0x2000,
    kPadCross = 0x4000,
    kPadSquare = 0x8000,
};

// The controllers this tick.
struct Controls {
    float left_x = 0, left_y = 0;    // sticks, -1..1, up positive
    float right_x = 0, right_y = 0;
    float left_trigger = 0, right_trigger = 0;
    float left_grip = 0, right_grip = 0;
    bool a = false, b = false, x = false, y = false;
    bool left_click = false, right_click = false;
};

// What the mod knows of the moment.
struct PadContext {
    bool in_menu = false;       // a menu or the title: buttons as a menu wants them, sticks the d-pad
    bool first_person = false;  // the head steers: the left stick is turned by head and camera
    float head_yaw = 0;         // world yaw of where forward on the stick goes
    float camera_yaw = 0;       // the game's camera yaw (stick angle plus this is the move yaw)
    bool quick_select = false;  // the right hand picks in the quick select: its stick goes out
    float pick_x = 0, pick_y = 0;  // the hand's push for that, -1..1, up positive
};

// The pad as it comes out.
struct VirtualPad {
    std::uint16_t held = 0;
    float left_x = 0, left_y = 0;  // up positive
    float right_x = 0, right_y = 0;
};

inline constexpr float kTriggerOn = 0.55f;
inline constexpr float kTriggerOff = 0.35f;
inline constexpr float kStickDead = 0.15f;

// A stick past its dead zone, with the rest of its travel stretched back to 0..1, round.
inline void shape_stick(float& x, float& y) {
    const float length = std::sqrt(x * x + y * y);
    if (length <= kStickDead) {
        x = y = 0;
        return;
    }
    const float strength = std::min(1.0f, (length - kStickDead) / (1.0f - kStickDead));
    x = x / length * strength;
    y = y / length * strength;
}

// Forward on the stick is `head_yaw` in the world; a stick up in the game is the camera's yaw, and
// the game adds the two. So the stick is turned by the angle from the camera to the head.
inline void turn_stick(float& x, float& y, float head_yaw, float camera_yaw) {
    const float angle = head_yaw - camera_yaw;
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    const float turned_x = x * c - y * s;
    const float turned_y = x * s + y * c;
    x = turned_x;
    y = turned_y;
}

// One side of an analog control read as a button, with a gap so that it does not chatter.
struct Latch {
    bool on = false;
    bool update(float value) {
        on = on ? value > kTriggerOff : value > kTriggerOn;
        return on;
    }
};

class PadMapper {
public:
    VirtualPad map(const Controls& c, const PadContext& context) {
        VirtualPad out;
        float left_x = c.left_x;
        float left_y = c.left_y;
        float right_x = c.right_x;
        float right_y = c.right_y;
        shape_stick(left_x, left_y);
        shape_stick(right_x, right_y);

        const bool fire = m_right_trigger.update(c.right_trigger);
        const bool wrench = m_right_grip.update(c.right_grip);
        const bool crouch_trigger = m_left_trigger.update(c.left_trigger);
        const bool crouch_grip = m_left_grip.update(c.left_grip);

        if (context.in_menu) {
            // Either stick moves the cursor, as the d-pad; A takes, B goes back, Y and X as in play.
            set(out, c.a || fire, kPadCross);
            set(out, c.b, kPadTriangle);
            set(out, c.y, kPadStart);
            set(out, c.x, kPadSelect);
            set(out, wrench, kPadSquare);
            set(out, crouch_trigger, kPadL1);
            set(out, crouch_grip, kPadR1);
            const float x = std::abs(left_x) > std::abs(right_x) ? left_x : right_x;
            const float y = std::abs(left_y) > std::abs(right_y) ? left_y : right_y;
            set(out, y > 0.5f, kPadUp);
            set(out, y < -0.5f, kPadDown);
            set(out, x < -0.5f, kPadLeft);
            set(out, x > 0.5f, kPadRight);
            out.left_x = left_x;
            out.left_y = left_y;
            out.right_x = right_x;
            out.right_y = right_y;
            return out;
        }

        set(out, c.a, kPadCross);
        set(out, c.b || crouch_trigger || crouch_grip, kPadR1);
        set(out, fire, kPadCircle);
        set(out, wrench, kPadSquare);
        set(out, c.y, kPadStart);
        set(out, c.x, kPadSelect);
        const bool recentring = c.left_click && c.right_click;
        set(out, c.right_click && !recentring, kPadTriangle);
        set(out, c.left_click && !recentring, kPadL1);

        if (context.quick_select) {
            // The ring reads the left stick as the slot's direction, on the screen: not turned.
            out.left_x = context.pick_x;
            out.left_y = context.pick_y;
            return out;
        }

        if (context.first_person) {
            turn_stick(left_x, left_y, context.head_yaw, context.camera_yaw);
        }

        out.left_x = left_x;
        out.left_y = left_y;

        // In first person the right stick turns the player and the game's camera stays out of it.
        if (!context.first_person) {
            out.right_x = right_x;
            out.right_y = right_y;
        }

        return out;
    }

private:
    static void set(VirtualPad& pad, bool on, PadBit bit) {
        if (on) {
            pad.held = static_cast<std::uint16_t>(pad.held | bit);
        }
    }

    Latch m_right_trigger, m_left_trigger, m_right_grip, m_left_grip;
};

// The game's stick reading: an axis byte counts from 0x7F, nothing within 0x30 of it, then
// (|b - 0x7F| - 0x30) / 0x4C up to 1 (keyboard-mouse's kbm.hpp, seen in game code). `v` (-1..1) as
// the byte the game reads back as about v, so a slow walk is a slow walk.
inline std::uint8_t stick_byte(float v) {
    if (v == 0.0f) {
        return 0x80;
    }
    const float distance = 0x30 + 1 + std::min(std::fabs(v), 1.0f) * (0x4C - 1);
    const float byte = 0x7F + (v > 0.0f ? distance : -distance);
    return static_cast<std::uint8_t>(std::clamp(std::round(byte), 0.0f, 255.0f));
}

// The button of each pressure byte, bytes 6..17: right, left, up, down, triangle, circle, cross,
// square, L1, R1, L2, R2 (kbm.hpp).
inline constexpr std::array<std::uint16_t, 12> kPressureBits = {
    kPadRight, kPadLeft, kPadUp, kPadDown, kPadTriangle, kPadCircle, kPadCross, kPadSquare,
    kPadL1, kPadR1, kPadL2, kPadR2,
};

// Into the game's report (18 bytes: buttons active low, right stick, left stick, twelve
// pressures): the controllers' buttons held as well as a real pad's; an axis of ours off centre
// takes that axis's place. Stick bytes run down and right: up on a stick is a low byte.
inline void merge_report(const VirtualPad& pad, std::uint8_t* report, int size) {
    if (size < 6) {
        return;
    }
    const std::uint16_t was = static_cast<std::uint16_t>(~(report[0] | (report[1] << 8)));
    const std::uint16_t held = static_cast<std::uint16_t>(was | pad.held);
    report[0] = static_cast<std::uint8_t>(~held & 0xFF);
    report[1] = static_cast<std::uint8_t>(~held >> 8);
    const auto axis = [&](int at, float value) {
        if (value != 0.0f) {
            report[at] = stick_byte(value);
        }
    };
    axis(2, pad.right_x);
    axis(3, -pad.right_y);
    axis(4, pad.left_x);
    axis(5, -pad.left_y);
    for (int i = 0; i < static_cast<int>(kPressureBits.size()) && 6 + i < size; ++i) {
        if ((pad.held & kPressureBits[i]) != 0) {
            report[6 + i] = 0xFF;
        }
    }
}

// How a turn control acts. One of these each tick gives the turn to make this tick, radians left.
struct TurnSettings {
    bool smooth = false;
    float snap_angle = 0.7853982f;   // 45 degrees
    float smooth_rate = 2.4f;        // radians a second at full stick
};

class TurnControl {
public:
    float update(float stick_x, float seconds, const TurnSettings& settings) {
        const float push = std::abs(stick_x) > 0.25f ? stick_x : 0.0f;
        if (settings.smooth) {
            return -push * settings.smooth_rate * seconds;
        }
        // A snap needs the stick back near the centre before the next one.
        if (m_armed && std::abs(stick_x) > 0.7f) {
            m_armed = false;
            return stick_x > 0 ? -settings.snap_angle : settings.snap_angle;
        }
        if (std::abs(stick_x) < 0.3f) {
            m_armed = true;
        }
        return 0.0f;
    }

private:
    bool m_armed = true;
};

}  // namespace quest
