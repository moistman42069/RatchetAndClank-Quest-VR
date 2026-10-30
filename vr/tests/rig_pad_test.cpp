// SPDX-License-Identifier: GPL-3.0-or-later
// Runs on any machine: the rig's and the pad's math, each against what the game and OpenXR expect.
// clang++ -std=c++20 -I../src rig_pad_test.cpp -o rig_pad_test && ./rig_pad_test

#include <cstdio>
#include <cstdlib>

#include "pad.h"
#include "rig.h"

using namespace quest;

namespace {

int g_failures = 0;

void near_(float a, float b, const char* what, float tolerance = 1e-3f) {
    if (std::fabs(a - b) > tolerance) {
        std::printf("FAIL %s: %.5f, wanted %.5f\n", what, a, b);
        ++g_failures;
    }
}

void angle_near(float a, float b, const char* what) { near_(wrap_angle(a - b), 0.0f, what); }

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL %s\n", what);
        ++g_failures;
    }
}

// A rotation about Y, as a head turned left by `a`.
Pose head_at(Vec3 position, float a) { return {position, yaw_quat(a)}; }

void test_spaces() {
    // The frame's matrix in views.cpp: world X, Z, -Y.
    const Vec3 f = world_to_frame({1, 2, 3});
    near_(f.x, 1, "world x");
    near_(f.y, 3, "world z is up");
    near_(f.z, -2, "world y is -z");
    const Vec3 w = frame_to_world(f);
    near_(w.x, 1, "x back");
    near_(w.y, 2, "y back");
    near_(w.z, 3, "z back");
    // The frame's -Z is the world's +Y: yaw 90 degrees.
    near_(frame_direction_yaw({0, 0, -1}), kPi / 2, "-z is +y");
    near_(frame_direction_yaw({1, 0, 0}), 0, "+x is yaw 0");
    // Handedness is kept: X cross Y is Z in both, so the world is not mirrored.
    const Vec3 x = world_to_frame({1, 0, 0}), y = world_to_frame({0, 1, 0}), z = world_to_frame({0, 0, 1});
    const Vec3 cross{x.y * y.z - x.z * y.y, x.z * y.x - x.x * y.z, x.x * y.y - x.y * y.x};
    near_(cross.x, z.x, "right-handed x");
    near_(cross.y, z.y, "right-handed y");
    near_(cross.z, z.z, "right-handed z");
}

void test_yaw() {
    near_(tracking_yaw(yaw_quat(0)), 0, "no turn");
    near_(tracking_yaw(yaw_quat(0.5f)), 0.5f, "left turn is positive");
    near_(world_yaw(0, yaw_quat(0)), kPi / 2, "head forward is the world's +Y");
    near_(world_yaw(0.3f, yaw_quat(0.2f)), kPi / 2 + 0.5f, "rig and head add");
}

void test_recentre() {
    for (float facing : {0.0f, 1.0f, -2.5f, 3.0f}) {
        for (float head : {0.0f, 0.7f, -1.9f}) {
            Rig rig;
            const Pose pose = head_at({0.3f, 1.6f, -0.2f}, head);
            rig.recentre(pose, facing);
            angle_near(world_yaw(rig.yaw, pose.orientation), facing, "recentred head looks at facing");
            const Vec3 anchor{5, 2, -7};
            const Pose framed = rig.to_frame(pose, anchor);
            near_(length(framed.position - anchor), 0, "recentred head is on the anchor");
            // The frame pose's forward is the same direction the yaw names.
            const Vec3 forward = rotate(framed.orientation, {0, 0, -1});
            angle_near(frame_direction_yaw(forward), facing, "frame forward matches");
        }
    }
}

