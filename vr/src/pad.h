// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 the Ratchet & Clank Quest VR contributors
//
// Touch controllers as the game's pad: what each control means in each phase of play, and the pad
// report it makes. Plain data, apart from OpenXR and the game (tests/rig_pad_test.cpp).
//
// The table is Rerac-vr's (npiriou, ISC; vr/game_input.rs), so a player of the PC VR mod finds the
// same buttons. In play:
//
//   left stick   walk                      right stick  turn; click: talk, quick select (Triangle)
//   A            jump (Cross)              B            hover, strafe, Hydro-Pack boost (R2)
//   X            map (Select); in a scene, its view (the mod's)   Y   pause, skip a scene (Start)
//   right trigger  fire (Circle)           right grip   the wrench, swung for real (Square)
//   left grip, left trigger  crouch, the packs (R1); with Physical crouch in first person, the trigger alone
//
// Mounted (a turret, a flown ship) X and B keep Circle and Square; on a turret the left stick steers
// it, the trigger fires it and the right stick's click leaves it. In a menu, where Rerac-vr has a
// pointer this port has not yet, A or the trigger confirm: Cross, and Circle as well, as the game's
// memory card questions ask for Yes with Circle. B goes back (Triangle), either stick moves, the left
// grip and trigger are the page shoulders L1 and R1.
//
// On the big screen (the launcher's 2D mode) the controllers are simply a pad: A Cross, B Circle,
// X Square, Y Triangle, triggers L1 and R1, grips L2 and R2, the menu button Start, the left stick's
// click Select, and the sticks are its sticks.

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
    bool menu = false;           // the left controller's menu button
    bool left_click = false, right_click = false;
    // The controllers' touch sensors: a first finger on the trigger, a thumb on a button, the stick or the thumb rest.
    bool left_first_touch = false, right_first_touch = false;
    bool left_thumb_touch = false, right_thumb_touch = false;
};

// What the mod knows of the moment.
struct PadContext {
    bool in_menu = false;       // a menu or the title: buttons as a menu wants them, sticks the d-pad
    bool screen = false;        // the big screen: the controllers are a plain pad
    bool first_person = false;  // the head steers: the left stick is turned by head and camera
    float head_yaw = 0;         // world yaw of where forward on the stick goes
    float camera_yaw = 0;       // the game's camera yaw (stick angle plus this is the move yaw)
    bool quick_select = false;  // the ring is open: the right hand picks, the trigger turns its page
    bool mounted = false;       // a turret or a flown ship
    bool flying = false;        // a flown ship: its lasers are Square
    bool swing = false;         // first person: the right hand is swinging the wrench
    bool button_moves = false;  // first person, Arm moves on Buttons: the right grip is the game's swing, as in third person
    bool trigger_held_back = false;  // the trigger is taking the gun out, not firing it
    bool throw_by_hand = false;      // the trigger holds a glove's bomb for the hand to throw
    bool punch = false;              // a punch of the hand: the Walloper's lunge, as its fire
    bool left_support = false;       // the left grip holds the weapon's foregrip, not R1
    bool crouch = false;             // Physical crouch: the player's own head is down, R1 as the left grip's crouch is
    // The left grip is R1 (crouch, the packs), Rerac-vr's; not with Physical crouch on foot in first person, where the
    // head crouches and the hand is left free for its fingers (the owner's ask). The left trigger is R1 still.
    bool grip_crouch = true;
    // Hanging by the hands: the grips hold on, so R1 and the stick are the mod's, and A climbs only
    // with both hands on and settled.
    bool ledge = false;
    bool ledge_climb = false;        // A may climb
    bool ledge_press = false;        // the shove: Cross
    bool ledge_drop = false;         // both hands let go: R1, the game's drop
    bool ledge_stick = false;        // the hands pull along it: the stick toward head_yaw
    bool dive = false;               // the hands pushed the water up, or stroked looking down: Square, the game's dive
    bool surface = false;            // at the water's surface, either view: the left trigger dives (0.1.17)
    bool raw_stick = false;          // the stick steers a Visibomb missile: not turned by the head
    bool board = false;              // on the hoverboard: boost, tricks, and the stick steers it
    bool board_steers = false;       // on it and not spinning in a trick: the stick's x turns it about its own heading
    bool third_person_walk = false;  // third person on foot: forward on the stick is where the head looks (rerac-vr-3p)
    float head_turn = 0;             // how far the head has turned from the game's camera's heading
    float board_yaw = 0;             // its heading
    // In the Hologuise disguise (Rerac-vr's): B, or A, takes it off (Square, A's Cross held too, so he
    // does not jump on coming back); the trigger and the raised open hand salute (Circle).
    bool disguised = false;
    bool salute = false;
    bool puzzle = false;             // at the Trespasser's rings: the sticks turn them
    bool left_weapon = false;        // Akimbo: the left trigger fires what the left hand holds, not R1
    bool left_ring = false;          // Akimbo: the left stick's click holds the ring open
    bool lead = false;               // Akimbo: the left hand's Walloper or Swingshot is the hand's item: the
                                     // left trigger is the game's fire, the right trigger nothing
    bool shopping = false;           // at the vendor on foot: the trigger and the right stick's click are the shop's
    bool menu_aimed = false;         // the trigger answers only what the pointer is on (the vendor)
    std::uint16_t menu_pulse = 0;    // a key the mod presses itself this tick (the vendor's strip, a step)
    std::uint16_t menu_key = 0;      // in a menu, the pointer on a line a key answers (the quit box's): the
                                     // trigger presses that key alone, not Cross
    bool menu_pointing = false;      // in a menu with things to point at, the right hand tracked: pointing is the
                                     // selection, A answers as the trigger does and the sticks leave the cursor alone
    bool menu_map = false;           // the pause menu's map: A is Cross alone (its missions), the trigger Circle alone
                                     // (the planet's infobot), as their icons there say
    bool skip_held = false;          // Y skipped a scene and is still down: it does nothing else until let go
    bool view_held = false;          // X switched a scene's view and is still down: no map (Select) until let go
};

