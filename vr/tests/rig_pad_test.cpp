// SPDX-License-Identifier: GPL-3.0-or-later
// Runs on any machine: the rig's and the pad's math, each against what the game and OpenXR expect.
// clang++ -std=c++20 -I../src rig_pad_test.cpp -o rig_pad_test && ./rig_pad_test

#include <cstdio>
#include <cstdlib>
#include <string>

#include "hand.h"
#include "board.h"
#include "heading.h"
#include "notice.h"
#include "pad.h"
#include "puzzle.h"
#include "rig.h"
#include "support.h"
#include "glyphs.h"
#include "pointer.h"
#include "turret.h"

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
    c.b = true;
    c.right_trigger = 0.9f;
    c.right_grip = 0.9f;
    c.y = true;
    c.x = true;
    VirtualPad out = mapper.map(c, context);
    check(out.held & kPadCross, "A jumps");
    check(out.held & kPadCircle, "the trigger fires");
    check(!(out.held & kPadSquare), "the grip alone does not swing");
    check(out.held & kPadR2, "B is the hover");
    check(!(out.held & kPadR1), "and not the crouch");
    check(out.held & kPadStart, "Y pauses");
    check(out.held & kPadSelect, "X maps");

    // The PC mod's table, control by control.
    PadMapper table;
    c = Controls{};
    c.left_grip = 1;
    check(table.map(c, context).held & kPadR1, "left grip crouches");
    PadMapper table2;
    c = Controls{};
    c.left_trigger = 1;
    check(table2.map(c, context).held & kPadR1, "left trigger crouches");
    {
        // Physical crouch in first person: the left grip is the hand's alone, the trigger still crouches.
        PadContext free_grip = context;
        free_grip.grip_crouch = false;
        PadMapper grip_mapper;
        Controls grip{};
        grip.left_grip = 1;
        check(!(grip_mapper.map(grip, free_grip).held & kPadR1), "with Physical crouch the left grip does not crouch");
        PadMapper trigger_mapper;
        Controls trigger{};
        trigger.left_trigger = 1;
        check(trigger_mapper.map(trigger, free_grip).held & kPadR1, "with Physical crouch the left trigger still crouches");
        PadMapper head_mapper;
        free_grip.crouch = true;
        check(head_mapper.map(Controls{}, free_grip).held & kPadR1, "and the head's crouch is R1");
    }
    PadMapper table3;
    c = Controls{};
    c.right_click = true;
    check(table3.map(c, context).held & kPadTriangle, "right stick click talks");
    c = Controls{};
    context.swing = true;
    check(table3.map(c, context).held & kPadSquare, "a swing is the wrench");
    context.swing = false;
    {
        // Arm moves on Buttons (a player's ask, 0.1.13): the grip alone is the game's swing in first person; held
        // or not; never on a ledge or in a turret; off again, the grip alone swings nothing.
        PadContext buttons = context;
        buttons.button_moves = true;
        Controls grip{};
        grip.right_grip = 0.9f;
        PadMapper on;
        check(on.map(grip, buttons).held & kPadSquare, "Arm moves on Buttons: the grip swings");
        check(!(on.map(Controls{}, buttons).held & kPadSquare), "and let go, it does not");
        PadContext hanging = buttons;
        hanging.ledge = true;
        PadMapper ledge;
        check(!(ledge.map(grip, hanging).held & kPadSquare), "Buttons: not on a ledge");
        PadContext turret = buttons;
        turret.mounted = true;
        PadMapper seat;
        check(!(seat.map(grip, turret).held & kPadSquare), "Buttons: not in a turret");
        PadMapper off;
        check(!(off.map(grip, context).held & kPadSquare), "For real: the grip alone does not swing");
    }

    // Mounted X and B keep Circle and Square; a flown ship's trigger is its lasers.
    PadMapper turret;
    context.mounted = true;
    c = Controls{};
    c.x = c.b = true;
    out = turret.map(c, context);
    check((out.held & kPadCircle) && (out.held & kPadSquare), "mounted X and B");
    check(!(out.held & (kPadSelect | kPadR2)), "not the map or the hover");
    context.flying = true;
    c = Controls{};
    c.right_trigger = 1;
    check(turret.map(c, context).held & kPadSquare, "the ship's lasers");
    context.mounted = context.flying = false;

    // The trigger that takes the gun out does not fire it.
    PadMapper back;
    context.trigger_held_back = true;
    c = Controls{};
    c.right_trigger = 1;
    check(!(back.map(c, context).held & kPadCircle), "held back");
    context.trigger_held_back = false;

    // Between the two levels a trigger stays as it was, on the way down.
    PadMapper latch;
    c = Controls{};
    c.right_trigger = 0.9f;
    latch.map(c, context);
    c.right_trigger = 0.45f;
    out = latch.map(c, context);
    check(out.held & kPadCircle, "held through the gap");
    c.right_trigger = 0.2f;
    out = latch.map(c, context);
    check(!(out.held & kPadCircle), "let go");
    c.right_trigger = 0.45f;
    out = latch.map(c, context);
    check(!(out.held & kPadCircle), "not pressed by the gap");

    // Both sticks clicked: a ring each (Akimbo), so the right's click still reaches the game.
    c = Controls{};
    c.left_click = c.right_click = true;
    out = mapper.map(c, context);
    check(out.held & kPadTriangle, "both clicks keep the right click");

    // A pull that answered a menu fires nothing after it, until let go.
    PadMapper closing;
    PadContext a_menu;
    a_menu.in_menu = true;
    PadContext playing;
    c = Controls{};
    c.right_trigger = 1;
    closing.map(c, a_menu);
    check(!(closing.map(c, playing).held & kPadCircle), "the menu's pull does not fire");
    c.right_trigger = 0;
    closing.map(c, playing);
    c.right_trigger = 1;
    check(closing.map(c, playing).held & kPadCircle, "the next pull fires");

    // Y that skipped a scene does not pause as play comes back under it.
    PadMapper skipped;
    PadContext after_scene;
    after_scene.skip_held = true;
    c = Controls{};
    c.y = true;
    check(!(skipped.map(c, after_scene).held & kPadStart), "the skip's Y does not pause");
    check(skipped.map(c, playing).held & kPadStart, "Y pauses once it is its own");

    // X that switched a scene's view does not open the map as play comes back under it.
    PadMapper switched;
    PadContext view_after;
    view_after.view_held = true;
    c = Controls{};
    c.x = true;
    check(!(switched.map(c, view_after).held & kPadSelect), "the view switch's X does not open the map");
    check(switched.map(c, playing).held & kPadSelect, "X opens the map once it is its own");

    // A stick still pushed as the ring or the shop lets it go turns nothing until it comes back.
    TurnControl snap;
    TurnSettings turning;
    near_(snap.update(1.0f, 0.02f, turning, true), 0, "held off, no turn");
    near_(snap.update(1.0f, 0.02f, turning), 0, "still pushed after, no turn");
    near_(snap.update(0.0f, 0.02f, turning), 0, "back at the centre");
    check(snap.update(1.0f, 0.02f, turning) != 0.0f, "the next push turns");
    TurnControl glide;
    TurnSettings smooth;
    smooth.smooth = true;
    near_(glide.update(1.0f, 0.02f, smooth, true), 0, "smooth, held off");
    near_(glide.update(1.0f, 0.02f, smooth), 0, "smooth, still pushed after");
    near_(glide.update(0.0f, 0.02f, smooth), 0, "smooth, back at the centre");
    check(glide.update(1.0f, 0.02f, smooth) != 0.0f, "smooth, the next push turns");

    // The right stick does not reach the game's camera in first person.
    c = Controls{};
    c.right_x = 1;
    out = mapper.map(c, context);
    near_(out.right_x, 0, "the right stick is the player's");
    context.first_person = false;
    out = mapper.map(c, context);
    near_(out.right_x, 1, "and the camera's in third person");

    // Third person: the grip is simply the wrench's button.
    PadMapper third;
    c = Controls{};
    c.right_grip = 1;
    check(third.map(c, context).held & kPadSquare, "third person's wrench");

    // In a menu a stick is the d-pad, A confirms either way the game asks, B goes back.
    PadMapper menu;
    context = PadContext{};
    context.in_menu = true;
    c = Controls{};
    c.left_y = -1;
    c.b = true;
    out = menu.map(c, context);
    check(out.held & kPadDown, "down");
    check(out.held & kPadTriangle, "B goes back");
    c = Controls{};
    c.right_x = 1;
    check(menu.map(c, context).held & kPadRight, "the right stick moves too");
    c = Controls{};
    c.a = true;
    out = menu.map(c, context);
    check((out.held & kPadCross) && (out.held & kPadCircle), "A answers a Cross or a Circle question");
    PadMapper menu2;
    c = Controls{};
    c.right_trigger = 1;
    out = menu2.map(c, context);
    check((out.held & kPadCross) && (out.held & kPadCircle), "so does the trigger");
    c = Controls{};
    c.x = true;
    out = menu.map(c, context);
    check((out.held & kPadCircle) && !(out.held & kPadCross), "X is Circle alone");
    c = Controls{};
    c.left_grip = 1;
    c.left_trigger = 1;
    PadMapper pages;
    out = pages.map(c, context);
    check((out.held & kPadL1) && (out.held & kPadR1), "the page shoulders");

    // The quick select leaves the left stick walking, and a pull of its trigger turns pages, firing nothing.
    PadMapper quick;
    context = PadContext{};
    context.first_person = true;
    context.quick_select = true;
    c = Controls{};
    c.right_click = true;
    c.left_y = 1.0f;
    out = quick.map(c, context);
    check(out.held & kPadTriangle, "held open");
    check(out.left_y > 0.9f, "still walking");
    c.right_trigger = 1.0f;
    out = quick.map(c, context);
    check(!(out.held & kPadCircle), "the page's trigger does not fire");
    context.quick_select = false;
    c.right_click = false;
    out = quick.map(c, context);
    check(!(out.held & kPadCircle), "nor after, until let go");
    c.right_trigger = 0.0f;
    quick.map(c, context);
    c.right_trigger = 1.0f;
    out = quick.map(c, context);
    check(out.held & kPadCircle, "fires again");
    // Held as the ring opens (hanging from the Swingshot, 0.1.18): the game's fire still, until let go; a fresh pull
    // in the ring then is its page's.
    context.quick_select = true;
    c.right_click = true;
    out = quick.map(c, context);
    check(out.held & kPadCircle, "a trigger held as the ring opens still fires");
    out = quick.map(c, context);
    check(out.held & kPadCircle, "and goes on firing while held");
    c.right_trigger = 0.0f;
    quick.map(c, context);
    c.right_trigger = 1.0f;
    out = quick.map(c, context);
    check(!(out.held & kPadCircle), "a fresh pull in the ring is its page's");

    // The Trespasser's rings: the mod's asks are the d-pad (puzzle.h), the stick itself nothing, nothing walks; a
    // trigger held from the push is not a cancel, a fresh pull is, and it does nothing else until let go.
    PadMapper rings;
    context = PadContext{};
    context.first_person = true;
    context.puzzle = true;
    c = Controls{};
    c.right_trigger = 1.0f;
    rings.map(Controls{c}, PadContext{});
    c.left_x = 1.0f;
    out = rings.map(c, context);
    check(!(out.held & (kPadRight | kPadLeft | kPadUp | kPadDown)), "the stick itself holds no way (0.1.21)");
    check(out.left_x == 0.0f, "and walks nothing");
    context.puzzle_turn = 1;
    check((rings.map(c, context).held & (kPadRight | kPadLeft)) == kPadRight, "a step asked: Right");
    context.puzzle_turn = -1;
    check((rings.map(c, context).held & (kPadRight | kPadLeft)) == kPadLeft, "the other way: Left");
    context.puzzle_turn = 0;
    context.puzzle_ring = -1;
    check((rings.map(c, context).held & (kPadUp | kPadDown)) == kPadUp, "the ring before: Up");
    context.puzzle_ring = 1;
    check((rings.map(c, context).held & (kPadUp | kPadDown)) == kPadDown, "the next ring: Down");
    context.puzzle_ring = 0;
    c.a = c.b = c.x = true;
    check(!(rings.map(c, context).held & (kPadCross | kPadCircle | kPadSquare)), "A, B and X are the mod's there");
    c.a = c.b = c.x = false;
    check(!(out.held & kPadTriangle), "the trigger held from the push is not a cancel");
    c = Controls{};
    rings.map(c, context);
    c.right_trigger = 1.0f;
    out = rings.map(c, context);
    check((out.held & kPadTriangle) && out.withdraw, "a fresh pull withdraws");
    context.puzzle = false;
    out = rings.map(c, context);
    check(!(out.held & kPadCircle), "and does not fire");
    c.right_trigger = 0.0f;
    rings.map(c, context);
    c.y = true;
    out = rings.map(c, context);
    check(out.held & kPadStart, "Y is the pause again");

    // Hanging by the hands, the trigger sends the game no fire: the free hand's gun is the mod's.
    PadMapper hanging;
    context = PadContext{};
    context.first_person = true;
    context.ledge = true;
    c = Controls{};
    c.right_trigger = 1.0f;
    out = hanging.map(c, context);
    check(!(out.held & kPadCircle), "no fire while hanging");

    // Akimbo: the left trigger is the left gun's, not a crouch; without one it is R1, as ever. The
    // left grip stays R1 either way, and the left stick's click holds the ring.
    PadMapper akimbo;
    context = PadContext{};
    context.first_person = true;
    context.left_weapon = true;
    c = Controls{};
    c.left_trigger = 1.0f;
    out = akimbo.map(c, context);
    check(!(out.held & kPadR1), "the left trigger is the left gun's");
    c = Controls{};
    c.left_grip = 1.0f;
    out = akimbo.map(c, context);
    check(out.held & kPadR1, "the left grip still crouches");
    context = PadContext{};
    context.first_person = true;
    c = Controls{};
    c.left_trigger = 1.0f;
    out = PadMapper{}.map(c, context);
    check(out.held & kPadR1, "with no left gun the trigger crouches");
    context.left_ring = true;
    out = PadMapper{}.map(Controls{}, context);
    check(out.held & kPadTriangle, "the left ring is held open");

    // The left hand leading (its Walloper or Swingshot the hand's item): the left trigger is the game's
    // fire, the right trigger nothing.
    context = PadContext{};
    context.first_person = true;
    context.left_weapon = true;
    context.lead = true;
    c = Controls{};
    c.left_trigger = 1.0f;
    out = PadMapper{}.map(c, context);
    check((out.held & kPadCircle) && !(out.held & kPadR1), "leading, the left trigger fires");
    c = Controls{};
    c.right_trigger = 1.0f;
    out = PadMapper{}.map(c, context);
    check(!(out.held & kPadCircle), "leading, the right trigger fires nothing");

    // The big screen is a plain pad, with no menu or head in it.
    PadMapper screen;
    context = PadContext{};
    context.screen = true;
    c = Controls{};
    c.a = c.b = c.x = c.y = c.menu = c.left_click = true;
    c.left_y = 1;
    c.right_x = 1;
    out = screen.map(c, context);
    check(out.held & kPadCross, "screen A");
    check(out.held & kPadCircle, "screen B");
    check(out.held & kPadSquare, "screen X");
    check(out.held & kPadTriangle, "screen Y");
    check(out.held & kPadStart, "screen menu");
    check(out.held & kPadSelect, "screen select");
    near_(out.left_y, 1, "screen left stick");
    near_(out.right_x, 1, "screen right stick");
    c = Controls{};
    c.right_trigger = c.left_trigger = c.right_grip = c.left_grip = 1;
    out = screen.map(c, context);
    check((out.held & (kPadR1 | kPadL1 | kPadR2 | kPadL2)) == (kPadR1 | kPadL1 | kPadR2 | kPadL2), "screen shoulders");
    context.in_menu = true;
    c = Controls{};
    c.left_y = -1;
    check(screen.map(c, context).held & kPadDown, "screen menus");
}

