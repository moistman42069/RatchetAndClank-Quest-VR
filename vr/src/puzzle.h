// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 the Ratchet & Clank Quest VR contributors
//
// The Trespasser's rings (0.1.21; a tester: "my joystick try to fight me when doing keydoor puzzels ... a grab object and
// a button/switch"; the owner: make them smoother, better, easier). The game's lock (class 615, tresspasser_pad_update)
// turns its chosen ring a twelfth while Right or Left is held and it is idle, about twelve a second held, and changes the
// ring on a press of Up (the ring before, to 1) or Down (the next, to 3). To 0.1.20 a stick held its way as the D-pad
// (Rerac-vr's): a held push overshot, and an arc of the thumb from the side to up changed the ring. Here each thing the
// player does asks for one step or one ring, in order, and the mod asks the game for them one at a time through its own
// pad, so its sounds and limits stay:
//   - a stick: one step a push, the push's first way kept until the stick is back near the middle; held to the side,
//     another step after 0.45 s, then four a second (the game ticks 60 a second); up or down only pushed clearly
//     so, one ring a push;
//   - A a step clockwise, B anticlockwise, X the next ring round (1, 2, 3, 1);
//   - the launcher's "Grab the Trespasser's rings": a grip over the rings' panel takes the ring under the hand, and the
//     hand taken round the middle turns it a step each twelfth of a circle (30 degrees), the way the hand goes.
// The game's ring 1 is the outer one (the lock starts on it; Up there does nothing), 2 the middle, 3 the inner.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "rig.h"

namespace quest {

class PuzzleInput {
public:
    static constexpr float kPress = 0.6f;           // a push of the stick past this asks
    static constexpr float kRelease = 0.35f;        // and it is let go back inside this
    static constexpr float kVertical = 0.7f;        // up or down: at least this far,
    static constexpr float kVerticalOver = 1.7f;    // and this many times the sideways
    static constexpr int kRepeatFirst = 27;         // ticks held to the side before the next step (0.45 s at the game's 60 a second)
    static constexpr int kRepeatEvery = 15;         // and then every (0.25 s)
    static constexpr int kMostSteps = 2;            // steps waiting, the stick and the buttons
    static constexpr int kMostGrabSteps = 3;        // and the grab
    static constexpr int kGiveUp = 150;             // ticks an ask waits for the lock before it is dropped (2.5 s)
    static constexpr float kGripOn = 0.6f, kGripOff = 0.4f;
    static constexpr float kStepAngle = 0.5235988f; // 30 degrees round the middle: a step
    // The grab: the rings' radii as fractions of half the panel's width (across) and height (up), measured on the
    // panel's picture (0.1.20's puzzle.sim); the frame's teeth are at about 0.74 across. Ring 1 is the outer.
    static constexpr std::array<float, 3> kAcross = {0.537f, 0.385f, 0.215f};
    static constexpr std::array<float, 3> kUp = {0.656f, 0.469f, 0.279f};
    static constexpr float kGrabNearest = 0.08f, kGrabFarthest = 0.78f;  // of half the width from the middle
    static constexpr float kTurnNearest = 0.06f;    // nearer the middle the hand's angle means nothing

    // What a tick did, for the log and the hands' feel.
    enum Kind : int { kStepStick, kStepRepeat, kRingStick, kStepA, kStepB, kRingX, kGrab, kGrabStep, kLetGo, kFull };
    struct Note {
        Kind kind;
        int value;  // a step's way (+1 clockwise), a ring (1..3), the grabbing hand (0 left, 1 right)
    };

    // Once a tick at the rings, before the grab and ask: the sticks (the one pushed further), A, B and X held.
    // `ring_now` is the lock's ring (1..3).
    void controls(float left_x, float left_y, float right_x, float right_y, bool a, bool b, bool x, int ring_now) {
        m_note_count = 0;
        const bool left = left_x * left_x + left_y * left_y >= right_x * right_x + right_y * right_y;
        const float sx = left ? left_x : right_x;
        const float sy = left ? left_y : right_y;
        const float push = std::sqrt(sx * sx + sy * sy);
        if (m_axis != kAxisNone && push < kRelease) {
            m_axis = kAxisNone;
        }
        if (m_axis == kAxisNone && push >= kPress) {
            // The push's way, once it is clear (a slow push up passes 0.6 before it is 0.7 up): until then nothing.
            if (std::abs(sy) >= kVertical && std::abs(sy) > kVerticalOver * std::abs(sx)) {
                m_axis = kAxisUpDown;
                // The game's Up is the ring before (toward 1, the outer), Down the next; at the end nothing, as there.
                const int last = last_ring(ring_now);
                const int want = std::clamp(last + (sy > 0 ? -1 : 1), 1, 3);
                if (want != last) {
                    ring(want);
                    note(kRingStick, want);
                }
            } else if (std::abs(sx) >= kPress && std::abs(sx) >= std::abs(sy)) {
                m_axis = kAxisSide;
                m_way = sx > 0 ? 1 : -1;
                m_repeat_in = kRepeatFirst;
                ask_step(m_way, kMostSteps, kStepStick);
            }
        } else if (m_axis == kAxisSide) {
            // Held to its first side: the repeats. Back over to the other side is a new push, through the middle.
            if (sx * static_cast<float>(m_way) >= kPress * 0.8f) {
                if (--m_repeat_in <= 0) {
                    m_repeat_in = kRepeatEvery;
                    ask_step(m_way, kMostSteps, kStepRepeat);
                }
            } else {
                m_repeat_in = kRepeatEvery;  // eased off but not let go: the next push on is a step after a while
            }
        }
        if (a && !m_a) {
            ask_step(1, kMostSteps, kStepA);
        }
        if (b && !m_b) {
            ask_step(-1, kMostSteps, kStepB);
        }
        if (x && !m_x) {
            const int want = last_ring(ring_now) % 3 + 1;
            ring(want);
            note(kRingX, want);
        }
        m_a = a;
        m_b = b;
        m_x = x;
    }