void test_walking_scales() {
    Rig rig;
    rig.scale = 2;
    rig.recentre(head_at({0, 1.6f, 0}, 0), kPi / 2);
    // Facing the world's +Y: a metre forward (tracking -Z) is two units along the frame's -Z.
    const Pose walked = rig.to_frame(head_at({0, 1.6f, -1}, 0), {0, 0, 0});
    near_(walked.position.z, -2, "a metre forward");
    near_(walked.position.x, 0, "no drift");
    // Up a metre is up in the frame, the world's Z.
    const Pose risen = rig.to_frame(head_at({0, 2.6f, 0}, 0), {0, 0, 0});
    near_(risen.position.y, 2, "a metre up");
}

void test_turn_about_head() {
    Rig rig;
    rig.recentre(head_at({0, 1.6f, 0}, 0), 0);
    const Pose off = head_at({0.5f, 1.6f, 0.25f}, 0.4f);
    const Vec3 anchor{1, 1, 1};
    const Vec3 before = rig.to_frame(off, anchor).position;
    const float yaw_before = world_yaw(rig.yaw, off.orientation);
    rig.turn(0.7f, off.position);
    const Vec3 after = rig.to_frame(off, anchor).position;
    near_(length(after - before), 0, "a turn keeps the head where it is");
    angle_near(world_yaw(rig.yaw, off.orientation), yaw_before + 0.7f, "a turn turns");
}

void test_camera_relative() {
    Rig rig;
    const Pose start = head_at({1, 1.6f, 2}, 1.1f);
    rig.recentre(start, 0);
    const float recentre_yaw = tracking_yaw(start.orientation);
    const Pose same = rig.to_camera(start, recentre_yaw);
    near_(length(same.position), 0, "the recentred head is on the camera");
    near_(tracking_yaw(same.orientation), 0, "and looks along it");
    const Pose looked = rig.to_camera(head_at({1, 1.6f, 2}, 1.1f + 0.5f), recentre_yaw);
    near_(tracking_yaw(looked.orientation), 0.5f, "a look is relative");
}

void test_stick_bytes() {
    near_(stick_byte(0), 0x80, "rest");
    // The game's reading of the byte gives the value back.
    for (float v : {0.1f, 0.25f, 0.5f, 0.75f, 1.0f, -0.1f, -0.5f, -1.0f}) {
        const int d = static_cast<int>(stick_byte(v)) - 0x7F;
        const int m = std::abs(d);
        check(m >= 0x30, "out of the game's dead zone");
        const float read = std::min(1.0f, static_cast<float>(m - 0x30) / 0x4C) * (d < 0 ? -1.0f : 1.0f);
        near_(read, v, "the game reads the stick back", 0.03f);
    }
}

void test_report() {
    std::uint8_t report[18];
    for (auto& b : report) {
        b = 0xFF;
    }
    report[2] = report[3] = report[4] = report[5] = 0x80;
    for (int i = 6; i < 18; ++i) {
        report[i] = 0;
    }
    VirtualPad pad;
    pad.held = kPadCross | kPadSelect;
    pad.left_y = 1;
    merge_report(pad, report, 18);
    check((report[0] & 1) == 0, "select is active low in byte 0");
    check((report[1] & 0x40) == 0, "cross is active low in byte 1");
    check((report[1] & 0x20) != 0, "circle stays up");
    check(report[5] < 0x80, "up on the stick is a low byte");
    check(report[4] == 0x80, "x stays at rest");
    check(report[6 + 6] == 0xFF, "cross's pressure");
}

void test_short_report() {
    std::uint8_t report[4] = {0xFF, 0xFF, 0x80, 0x80};
    VirtualPad pad;
    pad.held = kPadCross;
    merge_report(pad, report, 4);
    check(report[1] == 0xFF, "a short report is left alone");
}

void test_stick_turn() {
    float x = 0, y = 1;
    // The camera looks along yaw 0, the head along yaw 90 degrees left of it: forward on the stick
    // must become the pad's left (the game adds the stick's angle to the camera's).
    turn_stick(x, y, kPi / 2, 0);
    near_(x, -1, "pad left");
    near_(y, 0, "not up");
    x = 1;
    y = 0;
    turn_stick(x, y, 0.8f, 0.8f);
    near_(x, 1, "same yaw: no turn");
    near_(y, 0, "same yaw: no turn y");
}