void test_swing() {
    SwingDetector swing;
    const float dt = 1.0f / 72.0f;
    check(!swing.update(3.0f, false, dt), "no grip, no swing");
    check(!swing.update(0.5f, true, dt), "a still hand does not swing");
    check(swing.update(3.0f, true, dt), "a fast hand with the grip does");
    for (int i = 0; i < 20; ++i) {
        swing.update(3.0f, true, dt);
    }
    check(!swing.update(3.0f, true, dt), "one swing to a stroke");
    swing.update(0.5f, true, dt);
    check(swing.update(3.0f, true, dt), "the hand slowed: the next one");
}

// Rerac-vr's sums, on the wrench's and the bomb glove's own joints from the real disc.
void test_mounts() {
    const HandFrame hand{{10, 20, 30}, {0, 1, 0}, {-1, 0, 0}, {0, 0, 1}};

    // The wrench, class 71: its inverse bind from the disc (joint 1).
    const float wrench[16] = {0, 0, 1, 1203.11682f, -1, 0, 0, 350.32388f, 0, -1, 0, -89.98428f,
                              -1203.11682f, -350.32388f, 89.98428f, 0};
    const JointFrame joint = joint_frame(wrench);
    near_(joint.anchor.x, -89.98f, "the grip is CRANK_GRIP_X", 0.01f);
    near_(joint.anchor.y, -1203.1f, "the grip's y", 0.1f);
    near_(joint.anchor.z, -350.3f, "the grip's z", 0.1f);
    Mount m = mount_item(hand, kWrenchClass, 8, 0.1255f, joint);
    near_(m.scale, 1.2f, "the wrench is a fifth bigger");
    // The model's X, the shaft, runs along the fist.
    near_(m.axes.x.y, 1, "the shaft is forward");
    // The model's -Y, where the jaws are, is the hand's down, and Z its right.
    near_(m.axes.y.z, 1, "model Y is up");
    near_(m.axes.z.x, 1, "model Z is right");
    // The handle is in the palm.
    const Vec3 palm = m.position + m.axes * (joint.anchor * (0.1255f * 1.2f / 1024.0f));
    near_(length(palm - hand.at), 0, "the handle is in the palm");

    // A gun: X is its top, -Y its barrel, and the measured grip is in the palm.
    m = mount_item(hand, 180, 15, 0.1f, JointFrame{});
    near_(m.axes.x.z, 1, "the gun's top is up");
    near_(m.axes.y.y, -1, "its barrel is -Y");
    near_(m.axes.z.x, 1, "its Z is right");
    const Vec3 handle = m.position + m.axes * Vec3{0.0f, -0.16f, 0.0f};
    near_(length(handle - hand.at), 0, "the gun's handle is in the palm");

    // The Swingshot rolls a quarter about the fist, as Rerac-vr's hand * Rz(+90): forward stays, its side (the housing) up.
    m = mount_item(hand, kSwingshotClass, 12, 0.1f, JointFrame{});
    near_(m.axes.y.y, -1, "the Swingshot aims forward");
    near_(m.axes.x.x, -1, "its top is out of the wrist's left");
    near_(m.axes.z.z, 1, "its housing is on the back of the wrist");

    // The bomb glove, class 192: identity axes, the palm at its third joint.
    const float glove[16] = {1, 0, 0, 106, 0, 1, 0, -882, 0, 0, 1, -2, -131.82437f, 1193.24011f, 0, 0};
    const JointFrame palm_joint = joint_frame(glove);
    near_(palm_joint.anchor.y, -1193.24f, "the glove's palm", 0.01f);
    m = mount_item(hand, 192, 10, 0.1458f, palm_joint);
    near_(m.axes.y.y, -1, "fingers forward are -Y");
    const Vec3 glove_palm = m.position + m.axes * (palm_joint.anchor * (0.1458f / 1024.0f));
    near_(length(glove_palm - hand.at), 0, "the palm is in the hand");

    // Every mount is a proper rotation: nothing comes out mirrored.
    for (int oclass : {71, 180, 208, 483, 192, 999}) {
        m = mount_item(hand, oclass, 10, 0.1f, joint);
        const Vec3 cross{m.axes.x.y * m.axes.y.z - m.axes.x.z * m.axes.y.y,
                         m.axes.x.z * m.axes.y.x - m.axes.x.x * m.axes.y.z,
                         m.axes.x.x * m.axes.y.y - m.axes.x.y * m.axes.y.x};
        near_(dot(cross, m.axes.z), 1, "right-handed");
    }

    // The Gun size: what points shrinks about the hand, its handle still in the palm and its muzzle that much
    // nearer; the wrench, the gloves, the Swingshot and the remotes keep the game's size.
    const float blaster_joint[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 3000.0f, 0, 1};
    const JointFrame muzzle_joint = joint_frame(blaster_joint);
    const Mount full = mount_item(hand, 168, 15, 0.1f, muzzle_joint);
    const Mount half = mount_item(hand, 168, 15, 0.1f, muzzle_joint, false, 0.5f);
    near_(half.size, 0.5f, "the Blaster drawn at half size");
    near_(length(half.position + half.axes * (Vec3{-0.065f, -0.13f, 0.0f} * 0.5f) - hand.at), 0,
          "the half-size Blaster's handle is in the palm");
    near_(length(half.muzzle - hand.at), length(full.muzzle - hand.at) * 0.5f, "its muzzle half as far");
    const Mount unmeasured = mount_item(hand, 999, 15, 0.1f, JointFrame{}, false, 0.5f);
    near_(length(unmeasured.muzzle - hand.at), std::sqrt(0.40f * 0.40f + 0.03f * 0.03f) * 0.5f,
          "an unmeasured gun's muzzle half as far");
    near_(mount_item(hand, kWrenchClass, 8, 0.1255f, joint, false, 0.5f, 0.5f).size, 1, "the wrench keeps its size");
    near_(mount_item(hand, 192, 10, 0.1458f, palm_joint, false, 0.5f).size, 1, "a glove is not a gun");
    near_(mount_item(hand, kSwingshotClass, 12, 0.1f, JointFrame{}, false, 0.5f, 0.5f).size, 1, "the Swingshot keeps its size");
    near_(mount_item(hand, 619, 30, 0.1f, JointFrame{}, false, 0.5f, 0.5f).size, 1, "the PDA keeps its size");
    near_(mount_item(hand, 168, 15, 0.1f, muzzle_joint, false, 1.0f, 0.5f).size, 1, "the Hand size is not a gun's");
    // The Hand size: a glove, which is the hand, shrinks about the palm, which stays in the hand.
    const Mount small_glove = mount_item(hand, 192, 10, 0.1458f, palm_joint, false, 1.0f, 0.5f);
    near_(small_glove.size, 0.5f, "a glove at half the hand's size");
    near_(length(small_glove.position + small_glove.axes * (palm_joint.anchor * (0.1458f * 0.5f / 1024.0f)) - hand.at), 0,
          "the half-size glove's palm is in the hand");
    check(held_pointing(168, 15) && held_pointing(176, 16) && held_pointing(kWalloperClass, 26),
          "the Blaster, the Pyrocitor and the Walloper point where the controller points");
    check(!held_pointing(kWrenchClass, 8) && !held_pointing(192, 10) && !held_pointing(kSwingshotClass, 12)
              && !held_pointing(483, 21) && !held_pointing(619, 30),
          "the wrench, a glove, the Swingshot and the remotes sit as the grip has them");
}