    // Once a tick for each hand (0 left, 1 right), after controls: its grip, and where it is over the rings' panel in
    // half the panel's width and height (u to the right, v up from the middle), `over` false where it is not. `aspect` is
    // the panel's height over its width. A grip closed over a ring takes it (the ring asked for, what was waiting
    // dropped); round the middle it asks a step a twelfth of a circle; opened, it lets go.
    void grab(int side, float grip, bool over, float u, float v, float aspect) {
        const bool was = m_grip[side];
        m_grip[side] = was ? grip > kGripOff : grip > kGripOn;
        if (m_grab_hand == side && !m_grip[side]) {
            m_grab_hand = -1;
            note(kLetGo, side);
            return;
        }
        const float x = u, y = v * aspect;  // in half the width, as the panel's picture is
        const float from_middle = std::sqrt(x * x + y * y);
        if (m_grab_hand < 0 && m_grip[side] && !was && over) {
            const int band = ring_under(u, v, aspect);
            if (band > 0) {
                m_grab_hand = side;
                m_grab_angle = std::atan2(y, x);
                clear_asks();
                ring(band);
                note(kGrab, side);
            }
            return;
        }
        if (m_grab_hand != side || !over || from_middle < kTurnNearest) {
            return;
        }
        // Clockwise as seen (the angle falling) is the game's Right.
        float turned = wrap_angle(std::atan2(y, x) - m_grab_angle);
        while (turned <= -kStepAngle) {
            ask_step(1, kMostGrabSteps, kGrabStep);
            m_grab_angle = wrap_angle(m_grab_angle - kStepAngle);
            turned += kStepAngle;
        }
        while (turned >= kStepAngle) {
            ask_step(-1, kMostGrabSteps, kGrabStep);
            m_grab_angle = wrap_angle(m_grab_angle + kStepAngle);
            turned -= kStepAngle;
        }
    }

    // Once a tick, last: what the pad gives the game. `turn` holds Right (+1) or Left (-1) for the step waiting first;
    // `press` is Up (-1) or Down (+1) toward the ring waiting first, a tick on and a tick off. `ring_now` the lock's.
    void ask(int ring_now, int& turn, int& press) {
        turn = 0;
        press = 0;
        const bool pressed = m_pressed;
        m_pressed = false;
        while (m_count > 0) {
            const int front = m_asks[0];
            if (front >= kRingAsk) {
                const int want = front - kRingAsk;
                if (want == ring_now || ++m_waited > kGiveUp) {
                    pop();
                    continue;
                }
                if (!pressed) {
                    press = want < ring_now ? -1 : 1;
                    m_pressed = true;
                }
                return;
            }
            if (++m_waited > kGiveUp) {
                pop();
                continue;
            }
            turn = front;
            return;
        }
    }

    // The lock began a turn of its ring, `way` +1 clockwise: the step it answers.
    void turned(int way) {
        if (m_count > 0 && m_asks[0] == way) {
            pop();
        }
    }

    // The puzzle over or the lock not taking input any more: nothing waits, no ring held.
    void reset() {
        clear_asks();
        m_axis = kAxisSpent;  // a stick still pushed asks nothing until let go
        m_grab_hand = -1;
        m_pressed = false;
    }

    int grab_hand() const { return m_grab_hand; }
    int waiting() const { return m_count; }
    int notes() const { return m_note_count; }
    const Note& note_at(int i) const { return m_notes[static_cast<std::size_t>(i)]; }

