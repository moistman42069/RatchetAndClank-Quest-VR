// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 the Ratchet & Clank Quest VR contributors
//
// Ratchet & Clank in a headset, as a native mod of OpenRAC's port. The port's views (openrac_mod.h's
// on_views, sdk/docs/views.md) draw the eyes and the 2D; this mod owns the headset (xr.h), turns its
// poses into views, and turns the controllers into the game's pad (pad.h).
//
// What it shows, by what the game is doing:
//
// - in play: the world from Ratchet's eyes (first person) or from the game's camera (third person),
//   with the HUD on a panel that follows the head;
// - a scene (a cutscene, the ship landing): stereo from the game's camera, the head looking around,
//   or, with the cutscenes setting "screen", the game's picture of it on the big panel;
// - a menu, the title, loading, a movie: the game's picture on a big panel ahead.
//
// The walking and facing are lock-strafe's (OpenRAC/rac1-mods, GPL-3.0-or-later): strafe.h is its
// decision unchanged, and the hooks below are its, with the camera's yaw replaced by the yaw the
// player aims along. The controls' layout and the eye height are Rerac-vr's (npiriou, ISC).

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include <openrac/games/rac1.hpp>
#include <openrac/mod.hpp>

#include "discs.h"
#include "pad.h"
#include "rig.h"
#include "strafe.h"
#include "view_abi.h"
#include "xr.h"

using openrac::gaddr;

#if !defined(__ANDROID__)
namespace quest {
extern void* (*sim_gl_proc)(const char*);
}
#endif