void test_notice() {
    const std::vector<std::uint8_t> blank = render_notice("");
    check(blank.size() == static_cast<std::size_t>(kNoticeWidth) * kNoticeHeight * 4, "the picture's size");
    const std::vector<std::uint8_t> words = render_notice("LEFT CONTROLLER NOT FOUND  CHECK ITS BATTERY");
    int lit = 0;
    for (std::size_t i = 0; i < words.size(); i += 4) {
        lit += words[i] == 0xFF ? 1 : 0;
    }
    check(lit > 2000, "words are drawn");
    // The longest warning, the right hand's 45 letters, fits the bar: 6 columns of 4 pixels each.
    check(45 * 6 * 4 <= kNoticeWidth, "the longest line fits");
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


void test_support() {
    // The math: an arc takes its direction to the other, and a slerp ends where it should.
    const Quat arc = rotation_arc({1, 0, 0}, {0, 0, -1});
    const Vec3 turned = rotate(arc, {1, 0, 0});
    near_(turned.z, -1, "the arc's direction");
    near_(angle_between(slerp(Quat{}, yaw_quat(1.0f), 0.5f), yaw_quat(0.5f)), 0, "half a slerp");

    const Pose right{{0.2f, 1.2f, -0.3f}, Quat{}};
    const float dt = 1.0f / 72.0f;
    // Not two-handed: the weapon is the hand's.
    Support light;
    step(light, 15, false, right, nullptr, 0, dt);
    const Pose same = weapon_hand(right, nullptr, light);
    near_(length(same.position - right.position), 0, "a light weapon in the hand");
    near_(angle_between(same.orientation, right.orientation), 0, "a light weapon's turn");

    // Two-handed in one hand, the hand still: it settles drooped and sagged.
    Support heavy;
    for (int i = 0; i < 400; ++i) {
        step(heavy, 23, true, right, nullptr, 0, dt);
    }
    Pose weapon = weapon_hand(right, nullptr, heavy);
    near_(right.position.y - weapon.position.y, kHeavySag, "the sag");
    near_(angle_between(weapon.orientation, right.orientation), kHeavyDroop, "the droop");
    near_(rotate(weapon.orientation, {0, 0, -1}).y, -std::sin(kHeavyDroop), "drooped down, not up");

    // A fast turn of the hand: the weapon stays behind, by 0.7 rad at most.
    const Pose turned_hand{right.position, yaw_quat(1.5f)};
    step(heavy, 23, true, turned_hand, nullptr, 0, dt);
    weapon = weapon_hand(turned_hand, nullptr, heavy);
    const float behind = angle_between(weapon.orientation, normalized(turned_hand.orientation * pitch_quat(-kHeavyDroop)));
    check(behind > 0.5f && behind <= kHeavyMaxTurn + 1e-3f, "a heavy weapon turns behind the hand, within the limit");
    // A fast move: 10 cm behind at most.
    const Pose moved{right.position + Vec3{0.6f, 0, 0}, turned_hand.orientation};
    step(heavy, 23, true, moved, nullptr, 0, dt);
    weapon = weapon_hand(moved, nullptr, heavy);
    check(length(weapon.position - moved.position) <= kHeavyMaxLag + kHeavySag + 1e-3f, "a heavy weapon follows within 10 cm");
    check(length(weapon.position - moved.position) > 0.08f, "a heavy weapon is behind a fast move");

    // The left hand squeezed on the foregrip holds it: no lag, the weapon points from hand to hand.
    Support both;
    const Pose left_on{both.point(right), Quat{}};
    step(both, 23, true, right, &left_on, 0.8f, dt);
    check(both.held, "the left hand takes the foregrip");
    const Pose left_up{right.position + Vec3{0, 0.2f, -0.3f}, Quat{}};
    step(both, 23, true, right, &left_up, 0.5f, dt);
    check(both.held, "a squeeze between let-go and take keeps it");
    weapon = weapon_hand(right, &left_up, both);
    const Vec3 along = normalized(rotate(weapon.orientation, foregrip(23)));
    const Vec3 wanted = normalized(left_up.position - right.position);
    near_(dot(along, wanted), 1, "both hands aim it from the right to the left");
    near_(length(support_hand(right, left_up, both).position - both.point(weapon)), 0, "the left hand drawn on the foregrip");
    step(both, 23, true, right, &left_up, 0.3f, dt);
    check(!both.held, "let go");
    // Far from the foregrip, a squeeze takes nothing.
    Support far;
    const Pose left_far{right.position + Vec3{-0.6f, 0, 0}, Quat{}};
    step(far, 23, true, right, &left_far, 1.0f, dt);
    check(!far.held, "a squeeze away from the foregrip");

    // The Hydrodisplacer is two-handed, not heavy.
    Support hydro;
    step(hydro, kHydrodisplacerItem, true, right, nullptr, 0, dt);
    step(hydro, kHydrodisplacerItem, true, turned_hand, nullptr, 0, dt);
    near_(angle_between(weapon_hand(turned_hand, nullptr, hydro).orientation, turned_hand.orientation), 0, "the Hydrodisplacer follows exactly");
    // Another item forgets the lag.
    step(heavy, 11, true, right, nullptr, 0, dt);
    check(length(heavy.lag) < kHeavySag + 1e-3f, "a new weapon starts settled");
}

void test_glyphs() {
    using namespace quest::glyphs;
    // The A button: clear corners, the letter white, the disc's dark rim, its advance.
    Cell a;
    check(paint(a, kGlyphs[0]) == 18, "a button's advance");
    near_(a.rgba[0][0][3], 0, "a clear corner");
    near_(a.rgba[0][23][3], 0, "the cell's right edge clear");
    near_(a.rgba[3][9][0], 238, "the A's top");
    near_(a.rgba[3][9][3], kOpaque, "the A opaque");
    check(a.rgba[8][2][3] > 0 && a.rgba[8][2][0] < 60, "the disc's dark rim");
    // The trigger key and the stick click.
    Cell rt;
    check(paint(rt, kGlyphs[1]) == 21, "a key's advance");
    check(rt.rgba[2][1][0] > 120, "the key's lit corner");
    Cell click;
    check(paint(click, kGlyphs[2]) == 17, "a stick's advance");
    near_(click.rgba[0][9][0], 235, "the press arrow");

    // CSM1 swaps bits 3 and 4.
    check(csm1(0x08) == 0x10 && csm1(0x10) == 0x08 && csm1(0x18) == 0x18 && csm1(0x07) == 0x07, "csm1");
    // The nearest colour: clear for clear, the dark grey for the rim, white for the letter.
    std::uint8_t palette[1024] = {};
    const auto put = [&](int index, int r, int g, int b, int alpha) {
        std::uint8_t* p = palette + 4 * csm1(index);
        p[0] = static_cast<std::uint8_t>(r);
        p[1] = static_cast<std::uint8_t>(g);
        p[2] = static_cast<std::uint8_t>(b);
        p[3] = static_cast<std::uint8_t>(alpha);
    };
    for (int i = 0; i < 256; ++i) {
        put(i, 255, 255, 255, 0x80);  // white everywhere but
    }
    put(0, 0, 0, 0, 0);
    put(0x09, 30, 30, 32, 0x80);
    put(0x12, 80, 80, 84, 0x80);
    const float clear[4] = {0, 0, 0, 0};
    const float rim[4] = {30, 30, 31.8f, kOpaque};
    const float body[4] = {78, 78, 82, kOpaque};
    const float letter[4] = {238, 238, 244, kOpaque};
    check(nearest(palette, clear) == 0, "clear takes clear");
    check(nearest(palette, rim) == 0x09, "the rim's grey");
    check(nearest(palette, body) == 0x12, "the body's grey");
    check(palette[4 * csm1(nearest(palette, letter))] == 255, "the letter's white");

    // Xbox style (0.1.19): the plain pad's Y, a face button; the grips named as its bumpers in first person too.
    Cell y;
    check(paint(y, kPadGlyphs[2]) == 18, "the Y button's advance");
    near_(y.rgba[3][6][0], 238, "the Y's left arm");
    near_(y.rgba[11][9][0], 238, "the Y's stem");
    check(std::string(kGlyphs[3].label) == "RB" && std::string(kGlyphs[4].label) == "LB", "first person's grips are RB and LB");
    check(std::string(kPadGlyphs[1].label) == "B" && std::string(kPadGlyphs[7].label) == "RT", "the pad's Circle B, R2 RT");
    // First person's cells from the Xbox mod's: each an icon's, Cross its own, Circle the RT (R2's), R2 the B (Circle's).
    for (std::uint8_t cell : kVrCells) {
        check(cell >= 0x10 && cell <= 0x19, "a cell of the icons");
    }
    check(kVrCells[0] == 0x10 && kVrCells[1] == 0x17 && kVrCells[7] == 0x11 && kVrCells[3] == 0x15, "first person's cells");
    // The Xbox mod's green A on the Cross's cell, or not.
    std::uint8_t greens[1024] = {};
    std::uint8_t* g = greens + 4 * csm1(1);
    g[0] = 40;
    g[1] = 190;
    g[2] = 50;
    g[3] = 0x80;
    std::uint8_t font[32 * 16] = {};
    for (int i = 0; i < 20; ++i) {
        font[(i / 10 + 4) * 32 + 5 + i % 10] = 1;
    }
    check(green_a(font, 32, 0, 0, greens), "the Xbox mod's A found");
    std::uint8_t few[32 * 16] = {};
    for (int i = 0; i < 5; ++i) {
        few[6 * 32 + 5 + i] = 1;
    }
    check(!green_a(few, 32, 0, 0, greens), "a few green texels are not its A");
    check(!green_a(font, 32, 0, 0, palette), "the white palette's: not green");
}

void test_disguise() {
    // The salute: the right hand raised open by the head, five frames; a fist or a low hand does not.
    const Pose head{{0, 1.6f, 0}, Quat{}};
    const Pose raised{{0.2f, 1.65f, -0.3f}, Quat{}};
    const Pose low{{0.2f, 1.25f, -0.3f}, Quat{}};
    Salute salute;
    for (int i = 0; i < 4; ++i) {
        check(!salute.update(head, raised, 0.0f), "not before five frames");
    }
    check(salute.update(head, raised, 0.0f), "a salute on the fifth");
    check(!salute.update(head, low, 0.0f), "lowered, it ends");
    Salute fist;
    for (int i = 0; i < 10; ++i) {
        fist.update(head, raised, 1.0f);
    }
    check(!fist.active, "a fist does not salute");

    // Disguised: B and A take it off (A's Cross held too), the salute is Circle, B is not R2.
    PadMapper mapper;
    PadContext context;
    context.first_person = true;
    context.disguised = true;
    Controls c;
    c.b = true;
    VirtualPad out = mapper.map(c, context);
    check((out.held & kPadSquare) && !(out.held & kPadR2), "B takes the disguise off");
    c = Controls{};
    c.a = true;
    out = mapper.map(c, context);
    check((out.held & kPadSquare) && (out.held & kPadCross), "A too, Cross held");
    c = Controls{};
    context.salute = true;
    out = mapper.map(c, context);
    check(out.held & kPadCircle, "the salute is Circle");
    context = PadContext{};
    context.first_person = true;
    c.b = true;
    out = mapper.map(c, context);
    check((out.held & kPadR2) && !(out.held & kPadSquare), "undisguised, B is R2");
}

void test_widgets() {
    check(grid_cell_at(3, 5, 100.0f, 60.0f, 0.0f, 0.0f) == 0, "the first cell");
    check(grid_cell_at(3, 5, 100.0f, 60.0f, 99.0f, 59.0f) == 14, "the last cell");
    check(grid_cell_at(3, 5, 100.0f, 60.0f, 45.0f, 30.0f) == 7, "the middle");
    check(grid_cell_at(3, 5, 100.0f, 60.0f, 100.0f, 30.0f) == -1, "past its edge");
    check(grid_cell_at(0, 5, 100.0f, 60.0f, 10.0f, 10.0f) == -1, "no rows");
    check(spread_row_at(3, 100.0f, 24.0f, 25.0f) == 0, "the first of three");
    check(spread_row_at(3, 100.0f, 24.0f, 75.0f) == 2, "the last of three");
    check(spread_row_at(3, 100.0f, 24.0f, 37.5f) == -1, "between two rows");
    check(spread_row_at(3, 100.0f, 24.0f, 38.0f) == 1, "the next one starts");
    check(spread_row_at(3, 100.0f, 24.0f, 2.0f) == -1, "above them");
}

void test_window_row() {
    // VR Options' box: 149 high, five rows about 25 apart.
    check(window_row_at(5, 149.0f, 24.0f) == 0, "the first row");
    check(window_row_at(5, 149.0f, 124.0f) == 4, "the last row");
    check(window_row_at(5, 149.0f, 2.0f) == -1, "the top edge");
    check(window_row_at(5, 149.0f, 147.0f) == 5, "the bottom edge");
}

void test_beam() {
    Pose pose;
    float span = 0;
    check(beam_pose({0, 1, 0}, {0, 1, -2}, {0.5f, 1.2f, 0}, pose, span), "a beam");
    near_(span, 2.0f, "its length");
    const Vec3 along = rotate(pose.orientation, {1, 0, 0});
    near_(along.z, -1.0f, "the quad's X runs along it");
    const Vec3 middle{0, 1, -1};
    const Vec3 facing = rotate(pose.orientation, {0, 0, 1});
    check(dot(facing, Vec3{0.5f, 1.2f, 0} - middle) > 0.0f, "its face toward the eye");
    near_(dot(facing, along), 0.0f, "square to the beam");
    near_(pose.position.z, -1.0f, "centred");
    check(!beam_pose({0, 0, 0}, {0, 0, 0}, {1, 0, 0}, pose, span), "no length");
    check(!beam_pose({0, 0, 0}, {0, 0, -2}, {0, 0, 2}, pose, span), "end on");
}

void test_pointer() {
    // The ray onto a panel 2.4 m ahead: straight on, its middle; raised, higher; turned away, none.
    const Pose panel{{0, 1.5f, -2.4f}, Quat{}};
    float u = 0, v = 0;
    check(panel_point(panel, 3.0f, 2.25f, Pose{{0, 1.5f, 0}, Quat{}}, u, v), "straight at it");
    near_(u, 0.5f, "its middle across");
    near_(v, 0.5f, "its middle down");
    const float up = 0.2f;
    check(panel_point(panel, 3.0f, 2.25f, Pose{{0, 1.5f, 0}, Quat{std::sin(up * 0.5f), 0, 0, std::cos(up * 0.5f)}}, u, v),
          "raised");
    near_(v, 0.5f - 2.4f * std::tan(up) / 2.25f, "higher on it");
    check(!panel_point(panel, 3.0f, 2.25f, Pose{{0, 1.5f, 0}, yaw_quat(kPi)}, u, v), "turned away");
    check(!panel_point(panel, 3.0f, 2.25f, Pose{{0, 1.5f, 0}, yaw_quat(1.2f)}, u, v), "past its edge");

    // The weapon wheel at the hand: a hand seen from the eye on the HUD, the ring's middle there, kept in.
    const Vec3 eye{0, 1.5f, 0};
    check(panel_point_seen(panel, 3.0f, 2.25f, eye, {0, 1.5f, -1.0f}, u, v), "a hand straight ahead");
    near_(u, 0.5f, "seen in the middle across");
    near_(v, 0.5f, "and down");
    check(panel_point_seen(panel, 3.0f, 2.25f, eye, {0.3f, 1.2f, -0.6f}, u, v), "a hand low and right");
    near_(u, 0.5f + 0.3f * 4.0f / 3.0f, "seen right by its angle");
    near_(v, 0.5f + 0.3f * 4.0f / 2.25f, "and low, past the edge");
    check(!panel_point_seen(panel, 3.0f, 2.25f, eye, {0.3f, 1.2f, 0.1f}, u, v), "a hand beside the head is not seen ahead");
    WheelFit fit;  // a ring 192 square, its middle in its middle, on 512 by 416
    float x = 0, y = 0;
    wheel_middle_at(fit, 0.5f, 0.5f, x, y);
    near_(x, 256.0f, "the ring in the middle");
    near_(y, 208.0f, "and down");
    wheel_middle_at(fit, 1.4f, -0.3f, x, y);
    near_(x, 512.0f - 8.0f - 96.0f, "past the right edge: in by the gap");
    near_(y, 8.0f + 96.0f, "past the top: in by the gap");
    fit.reach_x = 60.0f;
    wheel_middle_at(fit, 0.0f, 0.5f, x, y);
    near_(x, 196.0f, "no further from straight ahead than the reach");
    fit.reach_x = 1e9f;
    // A right ring opened over the left one's: moved to its right; with no room there, below it.
    x = 200.0f;
    y = 208.0f;
    wheel_beside(fit, false, 180.0f, 208.0f, 6.0f, x, y);
    near_(x, 180.0f + 198.0f, "beside the other, to the right");
    near_(y, 208.0f, "at its height");
    x = 380.0f;
    y = 150.0f;
    wheel_beside(fit, false, 400.0f, 104.0f, 6.0f, x, y);
    near_(x, 380.0f, "no room right of it: where it was across");
    near_(y, 104.0f + 198.0f, "below it");
    x = 100.0f;
    y = 300.0f;
    wheel_beside(fit, true, 400.0f, 100.0f, 6.0f, x, y);
    near_(x, 100.0f, "already clear: left as it was");
    near_(y, 300.0f, "both ways");

    // A list of four rows 100 high: 20 a row, the first from 13; a second line takes two.
    MenuList list;
    list.width = 200;
    list.height = 100;
    list.rows = 4;
    check(list_row_at(list, 50, 15) == 0, "the first row");
    check(list_row_at(list, 50, 35) == 1, "the second");
    check(list_row_at(list, 2, 35) == -1, "left of the rows");
    check(list_row_at(list, 50, 99) == -1, "below them");
    list.scroll = 20;
    check(list_row_at(list, 50, 15) == 1, "scrolled by one");
    list.scroll = 0;
    list.second_line[0] = true;
    check(list_row_at(list, 50, 35) == 0, "a second line is the row's");
    check(list_row_at(list, 50, 55) == 1, "the next after it");
    MenuList font_rows;
    font_rows.flags = 0x10 | 4;
    font_rows.width = 200;
    font_rows.height = 300;
    font_rows.rows = 3;
    check(list_row_at(font_rows, 50, 10 + 17) == 1, "rows as high as the large font");
}
}  // namespace

