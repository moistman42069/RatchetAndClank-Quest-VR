// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 the Ratchet & Clank Quest VR contributors
//
// What is held in the hand, put where the hand is: Rerac-vr's mounts (npiriou, ISC; embodied.rs
// place, blaster.rs place and grip, bomb_glove.rs mount), the same game models on another engine.
// Plain math, apart from the game and OpenXR (tests/rig_pad_test.cpp).
//
// A hand is OpenXR's grip pose: `forward` along the fist, `up` out of the thumb; or, for what points
// (held_pointing), its aim pose turned by the player's Gun angle, at the grip's place: on the Quest's
// controllers the grip's forward runs up the handle, well above where the controller points. An
// item's moby has a matrix whose rows are where its model's X, Y and Z point, and a position:
//
// - guns and gadgets: their model has X out of the top, -Y out of the barrel, and is put with the
//   point Rerac-vr measured on each model's handle (kGrips) in the palm;
// - gloves: their first joint list ends at the palm, which is put in the hand;
// - the wrench: that joint's frame has the shaft along its Z, so the frame is turned a quarter
//   about the hand's side to lay the shaft along the fist, and it is 20% bigger, as on PC;
// - anything else: its first joint list ends at the muzzle, put ahead of the hand.

#pragma once

#include "rig.h"