// The pad as it comes out.
struct VirtualPad {
    std::uint16_t held = 0;
    bool withdraw = false;         // a fresh press at the Trespasser's rings: take the tool out
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

// The right hand's salute in the Hologuise (Rerac-vr's Salute): the right hand raised open, at the
// shoulders or above, within 0.75 m of the head and not across to the left (a salute at the brow, or
// a wave), held 5 frames; it lets go with 0.1 of slack. `head` and `hand` in tracking space.
struct Salute {
    int frames = 0;
    bool active = false;

    bool update(const Pose& head, const Pose& hand, float grip) {
        const Vec3 p = rotate(conjugate(head.orientation), hand.position - head.position);
        const auto pose = [&](float m) {
            return grip < 0.6f + m && p.y > -0.15f - m && p.x > -0.15f - m && length(p) < 0.75f + m;
        };
        if (active) {
            active = pose(0.1f);
        } else if (pose(0.0f)) {
            frames = std::min(frames + 1, 100);
            active = frames >= 5;
        } else {
            frames = 0;
        }
        return active;
    }
    void reset() { *this = Salute{}; }
};

class PadMapper {
public:
    VirtualPad map(const Controls& given, const PadContext& context) {
        VirtualPad out;
        Controls c = given;
        float left_x = c.left_x;
        float left_y = c.left_y;
        float right_x = c.right_x;
        float right_y = c.right_y;
        shape_stick(left_x, left_y);
        shape_stick(right_x, right_y);

        const bool right_fire = m_right_trigger.update(c.right_trigger);
        const bool wrench = m_right_grip.update(c.right_grip);
        const bool left_fire = m_left_trigger.update(c.left_trigger);
        bool fire = context.lead ? left_fire : right_fire;
        const bool left_squeeze = m_left_grip.update(c.left_grip);

        if (context.screen) {
            set(out, c.a, kPadCross);
            set(out, c.b, kPadCircle);
            set(out, c.x, kPadSquare);
            set(out, c.y, kPadTriangle);
            set(out, fire, kPadR1);
            set(out, left_fire, kPadL1);
            set(out, wrench, kPadR2);
            set(out, left_squeeze, kPadL2);
            set(out, c.menu, kPadStart);
            set(out, c.left_click, kPadSelect);
            if (context.in_menu) {
                directions(out, left_x, left_y, right_x, right_y);
            }
            out.left_x = left_x;
            out.left_y = left_y;
            out.right_x = right_x;
            out.right_y = right_y;
            return out;
        }

        if (context.in_menu) {
            const bool keyed = context.menu_key != 0;
            // A answers a pointed key label as the trigger does; at the vendor A stays Cross, which buys, as its screen says.
            const bool a_keyed = keyed && !context.menu_aimed;
            const bool confirm = (c.a && !a_keyed) || (fire && !keyed && !context.menu_aimed);
            if (context.menu_map) {
                // The map takes Cross for its missions page and Circle for the planet's infobot, which closes the
                // menu: both at once (A's confirm), the infobot won over the missions (a tester's, 0.1.12).
                set(out, c.a && !a_keyed, kPadCross);
                set(out, (fire && !keyed) || c.x, kPadCircle);
            } else {
                set(out, confirm, kPadCross);
                set(out, confirm || c.x, kPadCircle);
            }
            if ((keyed && fire) || (a_keyed && c.a)) {
                out.held = static_cast<std::uint16_t>(out.held | context.menu_key);
            }
            out.held = static_cast<std::uint16_t>(out.held | context.menu_pulse);
            set(out, c.b || c.right_click, kPadTriangle);
            set(out, c.y, kPadStart);
            set(out, c.left_click, kPadSelect);
            set(out, wrench, kPadSquare);
            set(out, left_squeeze, kPadL1);
            set(out, left_fire, kPadR1);
            if (context.menu_pointing) {
                m_menu_trigger = fire;
                return out;  // the game's lists also move by the analog sticks
            }
            directions(out, left_x, left_y, right_x, right_y);
            out.left_x = left_x;
            out.left_y = left_y;
            out.right_x = right_x;
            out.right_y = right_y;
            m_menu_trigger = fire;
            return out;
        }

        // A pull that answered a menu fires nothing after it, until let go (Rerac-vr's menu_pointer.rs).
        m_menu_trigger = fire && m_menu_trigger;
        fire = fire && !m_menu_trigger;
        if (context.skip_held) {
            c.y = false;
        }
        if (context.view_held) {
            c.x = false;
        }

        // At the vendor on foot the trigger and the right stick's click are the shop's, which the mod sends to its
        // update alone; a pull that was the shop's fires nothing after it, until let go (game_input.rs).
        if (context.shopping) {
            c.right_click = false;
        }
        m_shop_trigger = fire && (m_shop_trigger || context.shopping);
        fire = fire && !m_shop_trigger;

        // The Trespasser's rings (Rerac-vr's): the sticks turn them; Y, the click or a fresh pull of the
        // trigger withdraw the tool (Triangle). What withdrew it does nothing else until let go.
        const bool utility = c.y || c.right_click || fire;
        const bool fresh = utility && !m_utility_down;
        m_utility_down = utility;
        if (context.puzzle) {
            directions(out, left_x, left_y, right_x, right_y);
            set(out, fresh, kPadTriangle);
            out.withdraw = fresh;
            m_utility_spent = m_utility_spent || fresh;
            return out;
        }
        m_utility_spent = m_utility_spent && utility;
        if (m_utility_spent) {
            c.y = c.right_click = fire = false;
        }

        if (context.ledge) {
            set(out, (c.a && context.ledge_climb) || context.ledge_press, kPadCross);
            set(out, context.ledge_drop, kPadR1);
            left_x = 0;
            left_y = context.ledge_stick ? 1.0f : 0.0f;
        } else {
            set(out, c.a, kPadCross);
            if (context.board) {
                // The left trigger boosts, the left grip is L1, the right stick holds a trick's shoulder.
                set(out, left_fire, kPadSquare);
                set(out, left_squeeze, kPadL1);
                if (std::sqrt(right_x * right_x + right_y * right_y) >= 0.6f) {
                    if (std::abs(right_y) >= std::abs(right_x)) {
                        set(out, right_y > 0, kPadL1);
                        set(out, right_y <= 0, kPadL2);
                    } else {
                        set(out, right_x > 0, kPadR1);
                        set(out, right_x <= 0, kPadR2);
                    }
                }
            } else {
                // At the water's surface the left trigger, the crouch, dives: Square, the game's dive (a tester asked how
                // to dive in first person, where Square is only a swing; 0.1.17), and in third person as well, where the
                // right grip is Square too. The crouch does nothing there.
                const bool dives = left_fire && !context.left_weapon && context.surface;
                set(out, (left_squeeze && !context.left_support && context.grip_crouch) || (left_fire && !context.left_weapon && !dives) || context.crouch,
                    kPadR1);
                set(out, dives, kPadSquare);
            }
        }
        set(out, c.y, kPadStart);
        set(out, c.b, context.mounted || context.disguised ? kPadSquare : kPadR2);
        set(out, context.disguised && c.a, kPadSquare);
        set(out, c.x, context.mounted ? kPadCircle : kPadSelect);
        // The ring's page takes the trigger: it fires nothing there, nor after until let go.
        m_ring_trigger = fire && (m_ring_trigger || context.quick_select);
        // Hanging, the trigger is the free hand's gun's, which the mod runs: the game's fire would let go.
        if ((fire && !m_ring_trigger && !context.trigger_held_back && !context.throw_by_hand && !context.ledge) || context.punch) {
            out.held = static_cast<std::uint16_t>(out.held | (context.flying ? kPadSquare : kPadCircle));
        }
        set(out, context.salute, kPadCircle);
        set(out, c.right_click, kPadTriangle);

        // In first person the wrench swings when the hand does; else the grip is the button (and in first
        // person too with Arm moves on Buttons). A turret has no wrench to swing, and a hand on a ledge is
        // climbing: a shove with the right grip closed is not a swing, which would knock him off.
        const bool turret = context.mounted && !context.flying;
        const bool swung = context.first_person ? context.swing || (context.button_moves && wrench) : wrench;
        set(out, !turret && !context.ledge && swung, kPadSquare);
        set(out, context.dive, kPadSquare);
        set(out, context.left_ring, kPadTriangle);

        if (context.first_person && !context.raw_stick && !context.board) {
            turn_stick(left_x, left_y, context.head_yaw, context.camera_yaw);
        }
        if (context.third_person_walk) {
            turn_stick(left_x, left_y, context.head_turn, 0.0f);
        }
        // Rerac-vr's board steering: the stick's x turns the board about its own heading, a quarter turn off it at
        // full, at the game's rate times its amount. The game takes a direction about its camera: it is given that.
        if (context.first_person && context.board_steers) {
            const float amount = std::clamp(left_x, -1.0f, 1.0f);
            const float away = context.board_yaw - context.camera_yaw - amount * 1.5707964f;
            left_x = -std::sin(away) * std::fabs(amount);
            left_y = std::cos(away) * std::fabs(amount);
        }
        out.left_x = left_x;
        out.left_y = left_y;

        // In first person the right stick turns the player and the game's camera stays out of it; on
        // a turret it is nothing, the view being fixed while the turret turns beneath it.
        if (!context.first_person && !turret) {
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

    // Rerac-vr's menu_directions: the stick pushed further, along its longer axis, past 0.55.
    // Real d-pad buttons, as lists such as the ship's take no stick.
    static void directions(VirtualPad& pad, float left_x, float left_y, float right_x, float right_y) {
        const bool left = left_x * left_x + left_y * left_y >= right_x * right_x + right_y * right_y;
        const float x = left ? left_x : right_x;
        const float y = left ? left_y : right_y;
        if (std::max(std::abs(x), std::abs(y)) < 0.55f) {
            return;
        }
        if (std::abs(x) > std::abs(y)) {
            set(pad, x > 0, kPadRight);
            set(pad, x < 0, kPadLeft);
        } else {
            set(pad, y > 0, kPadUp);
            set(pad, y < 0, kPadDown);
        }
    }

    Latch m_right_trigger, m_left_trigger, m_right_grip, m_left_grip;
    bool m_ring_trigger = false;
    bool m_shop_trigger = false;
    bool m_menu_trigger = false;
    bool m_utility_down = false, m_utility_spent = false;
};

// A real swing of the wrench, from the hand's speed. Weak taps do not count (Rerac-vr's "put some
// energy into it"): the grip held and the hand past kSwingSpeed presses the game's swing for a
// moment, and the hand must slow down before the next.
inline constexpr float kSwingSpeed = 2.2f;   // metres a second
inline constexpr float kSwingRearm = 1.0f;
inline constexpr float kSwingSeconds = 0.12f;

class SwingDetector {
public:
    bool update(float speed, bool held, float seconds) {
        m_left = std::max(0.0f, m_left - seconds);
        if (speed < kSwingRearm) {
            m_armed = true;
        }
        if (held && m_armed && speed > kSwingSpeed) {
            m_armed = false;
            m_left = kSwingSeconds;
        }
        return m_left > 0.0f;
    }

private:
    bool m_armed = true;
    float m_left = 0;
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
    // `held_off`: the stick is another's now (a ring's hand, the shop, the Trespasser's rings, a menu). Pushed as that
    // ends it turns nothing until it has come back (Rerac-vr's).
    float update(float stick_x, float seconds, const TurnSettings& settings, bool held_off = false) {
        const bool centred = std::abs(stick_x) < 0.3f;
        m_waiting = !centred && (m_waiting || held_off);
        if (held_off || m_waiting) {
            m_armed = m_armed || centred;
            return 0.0f;
        }
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
    bool m_waiting = false;
};

}  // namespace quest