void test_ledge_pad() {
    PadMapper mapper;
    PadContext context;
    context.first_person = true;
    context.ledge = true;
    Controls c;
    c.left_grip = 1;
    c.left_trigger = 1;
    check((mapper.map(c, context).held & kPadR1) == 0, "holding a ledge is not crouching");
    context.ledge_drop = true;
    check((mapper.map(c, context).held & kPadR1) != 0, "the drop is the game's R1");
    context.ledge_drop = false;
    c = {};
    c.a = true;
    check((mapper.map(c, context).held & kPadCross) == 0, "A does not climb before both hands are on");
    context.ledge_climb = true;
    check((mapper.map(c, context).held & kPadCross) != 0, "A climbs with both hands on");
    c = {};
    context.ledge_climb = false;
    context.ledge_press = true;
    check((mapper.map(c, context).held & kPadCross) != 0, "the shove presses Cross");
    context.ledge_press = false;
    c.left_x = 1;
    near_(mapper.map(c, context).left_x, 0.0f, "the stick does not move him on a ledge");
    context.swing = true;
    check((mapper.map(c, context).held & kPadSquare) == 0, "a shove on a ledge is not a wrench swing");
    context.swing = false;
    context.ledge_stick = true;
    context.head_yaw = 1.0f;
    context.camera_yaw = 1.0f;
    near_(mapper.map(c, context).left_y, 1.0f, "the hands' pull is the stick, along head_yaw");
}

void test_dive_pad() {
    PadMapper mapper;
    PadContext context;
    context.first_person = true;
    Controls c;
    check((mapper.map(c, context).held & kPadSquare) == 0, "no dive, no Square");
    context.dive = true;
    check((mapper.map(c, context).held & kPadSquare) != 0, "the dive is the game's Square");
}

// RAC-Board (board.h): the reach behind the back, the riding pad, and the view with the board.
void test_jet_board() {
    const Pose head{{0, 1.6f, 0}, yaw_quat(0)};
    check(hand_behind_back(head, {0, 1.4f, 0.25f}, false), "a hand behind the head is behind the back");
    check(!hand_behind_back(head, {0, 1.2f, -0.3f}, false), "a hand ahead is not");
    check(!hand_behind_back(head, {0, 0.5f, 0.25f}, false), "nor one at the knees");
    check(!hand_behind_back(head, {0.6f, 1.4f, 0.25f}, false), "nor one well out to the side");
    check(!hand_behind_back(head, {0, 1.4f, 0.10f}, false), "at the edge, coming in: not yet");
    check(hand_behind_back(head, {0, 1.4f, 0.10f}, true), "at the edge, in already: still");
    const Pose turned{{0, 1.6f, 0}, yaw_quat(kPi / 2)};  // facing -x
    check(hand_behind_back(turned, {0.25f, 1.4f, 0}, false), "the head turned: behind it");
    check(!hand_behind_back(turned, {0, 1.4f, 0.25f}, false), "the head turned: the old behind is a side");
    const Pose down{{0, 1.6f, 0}, Quat{std::sin(-0.7f), 0, 0, std::cos(-0.7f)}};  // looking down 80 degrees
    check(hand_behind_back(down, {0, 1.3f, 0.25f}, false), "looking down: behind still by the head's top");

    check(board_toggle_bits("up") == kPadUp && board_toggle_bits("l2r2") == (kPadL2 | kPadR2), "the board's button");
    check(board_trick_bits("r1r2") == kPadR1 && board_grind_bits("r1r2x") == kPadR2 && board_grab_bits("none") == 0, "its trick, grind, grab");

    PadMapper mapper;
    PadContext context;
    context.first_person = true;
    context.jet_toggle = kPadUp;
    Controls c;
    check((mapper.map(c, context).held & kPadUp) != 0, "on foot the reach presses the board's button");
    context.jet_toggle = 0;
    context.jet_board = true;
    context.jet_trick = kPadR1;
    context.jet_grind = kPadR2;
    context.jet_grab = kPadL1;
    context.jet_yaw = kPi / 2;  // the board heads 90 degrees left of the game's camera
    context.head_yaw = -1.0f;   // and the head looks elsewhere
    c.right_trigger = 1.0f;
    c.a = true;
    c.left_y = 1.0f;
    PadMapper riding;
    const VirtualPad out = riding.map(c, context);
    check((out.held & kPadR1) != 0 && (out.held & kPadCross) != 0, "riding: the right trigger its trick, A the jump");
    check((out.held & (kPadCircle | kPadSquare | kPadTriangle)) == 0, "riding: none of the game's own play");
    // RAC-Board reads the stick about the board: in first person forward is turned from the board's heading to the head's,
    // at most kBoardSteerMost (0.1.19): here the head is 147 degrees right of the board, so a full carve right.
    near_(out.left_x, -std::sin(-kBoardSteerMost), "riding in first person, the head far right: forward a full carve that way (x)");
    near_(out.left_y, std::cos(-kBoardSteerMost), "riding in first person, the head far right: forward a full carve that way (y)");
    context.head_yaw = kPi / 2 + 0.6f;  // 34 degrees left of the board: within the most, turned as far as the head
    PadMapper near_left;
    const VirtualPad left_of = near_left.map(c, context);
    near_(left_of.left_x, -std::sin(0.6f), "riding in first person, the head near: forward is where it looks (x)");
    near_(left_of.left_y, std::cos(0.6f), "riding in first person, the head near: forward is where it looks (y)");
    context.head_yaw = kPi / 2 - kPi + 0.1f;  // across the turn's wrap, 170 degrees right: the short way, right
    PadMapper wrapped;
    const VirtualPad round = wrapped.map(c, context);
    near_(round.left_x, -std::sin(-kBoardSteerMost), "riding, the head behind on the right: a full carve right (x)");
    context.head_yaw = kPi / 2;
    PadMapper along;
    const VirtualPad ahead = along.map(c, context);
    near_(ahead.left_x, 0.0f, "the head along the board: the stick as it is");
    near_(ahead.left_y, 1.0f, "the head along the board: forward keeps its way");
    context.first_person = false;
    context.head_yaw = -1.0f;
    PadMapper third;
    const VirtualPad behind = third.map(c, context);
    near_(behind.left_y, 1.0f, "third person: the stick as it is, about the board");
    context.first_person = true;

    // RAC-Board's Weapons on the board (0.1.19): the right trigger fires (Circle), B is its trick and grind, the grips
    // and the left trigger its grab; the left trigger fires a left-hand gun instead; a hand's swing in first person is
    // no Square (the mod's hit, not the board's), the grip is in third person.
    context.jet_weapons = true;
    context.head_yaw = kPi / 2;
    PadMapper armed;
    Controls fire_c;
    fire_c.right_trigger = 1.0f;
    fire_c.a = true;
    VirtualPad shot = armed.map(fire_c, context);
    check((shot.held & kPadCircle) != 0 && (shot.held & kPadCross) != 0, "weapons on the board: the trigger fires, A jumps");
    check((shot.held & (kPadR1 | kPadR2 | kPadL1)) == 0, "weapons on the board: the trigger is no trick");
    Controls trick_c;
    trick_c.b = true;
    const VirtualPad tricked = PadMapper().map(trick_c, context);
    check((tricked.held & (kPadR1 | kPadR2)) == (kPadR1 | kPadR2), "weapons on the board: B its spins, flips and grind");
    Controls grab_c;
    grab_c.left_trigger = 1.0f;
    const VirtualPad grabbed = PadMapper().map(grab_c, context);
    check((grabbed.held & kPadL1) != 0 && (grabbed.held & kPadR1) == 0, "weapons on the board: the left trigger grabs, no crouch");
    context.left_weapon = true;
    const VirtualPad left_gun = PadMapper().map(grab_c, context);
    check((left_gun.held & kPadL1) == 0, "weapons on the board: a left-hand gun's trigger is the gun's");
    context.left_weapon = false;
    Controls swing_c;
    context.swing = true;
    check((PadMapper().map(swing_c, context).held & kPadSquare) == 0, "weapons on the board, first person: a swing is no Square");
    context.swing = false;
    context.first_person = false;
    Controls grip_c;
    grip_c.right_grip = 1.0f;
    check((PadMapper().map(grip_c, context).held & kPadSquare) != 0, "weapons on the board, third person: the grip swings (Square)");
    context.first_person = true;
    context.jet_weapons = false;

    near_(yaw_part(yaw_quat(0.7f)), 0.7f, "a yaw's part about up");
    check(angle_between(axes_quat({2, 0, 0}, {0.1f, 3, 0}), Quat{}) < 1e-3f, "the axes as they are: no turn");
    const Quat some = normalized(Quat{0.2f, 0.5f, -0.1f, 0.8f});
    check(angle_between(axes_quat(rotate(some, {1, 0, 0}), rotate(some, {0, 1, 0})), some) < 1e-3f, "a joint's turn from its axes");

    // The follow: a stance (crouched, side-on), a carve, a flip, a spin with the mod's heading jump at its end.
    const float dt = 1.0f / 72.0f;
    const Quat stance = normalized(yaw_quat(0.8f) * Quat{0.15f, 0, 0, 1});
    const auto pitch = [](float a) { return Quat{std::sin(a * 0.5f), 0, 0, std::cos(a * 0.5f)}; };
    BoardFollow follow;
    Quat turn{};
    float rig = follow.update(0.3f, yaw_quat(0.3f) * stance, true, dt, turn);
    near_(rig, 0.0f, "the ride's start turns nothing");
    check(angle_between(turn, Quat{}) < 1e-3f, "standing on it: no turn of the view");
    for (int i = 0; i < 100; ++i) {  // past getting on (kBoardMountSettle)
        rig += follow.update(0.3f, yaw_quat(0.3f) * stance, true, dt, turn);
    }
    near_(rig, 0.0f, "standing on it a while turns nothing");
    rig = follow.update(0.4f, yaw_quat(0.4f) * stance, true, dt, turn);
    near_(rig, 0.0f, "a carve turns no view: the head steers the board (0.1.19)");
    check(angle_between(turn, Quat{}) < 1e-3f, "a carve: no turn of the view besides");
    follow.update(0.4f, yaw_quat(0.4f) * pitch(1.2f) * stance, true, dt, turn);
    near_(angle_between(turn, Quat{}), 1.2f, "a flip turns the view as far", 2e-3f);
    near_(yaw_part(turn), 0.0f, "a flip has no yaw", 2e-3f);
    follow.update(0.4f, yaw_quat(0.4f) * stance, true, dt, turn);
    check(angle_between(turn, Quat{}) < 1e-3f, "landed: level");
    float rig_total = follow.update(0.4f, yaw_quat(0.4f + 3.0f) * stance, true, dt, turn);
    rig_total += follow.update(0.4f + 3.0f, yaw_quat(0.4f + 3.0f) * stance, true, dt, turn);
    check(angle_between(turn, Quat{}) > 2.5f, "the heading's jump at a spin's end leaves the view where it was");
    for (int i = 0; i < 400; ++i) {
        rig_total += follow.update(0.4f + 3.0f, yaw_quat(0.4f + 3.0f) * stance, true, dt, turn);
    }
    check(angle_between(turn, Quat{}) < 0.02f, "after the spin the turn goes to the rig");
    near_(wrap_angle(rig_total), wrap_angle(3.0f), "the rig has the spin", 0.03f);

    // Getting on (0.1.19, racboardhead.sim): from his standing pose RAC-Board turns his pelvis about 55 degrees to its
    // stance over half a second: his stance, taken into the rest, no turn of the rig or the view.
    BoardFollow mount;
    Quat mount_turn{};
    float mount_rig = mount.update(0.3f, yaw_quat(0.3f), true, dt, mount_turn);
    for (int i = 1; i <= 36; ++i) {
        mount_rig += mount.update(0.3f, yaw_quat(0.3f + 0.95f * static_cast<float>(i) / 36.0f) * pitch(0.15f), true, dt, mount_turn);
    }
    for (int i = 0; i < 100; ++i) {
        mount_rig += mount.update(0.3f, yaw_quat(0.3f + 0.95f) * pitch(0.15f), true, dt, mount_turn);
    }
    near_(mount_rig, 0.0f, "getting on: his stance's turn turns no rig", 1e-3f);
    near_(yaw_part(mount_turn), 0.0f, "getting on: and no view's yaw", 2e-3f);
    // Riding on, his pelvis's yaw jitters a tenth of a radian a frame: no turn of the rig, the view's yaw back to none.
    for (int i = 0; i < 300; ++i) {
        const float sway = (i % 2 == 0 ? 0.12f : -0.08f) * (i < 280 ? 1.0f : 0.0f);
        mount_rig += mount.update(0.3f, yaw_quat(0.3f + 0.95f + sway) * pitch(0.15f), true, dt, mount_turn);
    }
    near_(mount_rig, 0.0f, "the ride's sway turns no rig", 1e-3f);
    near_(yaw_part(mount_turn), 0.0f, "and leaves no yaw in the view", 0.02f);

    // What the view shows: a riding sway not, a trick all, the yaw all.
    check(angle_between(board_view_turn(pitch(0.12f)), Quat{}) < 1e-3f, "a sway of riding: the view stays level");
    check(angle_between(board_view_turn(pitch(2.0f)), pitch(2.0f)) < 1e-3f, "a flip: the view goes all the way with it");
    check(angle_between(board_view_turn(yaw_quat(0.5f) * pitch(0.1f)), yaw_quat(0.5f)) < 1e-3f, "a spin's yaw, all of it");
    const float mid = angle_between(board_view_turn(pitch(0.5f)), Quat{});
    check(mid > 0.1f && mid < 0.5f, "between: some of it");

    // The ease: there in a few frames, back to none as the ride ends.
    BoardView view;
    for (int i = 0; i < 30; ++i) {
        view.update(pitch(1.0f), true, dt);
    }
    check(view.on && angle_between(view.turn, pitch(1.0f)) < 0.01f, "the view's turn follows in under half a second");
    view.update(pitch(1.2f), true, dt);
    check(angle_between(view.turn, pitch(1.0f)) < 0.2f, "no frame jumps");
    for (int i = 0; i < 60; ++i) {
        view.update(pitch(1.0f), false, dt);
    }
    check(!view.on && angle_between(view.turn, Quat{}) < 1e-3f, "off the board: level again");
}