void test_mapper() {
    PadMapper mapper;
    Controls c;
    PadContext context;
    context.first_person = true;
    c.a = true;
    c.right_trigger = 0.9f;
    c.right_grip = 0.9f;
    c.y = true;
    c.x = true;
    VirtualPad out = mapper.map(c, context);
    check(out.held & kPadCross, "A jumps");
    check(out.held & kPadCircle, "the trigger fires");
    check(out.held & kPadSquare, "the grip swings");
    check(out.held & kPadStart, "Y pauses");
    check(out.held & kPadSelect, "X maps");
    // Between the two levels a trigger stays as it was, on the way down.
    c = Controls{};
    c.right_trigger = 0.45f;
    out = mapper.map(c, context);
    check(out.held & kPadCircle, "held through the gap");
    c.right_trigger = 0.2f;
    out = mapper.map(c, context);
    check(!(out.held & kPadCircle), "let go");
    c.right_trigger = 0.45f;
    out = mapper.map(c, context);
    check(!(out.held & kPadCircle), "not pressed by the gap");

    // Both sticks clicked is the recentre, not the game's.
    c = Controls{};
    c.left_click = c.right_click = true;
    out = mapper.map(c, context);
    check(!(out.held & (kPadTriangle | kPadL1)), "both clicks stay out of the game");

    // The right stick does not reach the game's camera in first person.
    c = Controls{};
    c.right_x = 1;
    out = mapper.map(c, context);
    near_(out.right_x, 0, "the right stick is the player's");
    context.first_person = false;
    out = mapper.map(c, context);
    near_(out.right_x, 1, "and the camera's in third person");

    // In a menu a stick is the d-pad and B goes back.
    PadMapper menu;
    context = PadContext{};
    context.in_menu = true;
    c = Controls{};
    c.left_y = -1;
    c.b = true;
    out = menu.map(c, context);
    check(out.held & kPadDown, "down");
    check(out.held & kPadTriangle, "B goes back");

    // The quick select reads the hand, not the turned stick.
    PadMapper quick;
    context = PadContext{};
    context.first_person = true;
    context.quick_select = true;
    context.head_yaw = 1.5f;
    context.pick_x = 0.6f;
    context.pick_y = 0.2f;
    c = Controls{};
    c.right_click = true;
    out = quick.map(c, context);
    check(out.held & kPadTriangle, "held open");
    near_(out.left_x, 0.6f, "the hand's push");
    near_(out.left_y, 0.2f, "the hand's push y");
}

void test_dead_zone() {
    float x = 0.1f, y = 0.05f;
    shape_stick(x, y);
    near_(x, 0, "dead");
    x = 1;
    y = 0;
    shape_stick(x, y);
    near_(x, 1, "full stays full");
}

void test_turn_control() {
    TurnControl turn;
    TurnSettings snap;
    near_(turn.update(1, 0.016f, snap), -snap.snap_angle, "a snap right");
    near_(turn.update(1, 0.016f, snap), 0, "one snap a push");
    near_(turn.update(0, 0.016f, snap), 0, "centred");
    near_(turn.update(-1, 0.016f, snap), snap.snap_angle, "a snap left");
    TurnSettings smooth;
    smooth.smooth = true;
    near_(turn.update(1, 0.5f, smooth), -smooth.smooth_rate * 0.5f, "smooth");
    near_(turn.update(0.1f, 0.5f, smooth), 0, "smooth dead zone");
}

}  // namespace

int main() {
    test_spaces();
    test_yaw();
    test_recentre();
    test_walking_scales();
    test_turn_about_head();
    test_camera_relative();
    test_stick_bytes();
    test_report();
    test_short_report();
    test_stick_turn();
    test_mapper();
    test_dead_zone();
    test_turn_control();
    if (g_failures == 0) {
        std::printf("all passed\n");
    }
    return g_failures == 0 ? 0 : 1;
}