    // The ring (1 the outer .. 3 the inner) whose band holds the point (u, v) of the panel, or 0 off the rings.
    static int ring_under(float u, float v, float aspect) {
        const float x = u, y = v * aspect;
        const float from_middle = std::sqrt(x * x + y * y);
        if (from_middle < kGrabNearest || from_middle > kGrabFarthest) {
            return 0;
        }
        const float c = x / from_middle, s = y / from_middle;
        int best = 0;
        float nearest = 1e9f;
        for (std::size_t i = 0; i < kAcross.size(); ++i) {
            // The ring's radius this way round: an ellipse across kAcross, up kUp of half the height.
            const float a = kAcross[i], b = kUp[i] * aspect;
            const float radius = 1.0f / std::sqrt((c / a) * (c / a) + (s / b) * (s / b));
            const float off = std::abs(from_middle - radius);
            if (off < nearest) {
                nearest = off;
                best = static_cast<int>(i) + 1;
            }
        }
        return best;
    }

    // Where the line from the eye through the hand meets the panel (its pose: +Z toward the viewer, X right, Y up; its
    // width and height), in half its width and height. False when the hand is not between the eye and a little past
    // the panel, or the line does not meet it.
    static bool panel_point(const Pose& panel, float width, float height, Vec3 eye, Vec3 hand, float& u, float& v) {
        const Vec3 normal = rotate(panel.orientation, Vec3{0, 0, 1});
        const Vec3 ray = hand - eye;
        const float toward = -dot3(ray, normal);                       // the hand out from the eye, toward the panel
        const float panel_far = -dot3(panel.position - eye, normal);   // the panel out from the eye
        if (toward < kHandOut || panel_far <= 0.0f || toward > panel_far + kPastPanel) {
            return false;
        }
        const Vec3 hit = eye + ray * (panel_far / toward);
        const Vec3 local = rotate(conjugate(panel.orientation), hit - panel.position);
        u = local.x / (width * 0.5f);
        v = local.y / (height * 0.5f);
        return true;
    }

private:
    static constexpr float kHandOut = 0.12f;    // metres the hand must be out from the eye
    static constexpr float kPastPanel = 0.25f;  // and at most past the panel
    static constexpr int kRingAsk = 10;         // an ask of ring r is kRingAsk + r; a step +1 or -1
    enum Axis : int { kAxisNone, kAxisSide, kAxisUpDown, kAxisSpent };

    static float dot3(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

    int last_ring(int ring_now) const {
        for (int i = m_count - 1; i >= 0; --i) {
            if (m_asks[static_cast<std::size_t>(i)] >= kRingAsk) {
                return m_asks[static_cast<std::size_t>(i)] - kRingAsk;
            }
        }
        return std::clamp(ring_now, 1, 3);
    }

    // A ring asked: the one waiting last, if it is a ring, is this one instead.
    void ring(int which) {
        if (m_count > 0 && m_asks[static_cast<std::size_t>(m_count - 1)] >= kRingAsk) {
            m_asks[static_cast<std::size_t>(m_count - 1)] = kRingAsk + which;
            return;
        }
        push(kRingAsk + which);
    }

    // A step asked: one the other way waiting last is taken back instead; no more than `most` the same way waiting.
    void ask_step(int way, int most, Kind kind) {
        if (m_count > 0 && m_asks[static_cast<std::size_t>(m_count - 1)] == -way) {
            if (--m_count == 0) {
                m_waited = 0;
            }
            note(kind, way);
            return;
        }
        int same = 0;
        for (int i = m_count - 1; i >= 0 && m_asks[static_cast<std::size_t>(i)] == way; --i) {
            ++same;
        }
        if (same >= most || m_count >= static_cast<int>(m_asks.size())) {
            note(kFull, way);
            return;
        }
        push(way);
        note(kind, way);
    }

    void push(int ask) {
        if (m_count < static_cast<int>(m_asks.size())) {
            m_asks[static_cast<std::size_t>(m_count++)] = ask;
        }
    }

    void pop() {
        for (int i = 1; i < m_count; ++i) {
            m_asks[static_cast<std::size_t>(i - 1)] = m_asks[static_cast<std::size_t>(i)];
        }
        m_count = std::max(0, m_count - 1);
        m_waited = 0;
    }

    void clear_asks() {
        m_count = 0;
        m_waited = 0;
    }

    void note(Kind kind, int value) {
        if (m_note_count < static_cast<int>(m_notes.size())) {
            m_notes[static_cast<std::size_t>(m_note_count++)] = {kind, value};
        }
    }

    std::array<int, 8> m_asks{};
    int m_count = 0;
    int m_waited = 0;       // ticks the first ask has waited
    bool m_pressed = false; // Up or Down given last tick
    int m_axis = kAxisNone;
    int m_way = 0;
    int m_repeat_in = 0;
    bool m_a = false, m_b = false, m_x = false;
    bool m_grip[2] = {false, false};
    int m_grab_hand = -1;
    float m_grab_angle = 0;
    std::array<Note, 8> m_notes{};
    int m_note_count = 0;
};

}  // namespace quest