// The Trespasser's rings (0.1.21, puzzle.h): a step a push, the axis kept, the repeats, A B X, the grab, the asks in order.
void test_puzzle_input() {
    // A tick of the lock: what the pad asks, the lock answering a held way as an idle lock would (a turn begun) and a
    // press as a ring chosen. `turning` ticks of a turn's animation, the lock reading nothing meanwhile.
    struct Lock {
        int ring = 1;
        int busy = 0;
        int turns = 0;  // clockwise less anticlockwise
        int steps = 0;  // turns begun
        int turned_ring = 0;  // the ring the last turn was of
        void tick(PuzzleInput& puzzle) {
            int turn = 0, press = 0;
            puzzle.ask(ring, turn, press);
            if (busy > 0) {
                --busy;
                return;
            }
            if (turn != 0) {
                puzzle.turned(turn);
                turns += turn;
                ++steps;
                turned_ring = ring;
                busy = 5;
            } else if (press != 0) {
                ring = std::clamp(ring + press, 1, 3);
            }
        }
    };
    const auto run = [](PuzzleInput& puzzle, Lock& lock, float x, float y, int ticks, bool a = false, bool b = false, bool xb = false) {
        for (int i = 0; i < ticks; ++i) {
            puzzle.controls(x, y, 0, 0, a, b, xb, lock.ring);
            lock.tick(puzzle);
        }
    };

    // A push to the side and back: one step, whatever the lock's speed.
    PuzzleInput puzzle;
    Lock lock;
    run(puzzle, lock, 1, 0, 10);
    run(puzzle, lock, 0, 0, 20);
    check(lock.steps == 1 && lock.turns == 1, "a push right: one step clockwise");
    run(puzzle, lock, -0.9f, 0.1f, 10);
    run(puzzle, lock, 0, 0, 20);
    check(lock.turns == 0 && lock.steps == 2, "a push left: one back");

    // Held 50 ticks: the first, a repeat at 27, another at 42: three, not ten.
    lock = Lock{};
    run(puzzle, lock, 1, 0, 50);
    run(puzzle, lock, 0, 0, 30);
    check(lock.steps == 3, "held 50 ticks: three steps");

    // An arc of the thumb from the side to up: no ring changed until let go.
    lock = Lock{};
    run(puzzle, lock, 1, 0, 3);
    run(puzzle, lock, 0.7f, 0.7f, 3);
    run(puzzle, lock, 0.05f, 1, 10);
    check(lock.ring == 1, "an arc from the side up: the ring kept");
    run(puzzle, lock, 0, 0, 20);
    // Then a clear push down: the next ring; a slow one up passes 0.6 before it is 0.7, and still counts.
    run(puzzle, lock, 0.1f, -1, 6);
    run(puzzle, lock, 0, 0, 6);
    check(lock.ring == 2, "down: the next ring");
    run(puzzle, lock, 0.05f, 0.62f, 3);
    run(puzzle, lock, 0.05f, 0.75f, 3);
    run(puzzle, lock, 0, 0, 6);
    check(lock.ring == 1, "a slow push up: the ring before");
    run(puzzle, lock, 0, 1, 6);
    run(puzzle, lock, 0, 0, 6);
    check(lock.ring == 1, "up at the outer ring: nothing, as the game");
    // A diagonal not clearly up is a turn.
    const int before = lock.steps;
    run(puzzle, lock, 0.65f, 0.6f, 4);
    run(puzzle, lock, 0, 0, 20);
    check(lock.steps == before + 1 && lock.ring == 1, "a diagonal: a step, not a ring");

    // A, B, X: one each a press, held or not; X round 1 2 3 1.
    lock = Lock{};
    run(puzzle, lock, 0, 0, 1, true);
    run(puzzle, lock, 0, 0, 30, true);
    run(puzzle, lock, 0, 0, 2);
    check(lock.turns == 1 && lock.steps == 1, "A held: one step clockwise");
    run(puzzle, lock, 0, 0, 1, false, true);
    run(puzzle, lock, 0, 0, 20);
    check(lock.turns == 0 && lock.steps == 2, "B: one anticlockwise");
    for (int want : {2, 3, 1}) {
        run(puzzle, lock, 0, 0, 1, false, false, true);
        run(puzzle, lock, 0, 0, 8);
        check(lock.ring == want, "X: the next ring round");
    }
    // In order: X then A turns the new ring, not the old.
    lock = Lock{};
    lock.busy = 3;  // the lock still turning: both wait
    run(puzzle, lock, 0, 0, 1, false, false, true);
    run(puzzle, lock, 0, 0, 1, true);
    run(puzzle, lock, 0, 0, 20);
    check(lock.turned_ring == 2 && lock.turns == 1, "X then A: the step on the new ring");
    // And A then X: the step on the old.
    lock = Lock{};
    lock.busy = 3;
    run(puzzle, lock, 0, 0, 1, true);
    run(puzzle, lock, 0, 0, 1, false, false, true);
    run(puzzle, lock, 0, 0, 20);
    check(lock.turned_ring == 1 && lock.ring == 2, "A then X: the step on the old ring, then the next");
    // Mashed: no more than two waiting; the other way takes one back.
    lock = Lock{};
    lock.busy = 100;  // the lock busy: nothing taken
    for (int i = 0; i < 5; ++i) {
        run(puzzle, lock, 0, 0, 1, true);
        run(puzzle, lock, 0, 0, 1);
    }
    check(puzzle.waiting() == 2, "A five times while busy: two wait");
    run(puzzle, lock, 0, 0, 1, false, true);
    run(puzzle, lock, 0, 0, 1);
    check(puzzle.waiting() == 1, "B takes one back");
    // An ask the lock never takes is dropped after 2.5 seconds.
    for (int i = 0; i < PuzzleInput::kGiveUp + 5; ++i) {
        puzzle.controls(0, 0, 0, 0, false, false, false, lock.ring);
        int turn = 0, press = 0;
        puzzle.ask(lock.ring, turn, press);
    }
    check(puzzle.waiting() == 0, "never taken: dropped");

    // Up and Down a tick on and a tick off (the game reads them pressed).
    PuzzleInput presses;
    presses.controls(0, 0, 0, 0, false, false, true, 1);  // X: ring 2
    int turn = 0, press = 0;
    presses.ask(1, turn, press);
    check(press == 1 && turn == 0, "toward ring 2: Down");
    presses.controls(0, 0, 0, 0, false, false, true, 1);
    presses.ask(1, turn, press);
    check(press == 0, "a tick off between presses");

    // The grab: the bands on a 4:3 panel, ring 1 the outer.
    constexpr float aspect = 0.75f;
    check(PuzzleInput::ring_under(0.537f, 0, aspect) == 1 && PuzzleInput::ring_under(0, 0.656f, aspect) == 1, "the outer ring");
    check(PuzzleInput::ring_under(-0.385f, 0, aspect) == 2 && PuzzleInput::ring_under(0, -0.47f, aspect) == 2, "the middle ring");
    check(PuzzleInput::ring_under(0.2f, 0, aspect) == 3 && PuzzleInput::ring_under(0.1f, 0.0f, aspect) == 3, "the inner ring");
    check(PuzzleInput::ring_under(0.03f, 0, aspect) == 0 && PuzzleInput::ring_under(0.9f, 0, aspect) == 0, "the middle and past the frame: none");
    // The point from the eye through the hand: a panel a metre ahead, 0.8 wide; the hand half way, 0.1 right.
    const Pose panel{{0, 1.6f, -1.0f}, Quat{}};  // facing +Z, toward the eye
    float u = 0, v = 0;
    check(PuzzleInput::panel_point(panel, 0.8f, 0.6f, {0, 1.6f, 0}, {0.1f, 1.6f, -0.5f}, u, v) && std::abs(u - 0.5f) < 1e-4f
              && std::abs(v) < 1e-4f,
          "the hand half way: seen at twice its offset");
    check(!PuzzleInput::panel_point(panel, 0.8f, 0.6f, {0, 1.6f, 0}, {0.1f, 1.6f, -0.05f}, u, v), "a hand at the face: not over it");
    check(!PuzzleInput::panel_point(panel, 0.8f, 0.6f, {0, 1.6f, 0}, {0.1f, 1.6f, 0.3f}, u, v), "behind the head: not over it");

    // Grabbed at the middle ring on the right, taken round a quarter clockwise (down): three steps on ring 2.
    PuzzleInput grab;
    Lock held;
    grab.controls(0, 0, 0, 0, false, false, false, held.ring);
    grab.grab(1, 1.0f, true, 0.385f, 0, aspect);
    check(grab.grab_hand() == 1, "the right grip takes the middle ring");
    held.tick(grab);
    held.tick(grab);
    held.tick(grab);
    check(held.ring == 2, "the grab chooses its ring");
    const float quarter = -1.66f;  // a little past a quarter round, clockwise (down from the right)
    for (int i = 1; i <= 30; ++i) {
        const float angle = quarter * static_cast<float>(i) / 30.0f;
        grab.controls(0, 0, 0, 0, false, false, false, held.ring);
        grab.grab(1, 1.0f, true, 0.385f * std::cos(angle), 0.385f * std::sin(angle) / aspect, aspect);
        held.tick(grab);
    }
    for (int i = 0; i < 30; ++i) {
        grab.controls(0, 0, 0, 0, false, false, false, held.ring);
        grab.grab(1, 1.0f, true, 0.385f * std::cos(quarter), 0.385f * std::sin(quarter) / aspect, aspect);
        held.tick(grab);
    }
    check(held.turns == 3 && held.turned_ring == 2, "a quarter clockwise: three steps on the middle ring");
    grab.controls(0, 0, 0, 0, false, false, false, held.ring);
    grab.grab(1, 0.0f, true, 0, -0.5f, aspect);
    check(grab.grab_hand() == -1, "opened: let go");
    // A grip closed away from the rings, or a grip held from before, takes nothing.
    grab.controls(0, 0, 0, 0, false, false, false, held.ring);
    grab.grab(0, 1.0f, false, 0, 0, aspect);
    check(grab.grab_hand() == -1, "not over the panel: nothing");
    grab.controls(0, 0, 0, 0, false, false, false, held.ring);
    grab.grab(0, 1.0f, true, 0.537f, 0, aspect);
    check(grab.grab_hand() == -1, "a grip already closed when it came over: nothing");
}