namespace quest {

// Three columns.
struct Mat3 {
    Vec3 x, y, z;
};

inline Vec3 operator*(const Mat3& m, Vec3 v) { return m.x * v.x + m.y * v.y + m.z * v.z; }
inline Mat3 operator*(const Mat3& a, const Mat3& b) { return {a * b.x, a * b.y, a * b.z}; }
inline float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Mat3 transpose(const Mat3& m) {
    return {{m.x.x, m.y.x, m.z.x}, {m.x.y, m.y.y, m.z.y}, {m.x.z, m.y.z, m.z.z}};
}

// A hand in the game's world.
struct HandFrame {
    Vec3 at, forward, left, up;
};

// The hand `pose` (OpenXR's grip, in the frame of rig.h) in the world.
inline HandFrame hand_in_world(const Pose& pose) {
    return {frame_to_world(pose.position), frame_to_world(rotate(pose.orientation, {0, 0, -1})),
            frame_to_world(rotate(pose.orientation, {-1, 0, 0})),
            frame_to_world(rotate(pose.orientation, {0, 1, 0}))};
}

// The first joint list's last joint in its model, from the class's inverse bind matrix `s`: its
// columns are the joint's rows, and its last row the place, in the joint's axes and negated.
struct JointFrame {
    bool valid = false;
    Mat3 basis;   // columns: the joint's X, Y and Z in the model
    Vec3 anchor;  // the joint in the model, in the model's own units
};

inline JointFrame joint_frame(const float s[16]) {
    JointFrame out;
    out.valid = true;
    out.basis = {{s[0], s[4], s[8]}, {s[1], s[5], s[9]}, {s[2], s[6], s[10]}};
    out.anchor = (out.basis * Vec3{s[12], s[13], s[14]}) * -1.0f;
    return out;
}

// Rerac-vr's blaster.rs grip: the handle's place on each model, in game units.
struct GunGrip {
    int oclass;
    Vec3 grip;
};

inline constexpr GunGrip kGrips[] = {
    {157, {0.0f, -0.04f, 0.0f}},       // Devastator
    {163, {0.0f, -0.18f, 0.0f}},       // Visibomb
    {168, {-0.065f, -0.13f, 0.0f}},    // Blaster
    {175, {-0.035f, -0.22f, 0.0f}},    // Taunter
    {176, {-0.045f, -0.14f, 0.0f}},    // Pyrocitor
    {177, {-0.115f, -0.17f, 0.0f}},    // Tesla Claw
    {180, {0.0f, -0.16f, 0.0f}},       // Walloper
    {185, {-0.07f, -0.22f, -0.04f}},   // Morph-o-Ray
    {188, {-0.015f, -0.145f, 0.0f}},   // Trespasser
    {208, {0.0f, -0.075f, 0.0f}},      // Swingshot
    {454, {-0.08f, -0.15f, 0.0f}},     // RYNO
    {483, {-0.06f, -0.22f, -0.05f}},   // Hologuise, Drone Device remote
    {585, {0.0f, -0.14f, -0.03f}},     // Metal Detector
    {619, {0.02f, -0.17f, -0.045f}},   // PDA
    {849, {0.0f, -0.14f, 0.0f}},       // Suck Cannon
    {1251, {0.0f, -0.15f, 0.0f}},      // Hydrodisplacer
};

inline constexpr int kWrenchClass = 71;
inline constexpr int kSwingshotClass = 208;
inline constexpr int kPyrocitorClass = 176;
inline constexpr int kWalloperClass = 180;
inline constexpr float kWrenchScale = 1.2f;

inline const GunGrip* gun_grip(int oclass) {
    for (const GunGrip& g : kGrips) {
        if (g.oclass == oclass) {
            return &g;
        }
    }
    return nullptr;
}

// The items a throwing glove holds (the hero's items 10, 17, 20 and 25).
inline bool glove_item(int item) { return item == 10 || item == 17 || item == 20 || item == 25; }

// What is held as a gun, its barrel where the controller points, and drawn at the Gun size: not the
// wrench (swung as the fist holds it), the gloves (on the palm, at the Hand size), the Swingshot's
// housing on the wrist, or the upright remotes.
inline bool held_pointing(int oclass, int item) {
    return oclass != kWrenchClass && oclass != kSwingshotClass && oclass != 483 && oclass != 619 && !glove_item(item);
}

// Where a moby goes.
struct Mount {
    Mat3 axes;       // columns: the rows of its matrix
    Vec3 position;
    float scale = 1; // times its class's scale
    float size = 1;  // the Gun or Hand size: drawn so (put_mount), its place and muzzle already sized
    Vec3 muzzle;     // where it fires from, Rerac-vr's blaster.rs place: where the hand aims from
};

// Rerac-vr's own muzzles where the first joint list is not one: the Visibomb's ends on its sight,
// the Trespasser has none (its prong tip).
inline bool fixed_muzzle(int oclass, Vec3& out) {
    if (oclass == 163) {
        out = {0.04f, -0.82f, 0.0f};
        return true;
    }
    if (oclass == 188) {
        out = {0.04f, -1.0f, -0.025f};
        return true;
    }
    return false;
}

// The models made for the right hand (Rerac-vr's akimbo.rs right_handed): the gloves, the Walloper's
// fist and the Swingshot's wrist housing. In the left hand they are mirrored, not held by a mirrored grip.
inline bool right_handed(int oclass, int item) {
    return glove_item(item) || oclass == kWalloperClass || oclass == kSwingshotClass;
}

inline Vec3 reflect(Vec3 v, Vec3 normal) { return v - normal * (2.0f * dot(v, normal)); }

// `m`, placed for the right hand at `hand`, turned into its mirror image across the hand's side plane
// (Rerac-vr's akimbo.rs mirror): the place and every axis reflected about the hand.
inline Mount mirrored(const Mount& m, const HandFrame& hand) {
    const float size = std::sqrt(dot(hand.left, hand.left));
    if (size < 1e-6f) {
        return m;
    }
    const Vec3 normal = hand.left * (1.0f / size);
    Mount out = m;
    out.axes = {reflect(m.axes.x, normal), reflect(m.axes.y, normal), reflect(m.axes.z, normal)};
    out.position = hand.at + reflect(m.position - hand.at, normal);
    out.muzzle = hand.at + reflect(m.muzzle - hand.at, normal);
    return out;
}

inline Mount mount_right(const HandFrame& hand, int oclass, int item, float scale, const JointFrame& joint, bool left,
                         float size);

// A moby of `oclass` holding hand item `item`, in `hand`. `scale`: the moby's own; `joint`: its
// first joint list's end. `left`: in the left hand, a model made for the right hand placed as the
// right's and then mirrored, and so a gun with `mirror_guns` (its handle, a side grip or a tank on the
// other side then, as a left-handed gun has them; else only its grip is mirrored, as Rerac-vr holds
// it). What points (held_pointing) is drawn `gun_size` of the game's size and a glove, which is the
// hand, `hand_size`, each shrunk about the hand. With `follow_hands` (Items match hand size) what
// points is drawn at both sizes and the rest, the wrench among it, at the Hand size; without it the
// rest keeps the game's.
inline Mount mount_item(const HandFrame& hand, int oclass, int item, float scale, const JointFrame& joint, bool left = false,
                        float gun_size = 1.0f, float hand_size = 1.0f, bool mirror_guns = false, bool follow_hands = false) {
    const float size = glove_item(item)               ? hand_size
                       : held_pointing(oclass, item) ? gun_size * (follow_hands ? hand_size : 1.0f)
                       : follow_hands                ? hand_size
                                                     : 1.0f;
    if (left && (right_handed(oclass, item) || (mirror_guns && held_pointing(oclass, item)))) {
        return mirrored(mount_right(hand, oclass, item, scale, joint, false, size), hand);
    }
    return mount_right(hand, oclass, item, scale, joint, left, size);
}

inline Mount mount_right(const HandFrame& hand, int oclass, int item, float scale, const JointFrame& joint, bool left,
                         float size) {
    Mount out;
    out.size = size;
    HandFrame h = hand;

    // The Swingshot's housing goes on the back of the wrist: a quarter about the fist, from his right side up (Rerac-vr's
    // blaster.rs, the hand turned +90 degrees about its Z). This was a quarter the other way until 0.1.12, which put the
    // housing under the fist and the whole thing upside down (a tester's "facing the wrong way").
    if (oclass == kSwingshotClass) {
        h.up = hand.left;
        h.left = hand.up * -1.0f;
    }
    const Vec3 right = h.left * -1.0f;
    const Mat3 gun{h.up, h.forward * -1.0f, right};

    if (oclass == kWrenchClass) {
        // The joint's Z is the shaft: Rerac-vr's desired axes are the hand turned a quarter about
        // its side (its "forward" the hand's down, its "up" the hand's forward).
        const Mat3 desired{h.up * -1.0f, h.left, h.forward};
        out.scale = kWrenchScale;
        out.axes = joint.valid ? desired * transpose(joint.basis) : desired;
        out.position = h.at;
        if (joint.valid) {
            out.position = h.at - out.axes * (joint.anchor * (scale * kWrenchScale * size / 1024.0f));
        }
        out.muzzle = h.at;
        return out;
    }

    if (const GunGrip* grip = gun_grip(oclass)) {
        // The upright remotes are wide along Y, their -Z face looking back at the holder.
        out.axes = (oclass == 483 || oclass == 619) ? Mat3{h.up, h.left * -1.0f, h.forward} : gun;
        // In the left hand the grip is mirrored across the gun's side.
        const Vec3 held = left ? Vec3{grip->grip.x, grip->grip.y, -grip->grip.z} : grip->grip;
        out.position = h.at - out.axes * (held * size);
        // The muzzle: the first joint list's end on the model (its bind pose here, where Rerac-vr
        // poses it), or Rerac-vr's own point; the remotes fire nothing and keep their grip.
        Vec3 point = grip->grip;
        if (!fixed_muzzle(oclass, point) && oclass != 483 && oclass != 619 && joint.valid) {
            point = joint.anchor * (scale / 1024.0f);
        }
        out.muzzle = out.position + out.axes * (point * size);
        return out;
    }

    out.axes = joint.valid ? gun * transpose(joint.basis) : gun;
    const Vec3 anchor = joint.valid ? joint.anchor * (scale * size / 1024.0f) : Vec3{};
    if (glove_item(item)) {
        out.position = h.at - out.axes * anchor;
        out.muzzle = h.at;
        return out;
    }

    // Unmeasured guns: the muzzle ahead of the hand, as far as Rerac-vr reaches for each.
    const float reach = oclass == kPyrocitorClass                                     ? 0.65f
                        : (oclass == 157 || oclass == 163 || oclass == 454 || oclass == 849) ? 0.70f
                                                                                          : 0.40f;
    out.muzzle = h.at + (h.forward * reach + h.up * 0.03f) * size;
    out.position = out.muzzle - out.axes * anchor;
    return out;
}

// Giant Clank's punch does its real damage with a sweep of the ground ahead (Rerac-vr's giant.rs sweep_toward:
// the walk's own spheres, 1.8 out and 1.4 up, an eighth of a turn apart), turned to the strike's flat `way`.
constexpr int kGiantSweepPoints = 3;
inline Vec3 giant_sweep_point(Vec3 hero, Vec3 way, int index) {
    constexpr float kOut = 1.8f, kUp = 1.4f, kApart = 0.7853982f;
    const float yaw = std::atan2(way.y, way.x) + static_cast<float>(index - 1) * kApart;
    return hero + Vec3{std::cos(yaw) * kOut, std::sin(yaw) * kOut, kUp};
}

}  // namespace quest