namespace {

using namespace quest;

// In the hero (seen in game code; lock-strafe's offsets).
constexpr uint32_t kHeroMoveYaw = 0x180u;
constexpr uint32_t kHeroFacing = 0x98u;
constexpr uint32_t kHeroState = 0x2084u;
constexpr uint32_t kHeroFrameMode = 0x20B3u;
constexpr uint32_t kHeroVehicleMode = 0x20A4u;
constexpr uint32_t kHeroSlot0 = 0x1090u;
constexpr uint32_t kSlotState = 0x24u;
constexpr uint32_t kSlotItem = 0x28u;

// In a moby (seen in game code; level_hero_attach.c's HeroAttachMoby).
constexpr uint32_t kMobyPosition = 0x10u;
constexpr uint32_t kMobyState = 0x30u;
constexpr uint32_t kMobyBasis = 0xC0u;
constexpr uint32_t kMobyRowSize = 0x10u;

// The game mode in play, and the level whose program holds the mode and the camera's words.
constexpr int32_t kModePlaying = 0;
constexpr int kMainLevel = 0;

// Hero_TurnToHeading's first argument for the turns that follow the move yaw.
constexpr int32_t kTurnFollowsMoveYaw = 0;
// The standing turn's rates, in the game's terms (lock-strafe: state 0x81's).
constexpr float kStandingAccel = 0.15f;
constexpr float kStandingDamping = 0.2f;
constexpr float kStandingLimit = 2.0f;

// Ratchet's eyes above his feet, in game units, a game unit being about a metre (Rerac-vr's: his
// own eye height, not the game's first-person camera's 1.6).
constexpr float kEyeHeight = 1.1f;
// How fast the view's room offset closes on the body while the stick moves him, metres a second.
constexpr float kCatchUp = 1.2f;

// The HUD's panel, ahead of the head: the game's 4:3 picture, metres.
constexpr float kHudDistance = 1.6f;
// The menus' panel.
constexpr float kPanelDistance = 2.4f;
constexpr float kPanelWidth = 3.0f;

// The quick select: how far the hand reaches for the full stick, metres.
constexpr float kPickReach = 0.18f;

// What the settings ask for.
struct Options {
    bool first_person = true;
    TurnSettings turn;
    float world_scale = 1.0f;
    float hud_scale = 1.0f;
    bool haptics = true;
    bool hand_aim = true;
    bool hide_body = true;
    bool hand_weapon = true;
    bool scenes_in_vr = true;  // cutscenes around the player; false: on the panel, as the game frames them
};

// What the mod holds. All of it is on the game's thread.
openrac::Mod* s_mod = nullptr;
const quest_disc* s_disc = nullptr;
std::unique_ptr<Headset> s_headset;
Options s_options;
XrSettings s_xr;
bool s_started = false;
bool s_failed = false;
std::string s_status_file;

Rig s_rig;
FrameInfo s_frame;
bool s_have_frame = false;
bool s_frame_open = false;
bool s_recentre_wanted = true;
float s_recentre_yaw = 0;
PadMapper s_pad_mapper;
TurnControl s_turn;
Pose s_panel_pose;
bool s_panel_placed = false;
int s_last_phase = -1;

// Two ticks' hero positions: the scene is drawn between them.
Vec3 s_hero_before, s_hero_after;
bool s_have_hero = false;
float s_facing = 0;

// What each frame showed, for its end.
bool s_eyes_shown = false;
bool s_panel_shown = false;
bool s_panel_is_hud = false;

// The turn and walk read from the frame, for the pad.
float s_aim_yaw = 0;
float s_head_yaw = 0;
bool s_steers = false;
bool s_menu_input = true;
bool s_quick_select = false;
Vec3 s_pick_from;
float s_pick_yaw = 0;

void log_line(int level, const char* text) {
    if (s_mod == nullptr) {
        return;
    }
    if (level >= 3) {
        s_mod->error(text);
    } else if (level == 2) {
        s_mod->warn(text);
    } else {
        s_mod->info(text);
    }
}

template <class T>
T& ref(gaddr address) {
    return s_mod->memory().ref<T>(address);
}

float get_float(gaddr at) { return s_mod->memory().read<float>(at); }
void put_float(gaddr at, float v) { s_mod->memory().write<float>(at, v); }

gaddr level0(uint32_t address) { return s_mod->relocate_level(kMainLevel, address); }

float camera_yaw() { return get_float(level0(s_disc->camera_yaw)); }

// Says why VR stopped where the launcher reads it, and leaves: nothing can be shown in the headset
// any more, and a blank headset is worse than the launcher saying so.
[[noreturn]] void fail_to_launcher(const std::string& why) {
    log_line(3, ("VR cannot start: " + why).c_str());
    if (!s_status_file.empty()) {
        std::ofstream status(s_status_file);
        status << why << '\n';
    }
    std::_Exit(3);
}

int phase_now() {
    openrac_play_status status{};
    status.size = sizeof status;
    s_mod->api().play_status(&status);
    return status.phase;
}

bool hero_on_foot() {
    return ref<uint8_t>(s_disc->hero + kHeroVehicleMode) == 0;
}

// Whether the player sees through Ratchet's eyes at this moment.
bool first_person_now(int phase) {
    return s_options.first_person && phase == OPENRAC_PLAY_PLAYING && s_have_hero && hero_on_foot();
}

// ---- Ratchet's body out of the head's way ----

// In the hero (level_hero_attach.c, as OpenRAC's hero_attachments.c reads it): the body's moby, and
// seven slots of 0x50 bytes whose mobys are at +0x20, +0x24 and +0x2C, +0x3A set while the item is
// thrown. Slot 0 is the hand's (kHeroSlot0); slot 3 the back's, the pack and Clank (class 601,
// Rerac-vr's moby_attach.rs).
constexpr uint32_t kHeroBody = 0x2080u;
constexpr uint32_t kHeroSlots = 0x1070u;
constexpr uint32_t kHeroSlotSize = 0x50u;
constexpr uint32_t kHeroSlotCount = 7u;
constexpr uint32_t kSlotThrown = 0x3Au;
constexpr uint32_t kSlotMobys[] = {0x20u, 0x24u, 0x2Cu};

bool s_capture_hook = false;  // the port calls before each capture: the body is hidden from there
std::vector<int> s_hidden;    // the slots this mod hid, shown again when the head leaves the body
int s_hidden_level = -1;      // the level they are of: another level's slots are other mobys

void show_hidden() {
    for (int slot : s_hidden) {
        s_mod->api().moby_hide(s_mod->handle(), slot, 0);
    }
    s_hidden.clear();
}

void log_hidden(const std::vector<int>& slots) {
    std::string line = "[vr-body] hidden in first person:";
    const std::vector<openrac_moby> mobys = s_mod->moby_list();
    for (int slot : slots) {
        int oclass = -1;
        for (const openrac_moby& m : mobys) {
            if (m.slot == slot) {
                oclass = m.oclass;
            }
        }
        line += " slot " + std::to_string(slot) + " class " + std::to_string(oclass) + ";";
    }
    log_line(1, slots.empty() ? "[vr-body] nothing hidden" : line.c_str());
}

// Ratchet's body and what he wears (Clank and the pack on his back among them) would fill a head's
// view from inside it. Hidden each tick while the player is in his head, as the game's update may
// show them again; the item in his hand stays, as it is in the player's.
void hide_body() {
    const int level = s_mod->loaded_level();
    if (level != s_hidden_level) {
        s_hidden.clear();  // the new level made its mobys as it wants them
        s_hidden_level = level;
    }
    if (!s_options.hide_body || !s_have_hero || !first_person_now(phase_now())) {
        if (!s_hidden.empty()) {
            show_hidden();
            log_hidden(s_hidden);
        }
        return;
    }
    std::vector<int> held;
    for (uint32_t offset : kSlotMobys) {
        const gaddr moby = ref<gaddr>(s_disc->hero + kHeroSlots + offset);
        if (moby != 0) {
            held.push_back(s_mod->moby_slot_of(moby));
        }
    }
    std::vector<int> slots;
    const auto add = [&](int slot) {
        if (slot >= 0 && std::find(held.begin(), held.end(), slot) == held.end()
            && std::find(slots.begin(), slots.end(), slot) == slots.end()) {
            slots.push_back(slot);
        }
    };
    add(s_mod->api().figure_slot(s_mod->handle(), -1));
    const gaddr body = ref<gaddr>(s_disc->hero + kHeroBody);
    add(body != 0 ? s_mod->moby_slot_of(body) : -1);
    for (uint32_t i = 1; i < kHeroSlotCount; ++i) {
        const gaddr at = s_disc->hero + kHeroSlots + i * kHeroSlotSize;
        if (ref<uint8_t>(at + kSlotThrown) != 0) {
            continue;
        }
        for (uint32_t offset : kSlotMobys) {
            const gaddr moby = ref<gaddr>(at + offset);
            add(moby != 0 ? s_mod->moby_slot_of(moby) : -1);
        }
    }
    for (int slot : s_hidden) {
        if (std::find(slots.begin(), slots.end(), slot) == slots.end()) {
            s_mod->api().moby_hide(s_mod->handle(), slot, 0);  // taken off, or now in the hand
        }
    }
    for (int slot : slots) {
        s_mod->api().moby_hide(s_mod->handle(), slot, 1);
    }
    if (slots != s_hidden) {
        log_hidden(slots);
    }
    s_hidden = slots;
}

// ---- The tick ----

void shots_and_hits(const openrac_pose& pose);

void on_tick() {
    openrac_pose pose{};
    pose.size = sizeof pose;
    if (s_mod->api().hero_pose(&pose) == OPENRAC_HOOK_OK) {
        const Vec3 position{pose.position[0], pose.position[1], pose.position[2]};
        s_hero_before = s_have_hero ? s_hero_after : position;
        s_hero_after = position;
        s_have_hero = true;
        s_facing = pose.rotation[2];
        shots_and_hits(pose);
    } else {
        s_have_hero = false;
    }

    // Without the port's call before each capture (patches/), the body is hidden here, after it:
    // the game's next update may show it again before the next frame.
    if (!s_capture_hook) {
        hide_body();
    }
}

// ---- Vibration ----

// A shot in the right hand; a hit on Ratchet in both.
void shots_and_hits(const openrac_pose& pose) {
    static int health = -1;
    if (!s_options.haptics || !s_headset || !s_started) {
        health = pose.health;
        return;
    }
    if (health >= 0 && pose.health < health) {
        s_headset->haptic(kLeft, 0.9f, 0.18f, 0);
        s_headset->haptic(kRight, 0.9f, 0.18f, 0);
    }
    health = pose.health;
    openrac_shot shots[OPENRAC_SHOTS_MAX];
    for (auto& shot : shots) {
        shot.size = sizeof shot;
    }
    if (s_mod->api().hero_shots(shots, OPENRAC_SHOTS_MAX) > 0) {
        s_headset->haptic(kRight, 0.55f, 0.05f, 0);
    }
}

// ---- The pad ----

void on_pad(int port, std::uint8_t* report, int size) {
    if (port != 0 || !s_have_frame) {
        return;
    }
    PadContext context;
    context.in_menu = s_menu_input;
    context.first_person = s_steers;
    context.head_yaw = s_head_yaw;
    context.camera_yaw = s_steers ? camera_yaw() : 0.0f;
    context.quick_select = s_quick_select;

    if (s_quick_select && s_frame.hand_valid[kRight]) {
        // The hand's reach from where it was when the ring opened, turned to the player's facing.
        const Vec3 reach = rotate(yaw_quat(-s_pick_yaw), s_frame.hand_grip[kRight].position - s_pick_from);
        context.pick_x = std::clamp(reach.x / kPickReach, -1.0f, 1.0f);
        context.pick_y = std::clamp(reach.y / kPickReach, -1.0f, 1.0f);
    }

    const VirtualPad pad = s_pad_mapper.map(s_frame.controls, context);
    merge_report(pad, report, size);
}

// ---- The walking and facing: lock-strafe's hooks, with the aim's yaw for the camera's ----

void read_input(rac1_strafe_input* in) {
    const gaddr hero = s_disc->hero;
    in->playing =
        s_mod->loaded_level() >= 0 && ref<int32_t>(level0(s_disc->level_mode)) == kModePlaying;
    in->state = ref<int32_t>(hero + kHeroState);
    in->frame_mode = ref<uint8_t>(hero + kHeroFrameMode);
    in->vehicle_mode = ref<uint8_t>(hero + kHeroVehicleMode);
    in->slot_state = ref<int32_t>(hero + kHeroSlot0 + kSlotState);
    in->slot_item = ref<int32_t>(hero + kHeroSlot0 + kSlotItem);
    in->held = 0;
    in->facing = get_float(hero + kHeroFacing);
    in->camera = s_aim_yaw;
    in->look_x = 0;
}

// While the player steers, he always faces where they aim, whatever is in his hand: that the view
// and the gun agree is what first person needs. In lock-strafe that is only with a weapon out.
int strafe_kind() {
    if (!s_steers) {
        return RAC1_STRAFE_NONE;
    }
    rac1_strafe_input in;
    read_input(&in);
    if (!in.playing || in.vehicle_mode != 0 || in.frame_mode != 0) {
        return RAC1_STRAFE_NONE;
    }
    if (in.state != RAC1_STRAFE_STATE_STAND && in.state != RAC1_STRAFE_STATE_RUN
        && in.state != RAC1_STRAFE_STATE_RUN_ALT && in.state != RAC1_STRAFE_STATE_JUMP) {
        return RAC1_STRAFE_NONE;
    }
    return RAC1_STRAFE_LOCKED;
}

template <class Original>
void turn_toward(Original original, float yaw, int32_t mode, float accel, float damping, float limit) {
    const gaddr move_yaw = s_disc->hero + kHeroMoveYaw;
    const float stick = get_float(move_yaw);
    put_float(move_yaw, yaw);
    original(mode, accel, damping, limit);
    put_float(move_yaw, stick);
}

template <class Original>
void turn_to_heading(Original original, int32_t mode, float accel, float damping, float limit) {
    if (mode != kTurnFollowsMoveYaw || strafe_kind() == RAC1_STRAFE_NONE) {
        original(mode, accel, damping, limit);
        return;
    }
    turn_toward(original, s_aim_yaw, mode, accel, damping, limit);
}

template <class Original>
void set_planar_velocity(Original original, float angle) {
    if (!(angle <= RAC1_STRAFE_PI)) {
        angle = rac1_strafe_planar_angle(strafe_kind(), angle, get_float(s_disc->hero + kHeroMoveYaw));
    }
    original(angle);
}

template <class Original>
void update_motion_input(Original original) {
    // Standing, the game's state does not turn him: he is turned here, as lock-strafe does.
    if (strafe_kind() != RAC1_STRAFE_NONE
        && ref<int32_t>(s_disc->hero + kHeroState) == RAC1_STRAFE_STATE_STAND) {
        const float frame = get_float(s_mod->relocate(s_disc->frame_scale));
        const float step = get_float(s_mod->relocate(s_disc->turn_step));
        turn_toward(s_mod->original<openrac::rac1::Hero_TurnToHeading>(), s_aim_yaw,
                    kTurnFollowsMoveYaw, frame * kStandingAccel, frame * kStandingDamping,
                    step * kStandingLimit);
    }
    original();
}

// ---- The weapon in the hand ----

Vec3 anchor_now(float alpha);

// What is in Ratchet's hand is a moby of its own, which the hero's update puts at his hand's joint
// each tick (update_hero_item_mobys). Before the frame is captured, it is put at the player's right
// hand instead: its first row of the matrix is where it points, as it is on him.
void place_weapon() {
    if (!s_options.hand_weapon || !s_steers || !s_have_frame || !s_frame.hand_valid[kRight]) {
        return;
    }
    // The slot's word is a moby only while the hero holds one: anything else (between items, in a
    // load) is not written through, as the memory view checks no bounds.
    const gaddr moby = ref<gaddr>(s_disc->hero + kHeroSlot0);
    if (moby == 0 || s_mod->moby_slot_of(moby) < 0 || ref<uint8_t>(moby + kMobyState) >= 0x80) {
        return;
    }
    const Pose grip = s_rig.to_frame(s_frame.hand_grip[kRight], anchor_now(1.0f));
    const Vec3 at = frame_to_world(grip.position);
    const Quat turn = yaw_quat(s_rig.yaw) * s_frame.hand_aim[kRight].orientation;
    const Vec3 forward = frame_to_world(rotate(turn, {0, 0, -1}));
    const Vec3 left = frame_to_world(rotate(turn, {-1, 0, 0}));
    const Vec3 up = frame_to_world(rotate(turn, {0, 1, 0}));

    const auto row = [&](uint32_t index, Vec3 v, float size) {
        const gaddr address = moby + kMobyBasis + kMobyRowSize * index;
        put_float(address, v.x * size);
        put_float(address + 4u, v.y * size);
        put_float(address + 8u, v.z * size);
    };
    // The rows' length is the moby's size, which stays.
    const gaddr first = moby + kMobyBasis;
    const float size = length({get_float(first), get_float(first + 4u), get_float(first + 8u)});
    row(0, forward, size);
    row(1, left, size);
    row(2, up, size);
    put_float(moby + kMobyPosition, at.x);
    put_float(moby + kMobyPosition + 4u, at.y);
    put_float(moby + kMobyPosition + 8u, at.z);
}

// ---- The frame ----

// The eyes' anchor, where the head at the last recentre stands in the frame: above the hero as the
// scene draws him, part way between his last two ticks.
Vec3 anchor_now(float alpha) {
    if (!s_have_hero) {
        return world_to_frame(s_hero_after);
    }
    const Vec3 hero = s_hero_before + (s_hero_after - s_hero_before) * alpha;
    return world_to_frame(hero + Vec3{0, 0, kEyeHeight * s_options.world_scale});
}

void place_panel(const Pose& head) {
    const float yaw = tracking_yaw(head.orientation);
    s_panel_pose.orientation = yaw_quat(yaw);
    s_panel_pose.position =
        head.position + rotate(yaw_quat(yaw), {0, 0, -kPanelDistance}) + Vec3{0, 0.0f, 0};
    s_panel_placed = true;
}

void start_headset() {
    s_started = true;
#if !defined(__ANDROID__)
    sim_gl_proc = s_mod->api().graphics()->gl_proc;
#endif
    s_headset.reset(make_headset(log_line));
    std::string why;
    if (!s_headset->start(s_xr, why)) {
        s_failed = true;
        fail_to_launcher(why);
    }
    log_line(1, "headset ready");
}

// One eye's view, world pose or relative to the game's camera.
void fill_eye(openrac_view& view, int eye, const FrameInfo& frame, bool first_person, float alpha) {
    int width = 0, height = 0;
    s_headset->target_size(static_cast<Target>(kTargetLeftEye + eye), width, height);
    view.kind = OPENRAC_VIEW_WORLD;
    view.width = width;
    view.height = height;
    const Pose pose = first_person ? s_rig.to_frame(frame.eye[eye], anchor_now(alpha))
                                   : s_rig.to_camera(frame.eye[eye], s_recentre_yaw);
    view.position[0] = pose.position.x;
    view.position[1] = pose.position.y;
    view.position[2] = pose.position.z;
    view.orientation[0] = pose.orientation.x;
    view.orientation[1] = pose.orientation.y;
    view.orientation[2] = pose.orientation.z;
    view.orientation[3] = pose.orientation.w;
    view.angle_left = frame.fov[eye][0];
    view.angle_right = frame.fov[eye][1];
    view.angle_up = frame.fov[eye][2];
    view.angle_down = frame.fov[eye][3];
    view.reserved[0] = first_person ? kViewFlagWorldPose : 0;
    view.texture = s_headset->acquire(static_cast<Target>(kTargetLeftEye + eye));
}

void fill_panel(openrac_view& view, uint32_t kind) {
    int width = 0, height = 0;
    s_headset->target_size(kTargetPanel, width, height);
    view.kind = kind;
    view.width = width;
    view.height = height;
    view.orientation[3] = 1.0f;
    view.texture = s_headset->acquire(kTargetPanel);
}

// What the head and hands do to the rig, once a frame.
void steer(const FrameInfo& frame, int phase) {
    const Pose& head = frame.head;
    const Controls& c = frame.controls;

    // Both sticks clicked, the settle of a new phase's first frame, or the session's first.
    static bool both_were = false;
    const bool both = c.left_click && c.right_click;
    if ((both && !both_were) || s_recentre_wanted) {
        s_rig.recentre(head, s_have_hero ? s_facing : 0.0f);
        s_recentre_yaw = tracking_yaw(head.orientation);
        s_recentre_wanted = false;
        s_panel_placed = false;
    }
    both_were = both;

    const bool first_person = first_person_now(phase);
    if (first_person) {
        // The turn is the player's in first person; in third the right stick is the game's camera.
        float stick = c.right_x;
        const float angle = s_turn.update(stick, static_cast<float>(frame.frame_seconds), s_options.turn);
        if (angle != 0.0f) {
            s_rig.turn(angle, head.position);
        }

        // Walking round the room leaves the view ahead of Ratchet's body; moving him closes it.
        if (std::abs(c.left_x) + std::abs(c.left_y) > 0.2f) {
            Vec3 offset = head.position - s_rig.origin;
            offset.y = 0;
            const float distance = length(offset);
            const float step = kCatchUp * static_cast<float>(frame.frame_seconds);
            if (distance > 0.001f) {
                s_rig.origin = s_rig.origin + offset * (std::min(step, distance) / distance);
            }
        }
    }

    s_steers = first_person;
    s_head_yaw = world_yaw(s_rig.yaw, head.orientation);
    s_aim_yaw = s_head_yaw;
    if (s_options.hand_aim && frame.hand_valid[kRight]) {
        s_aim_yaw = world_yaw(s_rig.yaw, frame.hand_aim[kRight].orientation);
    }

    // The quick select is opened by the right stick's click; where the hand then goes picks.
    const bool picking = first_person && c.right_click && !c.left_click && frame.hand_valid[kRight];
    if (picking && !s_quick_select) {
        s_pick_from = frame.hand_grip[kRight].position;
        s_pick_yaw = tracking_yaw(head.orientation);
    }
    s_quick_select = picking;
    s_menu_input = phase == OPENRAC_PLAY_FRONT_END || phase == OPENRAC_PLAY_MENU
                   || phase == OPENRAC_PLAY_NONE || phase == OPENRAC_PLAY_LOADING;
}

// A line of what the mod sees, each five seconds and on each change of phase: with no way to attach
// to a headset, the log is how a run is understood afterwards.
void trace_state(int phase, const FrameInfo& frame, const char* why) {
    char line[260];
    std::snprintf(line, sizeof line,
                  "[vr-state] %s: phase %d level %d first_person %d hero %d (%.2f %.2f %.2f) state %d "
                  "facing %.2f move %.2f camera %.2f head %.2f aim %.2f rig %.2f views %d hands %d%d focus %d",
                  why, phase, s_mod->loaded_level(), first_person_now(phase) ? 1 : 0, s_have_hero ? 1 : 0,
                  s_hero_after.x, s_hero_after.y, s_hero_after.z,
                  s_have_hero ? ref<int32_t>(s_disc->hero + kHeroState) : -1,
                  s_have_hero ? get_float(s_disc->hero + kHeroFacing) : 0.0f,
                  s_have_hero ? get_float(s_disc->hero + kHeroMoveYaw) : 0.0f,
                  s_mod->loaded_level() >= 0 ? camera_yaw() : 0.0f, s_head_yaw, s_aim_yaw, s_rig.yaw,
                  frame.views_valid ? 1 : 0, frame.hand_valid[0] ? 1 : 0, frame.hand_valid[1] ? 1 : 0,
                  frame.focused ? 1 : 0);
    log_line(1, line);
}

int views_begin(void*, openrac_view* views, int max) {
    const bool picture = views[0].reserved[kViewHintPicture] != 0;
    float alpha = 1.0f;
    std::memcpy(&alpha, &views[0].reserved[kViewHintAlpha], sizeof alpha);
    alpha = std::clamp(alpha, 0.0f, 1.0f);
    std::memset(views, 0, sizeof(openrac_view) * static_cast<std::size_t>(max));

    if (!s_started) {
        start_headset();
    }
    s_eyes_shown = false;
    s_panel_shown = false;
    s_frame_open = false;

    if (s_headset->wants_exit()) {
        log_line(1, "the headset closed the game");
        std::_Exit(0);
    }
    if (!s_headset->begin_frame(s_frame)) {
        return 0;
    }
    s_frame_open = true;
    s_have_frame = true;

    const int phase = picture ? OPENRAC_PLAY_SCENE : phase_now();
    const bool changed = phase != s_last_phase;
    if (changed) {
        // A new place or a scene starts looking where it looks: where Ratchet faces, or along the
        // game's camera. Back from the pause menu, the player stays as they were.
        if ((phase == OPENRAC_PLAY_PLAYING || phase == OPENRAC_PLAY_SCENE)
            && s_last_phase != OPENRAC_PLAY_MENU) {
            s_recentre_wanted = true;
        }
        s_last_phase = phase;
        s_panel_placed = false;
    }
    if (s_frame.views_valid) {
        steer(s_frame, phase);
    }
    static int frames = 0;
    if (changed || ++frames >= 360) {
        frames = 0;
        trace_state(phase, s_frame, changed ? "phase" : "tick");
    }
    if (!s_frame.should_render || !s_frame.views_valid) {
        return 0;
    }

    int count = 0;
    const bool world_phase = !picture
                             && (phase == OPENRAC_PLAY_PLAYING
                                 || (phase == OPENRAC_PLAY_SCENE && s_options.scenes_in_vr));
    if (world_phase) {
        const bool first_person = first_person_now(phase);
        fill_eye(views[count++], 0, s_frame, first_person, alpha);
        fill_eye(views[count++], 1, s_frame, first_person, alpha);
        s_eyes_shown = true;
        fill_panel(views[count++], OPENRAC_VIEW_2D);
        s_panel_shown = true;
        s_panel_is_hud = true;
    } else {
        if (!s_panel_placed) {
            place_panel(s_frame.head);
        }
        fill_panel(views[count++], kViewKindWindow);
        s_panel_shown = true;
        s_panel_is_hud = false;
    }
    return count;
}

void views_end(void*, const openrac_view* views, int count) {
    if (!s_frame_open) {
        return;
    }
    s_frame_open = false;

    // Nothing drawn at this view's size yet means nothing to show.
    bool eyes = s_eyes_shown;
    bool panel = s_panel_shown;
    for (int i = 0; i < count; ++i) {
        if (views[i].picture == 0 && views[i].drawn == 0) {
            (views[i].kind == OPENRAC_VIEW_WORLD ? eyes : panel) = false;
        }
    }

    PanelLayer layer;
    layer.visible = panel;
    if (s_panel_is_hud) {
        layer.follows_head = true;
        layer.blend = true;
        layer.pose.position = {0, 0, -kHudDistance};
        layer.width = kHudDistance * 1.05f * s_options.hud_scale;
        layer.height = layer.width * 3.0f / 4.0f;
    } else {
        layer.pose = s_panel_pose;
        layer.width = kPanelWidth;
        layer.height = kPanelWidth * 3.0f / 4.0f;
    }
    s_headset->finish_frame(eyes, layer);
}

void views_closing(void*) {
    if (s_headset) {
        s_headset->stop();
    }
}

void load(openrac::Mod& mod) {
    s_mod = &mod;

    if (mod.game() != "rac1") {
        throw openrac::Error("Quest VR is for Ratchet & Clank (rac1)");
    }
    for (const auto& disc : k_quest_discs) {
        if (mod.version() == disc.version) {
            s_disc = &disc;
        }
    }
    if (s_disc == nullptr) {
        throw openrac::Error("Quest VR knows the PAL and NTSC-U discs, not " + std::string(mod.version()));
    }

    s_options.first_person = mod.setting<std::string>("view") != "third";
    s_options.scenes_in_vr = mod.setting<std::string>("cutscenes") != "screen";
    s_options.turn.smooth = mod.setting<std::string>("turn") == "smooth";
    s_options.turn.snap_angle = static_cast<float>(mod.setting<double>("snap_degrees")) * kPi / 180.0f;
    s_options.turn.smooth_rate = static_cast<float>(mod.setting<double>("smooth_degrees")) * kPi / 180.0f;
    s_options.world_scale = static_cast<float>(mod.setting<double>("world_scale"));
    s_options.hud_scale = static_cast<float>(mod.setting<double>("hud_scale"));
    s_options.haptics = mod.setting<bool>("haptics");
    s_options.hand_aim = mod.setting<bool>("hand_aim");
    s_options.hide_body = mod.setting<bool>("hide_body");
    s_options.hand_weapon = mod.setting<bool>("hand_weapon");
    s_xr.refresh_hz = static_cast<float>(mod.setting<std::int64_t>("refresh_hz"));
    s_xr.resolution = static_cast<float>(mod.setting<double>("resolution"));
    s_rig.scale = s_options.world_scale;

    if (const char* status = std::getenv("QUEST_VR_STATUS")) {
        s_status_file = status;
    }

    mod.on_frame([] { on_tick(); });
    // The Quest fork's entry (patches/): a port without it has a shorter table, so it is looked
    // for as the SDK looks for its newer entries, and the weapon stays where the game puts it.
    const openrac_api& api = mod.api();
    if (api.size >= offsetof(openrac_api, on_before_capture) + sizeof(void*) && api.on_before_capture != nullptr
        && api.on_before_capture(mod.handle(), [](void*) {
               hide_body();
               place_weapon();
           }, nullptr) == 0) {
        s_capture_hook = true;
        log_line(1, "weapon in the hand: placed before each frame's capture");
    } else {
        log_line(2, "this game build lacks the Quest patch's on_before_capture: the weapon stays in "
                    "the game's hand, not yours");
    }
    mod.on_pad([](int port, std::uint8_t* report, int size) { on_pad(port, report, size); });

    namespace rac1 = openrac::rac1;
    mod.hook<rac1::Hero_UpdateMotionInput>([](auto original) { update_motion_input(original); });
    mod.hook<rac1::Hero_TurnToHeading>(
        [](auto original, int32_t m, float accel, float damping, float limit) {
            turn_to_heading(original, m, accel, damping, limit);
        }
    );
    mod.hook<rac1::Hero_SetPlanarVelocity>([](auto original, float angle) {
        set_planar_velocity(original, angle);
    });

    openrac_view_source source{};
    source.size = sizeof source;
    source.begin = views_begin;
    source.end = views_end;
    source.closing = views_closing;
    mod.on_views(source);

    mod.info(std::string("Quest VR on the ") + std::string(mod.version()) + " disc");
}

}  // namespace

OPENRAC_MOD_CPP("quest-vr", "0.1.0", load)