// The Third person buttons (0.1.21, a tester: with Gamepad the wrench was X, on the left controller): each control one
// button, the player's choice; the menus keep the Xbox layout; the hints' icons follow.
void test_pad_layout() {
    // The launcher's "Like first person": Cross A, Circle the right trigger, Square the right grip, Triangle the right
    // click, R1 the left trigger, R2 B, L1 the left grip, L2 Y, Select X, R3 the left click, L3 none.
    std::array<int, 11> like_first{};
    const char* like[11] = {"a", "right_trigger", "right_grip", "right_stick", "left_trigger", "b", "left_grip", "y", "x", "left_stick", "none"};
    for (int i = 0; i < 11; ++i) {
        like_first[static_cast<size_t>(i)] = pad_control_of(like[i]);
    }
    check(like_first[10] == -1 && pad_control_of("bogus") == -1 && pad_control_of(nullptr) == -1, "none and the unknown: no control");
    int refused = -1;
    const PadLayout layout = pad_layout_from(like_first, &refused);
    check(refused == 0, "Like first person: every control its own");
    check(layout[kControlRightGrip] == kPadSquare && layout[kControlRightTrigger] == kPadCircle
              && layout[kControlLeftTrigger] == kPadR1 && layout[kControlB] == kPadR2 && layout[kControlX] == kPadSelect
              && layout[kControlLeftClick] == kPadR3,
          "Like first person's layout");

    // The Xbox controller's defaults, as the launcher writes them, are the layout kept to 0.1.20.
    std::array<int, 11> xbox{};
    const char* xbox_keys[11] = {"a", "b", "x", "y", "right_grip", "right_trigger", "left_grip", "left_trigger", "left_stick", "right_stick", "none"};
    for (int i = 0; i < 11; ++i) {
        xbox[static_cast<size_t>(i)] = pad_control_of(xbox_keys[i]);
    }
    check(pad_layout_from(xbox) == kXboxLayout, "the launcher's Xbox controller is kXboxLayout");

    // A control asked for twice (a hand-edited file): the first keeps it, the second is left without one.
    std::array<int, 11> twice = xbox;
    twice[1] = kControlA;  // Circle on A as well as Cross
    refused = 0;
    const PadLayout kept = pad_layout_from(twice, &refused);
    check(refused == 1 && kept[kControlA] == kPadCross && kept[kControlB] == 0, "a control twice: the first keeps it");

    PadContext context;
    context.gamepad = true;
    PadMapper pad;
    pad.layout = layout;
    Controls c;
    c.right_grip = 1;
    check(pad.map(c, context).held == kPadSquare, "Like first person: the right grip swings (Square)");
    c = Controls{};
    c.right_trigger = 1;
    check(pad.map(c, context).held == kPadCircle, "Like first person: the right trigger fires (Circle)");
    c = Controls{};
    c.x = true;
    check(pad.map(c, context).held == kPadSelect, "Like first person: X the map (Select)");
    c = Controls{};
    c.left_click = true;
    check(pad.map(c, context).held == kPadR3, "Like first person: the left click R3");
    c = Controls{};
    c.menu = true;
    check(pad.map(c, context).held == kPadStart, "the menu button Start whatever the layout");
    // Every control at once: each button once, eleven buttons less L3 (none), with Start.
    c = Controls{};
    c.a = c.b = c.x = c.y = c.left_click = c.right_click = true;
    c.left_trigger = c.right_trigger = c.left_grip = c.right_grip = 1;
    const std::uint16_t all = kPadCross | kPadCircle | kPadSquare | kPadTriangle | kPadR1 | kPadR2 | kPadL1 | kPadL2 | kPadSelect | kPadR3;
    check(pad.map(c, context).held == all, "all ten controls: ten buttons");
    // A stick as the D-pad: its click L3, whatever the layout gives the click.
    context.left_dpad = true;
    c = Controls{};
    c.left_click = true;
    check(pad.map(c, context).held == kPadL3, "the left stick the D-pad: its click L3, not the layout's");
    context.left_dpad = false;
    // Akimbo's lead never makes one trigger two buttons on the plain pad.
    context.lead = true;
    c = Controls{};
    c.left_trigger = 1;
    check(pad.map(c, context).held == kPadR1, "the lead: the left trigger its own button alone");
    context.lead = false;

    // In the menus the Xbox layout stays: X Square, the right grip R1.
    PadMapper menu;
    menu.layout = layout;
    context.in_menu = true;
    c = Controls{};
    c.x = true;
    check(menu.map(c, context).held == kPadSquare, "the menus: X Square still");
    c = Controls{};
    c.right_grip = 1;
    check(menu.map(c, context).held == kPadR1, "the menus: the right grip R1 still");
    context.in_menu = false;

    // The big screen takes the layout in play and the Xbox one in its menus.
    PadContext screen;
    screen.screen = true;
    PadMapper big;
    big.layout = layout;
    c = Controls{};
    c.right_grip = 1;
    check(big.map(c, screen).held == kPadSquare, "the big screen: the layout in play");
    screen.in_menu = true;
    check(big.map(c, screen).held == kPadR1, "the big screen's menus: the Xbox layout");

    // The hints' icons: Cross Circle Triangle Square L1 R1 L2 R2, each the control pressing it.
    constexpr std::array<std::uint16_t, 8> icons = {kPadCross, kPadCircle, kPadTriangle, kPadSquare, kPadL1, kPadR1, kPadL2, kPadR2};
    const std::array<int, 10> shown = glyphs::layout_glyph_controls(layout, icons);
    check(shown[0] == kControlA && shown[1] == kControlRightTrigger && shown[2] == kControlRightClick && shown[3] == kControlRightGrip
              && shown[4] == kControlLeftGrip && shown[5] == kControlLeftTrigger && shown[6] == kControlY && shown[7] == kControlB
              && shown[8] == -1 && shown[9] == -1,
          "the icons: each the control pressing its button");
    check(glyphs::kControlCells[static_cast<size_t>(kControlRightGrip)] == 0x15 && glyphs::kControlCells[static_cast<size_t>(kControlX)] == 0x13,
          "the Xbox mod's cells: RB the R1's, X the Square's");
    const std::array<int, 10> xbox_shown = glyphs::layout_glyph_controls(kXboxLayout, icons);
    for (int i = 0; i < 8; ++i) {
        // Under the Xbox layout each icon's control shows the icon's own cell: the same as the Xbox scheme.
        check(xbox_shown[static_cast<size_t>(i)] >= 0
                  && glyphs::kControlCells[static_cast<size_t>(xbox_shown[static_cast<size_t>(i)])] == 0x10 + i,
              "the Xbox layout: each icon its own cell");
    }
    std::array<int, 11> none_cross = xbox;
    none_cross[0] = -1;
    check(glyphs::layout_glyph_controls(pad_layout_from(none_cross), icons)[0] == -1, "Cross on no control: the icon as it is");
}

// Third person with Gamepad, the big screen's Xbox layout, the D-pad and the controller by the head (0.1.19).
void test_gamepad() {
    PadContext context;
    context.gamepad = true;
    Controls c;
    c.a = c.b = c.x = c.y = c.menu = c.left_click = c.right_click = true;
    c.left_y = 1;
    c.right_x = -1;
    PadMapper pad;
    VirtualPad out = pad.map(c, context);
    const std::uint16_t face = kPadCross | kPadCircle | kPadSquare | kPadTriangle | kPadStart | kPadSelect | kPadR3;
    check((out.held & face) == face, "gamepad: A B X Y, menu Start, the clicks Select and R3");
    check(!(out.held & (kPadL3 | kPadR2 | kPadUp)), "gamepad: nothing else");
    near_(out.left_y, 1, "gamepad: the left stick");
    near_(out.right_x, -1, "gamepad: the right stick, the game's camera");
    c = Controls{};
    c.right_grip = 1;
    check(pad.map(c, context).held == kPadR1, "gamepad: the right grip RB, R1");
    c = Controls{};
    c.left_grip = 1;
    check(pad.map(c, context).held == kPadL1, "gamepad: the left grip LB, L1");
    c = Controls{};
    c.left_trigger = 1;
    check(pad.map(c, context).held == kPadL2, "gamepad: the left trigger LT, L2");
    c = Controls{};
    c.right_trigger = 1;
    check(pad.map(c, context).held == kPadR2, "gamepad: the right trigger RT, R2");
    // RAC-Board takes the plain pad: its own button pressed for the reach behind the back comes through.
    context.jet_toggle = kPadUp;
    check(pad.map(Controls{}, context).held == kPadUp, "gamepad: the board's button");
    context.jet_toggle = 0;

    // The left stick as the D-pad: its four ways and the diagonals, none of its own push; its click L3, not Select.
    PadMapper dpad;
    context.left_dpad = true;
    c = Controls{};
    c.left_y = 1;
    c.left_click = true;
    out = dpad.map(c, context);
    check((out.held & (kPadUp | kPadL3)) == (kPadUp | kPadL3) && !(out.held & (kPadSelect | kPadLeft | kPadRight)), "D-pad up, L3");
    near_(out.left_y, 0, "the D-pad's stick moves nothing");
    c.left_click = false;
    c.left_x = 0.7f;
    c.left_y = -0.7f;
    out = dpad.map(c, context);
    check((out.held & (kPadRight | kPadDown)) == (kPadRight | kPadDown) && !(out.held & kPadUp), "D-pad diagonal");
    c.left_x = 0.95f;
    c.left_y = 0.2f;
    check((dpad.map(c, context).held & (kPadRight | kPadUp | kPadDown)) == kPadRight, "D-pad right alone near the axis");
    // Past 0.55 to press, under 0.4 to let go (of the stick past its dead zone, stretched).
    c.left_x = 0;
    c.left_y = 0;
    check(!(dpad.map(c, context).held & kPadUp), "D-pad: let go at the middle");
    c.left_y = 0.5f;
    check(!(dpad.map(c, context).held & kPadUp), "D-pad: half a push is no press");
    c.left_y = 0.9f;
    check(dpad.map(c, context).held & kPadUp, "D-pad: pressed");
    c.left_y = 0.55f;
    check(dpad.map(c, context).held & kPadUp, "D-pad: held past the let-go");
    c.left_y = 0.3f;
    check(!(dpad.map(c, context).held & kPadUp), "D-pad: let go");
    context.left_dpad = false;
    context.right_dpad = true;
    c = Controls{};
    c.right_x = -1;
    c.right_click = true;
    out = dpad.map(c, context);
    check((out.held & (kPadLeft | kPadR3)) == (kPadLeft | kPadR3), "the right stick as the D-pad");
    near_(out.right_x, 0, "the camera stays");
    context.right_dpad = false;

    // The big screen has the same layout (to 0.1.18 the triggers were L1 and R1).
    PadContext screen;
    screen.screen = true;
    c = Controls{};
    c.right_trigger = 1;
    c.left_grip = 1;
    check(PadMapper{}.map(c, screen).held == (kPadR2 | kPadL1), "the big screen: RT R2, LB L1");

    // In the menus: B Circle, X Square, Y Triangle, the menu button Start, as the icons say; A confirms.
    PadMapper menu;
    context.in_menu = true;
    c = Controls{};
    c.y = true;
    check(menu.map(c, context).held == kPadTriangle, "gamepad menu: Y Triangle, back");
    c = Controls{};
    c.b = true;
    check(menu.map(c, context).held == kPadCircle, "gamepad menu: B Circle");
    c = Controls{};
    c.x = true;
    check(menu.map(c, context).held == kPadSquare, "gamepad menu: X Square");
    c = Controls{};
    c.menu = true;
    check(menu.map(c, context).held == kPadStart, "gamepad menu: the menu button Start");
    c = Controls{};
    c.a = true;
    check(menu.map(c, context).held & kPadCross, "gamepad menu: A confirms");
    c = Controls{};
    c.left_grip = 1;
    check(menu.map(c, context).held == kPadL1, "gamepad menu: the left grip L1, the page");
    // First person's menus keep theirs: B back (Triangle).
    context.gamepad = false;
    c = Controls{};
    c.b = true;
    check(menu.map(c, context).held == kPadTriangle, "VR menu: B back");

    test_pad_layout();

    // The controller held up beside the head, as a phone at the ear, its own side.
    const Pose head{{0, 1.6f, 0}, yaw_quat(0)};
    check(hand_by_head(head, {-0.14f, 1.58f, 0.03f}, true, false), "the left controller at the left ear");
    check(!hand_by_head(head, {-0.14f, 1.58f, 0.03f}, false, false), "the right hand there is across: no");
    check(hand_by_head(head, {0.15f, 1.65f, -0.05f}, false, false), "the right controller at the right ear");
    check(!hand_by_head(head, {0.02f, 1.55f, -0.10f}, false, false), "at the mouth: no");
    check(!hand_by_head(head, {-0.2f, 1.25f, -0.3f}, true, false), "held out in play: no");
    check(!hand_by_head(head, {-0.15f, 1.6f, 0.25f}, true, false) && hand_behind_back(head, {-0.15f, 1.6f, 0.25f}, false),
          "behind the head is the board's place, not the D-pad's");
    check(!hand_by_head(head, {-0.14f, 1.6f, 0.10f}, true, false) && hand_by_head(head, {-0.14f, 1.6f, 0.10f}, true, true),
          "at the back edge: kept once in, not taken");
    // Turned with the head: the ear goes round with it.
    const Pose turned{{0, 1.6f, 0}, yaw_quat(1.5707964f)};
    const Vec3 ear = turned.position + rotate(turned.orientation, Vec3{-0.14f, -0.02f, 0.03f});
    check(hand_by_head(turned, ear, true, false), "the left ear, the head turned");
}

// The view's up on Magneboots and what is left when he comes off (rig.h's follow_up, give_up_turn_to_rig).
void test_surface_turn() {
    const auto towards = [](Vec3 a, Vec3 b, float t) {
        const Vec3 v = a * (1.0f - t) + b * t;
        return v * (1.0f / length(v));
    };
    const auto walk = [&](Quat turn, Vec3 from, Vec3 to) {
        for (int i = 1; i <= 60; ++i) {
            turn = follow_up(turn, towards(from, to, static_cast<float>(i) / 60.0f));
        }
        return turn;
    };
    const Vec3 up{0, 1, 0}, wall_x{1, 0, 0}, wall_z{0, 0, 1};
    // Up a ramp to a wall and straight back down: nothing left.
    Quat turn = walk(walk(Quat{}, up, wall_x), wall_x, up);
    float rig = 0.4f;
    near_(give_up_turn_to_rig(turn, rig), 0.0f, "up the wall and back the same way: no turn left", 1e-3f);
    near_(rig, 0.4f, "the rig as it was");
    // Up a wall, round its corner onto the next, and off: a quarter turn about up was left.
    turn = walk(walk(Quat{}, up, wall_x), wall_x, wall_z);
    turn = follow_up(turn, up);  // he falls off: up is the world's at once
    near_(length(rotate(turn, {0, 1, 0}) - up), 0.0f, "off: the view's up is the world's", 1e-3f);
    near_(std::fabs(angle_between(turn, Quat{})), kPi * 0.5f, "round the corner and off: a quarter turn left", 2e-3f);
    // The view before the rig takes it, and after: the same, and the head's yaw (the stick's) now the view's.
    Rig before;
    before.yaw = 0.4f;
    const Pose head{{0.1f, 1.6f, -0.2f}, yaw_quat(0.3f)};
    const Vec3 anchor{5, 2, 1};
    Pose shown = before.to_frame(head, anchor);
    shown.position = anchor + rotate(turn, shown.position - anchor);
    shown.orientation = turn * shown.orientation;
    Rig after = before;
    const float given = give_up_turn_to_rig(turn, after.yaw);
    near_(std::fabs(given), kPi * 0.5f, "the rig takes the quarter turn", 2e-3f);
    check(turn.x == 0.0f && turn.y == 0.0f && turn.z == 0.0f && turn.w == 1.0f, "the view's own turn is none after");
    const Pose now = after.to_frame(head, anchor);
    near_(length(now.position - shown.position), 0.0f, "the eyes where they were shown", 1e-4f);
    near_(angle_between(now.orientation, shown.orientation), 0.0f, "looking where they looked", 2e-3f);
    const Vec3 look = rotate(shown.orientation, {0, 0, -1});
    near_(wrap_angle(world_yaw(after.yaw, head.orientation) - (std::atan2(-look.x, -look.z) + kPi * 0.5f)), 0.0f,
          "the stick's yaw is where the view looks", 2e-3f);
    // A half turn the arcs cannot undo (off a ceiling): upright again.
    Quat flipped{1, 0, 0, 0};
    flipped = follow_up(flipped, up);
    float rig2 = 0.0f;
    give_up_turn_to_rig(flipped, rig2);
    check(flipped.w == 1.0f, "off a ceiling: upright");
    // Nothing to give in plain play.
    Quat none{};
    float rig3 = 1.0f;
    near_(give_up_turn_to_rig(none, rig3), 0.0f, "no surface: nothing");
    near_(rig3, 1.0f, "no surface: the rig untouched");
}

void test_body_heading() {
    const Pose head{{0, 1.6f, 0}, yaw_quat(0)};
    angle_near(body_target(head, nullptr, nullptr), 0.0f, "no hands: the head's heading");
    Pose left{{-0.25f, 1.0f, -0.25f}, yaw_quat(0)};
    Pose right{{0.25f, 1.0f, -0.25f}, yaw_quat(0)};
    angle_near(body_target(head, &left, &right), 0.0f, "squared up: straight ahead");

    // A glance with the body still: the shoulders and hands hold it.
    BodyHeading body;
    const Pose glance{{0, 1.6f, 0}, yaw_quat(0.6f)};
    float yaw = body.update(head, &left, &right, 0.02f);
    for (int i = 0; i < 100; ++i) {
        yaw = body.update(glance, &left, &right, 0.02f);
    }
    check(std::abs(yaw) < 0.2f, "a glance does not turn the body");

    // The whole body turned: it follows.
    const Quat turn = yaw_quat(1.0f);
    const Pose head2{{0, 1.6f, 0}, turn};
    const Pose left2{rotate(turn, left.position), turn};
    const Pose right2{rotate(turn, right.position), turn};
    for (int i = 0; i < 200; ++i) {
        yaw = body.update(head2, &left2, &right2, 0.02f);
    }
    check(std::abs(wrap_angle(yaw - 1.0f)) < 0.08f, "a turned body is followed");

    // The head is never more than a quarter turn from it.
    BodyHeading neck;
    neck.update(head, nullptr, nullptr, 0.02f);
    const Pose behind{{0, 1.6f, 0}, yaw_quat(2.8f)};
    const float kept = neck.update(behind, &left, &right, 0.02f);
    check(std::abs(wrap_angle(2.8f - kept)) <= kPi * 0.5f + 1e-3f, "the neck turns a quarter at most");
}

void test_turret() {
    // Hoven's speeds on either disc's tick: it reaches the aim, never past it, and stays.
    for (const float tick : {0.02f, 1.0f / 60.0f}) {
        float at = 0, v = 0;
        const float aim = 1.2f;
        float most = 0;
        for (int i = 0; i < 400; ++i) {
            turret_yaw_speed(v, aim - at, tick);
            at += v;
            most = std::max(most, std::abs(v));
            check(at <= aim + 1e-4f, "a turret does not overshoot");
        }
        near_(at, aim, "a turret reaches the aim", 1e-3f);
        near_(v, 0.0f, "and stops", 1e-3f);
        check(most <= kTurretYawSpeed * kDegree * tick + 1e-5f, "a turret keeps to Hoven's speed");
        float pitch = 0, pv = 0;
        for (int i = 0; i < 400; ++i) {
            turret_pitch_speed(pv, -0.4f - pitch, tick);
            pitch += pv;
        }
        near_(pitch, -0.4f, "the barrels reach the aim", 1e-3f);
    }
    float v = 0;
    aim_speed(v, 0.0f, 0.05f, 0.01f, 0.02f);
    near_(v, 0.0f, "no error, no turn");

    // Gaspar's and Hoven's models look along +Y, Batalia's along +X.
    angle_near(turret_heading(TurretKind::kHoven, 0.0f), kPi * 0.5f, "Hoven's heading");
    angle_near(turret_heading(TurretKind::kGaspar, kPi * 0.5f), kPi, "Gaspar's heading");
    angle_near(turret_heading(TurretKind::kBatalia, 0.3f), 0.3f, "Batalia's heading");
    const Vec3 hoven = turret_seat(TurretKind::kHoven, {1, 2, 3}, 0.0f);
    near_(hoven.y, 2.45f, "Hoven's seat ahead");
    near_(hoven.z, 6.0f, "Hoven's seat up");
    const Vec3 batalia = turret_seat(TurretKind::kBatalia, {1, 2, 3}, kPi * 0.5f);
    near_(batalia.x, 1.0f, "Batalia's seat turned: x");
    near_(batalia.y, 6.074f, "Batalia's seat turned: y");
    near_(batalia.z, 9.5f, "Batalia's seat up");

    // The HUD along the aim.
    const Vec3 forward[] = {{0, 0, -1}, {1, 0, 0}, {0.6f, 0.0f, 0.8f}, {0, 0.6f, -0.8f}, {-0.48f, 0.6f, 0.64f}};
    for (const Vec3 f : forward) {
        const Quat q = looking_along(f);
        const Vec3 got = rotate(q, {0, 0, -1});
        near_(got.x, f.x, "looking along: x");
        near_(got.y, f.y, "looking along: y");
        near_(got.z, f.z, "looking along: z");
        near_(rotate(q, {1, 0, 0}).y, 0.0f, "the panel stays level");
        check(rotate(q, {0, 1, 0}).y > 0.0f, "and upright");
    }

    // On a turret: the trigger and X Circle, B Square, the grip nothing, the sticks as they are.
    PadMapper mapper;
    PadContext context;
    context.mounted = true;
    Controls c;
    c.right_grip = 1;
    check((mapper.map(c, context).held & kPadSquare) == 0, "a turret has no wrench on the grip");
    c.right_grip = 0;
    c.right_trigger = 1;
    check((mapper.map(c, context).held & kPadCircle) != 0, "the trigger fires a turret");
    c.right_trigger = 0;
    c.b = true;
    check((mapper.map(c, context).held & kPadSquare) != 0, "B is Square on a turret");
    c.b = false;
    c.left_x = 1;
    c.right_y = 1;
    const VirtualPad steering = mapper.map(c, context);
    near_(steering.left_x, 1.0f, "the left stick steers a turret, not turned by the head");
    near_(steering.right_y, 0.0f, "the right stick is nothing on a turret");
    c = {};
    c.right_click = true;
    check((mapper.map(c, context).held & kPadTriangle) != 0, "the stick's click leaves it");
    c = {};
    context.flying = true;
    c.right_trigger = 1;
    check((mapper.map(c, context).held & kPadSquare) != 0, "a flown ship's lasers stay on the trigger");
}

// The pause menu's other widgets as the pointer reads them (pointer.h), and the trigger on a keyed line.
void test_menu_widgets() {
    MenuList planets;
    planets.width = 160;
    planets.height = 200;
    planets.rows = 4;
    for (int i = 0; i < 4; ++i) {
        planets.second_line[i] = true;
    }
    check(planet_row_at(planets, 50, 40) == 0, "the first planet");
    check(planet_row_at(planets, 50, 100) == 1, "the second, below its planet's line");
    check(planet_row_at(planets, 50, 20) == -1, "above the planets");
    check(planet_row_at(planets, 1, 40) == -1, "left of them");

    int row = -1, value = -1;
    check(volume_at(300, 160, 159, 40, row, value) && row == 0 && value == 0, "the effects bar's left end");
    check(volume_at(300, 160, 197, 40, row, value) && row == 0 && value == 512, "its middle");
    check(volume_at(300, 160, 237, 40, row, value) && value == 1024, "just past its right end, full");
    check(volume_at(300, 160, 197, 80, row, value) && row == 1 && value == 512, "the music bar");
    check(!volume_at(300, 160, 197, 60, row, value), "between the bars");
    check(!volume_at(300, 160, 60, 40, row, value), "on the row's name");

    check(quit_line_at(200, 100, 100, 68) == 1, "the quit box's upper line");
    check(quit_line_at(200, 100, 100, 88) == 2, "its lower line");
    check(quit_line_at(200, 100, 100, 50) == 0, "its text above");
    check(quit_line_at(200, 100, 2, 68) == 0, "left of its lines");

    check(save_slot_at(true, 420, 10) == 0, "the first slot");
    check(save_slot_at(true, 420, 60) == -1, "between slots");
    check(save_slot_at(true, 420, 90) == 1, "the second slot, PAL");
    check(save_slot_at(false, 420, 80) == 1, "the second slot, NTSC-U");

    check(icon_at(64, 3, 0x100, 0x260, 32, 20) == 0, "the first icon");
    check(icon_at(64, 3, 0x100, 0x260, 32, 60) == 1, "the second icon");
    check(icon_at(64, 3, 0x100, 0x260, 5, 20) == -1, "beside the icons");
    check(icon_at(64, 8, 0x100, 0x252, 32, 16.0f + 0x252 * 7 / 16.0f + 1.0f) == 7, "the last icon on a PAL disc's step");
    check(icon_at(64, 8, 0x100, 0x260, 32, 16.0f + 0x252 * 7 / 16.0f + 1.0f) == -1, "which the US disc's step would miss");

    PadMapper mapper;
    PadContext context;
    context.in_menu = true;
    context.menu_key = kPadTriangle;
    Controls c;
    c.right_trigger = 1.0f;
    const std::uint16_t held = mapper.map(c, context).held;
    check((held & kPadTriangle) && !(held & kPadCross) && !(held & kPadCircle), "the trigger on a keyed line presses its key alone");
    c = {};
    c.a = true;
    const std::uint16_t a_held = PadMapper{}.map(c, context).held;
    check((a_held & kPadTriangle) && !(a_held & kPadCross), "A on a keyed line answers as the trigger does: its key alone");
    context.menu_key = 0;
    check((PadMapper{}.map(c, context).held & kPadCross) != 0, "A off the labels confirms with Cross");
    c = {};
    c.left_y = 1.0f;
    check((PadMapper{}.map(c, context).held & kPadUp) != 0, "the stick moves the cursor with the hand off the menu");
    context.menu_pointing = true;
    check((PadMapper{}.map(c, context).held & (kPadUp | kPadDown | kPadLeft | kPadRight)) == 0,
          "with the beam on the menu the sticks leave the cursor alone");
    check(PadMapper{}.map(c, context).left_y == 0.0f, "and the analog stick too, which the game's lists also read");
    {
        // Third person: forward on the stick is where the head looks, as rerac-vr-3p's.
        PadContext third;
        third.third_person_walk = true;
        third.head_turn = 1.5707964f;
        Controls forward;
        forward.left_y = 1.0f;
        const VirtualPad walked = PadMapper{}.map(forward, third);
        check(std::fabs(walked.left_x + 1.0f) < 0.01f && std::fabs(walked.left_y) < 0.01f, "the head a quarter turn left: forward is the stick's left");
    }
    {
        // The board turns about its own heading, wherever the game's camera is (Rerac-vr's).
        PadContext board;
        board.first_person = true;
        board.board = true;
        board.board_steers = true;
        Controls stick;
        stick.left_x = 1.0f;
        board.board_yaw = 1.0f;
        board.camera_yaw = 1.0f;
        VirtualPad steered = PadMapper{}.map(stick, board);
        check(std::fabs(steered.left_x - 1.0f) < 0.01f && std::fabs(steered.left_y) < 0.01f, "the board along the camera: the stick as it is");
        board.camera_yaw = 1.0f - 1.5707964f;
        steered = PadMapper{}.map(stick, board);
        check(std::fabs(steered.left_x) < 0.01f && std::fabs(steered.left_y - 1.0f) < 0.01f, "the board a quarter turn off it: still its own right");
        stick.left_x = 0.0f;
        stick.left_y = 1.0f;
        steered = PadMapper{}.map(stick, board);
        check(steered.left_x == 0.0f && steered.left_y == 0.0f, "forward on the stick does not steer the board");
    }

    // The strip's cells as drawn: two items from 200, four from 146 (the shot's third at 258), and ten as a
    // carousel with the selected one's centre at 257.
    check(vendor_strip_at(2, 0, 0, 230.0f) == 0, "two items: the first");
    check(vendor_strip_at(2, 0, 0, 280.0f) == 1, "two items: the second");
    check(vendor_strip_at(2, 0, 0, 180.0f) == -1 && vendor_strip_at(2, 0, 0, 330.0f) == -1, "beside a short strip");
    check(vendor_strip_at(4, 3, 0, 285.0f) == 2, "four items: the third, wherever the selection is");
    check(vendor_strip_at(10, 5, 0, 257.0f) == 5, "the carousel's middle is the selected one");
    check(vendor_strip_at(10, 5, 0, 313.0f) == 6 && vendor_strip_at(10, 5, 0, 201.0f) == 4, "the cells beside it");
    check(vendor_strip_at(10, 5, 0, 89.0f) == 2, "three cells left of it");
    check(vendor_strip_at(10, 0, 0, 201.0f) == 9, "the carousel wraps");
    check(vendor_strip_at(0, 0, 0, 256.0f) == -1, "an empty strip");
    check(vendor_strip_step(4, 1, 3) == 1 && vendor_strip_step(4, 3, 0) == -1 && vendor_strip_step(4, 2, 2) == 0, "a short strip's steps");
    check(vendor_strip_step(10, 9, 1) == 1 && vendor_strip_step(10, 1, 9) == -1, "the carousel goes the short way");

    // Giant Clank's sweep: three spheres about the strike's way, whichever way he punches.
    const Vec3 sweep_at{10.0f, 20.0f, 5.0f};
    const Vec3 middle = giant_sweep_point(sweep_at, {0.0f, 1.0f, 0.0f}, 1);
    check(length(middle - Vec3{10.0f, 21.8f, 6.4f}) < 1e-3f, "the sweep's middle sphere: 1.8 along the strike, 1.4 up");
    const Vec3 first = giant_sweep_point(sweep_at, {0.0f, 1.0f, 0.0f}, 0), last = giant_sweep_point(sweep_at, {0.0f, 1.0f, 0.0f}, 2);
    check(std::abs(first.y - last.y) < 1e-3f && std::abs((first.x - 10.0f) + (last.x - 10.0f)) < 1e-3f && std::abs(first.x - 10.0f) > 1.2f,
          "the sweep's sides, an eighth of a turn either way");

    // from_frame undoes to_frame, as the rings' panel needs.
    Rig turned;
    turned.yaw = 0.7f;
    turned.scale = 1.5f;
    turned.origin = {0.2f, 1.6f, -0.1f};
    const Vec3 anchor_at{3.0f, 2.0f, -4.0f}, tracked{0.5f, 1.2f, 0.4f};
    const Vec3 round_trip = turned.from_frame(turned.to_frame({tracked, {}}, anchor_at).position, anchor_at);
    check(length(round_trip - tracked) < 1e-4f, "a frame position back to tracking");

    // A ray on a monitor: the middle, a corner, past its edge, and a monitor behind the ray.
    {
        const Vec3 corner{1.0f, 5.0f, 2.0f}, across{2.0f, 0.0f, 0.0f}, down{0.0f, 0.0f, -1.0f};
        float far = 0, u = 0, v = 0;
        check(ray_on_quad({2.0f, 1.0f, 1.5f}, {0.0f, 1.0f, 0.0f}, corner, across, down, far, u, v), "the ray meets the screen");
        check(std::abs(far - 4.0f) < 1e-3f && std::abs(u - 0.5f) < 1e-3f && std::abs(v - 0.5f) < 1e-3f, "in its middle, 4 away");
        check(ray_on_quad({1.5f, 1.0f, 1.75f}, {0.0f, 1.0f, 0.0f}, corner, across, down, far, u, v) && std::abs(u - 0.25f) < 1e-3f
                  && std::abs(v - 0.25f) < 1e-3f, "a quarter in each way");
        check(!ray_on_quad({0.5f, 1.0f, 1.5f}, {0.0f, 1.0f, 0.0f}, corner, across, down, far, u, v), "past its edge");
        check(!ray_on_quad({2.0f, 9.0f, 1.5f}, {0.0f, 1.0f, 0.0f}, corner, across, down, far, u, v), "behind the ray");
    }

    // At the vendor on foot the trigger and the click do nothing in the hero's pad, and that pull stays spent.
    {
        PadMapper shop;
        PadContext walking;
        walking.shopping = true;
        walking.first_person = true;
        Controls pulled;
        pulled.right_trigger = 1.0f;
        pulled.right_click = true;
        const std::uint16_t held = shop.map(pulled, walking).held;
        check(!(held & kPadCircle) && !(held & kPadTriangle), "the shop's trigger and click are not the hero's");
        walking.shopping = false;
        pulled.right_click = false;
        check(!(shop.map(pulled, walking).held & kPadCircle), "the pull that was the shop's fires nothing after it");
        pulled.right_trigger = 0.0f;
        shop.map(pulled, walking);
        pulled.right_trigger = 1.0f;
        check((shop.map(pulled, walking).held & kPadCircle) != 0, "a fresh pull fires");
    }

    // The vendor: the trigger answers only what the hand is on; A is Cross everywhere, as the screen says.
    PadContext aimed;
    aimed.in_menu = true;
    aimed.menu_aimed = true;
    c = {};
    c.right_trigger = 1.0f;
    const std::uint16_t off_boxes = PadMapper{}.map(c, aimed).held;
    check(!(off_boxes & kPadCross) && !(off_boxes & kPadCircle) && !(off_boxes & kPadTriangle), "the trigger off the vendor's boxes presses nothing");
    aimed.menu_key = kPadTriangle;
    const std::uint16_t on_exit = PadMapper{}.map(c, aimed).held;
    check((on_exit & kPadTriangle) && !(on_exit & kPadCross), "the trigger on Exit is Triangle alone: Exit does not buy");
    aimed.menu_key = 0;
    c = {};
    c.a = true;
    check((PadMapper{}.map(c, aimed).held & kPadCross) != 0, "A buys");
    c = {};
    aimed.menu_pulse = kPadRight;
    check((PadMapper{}.map(c, aimed).held & kPadRight) != 0, "the strip's step is pressed with no hand on a key");
}

int main() {
    test_menu_widgets();
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
    test_swing();
    test_mounts();
    test_notice();
    test_dead_zone();
    test_turn_control();
    test_support();
    test_glyphs();
    test_disguise();
    test_pointer();
    test_beam();
    test_window_row();
    test_widgets();
    test_turret();
    test_ledge_pad();
    test_dive_pad();
    test_jet_board();
    test_gamepad();
    test_puzzle_input();
    test_surface_turn();
    test_body_heading();
    if (g_failures == 0) {
        std::printf("all passed\n");
    }
    return g_failures == 0 ? 0 : 1;
}
