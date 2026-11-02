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

#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <cstring>
#include <deque>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include <openrac/games/rac1.hpp>
#include <openrac/mod.hpp>

#include "discs.h"
#include "hand.h"
#include "heading.h"
#include "pad.h"
#include "rig.h"
#include "strafe.h"
#include "glyphs.h"
#include "pointer.h"
#include "support.h"
#include "turret.h"
#include "view_abi.h"
#include "xr.h"

using openrac::gaddr;

#if !defined(__ANDROID__)
namespace quest {
extern void* (*sim_gl_proc)(const char*);
extern bool sim_left_grip;
extern bool sim_ready;
}
#endif

namespace {

using namespace quest;

// In the hero (seen in game code; lock-strafe's offsets).
constexpr uint32_t kHeroMoveYaw = 0x180u;
constexpr uint32_t kHeroFacing = 0x98u;
constexpr uint32_t kHeroVelocityZ = 0xE8u, kHeroStateTimer = 0x198u;  // motion.velocity's z, per tick
constexpr uint32_t kHeroState = 0x2084u;
constexpr uint32_t kHeroFrameMode = 0x20B3u;
constexpr uint32_t kHeroVehicleMode = 0x20A4u;
constexpr uint32_t kHeroSlot0 = 0x1090u;
constexpr uint32_t kSlotState = 0x24u;
constexpr uint32_t kSlotItem = 0x28u;

// In a moby (seen in game code; level_hero_attach.c's HeroAttachMoby).
constexpr uint32_t kMobyPosition = 0x10u;
constexpr uint32_t kMobyState = 0x20u;  // >= 0x80 dead (the port's MOBY_STATE; 0x30 is its update distance)
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
// own eye height, not the game's first-person camera's 1.6), and the lower ones of his postures.
constexpr float kEyeHeight = 1.1f;
constexpr float kEyeCrouched = 0.6f;
constexpr float kEyeSwimming = 0.45f;
constexpr float kEyeHangLowest = 0.45f;
constexpr float kEyeBlend = 4.0f;  // units a second
constexpr float kEyeClank = 0.63f;
constexpr float kEyeGiant = 9.5f;
constexpr float kGiantScale = kEyeGiant / 1.6f;  // his tracking space is his size
constexpr uint8_t kBodyClank = 1, kBodyGiant = 2;
// A sitting player's eyes below a standing one's, metres: the view lifted by it (the VR Options).
constexpr float kSeatedLift = 0.45f;
// How fast the view's room offset closes on the body while the stick moves him, metres a second.
constexpr float kCatchUp = 1.2f;

// The HUD's panel, ahead of the head: the game's 4:3 picture, metres.
constexpr float kHudDistance = 1.6f;
// The menus' panel.
constexpr float kPanelDistance = 2.4f;
constexpr float kPanelWidth = 3.0f;

// The quick select: how far the hand reaches for the full stick, metres.
constexpr float kPickStart = 0.06f, kPickStop = 0.035f;  // metres of reach, a ring's slot chosen and let go

// The hero's state on a turret or in a flown ship (Rerac-vr's game_input.rs).
constexpr int32_t kHeroStateMounted = 0x32;

// What the settings ask for.
struct Options {
    bool first_person = true;
    bool screen = false;  // the launcher's 2D mode: the whole game on a big screen ahead
    TurnSettings turn;
    float world_scale = 1.0f;
    float hud_scale = 1.0f;
    bool haptics = true;
    bool hand_aim = true;
    bool hide_body = true;
    bool hand_weapon = true;
    bool scenes_in_vr = true;  // cutscenes around the player; false: on the panel, as the game frames them
    bool scene_eyes = true;    // and seen from Ratchet's eyes where he is in them (Rerac-vr's)
    bool throw_by_hand = true; // the gloves' bombs, mines and Decoys thrown by the hand (Rerac-vr's)
    bool glyphs = true;        // the pad icons in the game's texts drawn as the headset's controls (Rerac-vr's)
    bool two_handed = true;    // the two-armed weapons heavy in one hand, held by both (Rerac-vr's)
    bool show_hands = true;    // Ratchet's forearms and hands at the controllers (Rerac-vr's)
    bool grab_ledges = true;   // ledges are held with the hands, not caught by the game (Rerac-vr's)
    bool puzzle_over_tool = true;  // the Trespasser's rings hang over the plugged tool, not before the face
    bool shop_on_foot = true;      // the vendor's screens on the machine, the player walking (Rerac-vr's)
    bool menu_in_world = true;     // a menu stands in the level, by parts (Rerac-vr's spatial menus)
    bool room_scale = true;    // walking in the room walks Ratchet, through the game's collision (Rerac-vr's)
    bool swim_strokes = true;  // the arms swim, under the water where the head looks (Rerac-vr's)
    bool solid_hands = true;   // what is held stops at walls, and does not fire through them (Rerac-vr's)
    bool swing_wrench = true;  // the wrench hits what its head sweeps through (Rerac-vr's), not the game's swing
};

// The pause menu's VR Options (Rerac-vr's), kept in a file of their own: not the launcher's.
struct VrOptions {
    bool helmet = true;          // a helmet worn is seen from inside
    bool boots = true;           // boots worn are at his feet
    bool camera_flip = false;    // the view turns with his flips
    bool direction_body = false;  // the heading and the stick follow the gaze (the owner's default: wall jumps work
                                  // better on it; Rerac-vr's is the body); true: the body
    int body = 0;                // his body seen from his head: 0 never, 1 in cutscenes, 2 always
    bool akimbo = false;         // a gun in each hand
    bool seated = false;         // the view lifted by a sitting player's drop
};

// What the mod holds. All of it is on the game's thread.
openrac::Mod* s_mod = nullptr;
const quest_disc* s_disc = nullptr;
std::unique_ptr<Headset> s_headset;
Options s_options;
VrOptions s_vr;
XrSettings s_xr;
bool s_started = false;
bool s_failed = false;
std::string s_status_file;

Rig s_rig;
FrameInfo s_frame;
uint64_t s_tick = 0;
bool s_have_frame = false;
bool s_frame_open = false;
bool s_recentre_wanted = true;
float s_recentre_yaw = 0;
PadMapper s_pad_mapper;
TurnControl s_turn;
Pose s_panel_pose;
float s_panel_scale = 1.0f;  // the world menu brought nearer and smaller before a wall, the same size to the eye
float panel_width() { return kPanelWidth * s_panel_scale; }
bool s_panel_placed = false;
int s_last_phase = -1;

// Two ticks' hero positions: the scene is drawn between them.
Vec3 s_hero_before, s_hero_after;
bool s_have_hero = false;
float s_facing = 0;
float s_eye_height = kEyeHeight;
int s_health = 1;

// What each frame showed, for its end.
bool s_eyes_shown = false;
bool s_panel_shown = false;
bool s_panel_is_hud = false;
bool s_panel_is_monitor = false;

// The turn and walk read from the frame, for the pad.
float s_aim_yaw = 0;
float s_head_yaw = 0;
bool s_steers = false;
bool s_menu_input = true;
bool s_quick_select = false;
Vec3 s_pick_from[2];  // each hand's, where it was as its ring opened
Quat s_pick_head[2];  // and the head's turn then: the plane the reach is read in
bool s_pick_selecting[2]{};
bool s_page_wanted = false;
bool s_left_page_wanted = false;
bool s_left_ring = false;       // Akimbo: the left stick's click holds the ring open, the left hand picks
bool s_left_ring_open = false;  // that ring is up, until it has closed: its pick comes as it fades
int32_t s_left_pick = 0;       // what that ring last picked
bool s_page_trigger = false;
bool s_left_page_trigger = false;
int s_ring_page = 0;
uint64_t s_ring_seen = 0;  // the tick the ring last ran
SwingDetector s_swing;
bool s_trigger_back = false;  // the trigger is taking the gun out, not firing it
bool s_swinging = false;
Vec3 s_hand_before;
bool s_hand_seen = false;

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

// At the vendor on foot ("Shopping on foot" below): the game is in its vendor mode, the player in play.
bool s_shopping = false;
bool s_skip_held = false;  // skip_scene
float s_view_alpha = 1.0f;  // the frame's blend between its ticks, for what is placed by the eyes' rig

int phase_now() {
    openrac_play_status status{};
    status.size = sizeof status;
    s_mod->api().play_status(&status);
    return s_shopping ? OPENRAC_PLAY_PLAYING : status.phase;
}

bool hero_on_foot() {
    return ref<uint8_t>(s_disc->hero + kHeroVehicleMode) == 0;
}

// The turret Ratchet mans, a moby of the level, or 0 ("Manned turrets" below). In its seat the
// player keeps their own head, but not their hands' work: s_steers is false.
gaddr s_manned = 0;
bool in_ship();

// Whether the player sees through Ratchet's eyes at this moment.
bool first_person_now(int phase) {
    return s_options.first_person && !s_options.screen && phase == OPENRAC_PLAY_PLAYING && s_have_hero;
}

// A menu in the world ("The menu in the world" below): the eyes stay Ratchet's, the level around them. Not for what
// has its own view (a seat, a ship, a missile's monitor) or the vendor on its flat screen.
bool s_world_menu = false;
bool s_title_world = false;       // the title's own space scene in the eyes, its menus standing in the room (views_begin)
bool s_pointer_on_panel = false;  // the right hand's beam is on the menu's panel (menu_pointer)
bool s_page_targets = false;      // the page up has things to point at, and the right hand can (page_has_targets)
gaddr s_target_page = 0;          // the last page the beam took a target from
bool hands_shown() { return s_steers || s_world_menu; }
bool missile_flying();
bool vendor_open();
bool world_menu_now(int phase) {
    return s_options.menu_in_world && s_options.first_person && !s_options.screen && phase == OPENRAC_PLAY_MENU && s_have_hero
           && s_manned == 0 && !in_ship() && !missile_flying() && !vendor_open();
}

// ---- Ratchet's body out of the head's way ----

// In the hero (level_hero_attach.c, as OpenRAC's hero_attachments.c reads it): the body's moby, and
// seven slots of 0x50 bytes whose mobys are at +0x20, +0x24 and +0x2C, +0x3A set while the item is
// thrown. Slot 0 is the hand's (kHeroSlot0); slot 3 the back's, the pack and Clank (class 601,
// Rerac-vr's moby_attach.rs).
constexpr uint32_t kHeroBody = 0x2080u;
constexpr uint32_t kHeroSlots = 0x1070u;
constexpr uint32_t kHeroSlotSize = 0x50u;
constexpr uint32_t kFeetSlot = 1u;
constexpr uint32_t kHeroSlotCount = 7u;
constexpr uint32_t kSlotThrown = 0x3Au;
constexpr uint32_t kHeroTrails[] = {0x1670u, 0x17B0u};  // the hero's two MobyTrails (hero.h), 0x140 bytes each
constexpr uint32_t kSlotMobys[] = {0x20u, 0x24u, 0x2Cu};

bool arms_drawn();
bool wrench_put_down();
bool worn_on_head(gaddr moby);
bool right_on_ledge();
bool hanging_on();
bool wrench_on_bolt();

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
    if (!s_options.hide_body || !s_have_hero || (!first_person_now(phase_now()) && !s_world_menu)) {
        if (!s_hidden.empty()) {
            show_hidden();
            log_hidden(s_hidden);
        }
        return;
    }
    // The hand's item stays in the hand, unless the hands are on a turret, or it is the wrench and
    // the hand has let go of it (Rerac-vr shows it only on the grip).
    const bool no_item = s_manned != 0 || wrench_put_down() || right_on_ledge() || in_ship();
    std::vector<int> held;
    for (uint32_t offset : kSlotMobys) {
        const gaddr moby = ref<gaddr>(s_disc->hero + kHeroSlots + offset);
        if (moby != 0 && !no_item) {
            held.push_back(s_mod->moby_slot_of(moby));
        }
    }
    std::vector<int> slots;
    if (no_item) {
        for (uint32_t offset : kSlotMobys) {
            const gaddr moby = ref<gaddr>(s_disc->hero + kHeroSlots + offset);
            const int slot = moby != 0 ? s_mod->moby_slot_of(moby) : -1;
            if (slot >= 0) {
                slots.push_back(slot);
            }
        }
    }
    const auto add = [&](int slot) {
        if (slot >= 0 && std::find(held.begin(), held.end(), slot) == held.end()
            && std::find(slots.begin(), slots.end(), slot) == slots.end()) {
            slots.push_back(slot);
        }
    };
    // With his arms at the controllers his body stays, drawn as those alone (place_arms).
    if (!arms_drawn()) {
        add(s_mod->api().figure_slot(s_mod->handle(), -1));
        const gaddr body = ref<gaddr>(s_disc->hero + kHeroBody);
        add(body != 0 ? s_mod->moby_slot_of(body) : -1);
    }
    for (uint32_t i = 1; i < kHeroSlotCount; ++i) {
        const gaddr at = s_disc->hero + kHeroSlots + i * kHeroSlotSize;
        // The boots stay at his feet (Rerac-vr's Slot::Feet: a left and a right boot in the second slot).
        if (ref<uint8_t>(at + kSlotThrown) != 0 || (s_vr.boots && i == kFeetSlot)) {
            continue;
        }
        for (uint32_t offset : kSlotMobys) {
            // A slot's word is a moby only while it holds one: read through only once it is one.
            const gaddr moby = ref<gaddr>(at + offset);
            const int slot = moby != 0 ? s_mod->moby_slot_of(moby) : -1;
            if (slot >= 0 && s_vr.helmet && worn_on_head(moby)) {
                continue;  // place_helmet has it
            }
            add(slot);
        }
    }
#if !defined(__ANDROID__)
    // PC test aid (QUEST_BOOTS_TRACE=1): each slot's item and mobys, about every two seconds.
    static uint64_t slots_traced = 0;
    if (std::getenv("QUEST_BOOTS_TRACE") != nullptr && s_tick >= slots_traced) {
        slots_traced = s_tick + 100;
        std::string line = "[vr-body] slots:";
        for (uint32_t i = 0; i < kHeroSlotCount; ++i) {
            const gaddr at = s_disc->hero + kHeroSlots + i * kHeroSlotSize;
            char text[100];
            std::snprintf(text, sizeof text, " %u:item %d", i, ref<int32_t>(at + kSlotMobys[0] + kSlotItem));
            line += text;
            for (uint32_t offset : kSlotMobys) {
                const gaddr moby = ref<gaddr>(at + offset);
                const int slot = moby != 0 ? s_mod->moby_slot_of(moby) : -1;
                if (slot >= 0) {
                    std::snprintf(text, sizeof text, " m%d(st %d mode 0x%X%s)", slot, ref<uint8_t>(moby + kMobyState),
                                  ref<uint16_t>(moby + 0x34u),  // its mode (kMobyMode, declared further on)
                                  std::find(slots.begin(), slots.end(), slot) != slots.end() ? " hid" : "");
                    line += text;
                }
            }
        }
        log_line(1, line.c_str());
    }
#endif
    for (int slot : s_hidden) {
        if (std::find(slots.begin(), slots.end(), slot) == slots.end()) {
            s_mod->api().moby_hide(s_mod->handle(), slot, 0);  // taken off, or now in the hand
        }
    }
    for (int slot : slots) {
        s_mod->api().moby_hide(s_mod->handle(), slot, 1);
    }
    // His after-images: the hero's two moby trails (hero.h MobyTrail: fade 0x100, copies 0x120,
    // active 0x13C) make up to four ghost copies of his body behind a fast move (the Walloper's
    // lunge), which would cross the eyes from behind. They are the game's to delete, so they are
    // hidden each tick and never shown again here (a slot shown again could by then be another moby's).
    for (uint32_t offset : kHeroTrails) {
        const gaddr trail = s_disc->hero + offset;
        const bool active = ref<int32_t>(trail + 0x13Cu) != 0;
        for (uint32_t i = 0; i < 4; ++i) {
            const gaddr ghost = ref<gaddr>(trail + 0x120u + 4u * i);
            if (ghost == 0 || (!active && ref<int32_t>(trail + 0x100u + 4u * i) == 0)) {
                continue;
            }
            const int slot = s_mod->moby_slot_of(ghost);
            if (slot < 0) {
                continue;
            }
            s_mod->api().moby_hide(s_mod->handle(), slot, 1);
            static int logged = 0;
            if (logged < 4) {
                ++logged;
                char text[96];
                std::snprintf(text, sizeof text, "[vr-body] after-image hidden: trail 0x%X copy %u slot %d",
                    offset, i, slot);
                log_line(1, text);
            }
        }
    }
    if (slots != s_hidden) {
        log_hidden(slots);
    }
    s_hidden = slots;
}

// ---- The tick ----

void shots_and_hits(const openrac_pose& pose);
void sample_seat();
void sample_origin();
void sample_ship();
void sample_scene();

// Rerac-vr's eye_parked (vr/game.rs): dead, eaten (his control mode 0x14, or state 0x78), or held
// under by a Rilgar lurker (state 0x72, the hold's flag set, a lurker, class 35, in its grab state 3,
// which carries his body 20 units above itself until he drowns): the body no longer carries the eye.
// It stays where he last stood alive, the head tracked as ever, until he is back. The view and the
// hands follow it, nothing else. Death scripts can park the body far outside the level.
bool s_eye_parked = false;
Vec3 s_eye_parked_at;  // where he last stood alive (the world)

bool eye_parked() {
    constexpr uint32_t kControlMode = 0x208Cu, kHoldFlag = 0x20A5u;
    constexpr int32_t kModeGone = 0x14, kStateEaten = 0x78, kStateHeld = 0x72;
    constexpr int16_t kLurker = 35;
    constexpr uint8_t kLurkerGrab = 3;
    const gaddr hero = s_disc->hero;
    const int32_t state = ref<int32_t>(hero + kHeroState);
    // In a vehicle the game swaps the ship's hit points in: only a death on foot counts.
    if ((s_health <= 0 && hero_on_foot()) || ref<int32_t>(hero + kControlMode) == kModeGone || state == kStateEaten) {
        return true;
    }
    if (state == kStateHeld && ref<uint8_t>(hero + kHoldFlag) != 0) {
        for (const openrac_moby& m : s_mod->moby_list()) {
            if (m.oclass == kLurker && m.state == kLurkerGrab) {
                return true;
            }
        }
    }
    return false;
}

// Where the hero is, for the eyes and the hand: at the end of the tick, as the scene will show him.
void sample_hero() {
    openrac_pose pose{};
    pose.size = sizeof pose;
    if (s_mod->api().hero_pose(&pose) == OPENRAC_HOOK_OK) {
        const Vec3 position{pose.position[0], pose.position[1], pose.position[2]};
        s_hero_before = s_have_hero ? s_hero_after : position;
        s_hero_after = position;
        s_have_hero = true;
        s_facing = pose.rotation[2];
        s_health = pose.health;
        const bool parked = eye_parked();
        if (parked && !s_eye_parked) {
            s_eye_parked_at = s_hero_before;
            log_line(1, "[vr-view] the body no longer carries the eye: it stays where he last stood alive");
        }
        if (!parked && s_eye_parked) {
            log_line(1, "[vr-view] the eye is the body's again");
        }
        s_eye_parked = parked;
        sample_seat();
        sample_ship();
        sample_origin();
        sample_scene();
        shots_and_hits(pose);
    } else {
        s_have_hero = false;
        s_eye_parked = false;
    }
}

#if !defined(__ANDROID__)
// The PC test's items (vr/tests/*.sim), never in the APK: QUEST_SIM_GIVE="15,16" owns those items
// with 50 rounds each, and QUEST_SIM_HOLD=15 takes one into the hand, once in play. The tables are
// the game's own (item_available, weapon_ammo_counts; Rerac-vr's NTSC 0x13d4c0 / 0x13d428).
bool readable(gaddr address, uint32_t bytes);
Vec3 read_vec(gaddr at);

void sim_give() {
    static bool done = false;
    static int calm = 0;
    // Not in the level's opening (states from 100), whose end puts him back where it began: he has
    // been in control for half a second.
    calm = s_have_hero && phase_now() == OPENRAC_PLAY_PLAYING && ref<int32_t>(s_disc->hero + kHeroState) < 100 ? calm + 1 : 0;
    static int settle = 0;
    if (done && settle > 0 && --settle == 0) {
        s_facing = get_float(s_disc->hero + kHeroFacing);
        s_recentre_wanted = true;
    }
    // QUEST_SIM_KILL=n: n ticks of control in, his hit points none for 200 ticks, then given back:
    // what the view does while the body is lost, and after. (Moving him off the level only has the
    // game put him straight back.)
    static int controlled = 0;
    static int dead_for = -1;
    static int32_t hit_points = 0;
    if (const char* kill = std::getenv("QUEST_SIM_KILL"); kill != nullptr && s_have_hero) {
        if (dead_for < 0 && calm > 0 && ++controlled >= std::atoi(kill)) {
            hit_points = ref<int32_t>(s_disc->hero + 0x22A8u);
            dead_for = 200;
            log_line(1, "[sim] his hit points taken");
        }
        if (dead_for > 0) {
            ref<int32_t>(s_disc->hero + 0x22A8u) = 0;
            if (--dead_for == 0) {
                ref<int32_t>(s_disc->hero + 0x22A8u) = std::max(hit_points, 1);
                log_line(1, "[sim] his hit points given back");
            }
        }
    }
    // QUEST_SIM_PLANETS="1,3,5" puts those levels on the ship's list, unlocked, and QUEST_SIM_MENU=14 enters
    // the menu with that kind as the game does (PauseAllSounds, EnterMenuMode; 14 the ship's planet select, 10
    // the map), two seconds in control. These pages are out of a test's reach from a level's start.
    static bool menu_entered = false;
    if (const char* kind = std::getenv("QUEST_SIM_MENU"); kind != nullptr && !menu_entered && calm > 120) {
        menu_entered = true;
        const gaddr order = ref<gaddr>(s_mod->relocate(s_disc->planet_order));
        if (const char* planets = std::getenv("QUEST_SIM_PLANETS"); planets != nullptr && order >= 0x1000u) {
            int count = 0;
            for (const char* at = planets; *at != 0 && count < 20; ++count) {
                const int level = std::atoi(at);
                ref<int32_t>(order + 4u * static_cast<uint32_t>(count)) = level;
                ref<uint8_t>(s_disc->planets_unlocked + static_cast<uint32_t>(level)) = 1;
                while (*at != 0 && *at != ',') {
                    ++at;
                }
                at += *at == ',' ? 1 : 0;
            }
            if (count < 20) {
                ref<int32_t>(order + 4u * static_cast<uint32_t>(count)) = 0;
            }
        }
        using Enter = void (*)(int);
        const auto enter = reinterpret_cast<Enter>(s_mod->api().function("func_00219C70"));
        char line[100];
        std::snprintf(line, sizeof line, "[sim] the menu entered with kind %d, the ship's list at 0x%X", std::atoi(kind), order);
        log_line(1, line);
        if (enter != nullptr) {
            enter(std::atoi(kind));
        }
    }
    // QUEST_SIM_BOLTS=9000: that many bolts, once in control; what the vendor's popups ask of.
    static bool paid = false;
    if (const char* bolts = std::getenv("QUEST_SIM_BOLTS"); bolts != nullptr && !paid && calm > 0) {
        paid = true;
        ref<int32_t>(s_disc->bolts) = std::atoi(bolts);
    }
    // QUEST_SIM_PDA=1: the PDA's vendor, as the gadget opens it (open_vendor_menu with no machine).
    static bool remote = false;
    if (std::getenv("QUEST_SIM_PDA") != nullptr && !remote && calm > 120) {
        remote = true;
        log_line(1, "[sim] the PDA's vendor");
        if (void* open = s_mod->api().function("func_L00_0029BA20")) {
            reinterpret_cast<void (*)(uint32_t)>(open)(0);
        }
    }
    // QUEST_SIM_FREEZE=0 opens that kind of message box as the game does (mode_freeze_init), as above.
    static bool frozen = false;
    if (const char* kind = std::getenv("QUEST_SIM_FREEZE"); kind != nullptr && !frozen && calm > 120) {
        frozen = true;
        using Freeze = void (*)(int, int, int);
        const auto freeze = reinterpret_cast<Freeze>(s_mod->api().function("func_001FBC80"));
        log_line(1, ("[sim] the message box of kind " + std::string(kind)).c_str());
        if (freeze != nullptr) {
            freeze(std::atoi(kind), 0, 0);
        }
    }
    // QUEST_STATE_TRACE: each change of the hero's state, the game's own word for what he does.
    static int32_t state_was = -1;
    if (s_have_hero && std::getenv("QUEST_STATE_TRACE") != nullptr && ref<int32_t>(s_disc->hero + kHeroState) != state_was) {
        state_was = ref<int32_t>(s_disc->hero + kHeroState);
        char text[100];
        const Vec3 now = read_vec(s_disc->hero + 0x80u);
        std::snprintf(text, sizeof text, "[sim] hero state 0x%X at tick %llu (%.1f %.1f %.1f)", state_was, static_cast<unsigned long long>(s_tick), now.x, now.y, now.z);
        log_line(1, text);
    }
    // The bolts as the game counts them: what a broken crate or a purchase did.
    static int32_t bolts_was = -1;
    if (done && s_have_hero && ref<int32_t>(s_disc->bolts) != bolts_was) {
        bolts_was = ref<int32_t>(s_disc->bolts);
        log_line(1, ("[sim] bolts " + std::to_string(bolts_was)).c_str());
    }
    // The given items' rounds, logged as the game spends them: each hand's firing counted by the game.
    static int32_t rounds[40] = {};
    if (done && s_have_hero) {
        if (const char* give = std::getenv("QUEST_SIM_GIVE")) {
            for (const char* p = give; *p != 0;) {
                const int id = std::atoi(p);
                if (id > 0 && id < 40) {
                    const int32_t now = ref<int32_t>(s_disc->ammo + 4u * static_cast<uint32_t>(id));
                    if (now != rounds[id]) {
                        log_line(1, ("[sim] item " + std::to_string(id) + ": " + std::to_string(now) + " rounds").c_str());
                        rounds[id] = now;
                    }
                }
                while (*p != 0 && *p != ',') {
                    ++p;
                }
                if (*p == ',') {
                    ++p;
                }
            }
        }
    }
    if (done || calm < 25) {
        return;
    }
    done = true;
    settle = 2;
    sim_ready = true;  // QUEST_SIM_ALIGN's mark
    const gaddr owned = s_disc->owned;
    const gaddr ammo = s_disc->ammo;
    std::string line = "[sim] items given:";
    if (const char* give = std::getenv("QUEST_SIM_GIVE")) {
        for (const char* p = give; *p != 0;) {
            const int id = std::atoi(p);
            if (id > 0 && id < 40) {
                ref<uint8_t>(owned + static_cast<uint32_t>(id)) = 1;
                ref<int32_t>(ammo + 4u * static_cast<uint32_t>(id)) = 50;
                line += " " + std::to_string(id);
            }
            while (*p != 0 && *p != ',') {
                ++p;
            }
            if (*p == ',') {
                ++p;
            }
        }
    }
    // The hittable mobys nearest Ratchet, for a test to walk to.
    {
        const gaddr any = ref<gaddr>(s_disc->hero + kHeroBody);
        const int any_slot = any != 0 ? s_mod->moby_slot_of(any) : -1;
        if (any_slot >= 0) {
            const gaddr base = any - static_cast<gaddr>(any_slot) * 0x100u;
            std::vector<std::pair<float, std::string>> near;
            for (const openrac_moby& m : s_mod->moby_list()) {
                const gaddr address = base + static_cast<gaddr>(m.slot) * 0x100u;
                if (m.state >= 0x80 || (ref<uint16_t>(address + 0x34u) & 0x4000u) == 0) {
                    continue;
                }
                const Vec3 d = Vec3{m.position[0], m.position[1], m.position[2]} - s_hero_after;
                char text[120];
                std::snprintf(text, sizeof text, "slot %d class %d at (%.1f %.1f %.1f) %.1f away", m.slot, m.oclass,
                              m.position[0], m.position[1], m.position[2], length(d));
                near.emplace_back(length(d), text);
            }
            std::sort(near.begin(), near.end());
            for (std::size_t i = 0; i < near.size() && i < 8; ++i) {
                log_line(1, ("[sim] hittable: " + near[i].second).c_str());
            }
        }
    }
    // QUEST_SIM_NEAR=class: Ratchet put 1 unit before the nearest moby of that class, facing it.
    if (const char* near = std::getenv("QUEST_SIM_NEAR")) {
        const int oclass = std::atoi(near);
        float best = 1e9f;
        Vec3 target;
        for (const openrac_moby& m : s_mod->moby_list()) {
            const Vec3 at{m.position[0], m.position[1], m.position[2]};
            if (m.oclass == oclass && m.state < 0x80 && length(at - s_hero_after) < best) {
                best = length(at - s_hero_after);
                target = at;
            }
        }
        if (best < 1e8f) {
            Vec3 d = target - s_hero_after;
            d.z = 0;
            d = d * (1.0f / std::max(length(d), 0.001f));
            // QUEST_SIM_NEAR_AT="away,up,degrees": another place about it (a turret asks to be stood
            // on), the approach turned by those degrees (a row of crates is taken from its side).
            float away = 1.0f, up = 0.0f, degrees = 0.0f;
            if (const char* at = std::getenv("QUEST_SIM_NEAR_AT")) {
                std::sscanf(at, "%f,%f,%f", &away, &up, &degrees);
            }
            const float turned = degrees * kPi / 180.0f;
            d = {d.x * std::cos(turned) - d.y * std::sin(turned), d.x * std::sin(turned) + d.y * std::cos(turned), 0.0f};
            const Vec3 to = target - d * away;
            put_float(s_disc->hero + 0x80u, to.x);  // motion.pos
            put_float(s_disc->hero + 0x84u, to.y);
            put_float(s_disc->hero + 0x88u, target.z + up);
            // QUEST_SIM_DONE=1: the moby's mission is marked as Batalia's turret wants it (PAL).
            if (std::getenv("QUEST_SIM_DONE") != nullptr) {
                for (const openrac_moby& m : s_mod->moby_list()) {
                    if (m.oclass == oclass) {
                        const gaddr any = ref<gaddr>(s_disc->hero + kHeroBody);
                        const gaddr moby = any - static_cast<gaddr>(s_mod->moby_slot_of(any) - m.slot) * 0x100u;
                        const int mission = ref<uint8_t>(moby + 0xB0u);
                        ref<uint8_t>(0x0014C150u + static_cast<uint32_t>(s_mod->loaded_level()) * 16u
                                     + static_cast<uint32_t>(mission)) = 0xFF;
                        // Too late for its start, which opens the turret's cover: opened here.
                        put_float(ref<gaddr>(moby + 0x78u) + 0xA8u, 1.5707964f);
                    }
                }
            }
            put_float(s_disc->hero + kHeroFacing, std::atan2(d.y, d.x));  // motion.rot's yaw
            s_recentre_wanted = true;
            // QUEST_SIM_GRIP=1: the left grip closed from here, not before: held on the ground it crouches.
            sim_left_grip = std::getenv("QUEST_SIM_GRIP") != nullptr;
            char text[120];
            std::snprintf(text, sizeof text, "[sim] put before class %d at (%.1f %.1f %.1f)", oclass, target.x, target.y,
                          target.z);
            log_line(1, text);
        }
    }
    // QUEST_SIM_RAIL="i,up,node": the level's rails logged (PAL's table), and Ratchet put `up` over node `node` of rail i.
    if (const char* rail = std::getenv("QUEST_SIM_RAIL")) {
        int which = -1, node = 1;
        float up = 0.3f;
        std::sscanf(rail, "%d,%f,%d", &which, &up, &node);
        const gaddr table = ref<gaddr>(level0(0x0015F7ECu));
        const int count = ref<int32_t>(level0(0x0015F7F0u));
        for (int i = 0; i < count && i < 64 && readable(table, 0x20u); ++i) {
            const gaddr entry = table + 0x20u * static_cast<uint32_t>(i);
            const gaddr owner = ref<gaddr>(entry + 0x10u);
            const int nodes = readable(owner, 32u) ? ref<int32_t>(owner) : 0;
            const Vec3 centre = read_vec(entry);
            const Vec3 first = nodes > 0 ? read_vec(owner + 16u) : Vec3{};
            char text[140];
            std::snprintf(text, sizeof text, "[sim] rail %d kind %d, %d nodes, radius %.0f about (%.0f %.0f %.0f), first (%.1f %.1f %.1f)", i,
                          ref<int32_t>(entry + 0x14u), nodes, get_float(entry + 12u), centre.x, centre.y, centre.z, first.x, first.y, first.z);
            log_line(1, text);
            if (i == which && nodes > node + 1) {
                Vec3 at = read_vec(owner + 16u + 16u * static_cast<uint32_t>(node));
                const Vec3 next = read_vec(owner + 32u + 16u * static_cast<uint32_t>(node));
                at = nodes == 2 ? (at + next) * 0.5f : at;  // a cable: its middle
                put_float(s_disc->hero + 0x80u, at.x);
                put_float(s_disc->hero + 0x84u, at.y);
                put_float(s_disc->hero + 0x88u, at.z + up);
                put_float(s_disc->hero + kHeroFacing, std::atan2(next.y - at.y, next.x - at.x));
                ref<int16_t>(s_disc->hero + 0x1CAu) = 120;  // the rail catch's own grace: a test's drop takes it wherever it lands
                s_recentre_wanted = true;
            }
        }
    }
    if (const char* hold = std::getenv("QUEST_SIM_HOLD")) {
        ref<int32_t>(s_disc->hero + 0x20B8u) = std::atoi(hold);  // the pending item, as change_items
        line += std::string(", holding ") + hold;
    }
    log_line(1, line.c_str());
}
#endif

void swing_wrench();
void akimbo_tick();
void missile_diff_tick();
void place_left_gun();
void lunge_tick();
int32_t hero_state();
void watch_seat();
void ledge_tick();
void crank_tick();
void socket_tick();
void vibration_tick();
void watch_ship();
void fist_tick();
void giant_missiles();

#if !defined(__ANDROID__)
void ring_trace();
#endif

void paint_glyphs();
void release_roll();

#if !defined(__ANDROID__)
// PC test aid (QUEST_HANG_TRACE=1): each tick he hangs and the few after, what of the hero block and
// his body's moby the game's tick changed (words, before and after), to find what lets go of a ledge.
void hang_trace() {
    static const bool on = std::getenv("QUEST_HANG_TRACE") != nullptr;
    constexpr uint32_t kBytes = 0x2304u;
    static std::vector<uint8_t> hero_was(kBytes), body_was(0x100u);
    static int after = 0;
    if (!on || !s_have_hero) {
        return;
    }
    const gaddr hero = s_disc->hero;
    const int32_t state = ref<int32_t>(hero + 0x2084u);
    const bool hanging = state >= 0x18 && state <= 0x1B;
    const gaddr body = ref<gaddr>(hero + 0x2080u);
    openrac::Memory memory = s_mod->memory();
    const uint8_t* now = static_cast<const uint8_t*>(memory.pointer(hero));
    if (hanging || after > 0) {
        std::string line = "[vr-hang] tick " + std::to_string(s_tick) + " state " + std::to_string(state) + ":";
        // The moby the ledge is on: the hang follows it.
        const gaddr ledge = ref<gaddr>(hero + 0x4F8u);
        if (ledge != 0 && s_mod->moby_slot_of(ledge) >= 0) {
            char on[120];
            std::snprintf(on, sizeof on, " [ledge moby class %d state %d at %.3f %.3f %.3f]", ref<int16_t>(ledge + 0xA6u),
                          ref<uint8_t>(ledge + 0x20u), get_float(ledge + 0x10u), get_float(ledge + 0x14u), get_float(ledge + 0x18u));
            line += on;
            // Every other moby within 3 of it, by slot: what of the stack changes.
            const Vec3 at_ledge{get_float(ledge + 0x10u), get_float(ledge + 0x14u), get_float(ledge + 0x18u)};
            for (const openrac_moby& m : s_mod->moby_list()) {
                const Vec3 d = Vec3{m.position[0], m.position[1], m.position[2]} - at_ledge;
                if (length(d) < 3.0f && m.slot != s_mod->moby_slot_of(ledge)) {
                    char near[80];
                    std::snprintf(near, sizeof near, " {slot %d class %d state %d z %.2f}", m.slot, m.oclass, m.state, m.position[2]);
                    line += near;
                }
            }
        }
        int shown = 0;
        for (uint32_t at = 0; at < kBytes && shown < 60; at += 4u) {
            uint32_t a, b;
            std::memcpy(&a, hero_was.data() + at, 4);
            std::memcpy(&b, now + at, 4);
            // Not the slot words, timers and the rest that move every tick: the motion, ledge, state.
            if (a != b && (at < 0x600u || (at >= 0x2080u && at < 0x20C0u) || (at >= 0x22A0u && at < 0x2300u))) {
                char word[40];
                std::snprintf(word, sizeof word, " %X:%X>%X", at, a, b);
                line += word;
                ++shown;
            }
        }
        log_line(1, line.c_str());
        if (body != 0 && s_mod->moby_slot_of(body) >= 0) {
            std::string moby = "[vr-hang] body:";
            const uint8_t* b_now = static_cast<const uint8_t*>(memory.pointer(body));
            for (uint32_t at = 0; at < 0x100u; at += 4u) {
                uint32_t a, b;
                std::memcpy(&a, body_was.data() + at, 4);
                std::memcpy(&b, b_now + at, 4);
                if (a != b && at != 0x50u && at != 0x54u && at != 0x68u && at != 0x6Cu) {
                    char word[40];
                    std::snprintf(word, sizeof word, " %X:%X>%X", at, a, b);
                    moby += word;
                }
            }
            log_line(1, moby.c_str());
        }
    }
    after = hanging ? 8 : std::max(after - 1, 0);
    std::memcpy(hero_was.data(), now, kBytes);
    if (body != 0 && s_mod->moby_slot_of(body) >= 0) {
        std::memcpy(body_was.data(), memory.pointer(body), 0x100u);
    }
}
#endif

void on_tick() {
#if !defined(__ANDROID__)
    hang_trace();
#endif
    // The level drawn under a menu, run or not ("The menu in the world"): the port forgets it with each load.
    if (s_options.menu_in_world && !s_options.screen) {
        s_mod->api().play_overlay_pause(s_mod->handle(), s_options.first_person ? 2 : 0);
    }
    release_roll();
    paint_glyphs();
    // Without the port's call before each capture (patches/), it is all done here, after it: the
    // game's next update may show the body again before the next frame.
    if (!s_capture_hook) {
        sample_hero();
        hide_body();
    }
    swing_wrench();
    if (s_have_hero) {
        lunge_tick();
    }
    watch_seat();
    ledge_tick();
    crank_tick();
    socket_tick();
    vibration_tick();
    akimbo_tick();
    watch_ship();
    fist_tick();
    giant_missiles();
#if !defined(__ANDROID__)
    sim_give();
    ring_trace();
    missile_diff_tick();
#endif
}

// ---- Vibration ----

// A pulse by how each thing feels, Rerac-vr's vr/haptics.rs: a click as an item comes out, a kick for
// a round used (what the game counts, so a refused shot is none), a rumble while a pack works. A hit on
// Ratchet shakes both hands; the left has a share of what a gun held in both does.

struct Pulse {
    float strength;
    float seconds;
    float hertz;
};

Pulse item_comes_out(int32_t item) {
    switch (item) {
        case 8: return {0.26f, 0.020f, 155.0f};
        case 9: return {0.62f, 0.048f, 55.0f};
        case 10: return {0.28f, 0.024f, 130.0f};
        case 11: return {0.70f, 0.042f, 65.0f};
        case 12: return {0.30f, 0.018f, 175.0f};
        case 13: return {0.48f, 0.028f, 105.0f};
        case 14: return {0.36f, 0.030f, 90.0f};
        case 15: return {0.34f, 0.018f, 185.0f};
        case 16: return {0.46f, 0.034f, 75.0f};
        case 17: return {0.30f, 0.020f, 160.0f};
        case 18: return {0.66f, 0.045f, 50.0f};
        case 19: return {0.40f, 0.016f, 200.0f};
        case 20: return {0.38f, 0.027f, 120.0f};
        case 21: return {0.32f, 0.024f, 150.0f};
        case 22: return {0.48f, 0.038f, 60.0f};
        case 23: return {0.78f, 0.040f, 55.0f};
        case 24: return {0.26f, 0.018f, 190.0f};
        case 25: return {0.30f, 0.030f, 100.0f};
        case 26: return {0.24f, 0.017f, 170.0f};
        case 27: return {0.28f, 0.022f, 140.0f};
        case 31: return {0.24f, 0.018f, 195.0f};
        case 32: return {0.22f, 0.016f, 180.0f};
        default: return {0.25f, 0.020f, 140.0f};
    }
}

// Each item's take-out accents (Rerac-vr's deploy_profile): the click's strength at each mechanical step.
std::vector<float> item_accents(int32_t item) {
    switch (item) {
        case 8: return {0.55f, 1.0f};
        case 9: return {0.4f, 0.65f, 1.0f, 0.8f};       // the suction chamber and front jaws
        case 10: return {0.5f, 1.0f, 0.65f};
        case 11: return {0.5f, 0.85f, 0.6f, 1.0f};      // the launcher's rails, then the lock
        case 12: return {0.6f, 0.45f, 1.0f};
        case 13: return {0.4f, 0.7f, 0.5f, 1.0f};       // the scope's hinge and the screen's detent
        case 14: return {0.5f, 1.0f, 0.6f};
        case 15: return {0.45f, 1.0f, 0.65f};
        case 16: return {0.4f, 0.75f, 1.0f};
        case 17: return {0.35f, 1.0f, 0.6f};
        case 18: return {0.35f, 0.6f, 1.0f};
        case 19: return {0.4f, 0.7f, 0.45f, 1.0f};
        case 20: return {0.45f, 0.8f, 1.0f, 0.55f};
        case 21: return {0.5f, 0.7f, 0.4f, 1.0f};
        case 22: return {0.45f, 0.7f, 1.0f};
        case 23: return {0.3f, 0.5f, 0.75f, 0.6f, 1.0f};
        case 24: return {0.4f, 1.0f, 0.65f, 0.5f};
        case 25: return {0.5f, 0.8f, 1.0f};
        case 26: return {0.4f, 0.65f, 1.0f};
        case 27: return {0.5f, 0.75f, 1.0f};
        case 31: return {0.6f, 0.4f, 1.0f};
        case 32: return {0.5f, 1.0f, 0.6f};
        default: return {0.6f, 1.0f};
    }
}

Pulse round_used(int32_t item) {
    switch (item) {
        case 9: return {0.0f, 0.0f, 0.0f};  // the Suck Cannon's own, in feel_events
        case 10: return {0.48f, 0.060f, 90.0f};
        case 11: return {0.90f, 0.110f, 55.0f};
        case 13: return {0.72f, 0.090f, 65.0f};
        case 15: return {0.35f, 0.025f, 160.0f};
        case 17: return {0.36f, 0.035f, 150.0f};
        case 20: return {0.50f, 0.070f, 100.0f};
        case 23: return {0.80f, 0.065f, 70.0f};
        case 24: return {0.30f, 0.060f, 120.0f};
        case 25: return {0.25f, 0.045f, 125.0f};
        default: return {0.28f, 0.030f, 140.0f};
    }
}

void buzz(Hand hand, const Pulse& pulse, float share = 1.0f) {
    if (s_options.haptics && s_headset && pulse.strength > 0.0f) {
        s_headset->haptic(hand, pulse.strength * share, pulse.seconds, pulse.hertz);
    }
}

void buzz_later(float seconds, Hand hand, const Pulse& pulse);

// Rerac-vr's double: a beat, and a lighter, higher one 75 ms on.
void buzz_double(Hand hand, float strength) {
    buzz(hand, {strength, 0.035f, 140.0f});
    buzz_later(0.075f, hand, {strength * 0.75f, 0.030f, 180.0f});
}

// A round used, in a hand: the Glove of Doom's and the Drone Device's are doubles (Rerac-vr's).
void feel_round(Hand hand, int32_t item, float share = 1.0f) {
    if (item == 20 || item == 24) {
        buzz_double(hand, round_used(item).strength * share);
        return;
    }
    buzz(hand, round_used(item), share);
}

// A hit on Ratchet in both hands.
void shots_and_hits(const openrac_pose& pose) {
    static int health = -1;
    if (!s_options.haptics || !s_headset || !s_started) {
        health = pose.health;
        return;
    }
    if (health >= 0 && pose.health < health) {  // Rerac-vr's hurt: a heavy low beat, and a lower one after it
        for (const Hand hand : {kLeft, kRight}) {
            buzz(hand, {1.0f, 0.135f, 55.0f});
            buzz_later(0.16f, hand, {0.65f, 0.060f, 35.0f});
        }
    }
    health = pose.health;
}

// ---- The Hologuise ----
//
// Rerac-vr's (vr/game_input.rs): in the disguise (his vehicle mode 3) B, or A, takes it off; the
// trigger salutes, though not the press that put it on, and so does the right hand raised open
// (pad.h Salute). The grips only close its hands (change_items leaves the wrench out of it).

constexpr uint8_t kBodyDisguise = 3;
Salute s_salute;
bool s_disguised = false;
bool s_disguise_trigger_spent = false;  // the trigger that put it on, still held

// Once a frame.
void disguise_gesture() {
    const bool now = s_have_hero && s_have_frame && s_health > 0
                     && ref<uint8_t>(s_disc->hero + kHeroVehicleMode) == kBodyDisguise;
    const bool trigger = s_have_frame && s_frame.controls.right_trigger > kTriggerOn;
    if (now && !s_disguised && trigger) {
        s_disguise_trigger_spent = true;
    }
    if (!trigger) {
        s_disguise_trigger_spent = false;
    }
    if (now != s_disguised) {
        log_line(1, now ? "[vr-disguise] on" : "[vr-disguise] off");
    }
    s_disguised = now;
    const bool was = s_salute.active;
    if (!now || s_frame.controls.right_click || !s_frame.hand_valid[kRight]) {
        s_salute.reset();
    } else {
        s_salute.update(s_frame.head, s_frame.hand_grip[kRight], s_frame.controls.right_grip);
    }
    if (s_salute.active && !was) {
        log_line(1, "[vr-disguise] the hand salutes");
    }
}

// ---- The pad ----

bool throws_by_hand();
bool punching();
bool hand_punching();
uint16_t pointed_key();
bool vendor_open();
uint16_t vendor_pulse();
bool left_supports();
void ledge_context(PadContext& context);
bool dive_wanted();
bool socket_pressed();
bool missile_flying();
int32_t hero_mode();
constexpr int32_t kModeBoard = 0x16;
constexpr uint32_t kHeroOffGround = 0x30Cu, kHeroTrickInput = 0x88Du;

void crank_stick(Controls& c);
bool in_puzzle();
bool s_withdraw = false;  // the player left the Trespasser's rings
gaddr s_left_moby = 0;    // Akimbo: the left hand's gun
int s_off_side = kLeft;        // the hand the second gun is in: the right while the left alone hangs on
int32_t s_off_item = 0;        // the item the second gun is
// Akimbo: the left hand's Walloper or Swingshot leads (Rerac-vr's off_hand.rs lead): its use is a hero
// state, so for the use its slot is the hand's, the right hand's item kept aside ("The left hand leads").
bool s_leading = false;

void on_pad(int port, std::uint8_t* report, int size) {
    if (port != 0 || !s_have_frame) {
        return;
    }
    PadContext context;
    context.in_menu = s_menu_input;
    context.screen = s_options.screen;
    context.first_person = s_steers;
    context.third_person_walk = !s_steers && !s_options.first_person && !s_options.screen && s_have_frame && s_have_hero
                                && s_manned == 0 && !in_ship() && hero_on_foot() && phase_now() == OPENRAC_PLAY_PLAYING;
    context.head_turn = s_have_frame ? wrap_angle(tracking_yaw(s_frame.head.orientation) - s_recentre_yaw) : 0.0f;
    context.swing = s_swinging;
    context.throw_by_hand = throws_by_hand();
    context.punch = hand_punching();
    context.left_support = left_supports();
    context.trigger_held_back = s_trigger_back || s_disguise_trigger_spent;
    context.disguised = s_disguised;
    context.salute = s_salute.active;
    if (s_have_hero && !context.in_menu) {
        context.mounted = ref<int32_t>(s_disc->hero + kHeroState) == kHeroStateMounted;
        context.flying = context.mounted && s_manned == 0;
    }
    context.head_yaw = s_head_yaw;
    context.camera_yaw = s_steers ? camera_yaw() : 0.0f;
    context.quick_select = s_quick_select;
    ledge_context(context);
    context.dive = dive_wanted();
    context.raw_stick = missile_flying();
    context.board = s_have_hero && !context.in_menu && hero_mode() == kModeBoard;
    // A trick's spin takes the stick as it is; so does a game's own camera.
    context.board_steers = context.board && s_steers
                           && (ref<int16_t>(s_disc->hero + kHeroOffGround) == 0 || ref<uint8_t>(s_disc->hero + kHeroTrickInput) == 0);
    context.board_yaw = context.board ? get_float(s_disc->hero + kHeroFacing) : 0.0f;
    context.punch = context.punch || socket_pressed();
    context.puzzle = !context.in_menu && in_puzzle();
    context.left_weapon = !context.in_menu && s_left_moby != 0 && s_off_side == kLeft;
    context.left_ring = !context.in_menu && s_left_ring;
    context.skip_held = s_skip_held;
    context.lead = !context.in_menu && s_leading;
    context.menu_key = context.in_menu ? pointed_key() : 0;
    context.menu_pointing = context.in_menu && s_page_targets;
    context.shopping = s_shopping;
    context.menu_aimed = context.in_menu && vendor_open();
    context.menu_pulse = context.in_menu ? vendor_pulse() : 0;

    Controls controls = s_frame.controls;
    crank_stick(controls);
    const VirtualPad pad = s_pad_mapper.map(controls, context);
    s_withdraw = s_withdraw || pad.withdraw;
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

// The bodies face the player's heading in their free states (Rerac-vr's hero/tracked.rs tracked_free_facing): Clank
// idle, walking, falling, jumping and 0x50; Giant Clank in all but his death (0x62), so his missiles and beam go where
// the player looks; the Hologuise idle, walking, falling and saluting. Clank and the Hologuise then walk along the stick
// beside it (tracked_stick_steered), Giant Clank along his facing, as there.
bool body_faces_heading(bool* stick_steered = nullptr) {
    if (!s_steers || !s_have_hero || s_health <= 0) {
        return false;
    }
    const uint8_t body = ref<uint8_t>(s_disc->hero + kHeroVehicleMode);
    const int32_t state = ref<int32_t>(s_disc->hero + kHeroState);
    bool free = false;
    if (body == kBodyClank) {
        free = (state >= 0x43 && state <= 0x45) || state == 0x49 || state == 0x50;
    } else if (body == kBodyGiant) {
        free = state >= 0x5A && state <= 0x61;
    } else if (body == kBodyDisguise) {
        free = (state >= 0x53 && state <= 0x55) || state == 0x59;
    }
    if (stick_steered != nullptr) {
        *stick_steered = free && body != kBodyGiant;
    }
    return free;
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
    if (mode != kTurnFollowsMoveYaw || (strafe_kind() == RAC1_STRAFE_NONE && !body_faces_heading())) {
        original(mode, accel, damping, limit);
        return;
    }
    turn_toward(original, s_aim_yaw, mode, accel, damping, limit);
}

template <class Original>
void set_planar_velocity(Original original, float angle) {
    if (!(angle <= RAC1_STRAFE_PI)) {
        bool stick_steered = false;
        const int kind = body_faces_heading(&stick_steered) && stick_steered ? RAC1_STRAFE_LOCKED : strafe_kind();
        angle = rac1_strafe_planar_angle(kind, angle, get_float(s_disc->hero + kHeroMoveYaw));
    }
    original(angle);
}

void give_room_step();
void swim_tick();
void board_tick();

template <class Original>
void update_motion_input(Original original) {
    give_room_step();
    swim_tick();
    board_tick();
    // A body in a free state takes the heading outright, before its state's code moves it (Rerac-vr's tracked_face);
    // the game builds its matrix from the facing after that code.
    if (body_faces_heading()) {
        put_float(s_disc->hero + kHeroFacing, s_aim_yaw);
        static float logged = 100.0f;
        if (std::abs(wrap_angle(s_aim_yaw - logged)) > 0.5f) {
            logged = s_aim_yaw;
            char line[100];
            std::snprintf(line, sizeof line, "[vr-body] body %d faces the heading: yaw %.2f (state 0x%X)",
                          ref<uint8_t>(s_disc->hero + kHeroVehicleMode), s_aim_yaw, ref<int32_t>(s_disc->hero + kHeroState));
            log_line(1, line);
        }
    }
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

// ---- What is in the hand ----

Vec3 anchor_now(float alpha);
Vec3 read_vec(gaddr at);
Vec3 world_from_head();
constexpr int32_t kHydroItemId = 22, kTrespasserItemId = 26;
Pose in_frame(const Rig& rig, const Pose& pose, Vec3 anchor);

// The class's first joint list's end, from the class's own tables (hand.h says why): a word of the
// game's data each, checked as the memory view checks no bounds. Kept by class.
constexpr uint32_t kMobyClass = 0x24u;
constexpr uint32_t kMobyCommand = 0xBCu;
constexpr uint32_t kMobyScale = 0x2Cu;
constexpr uint32_t kMobyOclass = 0xA6u;
constexpr uint32_t kClassJointCount = 0x08u;
constexpr uint32_t kClassSkeleton = 0x14u;
constexpr uint32_t kClassJoints = 0x1Cu;
constexpr uint32_t kMatrixSize = 0x40u;
constexpr gaddr kGuestRam = 0x02000000u;

bool in_ram(gaddr address) { return address >= 0x1000u && address < kGuestRam - 0x1000u; }

// The port's own statics in game memory (runtime guest.h GUEST_STATICS): the rows it gives a list.
bool in_port_statics(gaddr address) { return address >= 0xE0200000u && address < 0xE0300000u - 0x1000u; }

// The game's stack, which the port keeps in its own region (guest.h GUEST_STACK_BOTTOM .. TOP).
bool in_port_stack(gaddr address) { return address >= 0xE0000000u && address < 0xE0100000u; }

// `bytes` from `address` lie in either.
bool readable(gaddr address, uint32_t bytes) {
    return (in_ram(address) && in_ram(address + bytes)) || (in_port_statics(address) && in_port_statics(address + bytes))
           || (in_port_stack(address) && in_port_stack(address + bytes));
}

// `list`: which joint list (the first by default; the wrench's second ends at its head).
const JointFrame& class_joint(int oclass, gaddr moby, int list_index = 0) {
    static std::map<std::pair<int, int>, JointFrame> known;
    const auto found = known.find({oclass, list_index});
    if (found != known.end()) {
        return found->second;
    }
    JointFrame frame;
    const gaddr cls = ref<gaddr>(moby + kMobyClass);
    if (in_ram(cls)) {
        const gaddr joints = ref<gaddr>(cls + kClassJoints);
        const gaddr skeleton = ref<gaddr>(cls + kClassSkeleton);
        if (in_ram(joints) && in_ram(skeleton) && ref<int32_t>(joints) > list_index) {
            const gaddr list = ref<gaddr>(joints + 4u + 4u * static_cast<uint32_t>(list_index));
            if (in_ram(list)) {
                const int chain = ref<int16_t>(list);
                const int last = chain > 0 ? ref<uint8_t>(list + 4u + static_cast<uint32_t>(chain - 1)) : -1;
                if (last >= 0 && last < ref<uint8_t>(cls + kClassJointCount)) {
                    float matrix[16];
                    for (uint32_t i = 0; i < 16u; ++i) {
                        matrix[i] = get_float(skeleton + kMatrixSize * static_cast<uint32_t>(last) + 4u * i);
                    }
                    frame = joint_frame(matrix);
                }
            }
        }
    }
    char line[120];
    std::snprintf(line, sizeof line, "[vr-hand] class %d: joint list %d's end %s", oclass, list_index,
                  frame.valid ? "read" : "none");
    log_line(1, line);
    return known.emplace(std::make_pair(oclass, list_index), frame).first->second;
}

// The wrench is shown a fifth bigger in the hand (Rerac-vr's); given back when the head leaves it.
gaddr s_grown = 0;
float s_grown_scale = 0;

void shrink_back() {
    if (s_grown != 0) {
        if (s_mod->moby_slot_of(s_grown) >= 0) {
            put_float(s_grown + kMobyScale, s_grown_scale);
        }
        s_grown = 0;
    }
}

// What is in Ratchet's hand is a moby of its own, which the hero's update puts at his hand's joint
// each tick (update_hero_item_mobys). Before the frame is captured, it is put in the player's right
// hand instead, as Rerac-vr mounts each kind of item (hand.h).
// The item in Ratchet's hand, a live moby, or 0: the slot's word is a moby only while the hero holds
// one, and anything else (between items, in a load) is not written through, as the memory view
// checks no bounds.
gaddr hand_moby() {
    const gaddr moby = ref<gaddr>(s_disc->hero + kHeroSlot0);
    if (moby == 0 || s_mod->moby_slot_of(moby) < 0 || ref<uint8_t>(moby + kMobyState) >= 0x80) {
        return 0;
    }
    return moby;
}

// ---- Two-handed weapons ----
//
// support.h: the weapons Ratchet holds with both arms are heavy in one hand, and the left hand
// squeezed on the foregrip holds them too (Rerac-vr's blaster.rs). Weighed each frame.

constexpr uint32_t kHeroArms = 0x20ABu;  // the hand item's arms: its definition's +0x18, 2 for both

Support s_support;
bool s_support_on = false;    // the hand's item is weighed this frame
bool s_left_squeezed = false;
bool s_left_support = false;  // the left squeeze began on the foregrip: not R1 until it lets go

// The hand items Rerac-vr weighs: the guns and gadgets, not the wrench or the gloves.
bool supported_item(int item) { return ((item >= 9 && item <= 27) || item == 31 || item == 32) && !glove_item(item); }

const Pose* left_grip_pose() { return s_frame.hand_valid[kLeft] ? &s_frame.hand_grip[kLeft] : nullptr; }

// The left grip holds the foregrip (or took it when squeezed): it is not R1.
bool left_supports() { return s_left_support || s_support.held; }

// Where the weapon's grip is: the right hand, or behind it for a heavy weapon, or turned to the left.
Pose weapon_grip() {
    return s_support_on ? weapon_hand(s_frame.hand_grip[kRight], left_grip_pose(), s_support)
                        : s_frame.hand_grip[kRight];
}

bool left_hand_holds();

void support_step() {
    using clock = std::chrono::steady_clock;
    static clock::time_point last = clock::now();
    const clock::time_point now = clock::now();
    const float dt = std::clamp(std::chrono::duration<float>(now - last).count(), 0.0f, 0.1f);
    last = now;

    const float grip = s_have_frame ? s_frame.controls.left_grip : 0.0f;
    const bool squeeze = s_left_squeezed ? grip > kSupportLet : grip > kSupportTake;
    const bool began = squeeze && !s_left_squeezed;
    s_left_squeezed = squeeze;
    if (!squeeze) {
        s_left_support = false;
    }

    const gaddr moby = hand_moby();
    const int item = moby != 0 ? ref<int32_t>(s_disc->hero + kHeroSlot0 + kSlotItem) : 0;
    s_support_on = s_options.two_handed && s_options.hand_weapon && s_steers && s_have_frame
                   && s_frame.hand_valid[kRight] && supported_item(item) && !s_leading && !left_hand_holds();
    if (!s_support_on) {
        s_support.held = false;
        s_support.release();
        s_left_support = false;
        return;
    }
    const bool two = ref<uint8_t>(s_disc->hero + kHeroArms) == 2;
    const bool held = s_support.held;
    const Pose& right = s_frame.hand_grip[kRight];
    step(s_support, item, two, right, left_grip_pose(), grip, dt);
    if (began && left_grip_pose() != nullptr) {
        s_left_support = s_support.reaches(right, *left_grip_pose());
    }

    static int logged_item = -1;
    if (item != logged_item) {
        logged_item = item;
        char line[96];
        std::snprintf(line, sizeof line, "[vr-hand] item %d held with %s", item,
                      two ? (item == kHydrodisplacerItem ? "both arms, not heavy" : "both arms: heavy in one hand")
                          : "one arm");
        log_line(1, line);
    }
    if (s_support.held != held) {
        log_line(1, s_support.held ? "[vr-hand] the left hand holds the foregrip" : "[vr-hand] the left hand lets go");
    }
}

// ---- Solid hands ----
//
// Rerac-vr's (vr/solid_contact.rs, the unobstructed test of its aim): what is in the hand stops at the
// world. The game's own line test runs from the eyes to the hand and on to the muzzle; where it meets
// something the item is pushed back along the way it came, with its muzzle, so it neither shows through
// a wall nor fires through it. Only the item moves: the hand stays the player's. Tools made to touch
// their pad keep touching it.

constexpr uint32_t kCollPoint = 0x20u;   // in the result
constexpr int kCollWorld = 6;
constexpr float kContactBack = 0.04f;    // left between the item and what it met, metres

bool line_blocked(Vec3 a, Vec3 b, Vec3& point) {
    using Line = int (*)(gaddr, gaddr, int, int, int);
    static const auto line = reinterpret_cast<Line>(s_mod->api().function(s_disc->coll_line));
    const gaddr body = ref<gaddr>(s_disc->hero + kHeroBody);
    if (line == nullptr || body == 0) {
        return false;
    }
    openrac::StackFrame frame(*s_mod, 32);
    const gaddr at = frame.address();
    const auto put = [&](gaddr to, Vec3 v) {
        put_float(to, v.x);
        put_float(to + 4u, v.y);
        put_float(to + 8u, v.z);
        put_float(to + 12u, 1.0f);
    };
    put(at, a);
    put(at + 16u, b);
    if (line(at, at + 16u, kCollWorld, static_cast<int>(body), 0) == 0) {
        return false;
    }
    point = read_vec(level0(s_disc->coll_result) + kCollPoint);
    return true;
}

void wrench_on_wall(const HandFrame& hand, int32_t item, bool touching);
Vec3 wrench_rebound();

bool s_left_active = false;    // Akimbo: what runs now is the left hand's
int32_t s_left_item = 0;       // what the left hand holds, 0 empty
bool left_hand_holds() { return s_vr.akimbo && s_left_item != 0; }  // and so it is no foregrip's (Rerac-vr's)

// The hand the hand slot's item is in now (the left while it leads), or, as it runs, the second item's.
int hand_side() { return s_left_active ? s_off_side : s_leading ? kLeft : kRight; }

// How far the item must come back this tick, world units. Asked of the game once a tick.
Vec3 contact_shift(const HandFrame& hand, Vec3 muzzle, int32_t item) {
    static uint64_t asked = ~0ull;
    static Vec3 shift;
    if (asked == s_tick) {
        return shift;
    }
    asked = s_tick;
    shift = {};
    if (!s_options.solid_hands || !s_have_hero || item == kHydroItemId || item == kTrespasserItemId || item == 18) {
        return shift;
    }
    const Vec3 eye = world_from_head();
    Vec3 point;
    const Vec3 back = hand.forward * (-kContactBack * s_rig.scale);
    bool touching = true;
    if (line_blocked(eye, hand.at, point)) {
        shift = point + back - hand.at;
    } else if (line_blocked(hand.at, muzzle, point)) {
        shift = point + back - muzzle;
    } else {
        touching = false;
    }
    wrench_on_wall(hand, item, touching);
    return shift;
}

// Where the item `moby` goes in hand `side` now, as item `item`, and the hand itself. `main`: the hand
// slot's own, which meets walls (contact_shift) and the right's the left foregrip (weapon_grip).
Mount mount_in(gaddr moby, HandFrame& hand, int side, int32_t item, bool main) {
    const int oclass = ref<int16_t>(moby + kMobyOclass);
    const float scale = moby == s_grown ? s_grown_scale : get_float(moby + kMobyScale);
    const Pose grip = in_frame(s_rig, main && side == kRight ? weapon_grip() : s_frame.hand_grip[side], anchor_now(1.0f));
    hand = hand_in_world(grip);
    Mount mount = mount_item(hand, oclass, item, scale, class_joint(oclass, moby), side == kLeft);
    const Vec3 shift = main ? contact_shift(hand, mount.muzzle, item) + wrench_rebound() : Vec3{};
    mount.position = mount.position + shift;
    mount.muzzle = mount.muzzle + shift;
    return mount;
}

// Where the hand item `moby` goes now (the second item's as it runs), and the hand itself.
Mount hand_mount(gaddr moby, HandFrame& hand) {
    if (s_left_active) {
        return mount_in(moby, hand, s_off_side, s_off_item, false);
    }
    return mount_in(moby, hand, hand_side(), ref<int32_t>(s_disc->hero + kHeroSlot0 + kSlotItem), true);
}

void put_mount(gaddr moby, const Mount& mount, float scale);
bool seat_wrench(gaddr wrench);
void centre_held_decoy(gaddr glove, Vec3 palm, int side, int32_t item);

constexpr int32_t kWrenchItem = 8;
constexpr float kCrankGripX = -90.0f;  // where the hand holds the wrench, along its model's shaft
float s_wrench_stretch = 1.0f;         // this tick's swing: its head reaches this much further

void place_weapon() {
    if (!s_options.hand_weapon || !hands_shown() || !s_have_frame || !s_frame.hand_valid[hand_side()]
        || (!s_leading && right_on_ledge())) {
        shrink_back();
        return;
    }
    const gaddr moby = hand_moby();
    if (moby == 0) {
        return;
    }
    // A thrown wrench (its states 10 out and 11 back) flies on its own.
    const uint8_t state = ref<uint8_t>(moby + kMobyState);
    if (state == 10 || state == 11) {
        return;
    }
    if (moby != s_grown) {
        shrink_back();
    }
    if (seat_wrench(moby)) {
        return;
    }
    const float scale = moby == s_grown ? s_grown_scale : get_float(moby + kMobyScale);
    HandFrame hand;
    Mount mount = hand_mount(moby, hand);
    if (s_wrench_stretch > 1.0f && ref<int32_t>(s_disc->hero + kHeroSlot0 + kSlotItem) == kWrenchItem) {
        // Drawn as far as its hits reach, about the hand (Rerac-vr's swing_stretch).
        const float k = scale * kWrenchScale / 1024.0f;
        mount.position = mount.position - mount.axes.x * ((s_wrench_stretch - 1.0f) * kCrankGripX * k);
        mount.axes.x = mount.axes.x * s_wrench_stretch;
    }
    put_mount(moby, mount, scale);
    centre_held_decoy(moby, hand.at, kRight, ref<int32_t>(s_disc->hero + kHeroSlot0 + kSlotItem));
}

void put_mount(gaddr moby, const Mount& mount, float scale) {
    const auto row = [&](uint32_t index, Vec3 v) {
        const gaddr address = moby + kMobyBasis + kMobyRowSize * index;
        put_float(address, v.x);
        put_float(address + 4u, v.y);
        put_float(address + 8u, v.z);
    };
    row(0, mount.axes.x);
    row(1, mount.axes.y);
    row(2, mount.axes.z);
    put_float(moby + kMobyPosition, mount.position.x);
    put_float(moby + kMobyPosition + 4u, mount.position.y);
    put_float(moby + kMobyPosition + 8u, mount.position.z);
    if (mount.scale != 1.0f) {
        s_grown = moby;
        s_grown_scale = scale;
        put_float(moby + kMobyScale, scale * mount.scale);
    }
}

// ---- Aiming with the hand ----
//
// Rerac-vr's tracked items (hero/gadgets.rs slot_item_update): around the hand item's own update,
// the hero's aim frame, facing and pitch and the game's camera are the hand's, the item already in
// the hand, so it aims and fires from its muzzle where the hand points; all is put back after. The
// first-person aim state (30, which aims from the camera) is not entered from a look (state 1), the
// ground's pitch and the arm raised at a wall are out of it, and the Blaster's auto-aim is not asked.

constexpr uint32_t kHeroRot = 0x90u;          // motion.rot: y the pitch, z the yaw
constexpr uint32_t kHeroAimRows = 0x640u;     // basis: the frame the weapons fire along
constexpr uint32_t kHeroGroundPitch = 0x2E4u;
constexpr uint32_t kHeroArmAtWall = 0x22C8u;  // s16: 1 while the gun arm is raised at a wall
constexpr int32_t kStateIdle = 0;
constexpr int32_t kStateLook = 1;             // L1 held: the game would enter its camera aim, 30
constexpr uint32_t kCameraYawOffset = 0x158u; // discs.h's camera yaw within the camera block
constexpr uint32_t kCameraMatrix = 0x00u;     // the rows the weapons read: columns -left, -up, forward
constexpr uint32_t kCameraEye = 0x140u;
constexpr uint32_t kCameraRows = 0x350u;      // forward, left, up (BuildViewMatrices builds the rest)

bool s_aiming = false;  // in a hand item's update the hand aims
Vec3 s_aim_muzzle;      // and its muzzle, the world's
int s_aim_class = -1;

// The Blaster's muzzle blast (Rerac-vr's blaster.rs fire: "if tracked { muzzle }"): each shot hits what a
// 0.42 sphere at (0.7, -0.15, 0.5) in Ratchet's own frame touches. Aimed by the hand it is at the muzzle,
// or it would break what is in front of his body whichever way the gun points: a crate he hangs from, a
// crate beside him fired past.
constexpr int16_t kBlasterClass = 168;

template <class Original>
void local_point(Original original, uint32_t out, float x, float y, float z) {
    if (s_aiming && s_aim_class == kBlasterClass && std::abs(x - 0.7f) < 1e-3f && std::abs(y + 0.15f) < 1e-3f
        && std::abs(z - 0.5f) < 1e-3f && readable(out, 16u)) {
        put_float(out, s_aim_muzzle.x);
        put_float(out + 4u, s_aim_muzzle.y);
        put_float(out + 8u, s_aim_muzzle.z);
        put_float(out + 12u, 0.0f);
        static int told = 0;
        if (told < 3) {
            ++told;
            char line[120];
            std::snprintf(line, sizeof line, "[vr-aim] the Blaster's muzzle blast at its muzzle (%.2f %.2f %.2f)",
                          s_aim_muzzle.x, s_aim_muzzle.y, s_aim_muzzle.z);
            log_line(1, line);
        }
        return;
    }
    original(out, x, y, z);
}

// The camera block (gCamera; level 00 player_camera), through the level as the yaw is.
gaddr camera_block() { return level0(s_disc->camera_yaw) - kCameraYawOffset; }

// Items the hand aims, and with them the classes whose update goes through aimed_update: every hand
// item but the wrench, whose swing is the hand's own.
bool hand_aims(gaddr moby) {
    return s_options.hand_aim && s_options.hand_weapon && s_steers && s_have_frame
           && s_frame.hand_valid[hand_side()]
           && moby != 0 && moby == hand_moby() && ref<int16_t>(moby + kMobyOclass) != kWrenchClass;
}

template <class Original>
void aimed_update(Original original, uint32_t moby) {
    if (!hand_aims(moby)) {
        original(moby);
        return;
    }
    HandFrame hand;
    const Mount mount = hand_mount(moby, hand);
    // In the hand before its update: its joints (the muzzle the game asks for) are the hand's.
    put_mount(moby, mount, get_float(moby + kMobyScale));

    openrac::Memory memory = s_mod->memory();
    const gaddr hero = s_disc->hero;
    const gaddr camera = camera_block();
    uint8_t rows[48], cam_matrix[48], cam_rows[48], eye[12], rot[8], ground[4], arm[2];
    std::memcpy(rows, memory.pointer(hero + kHeroAimRows), sizeof rows);
    std::memcpy(rot, memory.pointer(hero + kHeroRot + 4u), sizeof rot);
    std::memcpy(ground, memory.pointer(hero + kHeroGroundPitch), sizeof ground);
    std::memcpy(arm, memory.pointer(hero + kHeroArmAtWall), sizeof arm);
    std::memcpy(cam_matrix, memory.pointer(camera + kCameraMatrix), sizeof cam_matrix);
    std::memcpy(cam_rows, memory.pointer(camera + kCameraRows), sizeof cam_rows);
    std::memcpy(eye, memory.pointer(camera + kCameraEye), sizeof eye);
    const bool looking = ref<int32_t>(hero + kHeroState) == kStateLook;

    const Vec3 f = hand.forward, l = hand.left, u = hand.up;
    const auto put3 = [&](gaddr at, Vec3 v) {
        put_float(at, v.x);
        put_float(at + 4u, v.y);
        put_float(at + 8u, v.z);
    };
    put3(hero + kHeroAimRows, f);
    put3(hero + kHeroAimRows + 0x10u, l);
    put3(hero + kHeroAimRows + 0x20u, u);
    put_float(hero + kHeroRot + 4u, -std::atan2(f.z, std::sqrt(f.x * f.x + f.y * f.y)));
    put_float(hero + kHeroRot + 8u, std::atan2(f.y, f.x));
    ref<int32_t>(hero + kHeroGroundPitch) = 0;
    ref<int16_t>(hero + kHeroArmAtWall) = 0;
    put3(camera + kCameraRows, f);
    put3(camera + kCameraRows + 0x10u, l);
    put3(camera + kCameraRows + 0x20u, u);
    put3(camera + kCameraMatrix, {-l.x, -u.x, f.x});
    put3(camera + kCameraMatrix + 0x10u, {-l.y, -u.y, f.y});
    put3(camera + kCameraMatrix + 0x20u, {-l.z, -u.z, f.z});
    put3(camera + kCameraEye, mount.muzzle);
    if (looking) {
        ref<int32_t>(hero + kHeroState) = kStateIdle;
    }

    s_aiming = true;
    s_aim_muzzle = mount.muzzle;
    s_aim_class = ref<int16_t>(moby + kMobyOclass);
    original(moby);
    s_aiming = false;

    std::memcpy(memory.pointer(hero + kHeroAimRows), rows, sizeof rows);
    std::memcpy(memory.pointer(hero + kHeroRot + 4u), rot, sizeof rot);
    std::memcpy(memory.pointer(hero + kHeroGroundPitch), ground, sizeof ground);
    std::memcpy(memory.pointer(hero + kHeroArmAtWall), arm, sizeof arm);
    std::memcpy(memory.pointer(camera + kCameraMatrix), cam_matrix, sizeof cam_matrix);
    std::memcpy(memory.pointer(camera + kCameraRows), cam_rows, sizeof cam_rows);
    std::memcpy(memory.pointer(camera + kCameraEye), eye, sizeof eye);
    // The look comes back unless the update changed the state itself.
    if (looking && ref<int32_t>(hero + kHeroState) == kStateIdle) {
        ref<int32_t>(hero + kHeroState) = kStateLook;
    }
    // Three lines a class, and one each time the trigger is pulled: what each shot was aimed by.
    static int last_class = -1;
    static int logged = 0;
    static bool trigger_was = false;
    const int oclass = ref<int16_t>(moby + kMobyOclass);
    const bool trigger = s_frame.controls.right_trigger > kTriggerOn;
    if (oclass != last_class) {
        last_class = oclass;
        logged = 0;
    }
    if (logged == 0) {
        // What else the hand's slot holds with this item (its +0x24 and +0x2C mobys).
        char parts[200];
        int at = std::snprintf(parts, sizeof parts, "[vr-aim] class %d's slot also holds:", oclass);
        for (uint32_t offset : {0x24u, 0x2Cu}) {
            const gaddr other = ref<gaddr>(s_disc->hero + kHeroSlots + offset);
            if (other != 0 && other != moby && s_mod->moby_slot_of(other) >= 0 && at < 150) {
                at += std::snprintf(parts + at, sizeof parts - static_cast<std::size_t>(at),
                                    " +0x%X class %d at (%.2f %.2f %.2f);", offset,
                                    ref<int16_t>(other + kMobyOclass), get_float(other + kMobyPosition),
                                    get_float(other + kMobyPosition + 4u), get_float(other + kMobyPosition + 8u));
            }
        }
        log_line(1, parts);
        // Every other moby drawn near Ratchet's head: what could stand in the eye's way.
        const Vec3 head = s_hero_after + Vec3{0, 0, 1.0f};
        for (const openrac_moby& m : s_mod->moby_list()) {
            const Vec3 d = Vec3{m.position[0], m.position[1], m.position[2]} - head;
            if ((m.flags & OPENRAC_MOBY_HIDDEN) == 0 && static_cast<gaddr>(m.slot) != 0 && length(d) < 2.0f) {
                char near[160];
                std::snprintf(near, sizeof near, "[vr-aim] near the head: slot %d class %d at (%.2f %.2f %.2f) flags 0x%X",
                              m.slot, m.oclass, m.position[0], m.position[1], m.position[2], m.flags);
                log_line(1, near);
            }
        }
    }
    if (logged < 3 || (trigger && !trigger_was)) {
        ++logged;
        char line[200];
        std::snprintf(line, sizeof line,
                      "[vr-aim] class %d aimed by the hand: forward (%.2f %.2f %.2f) muzzle (%.2f %.2f %.2f)",
                      ref<int16_t>(moby + kMobyOclass), f.x, f.y, f.z, mount.muzzle.x, mount.muzzle.y,
                      mount.muzzle.z);
        log_line(1, line);
    }
    trigger_was = trigger;
}

template <class HookPoint>
void aim_hook(openrac::Mod& mod) {
    mod.hook<HookPoint>([](auto original, uint32_t moby) { aimed_update(original, moby); });
}

// The Blaster's target search, outside its first-person aim: none while the hand aims (Rerac-vr's
// tracked Blaster searches none), so a shot goes where the hand points.
template <class Original>
uint32_t blaster_auto_aim(Original original, int32_t blaster, uint32_t from, uint32_t yaw, uint32_t pitch) {
    if (s_aiming) {
        return 0;
    }
    return original(blaster, from, yaw, pitch);
}

// ---- Bombs thrown by hand ----
//
// Rerac-vr's (vr/bomb_glove.rs, vr/embodied.rs): the trigger holds the glove's bomb, which the game
// keeps at the glove's grip, so in the palm; let go at 1 m/s or more and it flies with the hand's
// velocity three times over (18 m/s at most) plus Ratchet's own, let go slowly and it stays. The
// hand's velocity is its tracking-space path over the last 80 ms, so the stick and turning never
// throw. The game throws it: the glove's state 3 launches the bomb it holds, one round used; the
// bomb is then given the grip and the hand's speed, with no landing reticle (the hand aims).

// The four gloves keep the same vars (the bomb, mine, Doom and Decoy gloves' updates): the grip,
// and the moby they hold and throw. What each throws keeps its velocity at its own place (Rerac-vr's
// bomb.rs, mine.rs, doom_canister.rs, decoy.rs pv): the Decoy's at 0x10, the others' at 0.
constexpr int32_t kBombGloveItem = 10;
constexpr int32_t kDecoyGloveItem = 25;
constexpr uint32_t kMobyPvars = 0x78u;
constexpr uint32_t kGloveGrip = 0x40u;
constexpr uint32_t kGloveHeld = 0x50u;
constexpr uint32_t kBombNoReticle = 0x56u;   // the bomb's: no landing reticle
constexpr uint8_t kDecoyFlying = 2;          // a Decoy whose release hit nothing (else it stands there)
constexpr uint8_t kGloveThrows = 3;

uint32_t thrown_velocity_at(int32_t item) { return item == kDecoyGloveItem ? 0x10u : 0x00u; }
constexpr float kThrowGain = 3.0f;
constexpr float kThrowMax = 18.0f;           // metres a second
constexpr float kThrowMin = 1.0f;
constexpr float kTriggerHold = 0.6f;
constexpr float kTriggerLetGo = 0.35f;
constexpr double kHandWindow = 0.08;         // seconds of the hand's path its velocity is taken over
constexpr double kReleaseKept = 0.12;        // a release waits this long for the glove to be ready

std::deque<std::pair<double, Vec3>> s_hand_path;  // the right hand's tracking positions, metres
Vec3 s_hand_velocity;                              // metres a second, tracking space
std::deque<std::pair<double, Vec3>> s_left_path;   // the left hand's, for a glove in it
Vec3 s_left_velocity;

// Each hand's bomb held and thrown: the right's, and the left's for a glove there (Akimbo, Rerac-vr's
// bomb_glove.rs left_update), or the right's second while the left alone hangs.
struct GloveThrow {
    bool held = false;
    bool pending = false;
    double at = 0;
    Vec3 velocity;
};
GloveThrow s_throw[2];

double now_seconds() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

// The game's tick, seconds (g_frame_time: 0.02 on PAL, 1/60 on NTSC-U).
float tick_seconds() { return get_float(s_mod->relocate(s_disc->turn_step)); }

bool throws_by_hand() {
    const gaddr moby = hand_moby();
    return s_options.throw_by_hand && hand_aims(moby) && glove_item(ref<int32_t>(s_disc->hero + kHeroSlot0 + kSlotItem));
}

// The second item is a glove, in hand `side`.
bool off_glove(int side) { return s_left_moby != 0 && s_off_side == side && glove_item(s_off_item); }

// One hand's path over the last 80 ms, and its velocity.
void track_hand(std::deque<std::pair<double, Vec3>>& path, Vec3& velocity, int side, double now) {
    if (!s_frame.hand_valid[side]) {
        path.clear();
        velocity = {};
        return;
    }
    const Vec3 at = s_frame.hand_grip[side].position;
    if (!path.empty() && (now - path.back().first > kReleaseKept || length(at - path.back().second) > 0.5f)) {
        path.clear();
    }
    path.emplace_back(now, at);
    while (path.size() > 2 && now - path.front().first > kHandWindow) {
        path.pop_front();
    }
    const double dt = now - path.front().first;
    velocity = dt > 0.008 ? (at - path.front().second) * static_cast<float>(1.0 / dt) : Vec3{};
}

// Once a frame: the hand's velocity, then the trigger's hold and release.
void throw_gesture() {
    const double now = now_seconds();
    track_hand(s_hand_path, s_hand_velocity, kRight, now);
    track_hand(s_left_path, s_left_velocity, kLeft, now);
    for (int side = 0; side < 2; ++side) {
        GloveThrow& t = s_throw[side];
        if (t.pending && now - t.at > kReleaseKept) {
            t.pending = false;
        }
        const bool throws = s_options.throw_by_hand && (off_glove(side) || (side == kRight && throws_by_hand()));
        if (!throws) {
            t.held = false;
            t.pending = false;
            continue;
        }
        const float trigger = side == kRight ? s_frame.controls.right_trigger : s_frame.controls.left_trigger;
        const Vec3& velocity = side == kRight ? s_hand_velocity : s_left_velocity;
        if (trigger > kTriggerHold && !t.held) {
            t.held = true;
            t.pending = false;
        }
        if (trigger < kTriggerLetGo && t.held) {
            t.held = false;
            const float speed = length(velocity);
            if (speed >= kThrowMin) {
                t.pending = true;
                t.at = now;
                t.velocity = velocity;
            }
            char line[120];
            std::snprintf(line, sizeof line, "[vr-throw] %s hand let go at %.2f m/s: %s", side == kRight ? "right" : "left",
                          speed, speed >= kThrowMin ? "thrown" : "kept, too slow");
            log_line(1, line);
        }
    }
}

// The Decoy's ball is not modelled about its origin (Rerac-vr's bomb_glove.rs hold): the centre of its
// drawn pose, the mean of its joints, goes in the palm. The game puts it back at the grip each tick, so
// this does not add up.
void centre_held_decoy(gaddr glove, Vec3 palm, int side, int32_t item) {
    constexpr int kMostJoints = 24;
    const openrac_api& api = s_mod->api();
    if (item != kDecoyGloveItem || !s_throw[side].held || api.moby_joints == nullptr) {
        return;
    }
    const gaddr vars = ref<gaddr>(glove + kMobyPvars);
    const gaddr ball = in_ram(vars) ? ref<gaddr>(vars + kGloveHeld) : 0;
    const int slot = ball != 0 ? s_mod->moby_slot_of(ball) : -1;
    if (slot < 0) {
        return;
    }
    const gaddr cls = ref<gaddr>(ball + kMobyClass);
    const int count = in_ram(cls) ? std::min<int>(ref<uint8_t>(cls + kClassJointCount), kMostJoints) : 0;
    if (count <= 0) {
        return;
    }
    int joints[kMostJoints];
    float xyz[kMostJoints * 3];
    for (int i = 0; i < count; ++i) {
        joints[i] = i;
    }
    if (api.moby_joints(slot, joints, count, xyz) != OPENRAC_HOOK_OK) {
        return;
    }
    Vec3 centre;
    for (int i = 0; i < count; ++i) {
        centre = centre + Vec3{xyz[i * 3], xyz[i * 3 + 1], xyz[i * 3 + 2]};
    }
    centre = centre * (1.0f / static_cast<float>(count));
    if (!std::isfinite(centre.x + centre.y + centre.z)) {
        return;
    }
    const Vec3 at = read_vec(ball + kMobyPosition) + (palm - centre);
    put_float(ball + kMobyPosition, at.x);
    put_float(ball + kMobyPosition + 4u, at.y);
    put_float(ball + kMobyPosition + 8u, at.z);
}

template <class Original>
void glove_update(Original original, uint32_t moby) {
    const int32_t item = ref<int32_t>(s_disc->hero + kHeroSlot0 + kSlotItem);
    const gaddr vars = ref<gaddr>(moby + kMobyPvars);
    const gaddr bomb = in_ram(vars) ? ref<gaddr>(vars + kGloveHeld) : 0;
    const uint8_t state = ref<uint8_t>(moby + kMobyState);
    const gaddr ammo = s_disc->ammo + 4u * static_cast<uint32_t>(glove_item(item) ? item : 0);
    const int side = s_left_active ? s_off_side : kRight;
    GloveThrow& t = s_throw[side];
    // Not a throw now, or the glove not ready with a bomb (the release then waits a moment).
    if (!t.pending || !throws_by_hand() || moby != hand_moby() || bomb == 0
        || s_mod->moby_slot_of(bomb) < 0 || (state != 1 && state != 2) || ref<int32_t>(ammo) <= 0) {
#if !defined(__ANDROID__)
        static const bool trace = std::getenv("QUEST_AKIMBO_TRACE") != nullptr;
        if (trace && t.pending) {
            char line[200];
            std::snprintf(line, sizeof line, "[vr-throw] waiting, %s: by hand %d, in hand %d, bomb 0x%X, state %d, ammo %d, item %d",
                          side == kRight ? "right" : "left", throws_by_hand() ? 1 : 0, moby == hand_moby() ? 1 : 0, bomb, state,
                          ref<int32_t>(ammo), item);
            log_line(1, line);
        }
#endif
        aimed_update(original, moby);
        return;
    }
    t.pending = false;
    ref<int32_t>(ammo) -= 1;
    feel_round(static_cast<Hand>(side), item);
    ref<uint8_t>(moby + kMobyState) = kGloveThrows;
    aimed_update(original, moby);  // the game launches the bomb it held

    Vec3 v = frame_to_world(rotate(yaw_quat(s_rig.yaw), t.velocity)) * (s_rig.scale * kThrowGain);
    const float most = kThrowMax * s_rig.scale;
    if (length(v) > most) {
        v = v * (most / length(v));
    }
    v = v * tick_seconds() + (s_hero_after - s_hero_before);
    const gaddr bomb_vars = ref<gaddr>(bomb + kMobyPvars);
    // A Decoy released into something stands there: only a flying one is given the hand's speed. Its
    // release already started it at the hand, through its own line test.
    const bool decoy = item == kDecoyGloveItem;
    bool given = false;
    if (in_ram(bomb_vars) && s_mod->moby_slot_of(bomb) >= 0 && (!decoy || ref<uint8_t>(bomb + kMobyState) == kDecoyFlying)) {
        const uint32_t at = thrown_velocity_at(item);
        put_float(bomb_vars + at, v.x);
        put_float(bomb_vars + at + 4u, v.y);
        put_float(bomb_vars + at + 8u, v.z);
        if (item == kBombGloveItem) {
            ref<int16_t>(bomb_vars + kBombNoReticle) = 1;
        }
        if (!decoy) {
            for (uint32_t i = 0; i < 3u; ++i) {
                put_float(bomb + kMobyPosition + 4u * i, get_float(vars + kGloveGrip + 4u * i));
            }
        }
        given = true;
    }
    char line[180];
    std::snprintf(line, sizeof line, "[vr-throw] item %d thrown by the %s hand: %.3f %.3f %.3f a tick%s, ammo %d", item,
                  side == kRight ? "right" : "left", v.x, v.y, v.z, given ? "" : " (it stopped at once: not given)",
                  ref<int32_t>(ammo));
    log_line(1, line);
}

template <class HookPoint>
void glove_hook(openrac::Mod& mod) {
    mod.hook<HookPoint>([](auto original, uint32_t moby) { glove_update(original, moby); });
}

// A held bomb, mine or Decoy deletes itself when its glove is not the hand's item (Rerac-vr's bomb.rs,
// mine.rs and decoy.rs update; the Doom canister only previews by it). The left glove's (Akimbo, or the
// right's second while the left hangs) is not: while the one it holds updates, the hand slot shows it.
template <class Original>
void held_projectile_update(Original original, uint32_t moby) {
    const gaddr glove = s_left_moby;
    const gaddr vars = glove != 0 && s_have_hero && glove_item(s_off_item) ? ref<gaddr>(glove + kMobyPvars) : 0;
    if (!in_ram(vars) || ref<gaddr>(vars + kGloveHeld) != moby) {
        original(moby);
        return;
    }
    const gaddr slot = s_disc->hero + kHeroSlot0;
    const gaddr hand = ref<gaddr>(slot);
    ref<gaddr>(slot) = glove;
    original(moby);
    ref<gaddr>(slot) = hand;
}

template <class HookPoint>
void held_projectile_hook(openrac::Mod& mod) {
    mod.hook<HookPoint>([](auto original, uint32_t moby) { held_projectile_update(original, moby); });
}

// ---- The wrench swung for real ----
//
// Rerac-vr's (vr/embodied.rs interact): the wrench hits what its head sweeps through, not what the
// game's swing would reach in front of Ratchet. Each tick the head's path since the last is swept
// in steps, a sphere about each; a moby that can be hit (mode 0x4000) is posted the game's own
// wrench hit (PostHitMessage: mask 0x10000 from the wrench, damage 1, pushed along the swing).
// Only a deliberate stroke hits: the head at 5.5 m/s, the hand at 2.4 m/s, 14 cm since it last
// slowed below 0.65 m/s; a crate (classes 500 to 540) asks 6.5, 2.8 and 16 cm. A moby hit waits
// 18 ticks for the next. In the air a downward slam (8 cm at 1.6 m/s, mostly down) is the game's
// own Hyper Strike, its jump attack: the swing button, for a moment. The swing button is otherwise
// never pressed: the game's swing would hit in front of the body the player is not in.

constexpr uint32_t kMobySize = 0x100u;
constexpr uint32_t kMobyMode = 0x34u;
constexpr uint16_t kMobyHittable = 0x4000u;
constexpr uint32_t kWrenchHitMask = 0x10000u;
constexpr uint32_t kHeroGroundDistance = 0x2DCu;
constexpr int32_t kStateJumpAttack = 0x14;
constexpr float kHeadSpeed = 5.5f, kHeadSpeedCrate = 6.5f;
constexpr float kStrokeSpeed = 2.4f, kStrokeSpeedCrate = 2.8f;
constexpr float kStrokeTravel = 0.14f, kStrokeTravelCrate = 0.16f;
constexpr float kStrokeRest = 0.65f;
constexpr float kHitRadius = 0.35f;
constexpr float kSweepStep = 0.12f;
constexpr uint64_t kHitWait = 18;
constexpr double kSlamWindow = 0.028;
constexpr double kSlamWait = 0.3;

Vec3 s_head_before;                // the wrench's head at the last tick, world
bool s_head_seen = false;
Vec3 s_stroke_from;                // the hand where the stroke began, tracking metres
bool s_stroke_seen = false;
Vec3 s_slam_from;
bool s_slam_seen = false;
double s_slam_until = 0;
double s_strike_until = 0;         // the swing button pressed for the Hyper Strike until then
std::map<int, uint64_t> s_hit_at;  // by slot, the tick each moby was last hit

// Ratchet's own: in a body (Clank, Giant Clank, the Hologuise) his hand slot may still hold the wrench's moby, unseen,
// and the hands are that body's fists (Rerac-vr's review fixes: Clank's grip no longer arms Ratchet's wrench).
bool swings_by_hand() {
    const gaddr moby = hand_moby();
    return s_options.swing_wrench && s_steers && s_have_frame && s_frame.hand_valid[kRight] && moby != 0
           && ref<int16_t>(moby + kMobyOclass) == kWrenchClass && hero_on_foot();
}

// The hands' motions are climbing while he hangs: no swing, no slam, whatever is in the hand.
bool wrench_swung_now() { return swings_by_hand() && !hanging_on(); }

bool airborne() { return get_float(s_disc->hero + kHeroGroundDistance) > 0.25f; }

// The hand's velocity over the last `window` seconds (its path, interpolated), metres a second.
Vec3 hand_velocity_over(double window) {
    if (s_hand_path.size() < 2) {
        return {};
    }
    const double now = s_hand_path.back().first;
    const double start = now - window;
    for (std::size_t i = 1; i < s_hand_path.size(); ++i) {
        const auto& [a, p] = s_hand_path[i - 1];
        const auto& [b, q] = s_hand_path[i];
        if (a <= start && start <= b && b > a) {
            const Vec3 from = p + (q - p) * static_cast<float>((start - a) / (b - a));
            return (s_hand_path.back().second - from) * static_cast<float>(1.0 / window);
        }
    }
    return {};
}

// Once a frame: the Hyper Strike's slam, and the swing button for it.
void wrench_gesture() {
    const double now = now_seconds();
    if (!wrench_swung_now()) {
        s_slam_seen = false;
        s_strike_until = 0;
        return;
    }
    const Vec3 slam = hand_velocity_over(kSlamWindow);
    const Vec3 at = s_frame.hand_grip[kRight].position;
    if (slam.y > -0.25f || !s_slam_seen) {
        s_slam_from = at;
        s_slam_seen = true;
    }
    const float down = -slam.y;
    if (airborne() && now >= s_slam_until && down > 1.6f
        && down > 0.8f * std::sqrt(slam.x * slam.x + slam.z * slam.z) && s_slam_from.y - at.y >= 0.08f) {
        s_slam_until = now + kSlamWait;
        s_strike_until = now + 0.3;
        s_slam_seen = false;
        log_line(1, "[vr-wrench] Hyper Strike: a slam down in the air");
    }
    // The button held until the strike begins or the moment passes, and not on the ground.
    const bool striking = ref<int32_t>(s_disc->hero + kHeroState) == kStateJumpAttack;
    if (striking && s_strike_until > 0) {
        log_line(1, "[vr-wrench] Hyper Strike begun: the game's jump attack");
        // The real slam was the windup, so not the game's rise as well (Rerac-vr's tracked_dive): past that,
        // its 14 ticks, he falls at once, and the game's own speed for the fall brings the blow to the landing.
        ref<int32_t>(s_disc->hero + kHeroStateTimer) = static_cast<int32_t>(std::lround(14.0f / 60.0f / tick_seconds())) + 1;
        put_float(s_disc->hero + kHeroVelocityZ, std::min(get_float(s_disc->hero + kHeroVelocityZ), 0.0f));
    }
    if (striking || !airborne()) {
        s_strike_until = 0;
    }
    s_swinging = now < s_strike_until;
#if !defined(__ANDROID__)
    // PC test aid (QUEST_POINTER_TRACE=1): how the strike went, height and time and the frame it lands on.
    static uint64_t began = 0;
    static float start_z = 0, peak = 0, last_frame = 0;
    if (std::getenv("QUEST_POINTER_TRACE") != nullptr) {
        const float z = s_hero_after.z;
        const gaddr body = ref<gaddr>(s_disc->hero + kHeroBody);
        if (striking && began == 0) {
            began = s_tick;
            start_z = peak = z;
        }
        if (striking) {
            peak = std::max(peak, z);
            last_frame = body != 0 ? get_float(body + 0x54u) : 0.0f;
        } else if (began != 0) {
            char line[120];
            std::snprintf(line, sizeof line, "[vr-wrench] the strike: %llu ticks, rose %.2f, ended on frame %.1f",
                          static_cast<unsigned long long>(s_tick - began), peak - start_z, last_frame);
            log_line(1, line);
            began = 0;
        }
    }
#endif
}

// The segment from a to b's nearest point to p.
Vec3 nearest_on(Vec3 a, Vec3 b, Vec3 p) {
    const Vec3 ab = b - a;
    const float len2 = dot(ab, ab);
    const float t = len2 > 0.0f ? std::clamp(dot(p - a, ab) / len2, 0.0f, 1.0f) : 0.0f;
    return a + ab * t;
}

constexpr int kSoundSwing = 27;  // of Ratchet's class
constexpr float kWhooshSpeed = 3.8f, kWhooshTravel = 0.35f, kWhooshRearm = 1.0f, kWhooshGap = 0.5f;

// A sound of Ratchet's class, on him.
void hero_sound(int sound) {
    using Play = int (*)(int, int, gaddr);
    static const auto play = reinterpret_cast<Play>(s_mod->api().function(s_disc->play_sound));
    const gaddr body = ref<gaddr>(s_disc->hero + kHeroBody);
    if (play != nullptr && body != 0) {
        play(sound, 0, body);
    }
}

// The wrench against the world (Rerac-vr's embodied.rs): a real stroke into a wall gives the game's wall
// sound and sparks, a knock in the hand that grows with the blow, and a short recoil of the wrench. Only
// a new stroke counts: it must come away from the wall first, so pressing on it does nothing.
constexpr uint32_t kCollNormal = 0x40u;  // in the line test's result
constexpr int kSoundWrenchWall = 2;      // of the wrench's class
constexpr float kWallForce = 1.3f, kWallHand = 0.65f;
constexpr double kWallGap = 0.22, kReboundSeconds = 0.15;

struct WallTouch {
    bool armed = true;
    double next = 0;
    double rebound_at = -1.0;
    Vec3 rebound;
    bool seen = false;
    Vec3 last;  // the hand from his feet, a tick ago
};
WallTouch s_wall;

void wrench_on_wall(const HandFrame& hand, int32_t item, bool touching) {
    const Vec3 from_body = hand.at - read_vec(s_disc->hero + 0x80u);
    const Vec3 velocity = s_wall.seen ? (from_body - s_wall.last) * (1.0f / (tick_seconds() * s_rig.scale)) : Vec3{};
    s_wall.last = from_body;
    s_wall.seen = true;
    if (item != kWrenchItem) {
        s_wall.armed = true;
        return;
    }
    if (!touching) {
        s_wall.armed = true;
        return;
    }
    const double now = now_seconds();
    // The game's normal is the triangle's, not of unit length.
    const Vec3 raw = read_vec(level0(s_disc->coll_result) + kCollNormal);
    const float raw_length = length(raw);
    const Vec3 normal = raw_length > 0.0f && std::isfinite(raw_length) ? raw * (1.0f / raw_length) : Vec3{};
    const float force = std::max(0.0f, -dot(velocity, normal));
    if (s_wall.armed && now >= s_wall.next && force >= kWallForce && length(s_hand_velocity) >= kWallHand
        && ref<int32_t>(s_disc->hero + kHeroState) != kStateJumpAttack) {
        const gaddr wrench = hand_moby();
        buzz(kRight, {std::clamp(0.3f + force * 0.085f, 0.35f, 0.9f), std::clamp(0.030f + force * 0.007f, 0.035f, 0.085f), 95.0f});
        if (wrench != 0) {
            using Play = int (*)(int, int, gaddr);
            using Sparks = void (*)(gaddr);
            static const auto play = reinterpret_cast<Play>(s_mod->api().function(s_disc->play_sound));
            static const auto sparks = reinterpret_cast<Sparks>(s_mod->api().function(s_disc->wrench_sparks));
            if (play != nullptr) {
                play(kSoundWrenchWall, 0, wrench);
            }
            if (sparks != nullptr) {
                sparks(wrench);
            }
        }
        s_wall.next = now + kWallGap;
        s_wall.rebound_at = now;
        s_wall.rebound = velocity * (-std::clamp(0.025f + force * 0.003f, 0.03f, 0.055f) * s_rig.scale / length(velocity));
        char line[140];
        std::snprintf(line, sizeof line, "[vr-wrench] the wall, at %.2f m/s (hand %.2f, normal length %.3g)", force,
                      length(s_hand_velocity), raw_length);
        log_line(1, line);
    }
    s_wall.armed = false;
}

// The recoil from the last blow, a quick arc out and back.
Vec3 wrench_rebound() {
    const double phase = (now_seconds() - s_wall.rebound_at) / kReboundSeconds;
    if (s_wall.rebound_at < 0.0 || phase < 0.0 || phase >= 1.0) {
        return {};
    }
    return s_wall.rebound * std::sin(kPi * static_cast<float>(phase));
}

// Once a tick: the head's sweep, and the hits.
void swing_wrench() {
    ++s_tick;
    // What became of the last moby hit (the wrench's or a fist's), a quarter second on: the log's proof the hit landed.
    static int watched = -1;
    static uint64_t watch_at = 0;
    if (watched < 0) {
        for (const auto& [slot, at] : s_hit_at) {
            if (at + 1 == s_tick) {
                watched = slot;
                watch_at = s_tick + 15;
            }
        }
    } else if (s_tick >= watch_at) {
        int state = -1;
        for (const openrac_moby& m : s_mod->moby_list()) {
            if (m.slot == watched) {
                state = static_cast<int>(m.state);
            }
        }
        char line[120];
        std::snprintf(line, sizeof line, "[vr-hit] slot %d after the hit: %s (state %d)", watched,
                      state < 0 || state >= 0x80 ? "gone" : "still there", state);
        log_line(1, line);
        watched = -1;
    }
    if (!wrench_swung_now() || ref<int32_t>(s_disc->hero + kHeroState) == kStateJumpAttack || wrench_on_bolt()) {
        s_head_seen = false;
        s_stroke_seen = false;
        return;
    }
    const gaddr wrench = hand_moby();
    const float hand_speed = length(s_hand_velocity);
    const Vec3 hand_at = s_frame.hand_grip[kRight].position;
    if (hand_speed < kStrokeRest || !s_stroke_seen) {
        s_stroke_from = hand_at;
        s_stroke_seen = true;
    }
    const float travel = length(hand_at - s_stroke_from);

    // Ratchet's swings whoosh, a sound the wrench's own animations do not carry: once for a big fast
    // stroke, hit or miss, again when the hand has slowed (Rerac-vr's).
    static bool whooshed = false;
    static uint64_t whoosh_next = 0;
    if (hand_speed < kWhooshRearm) {
        whooshed = false;
    } else if (!whooshed && hand_speed >= kWhooshSpeed && travel >= kWhooshTravel && s_tick >= whoosh_next) {
        whooshed = true;
        whoosh_next = s_tick + static_cast<uint64_t>(kWhooshGap / tick_seconds());
        hero_sound(kSoundSwing);
    }

    // The head: the end of the wrench's second joint list, where the hand holds it now, reaching a
    // little further along the shaft the faster it swings (Rerac-vr's extend and swing_stretch).
    HandFrame hand;
    const Mount mount = hand_mount(wrench, hand);
    const float scale = s_grown == wrench ? s_grown_scale : get_float(wrench + kMobyScale);
    const JointFrame& head_joint = class_joint(kWrenchClass, wrench, 1);
    Vec3 head = head_joint.valid
                    ? mount.position + mount.axes * (head_joint.anchor * (scale * kWrenchScale / 1024.0f))
                    : hand.at + hand.forward * (0.5f * s_rig.scale);
    const Vec3 shaft = head - hand.at;
    const float stretch = 1.0f + std::clamp((hand_speed - 1.0f) / 2.5f, 0.0f, 1.0f) * 0.35f;
    s_wrench_stretch = stretch;
    if (length(shaft) > 0.0f) {
        head = head + shaft * (0.17f * stretch * s_rig.scale / length(shaft));
    }
    if (!s_head_seen) {
        s_head_before = head;
        s_head_seen = true;
        return;
    }
    const Vec3 start = s_head_before;
    s_head_before = head;
    const float head_speed = length(head - start) / (tick_seconds() * s_rig.scale);
    if (head_speed < kHeadSpeed || hand_speed < kStrokeSpeed || travel < kStrokeTravel) {
        return;
    }

    const int wrench_slot = s_mod->moby_slot_of(wrench);
    const gaddr base = wrench - static_cast<gaddr>(wrench_slot) * kMobySize;
    const int hero_slot = s_mod->api().figure_slot(s_mod->handle(), -1);
    Vec3 push = head - start;
    push = push * (1.0f / length(push));
    const float sweep = length(head - start);
    bool hit_any = false;
    for (const openrac_moby& m : s_mod->moby_list()) {
        if (m.slot == wrench_slot || m.slot == hero_slot || (m.flags & (OPENRAC_MOBY_HIDDEN | OPENRAC_MOBY_DEAD)) != 0
            || m.state >= 0x80) {
            continue;
        }
        const gaddr address = base + static_cast<gaddr>(m.slot) * kMobySize;
        if ((ref<uint16_t>(address + kMobyMode) & kMobyHittable) == 0) {
            continue;
        }
        const bool crate = m.oclass >= 500 && m.oclass <= 540;
        if (crate && (head_speed < kHeadSpeedCrate || hand_speed < kStrokeSpeedCrate || travel < kStrokeTravelCrate)) {
            continue;
        }
        const auto last = s_hit_at.find(m.slot);
        if (last != s_hit_at.end() && s_tick - last->second < kHitWait) {
            continue;
        }
        // Its bounding sphere (the game keeps it in 1/1024 units), against the head's path.
        const Vec3 centre{get_float(address) / 1024.0f, get_float(address + 4u) / 1024.0f,
                          get_float(address + 8u) / 1024.0f};
        const float radius = std::min(get_float(address + 12u) / 1024.0f, 1.5f * s_rig.scale);
        const Vec3 nearest = nearest_on(start, head, centre);
        if (length(centre - nearest) > kHitRadius * s_rig.scale + 0.75f * radius || sweep > 4.0f * s_rig.scale) {
            continue;
        }
        openrac_hit hit{};
        hit.size = sizeof hit;
        hit.victim = m.slot;
        hit.attacker = wrench_slot;
        hit.mask = kWrenchHitMask;
        hit.damage = 1.0f;
        hit.position[0] = nearest.x;
        hit.position[1] = nearest.y;
        hit.position[2] = nearest.z;
        hit.velocity[0] = push.x;
        hit.velocity[1] = push.y;
        hit.velocity[2] = 1.0f;
        if (s_mod->api().moby_hit(s_mod->handle(), &hit) == OPENRAC_HOOK_OK) {
            s_hit_at[m.slot] = s_tick;
            hit_any = true;
            char line[160];
            std::snprintf(line, sizeof line,
                          "[vr-wrench] hit slot %d class %d: head %.1f m/s, hand %.1f m/s, stroke %.2f m", m.slot,
                          m.oclass, head_speed, hand_speed, travel);
            log_line(1, line);
        }
    }
    for (auto it = s_hit_at.begin(); it != s_hit_at.end();) {
        it = s_tick - it->second > 120 ? s_hit_at.erase(it) : std::next(it);
    }
    if (hit_any) {  // Rerac-vr's wrench hit: a heavy knock, and a ring after it
        buzz(kRight, {0.92f, 0.085f, 75.0f});
        buzz_later(0.09f, kRight, {0.32f, 0.030f, 135.0f});
    }
}

// The wrench on the grip and the gun on the trigger, as on PC: a fresh grip takes the wrench out, and
// the trigger, with the wrench in hand, takes the last gun back instead of firing. The game's own
// way of changing items: the word the Quick Select writes.
constexpr uint32_t kHeroPendingItem = 0x20B8u;

int32_t s_last_gun = 0;
uint64_t s_wrench_at = 0;
bool s_grip_was = false;
bool s_trigger_was = false;

// ---- The wrench thrown, and put away ----
//
// Rerac-vr's (vr/embodied.rs launch_wrench, hero/comet.rs): let go of the grip at 2.5 m/s or more and
// the wrench flies the game's own throw (the Comet flight of its update: out along its direction
// until it stops, then back, hitting what it meets), from its middle, at four times the hand's speed
// (10 to 23 m/s), spinning flat; on the way back it comes to the player's hand, not to the body's.
// Let go slowly, or catch it with the grip open, and it is put away: the gun before it comes back.

constexpr uint8_t kWrenchOut = 10;
constexpr uint8_t kWrenchBack = 11;
constexpr uint32_t kSlotThrownFlag = 0x1Au;     // HeroItemSlot.thrown: the item away from the hand
constexpr uint32_t kSlotSwapLock = 0x1Cu;       // HeroItemSlot.switch_lock: 2 refuses a change of item
constexpr uint32_t kHeroSelectedItem = 0x20D4u; // the swap's target (selected_item[0])
constexpr uint32_t kHeroHandPoint = 0x1070u;    // where a detached item comes back to
constexpr uint32_t kMobyEuler = 0x40u;
constexpr uint32_t kWrenchDirection = 0x40u;    // the wrench's vars (WData48)
constexpr uint32_t kWrenchSpeed = 0x60u;
constexpr uint32_t kWrenchSlowing = 0x64u;
constexpr uint32_t kWrenchWallClose = 0x76u;
constexpr float kWrenchThrowSpeed = 2.5f;
const Vec3 kWrenchMiddle{1513.0f, -1200.0f, -350.0f};  // in its model (Rerac-vr's WRENCH_MIDDLE)

bool wrench_flying(gaddr wrench) {
    const uint8_t state = ref<uint8_t>(wrench + kMobyState);
    return state == kWrenchOut || state == kWrenchBack;
}

// The wrench in the hand thrown with the hand's tracking-space `velocity`; false if it was not ready.
bool throw_wrench(gaddr wrench, Vec3 velocity) {
    const gaddr slot = s_disc->hero + kHeroSlot0;
    const gaddr vars = ref<gaddr>(wrench + kMobyPvars);
    if (ref<int32_t>(slot + kSlotState) != 2 || ref<uint8_t>(wrench + kMobyState) != 0 || !in_ram(vars)) {
        return false;
    }
    HandFrame hand;
    const Mount mount = hand_mount(wrench, hand);
    const float scale = s_grown == wrench ? s_grown_scale : get_float(wrench + kMobyScale);
    const Vec3 from = mount.position + mount.axes * (kWrenchMiddle * (scale * kWrenchScale / 1024.0f));
    Vec3 dir = frame_to_world(rotate(yaw_quat(s_rig.yaw), velocity));
    dir = dir * (1.0f / std::max(length(dir), 0.0001f));
    const float speed = std::clamp(length(velocity) * 4.0f, 10.0f, 23.0f) * tick_seconds();
    for (uint32_t i = 0; i < 3u; ++i) {
        put_float(wrench + kMobyPosition + 4u * i, (&from.x)[i]);
        put_float(vars + kWrenchDirection + 4u * i, (&dir.x)[i]);
    }
    put_float(vars + kWrenchDirection + 12u, 0.0f);
    put_float(wrench + kMobyEuler, 0.0f);
    put_float(wrench + kMobyEuler + 4u, 0.0f);
    put_float(wrench + kMobyEuler + 8u, std::atan2(dir.y, dir.x));  // spinning flat, whatever the wrist
    put_float(vars + kWrenchSpeed, speed);
    put_float(vars + kWrenchSlowing, 0.0f);
    ref<int16_t>(vars + kWrenchWallClose) = 0;
    ref<uint8_t>(slot + kSlotThrownFlag) = 1;
    ref<uint8_t>(wrench + kMobyState) = kWrenchOut;
    char line[160];
    std::snprintf(line, sizeof line, "[vr-wrench] thrown at %.1f m/s: along (%.2f %.2f %.2f), %.3f a tick",
                  length(velocity), dir.x, dir.y, dir.z, speed);
    log_line(1, line);
    return true;
}

// The wrench's update: coming back, it comes to the player's hand.
template <class Original>
void wrench_update(Original original, uint32_t moby) {
    if (s_options.swing_wrench && s_steers && s_have_frame && s_frame.hand_valid[kRight] && moby == hand_moby()
        && ref<uint8_t>(moby + kMobyState) == kWrenchBack) {
        HandFrame hand;
        hand_mount(moby, hand);
        put_float(s_disc->hero + kHeroHandPoint, hand.at.x);
        put_float(s_disc->hero + kHeroHandPoint + 4u, hand.at.y);
        put_float(s_disc->hero + kHeroHandPoint + 8u, hand.at.z);
    }
    original(moby);
}

// ---- The Walloper's punch ----
//
// Rerac-vr's (vr/walloper.rs): a punch is the hand with the Walloper thrown forward, measured from the
// head (so walking or turning is none), at 1.45 m/s or more over 7 cm, within 55 degrees of where the
// player faces and away from the body (the spine 15 cm behind the eyes); it re-arms on a pause, on
// slowing to half its peak or on pulling back, 0.22 s apart. It is the game's own lunge: its fire,
// for a moment. The trigger is its fire as well.

constexpr int32_t kWalloperItem = 18;
constexpr float kPunchSpeed = 1.45f;
constexpr float kPunchCone = 0.57f;   // cos 55 degrees
constexpr float kPunchAway = 0.2f;
constexpr float kPunchSpine = 0.15f;
constexpr double kPunchWindow = 0.05;

std::deque<std::pair<double, Vec3>> s_punch_path;  // the hand from the head, tracking metres
Vec3 s_punch_rest;
bool s_punch_rest_seen = false;
bool s_punch_ready = false;
float s_punch_peak = 0;
double s_punch_wait = 0;
double s_punch_until = 0;

int s_punch_side = kRight;   // the hand of the last punch
constexpr int32_t kStateLunge = 0x20;
bool s_lunge_quick = false;  // the lunge running began past its windup (a punch's)

// The game's ticks for `n` of the US disc's (60 a second).
int game_ticks(float n) { return static_cast<int>(std::lround(n / 60.0f / tick_seconds())); }

// The hand whose Walloper punches by hand, or -1: the hand slot's (the left while it leads), or the left
// hand's second item (Akimbo: Rerac-vr's walloper.rs TrackedPunch left, which then leads).
int punch_side() {
    if (!s_options.swing_wrench || !s_steers || !s_have_frame) {
        return -1;
    }
    const gaddr hero = s_disc->hero;
    const int side = s_leading ? kLeft : kRight;
    if (s_frame.hand_valid[side] && hand_moby() != 0 && ref<int32_t>(hero + kHeroSlot0 + kSlotItem) == kWalloperItem
        && ref<int32_t>(hero + kHeroSlot0 + kSlotState) == 2) {
        return side;
    }
    if (s_vr.akimbo && s_left_moby != 0 && s_off_side == kLeft && s_off_item == kWalloperItem && s_frame.hand_valid[kLeft]) {
        return kLeft;
    }
    return -1;
}

bool punching() { return now_seconds() < s_punch_until; }

// A punch the hand slot's Walloper answers: the right hand's, or the left's while it leads. One thrown during a
// lunge waits for that lunge to take the next (Rerac-vr's tracked_fire: 22 ticks from its start, 10 more for one
// begun past its windup). Each poll the game's fire is pressed and let go by turns while it waits, so whenever the
// game can take it, mid-lunge or as the lunge ends, the press is a fresh one.
bool hand_punching() {
    static bool pressed = false;
    const bool lunging = hero_state() == kStateLunge;
    const bool can = punching() && (s_punch_side == kLeft) == s_leading
                     && (!lunging || ref<int32_t>(s_disc->hero + kHeroStateTimer) > game_ticks(22.0f) + (s_lunge_quick ? game_ticks(10.0f) : 0));
    pressed = can && !pressed;
    return pressed;
}

// A lunge a real punch started begins past its windup: the arm already threw it, and the game's 10
// still ticks read as lag after one (Rerac-vr's walloper.rs tracked_fire). Its speed, hit window and
// length stay the game's. Once a tick.
void lunge_tick() {
    static int32_t was = 0, timer_was = 0;
    const int32_t state = hero_state();
    const int32_t timer = ref<int32_t>(s_disc->hero + kHeroStateTimer);
    // A lunge begun, or begun again within one (the next punch chained).
    const bool chained = state == kStateLunge && was == kStateLunge && timer < timer_was;
    if (state == kStateLunge && (was != kStateLunge || chained)) {
        s_lunge_quick = punching();
        if (s_lunge_quick) {
            ref<int32_t>(s_disc->hero + kHeroStateTimer) = std::max(1, game_ticks(10.0f));
            s_punch_until = 0;  // taken
            log_line(1, chained ? "[vr-punch] the next lunge chained, past its windup" : "[vr-punch] the lunge starts past its windup");
        }
    }
    // Rerac-vr's chain: a punch thrown during a lunge starts the next once this one's hit window is over (22 ticks
    // from its start, 10 more for one begun past its windup). The game lunges again only from standing, and ends a
    // lunge only as its animation does, so there he is stood for the tick: the pulsed fire then lunges anew.
    if (state == kStateLunge && was == kStateLunge && punching() && (s_punch_side == kLeft) == s_leading
        && timer > game_ticks(22.0f) + (s_lunge_quick ? game_ticks(10.0f) : 0)) {
        constexpr uint32_t kHeroSubstate = 0x2088u, kHeroGroup = 0x208Cu;  // Rerac-vr's hero.rs: 0x1413d8, 0x1413dc
        ref<int32_t>(s_disc->hero + kHeroState) = 0;
        ref<int32_t>(s_disc->hero + kHeroSubstate) = 0;
        ref<int32_t>(s_disc->hero + kHeroGroup) = 0;
        log_line(1, "[vr-punch] the lunge cut at its chain point: the next takes over");
    }
    was = state;
    timer_was = ref<int32_t>(s_disc->hero + kHeroStateTimer);
}

// Once a frame.
void punch_gesture() {
    const double now = now_seconds();
    const int side = punch_side();
    static int side_was = -1;
    if (side < 0 || side != side_was) {
        s_punch_path.clear();
        s_punch_rest_seen = false;
        s_punch_ready = false;
        side_was = side;
    }
    if (side < 0) {
        return;
    }
    const Vec3 relative = s_frame.hand_grip[side].position - s_frame.head.position;
    if (!s_punch_path.empty()
        && (now - s_punch_path.back().first > 0.12 || length(relative - s_punch_path.back().second) > 0.3f)) {
        s_punch_path.clear();
        s_punch_rest_seen = false;
        s_punch_ready = false;
    }
    s_punch_path.emplace_back(now, relative);
    while (s_punch_path.size() > 2 && now - s_punch_path.front().first > kPunchWindow) {
        s_punch_path.pop_front();
    }
    const double dt = now - s_punch_path.front().first;
    const Vec3 v = dt > 0.008 ? (relative - s_punch_path.front().second) * static_cast<float>(1.0 / dt) : Vec3{};
    Vec3 facing = rotate(s_frame.head.orientation, {0, 0, -1});
    facing.y = 0;
    if (length(facing) < 0.001f) {
        return;
    }
    facing = facing * (1.0f / length(facing));
    const float speed = length(v);
    const float along = dot(v, facing);
    s_punch_peak = std::max(s_punch_peak, speed);
    if (speed < 0.6f || speed < 0.5f * s_punch_peak || along < -0.3f * speed || !s_punch_rest_seen) {
        s_punch_rest = relative;
        s_punch_rest_seen = true;
        if (now >= s_punch_wait) {
            s_punch_ready = true;
            s_punch_peak = speed;
        }
    }
    Vec3 reach = relative + facing * kPunchSpine;
    reach.y = 0;
    reach = length(reach) > 0.0f ? reach * (1.0f / length(reach)) : Vec3{};
    const float away = v.x * reach.x + v.z * reach.z;
    if (s_punch_ready && now >= s_punch_wait && speed >= kPunchSpeed && length(relative - s_punch_rest) >= 0.07f
        && along >= kPunchCone * speed && away >= kPunchAway * speed) {
        s_punch_ready = false;
        s_punch_peak = speed;
        s_punch_wait = now + 0.22;
        s_punch_until = now + 0.45;  // offered until a lunge takes it (lunge_tick)
        s_punch_side = side;
        char line[140];
        std::snprintf(line, sizeof line, "[vr-punch] Walloper punch at %.2f m/s (along %.2f, away %.2f)%s", speed,
                      along / speed, away / speed, side == kLeft ? ", the left hand" : "");
        log_line(1, line);
    }
}

void change_items() {
    const Controls& c = s_frame.controls;
    const bool grip = c.right_grip > kTriggerOn;
    const bool trigger = c.right_trigger > kTriggerOn;
    // Only Ratchet's own body takes the wrench on the grip: as Clank, Giant Clank or the Hologuise the
    // grips close the hands, and a change of item would end the disguise.
    // Hanging on, the game has put the hand's item away and a change of item would let go of the ledge:
    // the free hand's gun needs none (akimbo_tick).
    // While the left hand leads, the hand slot is its: nothing is changed.
    if (!s_steers || !s_have_frame || !hero_on_foot() || hanging_on() || s_leading) {
        s_grip_was = grip;
        s_trigger_was = trigger;
        s_trigger_back = false;
        return;
    }
    const gaddr hero = s_disc->hero;
    const int32_t item = ref<int32_t>(hero + kHeroSlot0 + kSlotItem);
    if (item != kWrenchItem && item > 0) {
        s_last_gun = item;
    }
    if (s_options.swing_wrench && item == kWrenchItem) {
        const gaddr wrench = hand_moby();
        static int traced = 0;
        if (std::getenv("QUEST_WRENCH_TRACE") != nullptr && ++traced % 20 == 0) {
            char line[200];
            std::snprintf(line, sizeof line,
                          "[vr-wrench] trace: grip %d moby %s state %d slot state %d last gun %d pending %d timer %d "
                          "thrown %d lock %d force %d only %d fc %d target %d",
                          grip ? 1 : 0, wrench != 0 ? "yes" : "no", wrench != 0 ? ref<uint8_t>(wrench + kMobyState) : -1,
                          ref<int32_t>(hero + kHeroSlot0 + kSlotState), s_last_gun, ref<int32_t>(hero + kHeroPendingItem),
                          ref<int16_t>(hero + kHeroSlot0 + 0x18u), ref<uint8_t>(hero + kHeroSlot0 + 0x1Au),
                          ref<uint8_t>(hero + kHeroSlot0 + 0x1Cu), ref<uint8_t>(hero + 0x20A6u),
                          ref<uint8_t>(hero + 0x20A7u), ref<uint8_t>(hero + 0x20ACu), ref<int32_t>(hero + 0x20D4u));
            log_line(1, line);
        }
        // Let go: fast, the throw; slowly, put away (a caught wrench too, the grip open).
        if (!grip && s_grip_was && wrench != 0 && length(s_hand_velocity) >= kWrenchThrowSpeed
            && throw_wrench(wrench, s_hand_velocity)) {
            s_grip_was = grip;
            s_trigger_was = trigger;
            return;
        }
        // As the grip opens slowly, or the wrench is caught with it open: asked once, when the swap
        // lock is off (the flight keeps it on to the catch). The game's swap keeps the item it was
        // asked for as its target even when the lock refused it, then drops any later request for
        // that item as already done (Rerac-vr fixes the same): the target is set back to the wrench
        // before the request, so it is one.
        static uint8_t state_was = 0;
        static bool wanted = false;
        const uint8_t state = wrench != 0 ? ref<uint8_t>(wrench + kMobyState) : 0;
        const bool let_go = !grip && s_grip_was;
        const bool caught = !grip && state_was == kWrenchBack && state == 0;
        state_was = state;
        if (let_go || caught) {
            wanted = true;
            log_line(1, caught ? "[vr-wrench] caught with the grip open: put away" : "[vr-wrench] let go slowly: put away");
        }
        if (grip || s_last_gun <= 0) {
            wanted = false;
        }
        if (wanted && wrench != 0 && !wrench_flying(wrench) && ref<uint8_t>(hero + kHeroSlot0 + kSlotSwapLock) == 0
            && ref<int32_t>(hero + kHeroSlot0 + kSlotState) == 2) {
            ref<int32_t>(hero + kHeroSelectedItem) = kWrenchItem;
            ref<int32_t>(hero + kHeroPendingItem) = s_last_gun;
            wanted = false;
            log_line(1, "[vr-wrench] the gun asked back");
        }
    }
    // A fresh grip takes the wrench, a tick later: the ledge it may have closed on is found first.
    if (grip && !s_grip_was && item != kWrenchItem) {
        s_wrench_at = s_tick + 1;
    }
    if (!grip) {
        s_wrench_at = 0;
    }
    if (s_wrench_at != 0 && s_tick >= s_wrench_at) {
        s_wrench_at = 0;
        if (!right_on_ledge()) {
            ref<int32_t>(hero + kHeroPendingItem) = kWrenchItem;
        }
    }
    if (trigger && !s_trigger_was && item == kWrenchItem && s_last_gun > 0) {
        ref<int32_t>(hero + kHeroPendingItem) = s_last_gun;
        s_trigger_back = true;
    }
    if (!trigger) {
        s_trigger_back = false;
    }
    s_grip_was = grip;
    s_trigger_was = trigger;
}

// ---- Ledges ----
//
// Rerac-vr's (hero/tracked.rs, hero/ledge.rs, vr/traversal.rs): a ledge is held with the hands. The
// game's probe still finds it and decides whether it can be held, but it is only taken with a hand
// gripping near it, and a higher one can be reached by the hand alone. The hands stay where they took
// hold. All let go, he drops; both hands on and settled, a shove down with both (or A) climbs, as the
// game's own climb; the hands pulling along the ledge take him along it, by the game's shimmy (Rerac-vr
// moves him itself; the stick here keeps the game's own ends and probes). The game's drop and climb
// are pressed for it: R1 and Cross.

constexpr int32_t kStateLedgeGrab = 0x18;
constexpr int32_t kStateShimmyLeft = 0x1A;
constexpr int32_t kStateShimmyRight = 0x1B;
constexpr int32_t kStateLedgeLast = kStateShimmyRight;
constexpr int32_t kStateClimb = 0x1C;
// Clank's (Rerac-vr's tracked.rs ledge_hold): his grab, hang and shimmies, and his climb.
constexpr int32_t kClankLedgeGrab = 0x4A, kClankHang = 0x4B, kClankClimb = 0x4C, kClankShimmyLeft = 0x4D, kClankShimmyRight = 0x4E;
constexpr int32_t kModeFalling = 2;
constexpr int32_t kModeJumping = 4;
constexpr uint32_t kHeroMode = 0x208Cu;           // state.control_mode
constexpr uint32_t kHeroPosition = 0x80u;         // motion.pos
constexpr uint32_t kHeroLedgeCooldown = 0x1B0u;
constexpr uint32_t kHeroLedgePoint = 0x4D0u;      // where he hangs from, then top z, yaw, found ...
constexpr uint32_t kHeroLedgeTop = 0x4E0u;
constexpr uint32_t kHeroLedgeYaw = 0x4E4u;
constexpr uint32_t kHeroLedgeFound = 0x4E8u;
constexpr uint32_t kHeroLedgeMoby = 0x4F8u;
constexpr uint32_t kHeroLedgeBlock = 0x30u;       // the bytes from kHeroLedgePoint a probe writes
constexpr float kLedgeBack = 0.435f;              // his hang point from the lip
constexpr float kLedgeBackClank = 0.305f;
constexpr float kLedgeCatchLow = 0.2f;            // his feet this far under the lip, at least, to catch it
constexpr float kLedgeProbeUp = 1.5f;             // the probe takes a top within this of his feet
constexpr float kReachAlong = 1.05f, kReachUp = 0.75f, kReachAcross = 0.65f, kAnchorSpread = 0.65f;
constexpr int kLedgeSettle = 30;                  // ticks on it before a climb counts
constexpr float kHandReachBody = 1.7f;
constexpr float kGripTake = 0.45f, kGripHold = 0.20f;
constexpr float kShimmyError = 0.05f;

Vec3 s_hand_stroke[2];  // each hand's speed in the room, metres a second (ledge_gesture)

struct LedgeHands {
    bool grips[2] = {false, false};
    bool grabbed[2] = {false, false};
    bool anchored[2] = {false, false};
    float along[2] = {0, 0};          // where each hand took hold, along the ledge from his hang point
    int held_ticks = 0;
    bool shoved = false;
    int press_ticks = 0;              // Cross held for the game
    int drop_ticks = 0;               // R1 held for the game
    float error = 0;                  // how far the hands have pulled along it
    bool top_seen[2] = {false, false};
    Vec3 top[2];
    double shove_at[2] = {0, 0};
};
LedgeHands s_ledge;

bool as_clank() { return ref<uint8_t>(s_disc->hero + kHeroVehicleMode) == kBodyClank; }

bool hands_hold_ledges() {
    return s_options.grab_ledges && s_steers && s_have_frame && s_have_hero && (hero_on_foot() || as_clank())
           && (s_frame.hand_valid[kLeft] || s_frame.hand_valid[kRight]);
}

int32_t hero_state() { return ref<int32_t>(s_disc->hero + kHeroState); }
bool ledge_state(int32_t state) {
    return (state >= kStateLedgeGrab && state <= kStateLedgeLast) || state == kClankLedgeGrab || state == kClankHang
           || state == kClankShimmyLeft || state == kClankShimmyRight;
}

bool shimmying(int32_t state) {
    return state == kStateShimmyLeft || state == kStateShimmyRight || state == kClankShimmyLeft || state == kClankShimmyRight;
}

bool climbing(int32_t state) { return state == kStateClimb || state == kClankClimb; }

Vec3 hand_at(int side) { return hand_in_world(in_frame(s_rig, s_frame.hand_grip[side], anchor_now(1.0f))).at; }

float ledge_yaw() { return get_float(s_disc->hero + kHeroLedgeYaw); }

// The lip above where he hangs.
Vec3 ledge_anchor() {
    const Vec3 point = read_vec(s_disc->hero + kHeroLedgePoint);
    const float yaw = ledge_yaw();
    const float back = as_clank() ? kLedgeBackClank : kLedgeBack;
    return {point.x - std::cos(yaw) * back, point.y - std::sin(yaw) * back,
            get_float(s_disc->hero + kHeroLedgeTop)};
}

Vec3 ledge_tangent() { return {-std::sin(ledge_yaw()), std::cos(ledge_yaw()), 0}; }

bool within_reach(Vec3 hand, Vec3 anchor) {
    return std::hypot(hand.x - anchor.x, hand.y - anchor.y) < kReachAlong * s_rig.scale
           && std::abs(hand.z - anchor.z) < kReachUp * s_rig.scale;
}

// The left hand alone holds on and the right is out of reach of it: the right is free for the wrench.
bool right_hand_free() {
    if (!s_ledge.grabbed[kLeft] || s_ledge.grabbed[kRight]) {
        return false;
    }
    return !s_frame.hand_valid[kRight] || !within_reach(hand_at(kRight), ledge_anchor());
}

bool hanging_on() { return hands_hold_ledges() && ledge_state(hero_state()); }

// A crate stack's top is a ledge, but the hand with the wrench out swings at it, it does not catch it.
// Hanging already, any hand may join (and holds nothing else then).
bool wrench_keeps_off_ledge() {
    if (!swings_by_hand() || wrench_put_down()) {
        return false;
    }
    const gaddr ledge = ref<gaddr>(s_disc->hero + kHeroLedgeMoby);
    if (ledge == 0 || s_mod->moby_slot_of(ledge) < 0) {
        return false;
    }
    const int oclass = ref<int16_t>(ledge + kMobyOclass);
    return oclass >= 500 && oclass <= 540;
}

bool right_on_ledge() {
    if (!hands_hold_ledges()) {
        return false;
    }
    return s_ledge.grabbed[kRight] || (ledge_state(hero_state()) && !right_hand_free());
}

void ledge_context(PadContext& context) {
    if (!s_have_frame || context.in_menu || !hands_hold_ledges() || !ledge_state(hero_state())) {
        return;
    }
    context.ledge = true;
    context.ledge_climb = s_ledge.grabbed[kLeft] && s_ledge.grabbed[kRight] && s_ledge.held_ticks >= kLedgeSettle;
    context.ledge_press = s_ledge.press_ticks > 0;
    context.ledge_drop = s_ledge.drop_ticks > 0;
    context.ledge_stick = std::abs(s_ledge.error) > kShimmyError * s_rig.scale;
    if (context.ledge_stick) {
        const Vec3 way = ledge_tangent() * (s_ledge.error > 0.0f ? 1.0f : -1.0f);
        context.head_yaw = std::atan2(way.y, way.x);
    }
}

void forget_ledge() {
    for (int side = 0; side < 2; ++side) {
        s_ledge.grabbed[side] = false;
        s_ledge.anchored[side] = false;
    }
    s_ledge.held_ticks = 0;
    s_ledge.shoved = false;
    s_ledge.error = 0;
}

// Each change of his state from a hold until a little after it: what a climb or a drop became.
void trace_ledge_states() {
    static int32_t state_was = -1;
    static uint64_t until = 0;
    const int32_t state = hero_state();
    if (ledge_state(state)) {
        until = s_tick + 180;
    }
    if (state != state_was && s_tick < until) {
        const Vec3 at = read_vec(s_disc->hero + kHeroPosition);
        char line[120];
        std::snprintf(line, sizeof line, "[vr-ledge] state %d -> %d at (%.2f %.2f %.2f) mode %d found %d", state_was, state, at.x,
                      at.y, at.z, ref<int32_t>(s_disc->hero + kHeroMode), ref<int32_t>(s_disc->hero + kHeroLedgeFound));
        log_line(1, line);
    }
    state_was = state;
}

// Once a tick, after the hero's update.
void ledge_tick() {
    trace_ledge_states();
    s_ledge.press_ticks = std::max(0, s_ledge.press_ticks - 1);
    s_ledge.drop_ticks = std::max(0, s_ledge.drop_ticks - 1);
    if (!hands_hold_ledges()) {
        forget_ledge();
        s_ledge.grips[kLeft] = s_ledge.grips[kRight] = false;
        return;
    }
    const float values[2] = {s_frame.controls.left_grip, s_frame.controls.right_grip};
    for (int side = 0; side < 2; ++side) {
        s_ledge.grips[side] = s_frame.hand_valid[side] && values[side] > (s_ledge.grabbed[side] ? kGripHold : kGripTake);
    }
    if (!ledge_state(hero_state())) {
        forget_ledge();
        return;
    }
    const Vec3 anchor = ledge_anchor();
    const Vec3 tangent = ledge_tangent();
    for (int side = 0; side < 2; ++side) {
        s_ledge.grabbed[side] = s_ledge.grabbed[side] && s_ledge.grips[side];
        if (s_ledge.grips[side] && within_reach(hand_at(side), anchor)) {
            s_ledge.grabbed[side] = true;
            if (!s_ledge.anchored[side]) {
                s_ledge.anchored[side] = true;
                s_ledge.along[side] = std::clamp(dot(hand_at(side) - anchor, tangent), -kAnchorSpread, kAnchorSpread);
                if (s_options.haptics && s_headset) {
                    s_headset->haptic(static_cast<Hand>(side), 0.23f, 0.025f, 175.0f);
                }
                char line[120];
                std::snprintf(line, sizeof line, "[vr-ledge] %s hand holds on, top %.2f, %.2f along", side == 0 ? "left" : "right",
                              anchor.z, s_ledge.along[side]);
                log_line(1, line);
            }
        }
        if (!s_ledge.grabbed[side]) {
            s_ledge.anchored[side] = false;
        }
    }
    if (!s_ledge.grabbed[kLeft] && !s_ledge.grabbed[kRight]) {
        s_ledge.drop_ticks = 3;
        s_ledge.held_ticks = 0;
        log_line(1, "[vr-ledge] both hands let go: he drops");
        return;
    }
    ++s_ledge.held_ticks;

    // Each hand's pull from where it took hold, along the ledge: the mean is how far he should move.
    float pull = 0;
    int hands = 0;
    for (int side = 0; side < 2; ++side) {
        if (s_ledge.grabbed[side]) {
            pull += s_ledge.along[side] - dot(hand_at(side) - anchor, tangent);
            ++hands;
        }
    }
    s_ledge.error = hands > 0 ? pull / static_cast<float>(hands) : 0.0f;

    if (s_ledge.shoved) {
        s_ledge.shoved = false;
        if (s_ledge.grabbed[kLeft] && s_ledge.grabbed[kRight] && s_ledge.held_ticks >= kLedgeSettle) {
            // The hands stay on until the climb has begun: only a grip let go unbinds one.
            s_ledge.press_ticks = 3;
            if (s_options.haptics && s_headset) {
                s_headset->haptic(kLeft, 0.65f, 0.075f, 85.0f);
                s_headset->haptic(kRight, 0.65f, 0.075f, 85.0f);
            }
            log_line(1, "[vr-ledge] a shove with both hands: he climbs");
        } else {
            log_line(1, "[vr-ledge] a shove too soon, or with one hand: nothing");
        }
    }
}

// Once a frame: each hand's speed and how far it has come down from its top, for the shove.
void ledge_gesture() {
    static bool seen[2] = {false, false};
    static double was_at[2] = {0, 0};
    static Vec3 was[2];
    const double now = now_seconds();
    const bool hanging = s_ledge.grabbed[kLeft] || s_ledge.grabbed[kRight];
    for (int side = 0; side < 2; ++side) {
        Vec3 velocity;
        s_hand_stroke[side] = {};
        if (!s_frame.hand_valid[side]) {
            seen[side] = false;
            s_ledge.top_seen[side] = false;
            s_ledge.shove_at[side] = 0;
            continue;
        }
        const Vec3 at = s_frame.hand_grip[side].position;
        if (seen[side]) {
            const double dt = now - was_at[side];
            const Vec3 delta = at - was[side];
            if (dt >= 0.3 || length(delta) >= 0.5f) {
                s_ledge.top_seen[side] = false;
            }
            if (dt >= 0.002 && dt < 0.12 && length(delta) < 0.3f) {
                velocity = delta * static_cast<float>(1.0 / dt);
                const float speed = length(velocity);
                velocity = speed <= 0.25f ? Vec3{} : speed > 4.0f ? velocity * (4.0f / speed) : velocity;
            }
        }
        seen[side] = true;
        was_at[side] = now;
        was[side] = at;
        s_hand_stroke[side] = velocity;
        if (!hanging || !s_ledge.grips[side]) {
            s_ledge.top_seen[side] = false;
            s_ledge.shove_at[side] = 0;
            continue;
        }
        // From the hand's recent top, not from where it took hold: players raise their hands to the
        // hang. A slow sag is forgotten.
        Vec3& top = s_ledge.top[side];
        if (!s_ledge.top_seen[side] || at.y > top.y) {
            top = at;
            s_ledge.top_seen[side] = true;
        }
        top.y = std::min(top.y, at.y + 0.35f);
        const float down = top.y - at.y;
        if (down > 0.12f && velocity.y < -0.9f) {
            s_ledge.shove_at[side] = now;
        }
        if (down < 0.04f) {
            s_ledge.shove_at[side] = 0;
        }
    }
    const auto recent = [&](int side) { return s_ledge.shove_at[side] != 0 && now - s_ledge.shove_at[side] < 0.18; };
    if (hanging && recent(kLeft) && recent(kRight)) {
        s_ledge.shoved = true;
        s_ledge.shove_at[kLeft] = s_ledge.shove_at[kRight] = 0;
    }
}

// The probe again at the height of a hand, for a ledge only the arm reaches, or a higher one from a
// hang. The game's probe, its checks kept: it only runs from where it stands, so he stands there for
// it, falling, and is put back.
template <class Original>
bool reach_ledge(Original original, bool hanging) {
    const gaddr hero = s_disc->hero;
    if ((!s_ledge.grips[kLeft] && !s_ledge.grips[kRight])) {
        return false;
    }
    const int32_t mode = ref<int32_t>(hero + kHeroMode);
    if (!hanging && mode != kModeFalling && mode != kModeJumping) {
        return false;
    }
    openrac::Memory memory = s_mod->memory();
    uint8_t ledge[kHeroLedgeBlock];
    std::memcpy(ledge, memory.pointer(hero + kHeroLedgePoint), sizeof ledge);
    const Vec3 position = read_vec(hero + kHeroPosition);
    const float top_was = get_float(hero + kHeroLedgeTop);
    const int16_t cooldown = ref<int16_t>(hero + kHeroLedgeCooldown);
    bool found = false;
    ref<int32_t>(hero + kHeroMode) = kModeFalling;
    for (int side = 0; side < 2 && !found; ++side) {
        if (!s_ledge.grips[side] || (hanging && s_ledge.grabbed[side])) {
            continue;
        }
        const Vec3 hand = hand_at(side);
        if (std::hypot(hand.x - position.x, hand.y - position.y) > kHandReachBody * s_rig.scale) {
            continue;
        }
        for (const float lift : {-0.15f, 0.15f, 0.40f}) {
            put_float(hero + kHeroPosition + 8u, hand.z - kLedgeProbeUp + lift);
            ref<int16_t>(hero + kHeroLedgeCooldown) = 0;
            original();
            if (ref<int32_t>(hero + kHeroLedgeFound) != 0 && (!hanging || get_float(hero + kHeroLedgeTop) > top_was + 0.18f)
                && within_reach(hand, ledge_anchor())) {
                found = true;
                break;
            }
        }
    }
    ref<int32_t>(hero + kHeroMode) = mode;
    put_float(hero + kHeroPosition + 8u, position.z);
    ref<int16_t>(hero + kHeroLedgeCooldown) = cooldown;
    if (!found) {
        std::memcpy(memory.pointer(hero + kHeroLedgePoint), ledge, sizeof ledge);
    }
    return found;
}

template <class Original>
void check_ledge(Original original) {
    static int logged_level = -2;
    if (s_mod->loaded_level() != logged_level) {
        logged_level = s_mod->loaded_level();
        char line[80];
        std::snprintf(line, sizeof line, "[vr-ledge] the game probes for ledges on level %d", logged_level);
        log_line(1, line);
    }
    if (!hands_hold_ledges()) {
        original();
        return;
    }
    const gaddr hero = s_disc->hero;
    // The game's shimmy moves him by this probe, ahead of him and at him.
    if (shimmying(hero_state())) {
        original();
        return;
    }
    // Held, the ledge stays: a probe from a body the hands move would slide him. Only a free hand
    // reaches on for a higher one.
    if (ledge_state(hero_state()) && (s_ledge.grabbed[kLeft] || s_ledge.grabbed[kRight])) {
        ref<int32_t>(hero + kHeroLedgeFound) = 0;  // as the probe clears it each tick, or the climb re-catches it
        if (!as_clank() && reach_ledge(original, true)) {
            log_line(1, "[vr-ledge] a higher ledge in reach");
        }
        return;
    }
    original();
    // Not on the way up a climb: the hands, still closed, would catch what they just pushed off. The
    // game's own probe stays, so a climb that falls short still catches the lip.
    if (ref<int32_t>(hero + kHeroLedgeFound) == 0 && !climbing(hero_state()) && !as_clank() && reach_ledge(original, false)) {
        log_line(1, "[vr-ledge] the arm reaches a ledge");
    }
    if (ref<int32_t>(hero + kHeroLedgeFound) == 0) {
        return;
    }
    // Falling with a grip already closed, the hand's reach finds the lip while his feet are still over
    // it. The game's grab pulls him down a fixed way and hands over to the hang wherever that ends, and
    // from up there the hang lets go at once: he falls a little further first, and takes it then.
    if (!as_clank() && !ledge_state(hero_state())
        && get_float(hero + kHeroLedgeTop) - get_float(hero + kHeroPosition + 8u) < kLedgeCatchLow) {
        ref<int32_t>(hero + kHeroLedgeFound) = 0;
        return;
    }
    // Taken only with a hand on it: reach is wider along the edge than across it. The hand with the
    // wrench out swings onto crates, it does not hold them.
    const Vec3 anchor = ledge_anchor();
    const float yaw = ledge_yaw();
    bool any = false;
    for (int side = 0; side < 2; ++side) {
        if (!s_ledge.grips[side] || (side == kRight && wrench_keeps_off_ledge())) {
            continue;
        }
        const Vec3 d = hand_at(side) - anchor;
        const float across = d.x * std::cos(yaw) + d.y * std::sin(yaw);
        if (within_reach(hand_at(side), anchor) && std::abs(across) < kReachAcross * s_rig.scale) {
            any = true;
        }
    }
    if (!any) {
        ref<int32_t>(hero + kHeroLedgeFound) = 0;
    }
}

// ---- Walking in the room, and swimming ----
//
// Rerac-vr's (vr/game.rs align_body, hero/physics.rs roomscale_move, vr/traversal.rs, hero/tracked.rs
// tracked_swim). The head turns and nods about the neck, behind and below the eyes, as a real head does:
// a step of the neck in the room is a step of Ratchet: he is moved at the start of his update, a short
// way, and that tick's own collision takes it from there, on every level whatever its code. So a wall
// stops the player as it stops him, and the view is always over the body. (Hero.move_impulse looks made
// for this, but walking and standing rewrite it.) The view takes the same step in the same tick: head tracking
// stays immediate, with the origin between its last two places as the hero is between his.
// In water, a palm pushing broad on stays ahead of the head's gaze: a stroke adds to a speed that drag
// takes away, along the gaze (level at the surface), and pushing the water up with both hands dives.

constexpr float kNeckBack = 0.10f, kNeckDown = 0.15f;
constexpr float kStepMost = 0.12f;                // units a tick: thin walls stay walls
constexpr uint32_t kHeroCollisionOff = 0x1CCu;
constexpr uint32_t kHeroJumpLock = 0x1F2u;
constexpr uint32_t kHeroEdgeBrake = 0x1F4u;
constexpr uint32_t kHeroWallSpline = 0x22DAu;
constexpr uint32_t kHeroFrozen = 0x20ADu;
constexpr uint32_t kHeroWaterZ = 0x2F0u;
constexpr int32_t kModeHanging = 3, kModeRail = 0xF, kModeUnderwater = 0x11, kModeSurface = 0x12;
constexpr int32_t kModeScripted = 0x15;
constexpr int32_t kStateCrouch = 4, kStateWade = 0x73;
constexpr float kStrokeGain = 0.65f, kStrokeMost = 3.5f;
constexpr float kSwimDrag = 0.96f, kSwimPush = 0.12f;  // a 60th of a second's
constexpr float kSwimMostSurface = 3.0f, kSwimMostUnder = 4.5f;
constexpr float kDivePush = 0.8f;

Vec3 s_neck;
bool s_neck_seen = false;
Vec3 s_room_step;                        // metres of tracking, not yet given to him
Vec3 s_origin_tick, s_origin_before, s_origin_after;
Vec3 s_swim_velocity;                    // world, units a second
int s_dive_ticks = 0;

Vec3 neck_of(const Pose& head) { return head.position + rotate(head.orientation, {0, -kNeckDown, kNeckBack}); }

int32_t hero_mode() { return ref<int32_t>(s_disc->hero + kHeroMode); }

bool leaning_only() {
    const int32_t mode = hero_mode();
    return mode == kModeRail || mode == kModeBoard;
}

// Once a frame.
void follow_head(const Pose& head, bool steering) {
    if (!steering || !s_options.room_scale || leaning_only()) {
        s_neck_seen = false;
        s_room_step = {};
        return;
    }
    const Vec3 neck = neck_of(head);
    if (s_neck_seen) {
        Vec3 d = neck - s_neck;
        d.y = 0;
        if (length(d) <= 0.5f) {  // more is the tracking jumping, not a step
            s_room_step = s_room_step + d;
        }
    } else {
        s_origin_tick = s_origin_before = s_origin_after = {neck.x, s_rig.origin.y, neck.z};
        s_rig.origin = s_origin_after;
    }
    s_neck = neck;
    s_neck_seen = true;
}

// Whether a step may move him now: not in the game's own hold of him (Rerac-vr's roomscale_move).
bool body_can_step() {
    const gaddr hero = s_disc->hero;
    const int32_t mode = hero_mode();
    return s_health > 0 && ref<uint8_t>(hero + kHeroFrozen) == 0 && ref<int32_t>(hero + kHeroCollisionOff) == 0
           && ref<int16_t>(hero + kHeroWallSpline) == 0 && ref<int16_t>(hero + kHeroEdgeBrake) == 0
           && ref<int16_t>(hero + kHeroJumpLock) == 0 && mode != kModeHanging && mode != kModeRail
           && mode != kModeUnderwater && mode != kModeSurface && mode != kModeScripted && mode != kModeBoard;
}

void step_hero(Vec3 world) {
    const float size = length(world);
    if (size > kStepMost) {
        world = world * (kStepMost / size);
    }
    const gaddr at = s_disc->hero + kHeroPosition;
    put_float(at, get_float(at) + world.x);
    put_float(at + 4u, get_float(at + 4u) + world.y);
    put_float(at + 8u, get_float(at + 8u) + world.z);
}

// At the start of the hero's update.
void give_room_step() {
    const Vec3 step = s_room_step;
    s_room_step = {};
    if (!s_neck_seen) {
        return;
    }
    s_origin_tick = {s_neck.x, s_rig.origin.y, s_neck.z};
    static float asked = 0;
    static Vec3 from;
    static uint64_t since = 0;
    if (body_can_step()) {
        Vec3 world = frame_to_world(rotate(yaw_quat(s_rig.yaw), step)) * s_rig.scale;
        world.z = 0;
        step_hero(world);
        if (length(world) > 0.0001f) {
            if (asked == 0.0f) {
                from = s_hero_after;
                since = s_tick;
            }
            asked += length(world);
        }
    }
    // What the steps became, a second at a time: what a wall took away shows here.
    if (asked > 0.0f && s_tick > since + 50) {
        const Vec3 moved = s_hero_after - from;
        char line[140];
        std::snprintf(line, sizeof line, "[vr-room] asked %.2f, moved %.2f, now (%.2f %.2f %.2f)", asked,
                      std::hypot(moved.x, moved.y), s_hero_after.x, s_hero_after.y, s_hero_after.z);
        log_line(1, line);
        asked = 0;
    }
}

// Before the capture, as sample_hero's two positions.
void sample_origin() {
    s_origin_before = s_origin_after;
    if (s_neck_seen) {
        s_origin_after = s_origin_tick;
        s_rig.origin.x = s_origin_after.x;
        s_rig.origin.z = s_origin_after.z;
    } else {
        s_origin_after = s_rig.origin;
    }
}

bool dive_wanted() { return s_dive_ticks > 0; }

void swim_tick() {
    s_dive_ticks = std::max(0, s_dive_ticks - 1);
    const int32_t mode = s_have_hero ? hero_mode() : 0;
    if (!s_options.swim_strokes || !s_steers || !s_have_frame || (mode != kModeUnderwater && mode != kModeSurface)) {
        s_swim_velocity = {};
        return;
    }
    const gaddr hero = s_disc->hero;
    const Quat turn = yaw_quat(s_rig.yaw);
    const float water = get_float(hero + kHeroWaterZ);
    Vec3 force;
    for (int side = 0; side < 2; ++side) {
        if (!s_frame.hand_valid[side] || hand_at(side).z > water + 0.04f * s_rig.scale) {
            continue;
        }
        const Vec3 v = s_hand_stroke[side];
        const float speed = length(v);
        if (speed <= 0.0f) {
            continue;
        }
        // A broad palm pushes water; turned edge-on it recovers for nothing.
        const float drag = std::abs(dot(v * (1.0f / speed), rotate(s_frame.hand_grip[side].orientation, {1, 0, 0})));
        force = force - frame_to_world(rotate(turn, v)) * (drag * kStrokeGain * s_rig.scale);
    }
    Vec3 gaze = frame_to_world(rotate(turn * s_frame.head.orientation, {0, 0, -1}));
    const float size = length(gaze);
    gaze = size > 0.01f ? gaze * (1.0f / size) : Vec3{0, 1, 0};
    Vec3 stroke = gaze * std::max(dot(force, gaze), 0.0f);
    if (length(stroke) > kStrokeMost) {
        stroke = stroke * (kStrokeMost / length(stroke));
    }
    // Pushing the water up with both hands dives: the force before it is taken along the gaze.
    if (mode == kModeSurface && force.z < -kDivePush) {
        s_dive_ticks = 3;
    }
    const float tick = tick_seconds();
    const float sixtieths = tick * 60.0f;
    const bool surface = mode == kModeSurface;
    s_swim_velocity = s_swim_velocity * std::pow(kSwimDrag, sixtieths) + stroke * (kSwimPush * sixtieths);
    if (surface) {
        s_swim_velocity.z = 0;
    }
    Vec3 along = gaze;
    if (surface) {
        along.z = 0;
    }
    const float reach = length(along);
    float speed = length(s_swim_velocity);
    if (reach > 0.01f) {
        s_swim_velocity = along * (speed / reach);
    }
    const float most = surface ? kSwimMostSurface : kSwimMostUnder;
    if (speed > most) {
        s_swim_velocity = s_swim_velocity * (most / speed);
        speed = most;
    }
    step_hero(s_swim_velocity * tick);
    // Under the water he faces the gaze, and swims where the head looks.
    if (mode == kModeUnderwater) {
        const float yaw = std::atan2(gaze.y, gaze.x);
        put_float(hero + kHeroFacing, yaw);
        put_float(hero + kHeroMoveYaw, yaw);
        put_float(hero + kHeroRot + 4u, -std::asin(std::clamp(gaze.z, -1.0f, 1.0f)));
    }
    static uint64_t logged = 0;
    if (speed > 0.1f && s_tick >= logged) {
        logged = s_tick + 25;
        char line[120];
        std::snprintf(line, sizeof line, "[vr-swim] stroke %.2f, speed %.2f along (%.2f %.2f %.2f)%s", length(stroke), speed,
                      gaze.x, gaze.y, gaze.z, surface ? " at the surface" : "");
        log_line(1, line);
    }
}

// Once a frame: how high his eyes are for what he is doing, eased so that a posture is not a jump.
constexpr int32_t kStateCable = 0x74;
constexpr uint32_t kHeroCablePoint = 0x5E0u;  // the point on the cable he hangs from

void update_eye_height(float seconds) {
    if (!s_have_hero) {
        return;
    }
    const gaddr hero = s_disc->hero;
    const int32_t state = hero_state();
    const int32_t mode = hero_mode();
    float target = kEyeHeight;
    const uint8_t body = ref<uint8_t>(hero + kHeroVehicleMode);
    const float size = s_options.world_scale * (body == kBodyGiant ? kGiantScale : 1.0f);
    if (size != s_rig.scale && !in_ship()) {
        s_rig.scale = size;
        s_eye_height = body == kBodyGiant ? kEyeGiant : kEyeHeight;
        s_recentre_wanted = true;
    }
    if (body == kBodyClank) {
        target = kEyeClank;
        if (ledge_state(state)) {
            const float top = get_float(hero + kHeroLedgeTop) - get_float(hero + kHeroPosition + 8u);
            target = std::clamp(top - 0.38f, 0.25f, kEyeClank);
        }
    } else if (body == kBodyGiant) {
        target = kEyeGiant;
    } else if (ledge_state(state)) {
        const float top = get_float(hero + kHeroLedgeTop) - get_float(hero + kHeroPosition + 8u);
        target = std::clamp(top - 0.38f, kEyeHangLowest, kEyeHeight);
    } else if (state == kStateCable) {
        // Room above the eyes for the cable and the wrench on it (Rerac-vr's).
        const float room = get_float(hero + kHeroCablePoint + 8u) - get_float(hero + kHeroPosition + 8u) - 0.55f;
        target = std::clamp(room, 0.55f, kEyeHeight);
    } else if (mode == kModeUnderwater || mode == kModeSurface) {
        target = kEyeSwimming;
    } else if (state == kStateCrouch || (state == kStateWade && s_frame.controls.left_grip > kTriggerOn)) {
        target = kEyeCrouched;
    }
    const float most = seconds * kEyeBlend;
    s_eye_height += std::clamp(target - s_eye_height, -most, most);
}

// ---- Plugging the Hydrodisplacer and the Trespasser in ----
//
// Rerac-vr's (vr/gadget_socket.rs): the tool's tip brought to its pad and pushed in starts the game's own
// use of it, from where the player stands. The game asks that Ratchet stand on the pad and press fire;
// for the push the pad is what he stands on, in the two updates that ask, and the fire is pressed. The
// trigger on the pad, as the game has it, still works. The hand's push is counted, not the body's: a
// walk or a turned wrist does not plug anything. Rerac-vr finds the socket by posing the item as the game
// would at the pad; here the pad's own bounding sphere stands for it.

constexpr int32_t kHydroItem = 22, kTrespasserItem = 26;
constexpr int16_t kHydroPadClass = 341, kLockClass = 615;
constexpr uint8_t kLockReady = 1;
constexpr uint32_t kHeroGroundMoby = 0x2FCu;
constexpr float kSocketReach = 0.30f, kSocketFacing = 0.35f, kSocketPush = 0.005f, kSocketFar = 3.0f;
constexpr double kSocketHold = 1.5;       // how long the use is offered for the game to take it

struct Socket {
    gaddr target = 0;
    float travel = 0;
    bool plugged = false;
    double plugged_at = 0;
    bool hand_seen = false;
    Vec3 hand_was;
};
Socket s_socket;

bool socket_item(int32_t item) { return item == kHydroItem || item == kTrespasserItem; }

// Where a moby's bounding sphere is (the game keeps it in 1/1024 units at the moby's start).
Vec3 moby_centre(gaddr moby) {
    return Vec3{get_float(moby), get_float(moby + 4u), get_float(moby + 8u)} * (1.0f / 1024.0f);
}

bool socket_pressed() {
    return s_socket.target != 0 && s_socket.plugged && now_seconds() - s_socket.plugged_at < kSocketHold;
}

// Once a tick.
void socket_tick() {
    const gaddr hero = s_disc->hero;
    const gaddr tool = hand_moby();
    const int32_t item = tool != 0 ? ref<int32_t>(hero + kHeroSlot0 + kSlotItem) : 0;
    if (!socket_item(item) || !s_steers || !s_have_frame || !s_frame.hand_valid[kRight] || hanging_on()) {
        s_socket = {};
        return;
    }
    if (socket_pressed()) {
        return;
    }
    HandFrame hand;
    const Mount mount = hand_mount(tool, hand);
    const Vec3 tip = mount.muzzle;
    const Vec3 at = s_frame.hand_grip[kRight].position;
    Vec3 moved;
    if (s_socket.hand_seen && length(at - s_socket.hand_was) < 0.3f) {
        moved = frame_to_world(rotate(yaw_quat(s_rig.yaw), at - s_socket.hand_was)) * s_rig.scale;
    }
    s_socket.hand_was = at;
    s_socket.hand_seen = true;

    const int16_t wanted = item == kHydroItem ? kHydroPadClass : kLockClass;
    const int slot = s_mod->moby_slot_of(tool);
    const gaddr base = tool - static_cast<gaddr>(slot) * kMobySize;
    gaddr best = 0;
    float nearest = 1e9f;
    for (const openrac_moby& m : s_mod->moby_list()) {
        if (m.oclass != wanted || m.state >= 0x80 || (wanted == kLockClass && m.state != kLockReady)) {
            continue;
        }
        const gaddr pad = base + static_cast<gaddr>(m.slot) * kMobySize;
        const Vec3 centre = moby_centre(pad);
        const float radius = std::min(get_float(pad + 12u) / 1024.0f, 1.5f * s_rig.scale);
        const float gap = length(tip - centre) - radius;
        if (gap < kSocketReach * s_rig.scale && length(centre - s_hero_after) < kSocketFar * s_rig.scale && gap < nearest) {
            const Vec3 to = centre - tip;
            if (dot(to, hand.forward) > kSocketFacing * std::max(length(to), 0.001f)) {
                nearest = gap;
                best = pad;
            }
        }
    }
    if (best != s_socket.target) {
        s_socket.target = best;
        s_socket.travel = 0;
    }
    static uint64_t logged = 0;
    if (s_tick >= logged) {
        logged = s_tick + 50;
        for (const openrac_moby& m : s_mod->moby_list()) {
            if (m.oclass == wanted) {
                const gaddr pad = base + static_cast<gaddr>(m.slot) * kMobySize;
                const Vec3 c = moby_centre(pad);
                const Vec3 to = c - tip;
                // A tap's water (the moby its first word names): what the game lets the tool do there
                // (test_hydrodisplacer_activation_flag: bit 2 drains into an empty tank, bit 1 fills
                // from a full one), the tank, and the tool's own state (3: in use).
                int water = -1;
                const gaddr vars = ref<gaddr>(pad + kMobyPvars);
                if (wanted == kHydroPadClass && in_ram(vars)) {
                    const int32_t index = ref<int32_t>(vars);
                    if (index >= 0 && index < 4096) {
                        water = ref<uint8_t>(base + static_cast<gaddr>(index) * kMobySize + 0xBCu);
                    }
                }
                char line[220];
                std::snprintf(line, sizeof line,
                              "[vr-socket] pad state %d centre (%.2f %.2f %.2f) tip (%.2f %.2f %.2f) gap %.2f facing %.2f water "
                              "%d tank %d tool %d hero %d",
                              m.state, c.x, c.y, c.z, tip.x, tip.y, tip.z, length(to),
                              dot(to, hand.forward) / std::max(length(to), 0.001f), water, ref<uint8_t>(hero + 0x20B0u),
                              ref<uint8_t>(tool + kMobyState), hero_state());
                log_line(1, line);
                break;
            }
        }
    }
    if (best == 0) {
        return;
    }
    s_socket.travel = std::max(0.0f, s_socket.travel + dot(moved, hand.forward));
    if (s_socket.travel >= kSocketPush * s_rig.scale || s_frame.controls.right_trigger > kTriggerHold) {
        s_socket.plugged = true;
        s_socket.plugged_at = now_seconds();
        log_line(1, "[vr-socket] the tool is pushed in");
        if (s_options.haptics && s_headset) {
            s_headset->haptic(kRight, 0.5f, 0.06f, 0);
        }
    }
}

// Around an update that asks whether he stands on the pad.
template <class Update>
void as_on_pad(gaddr pad, Update update) {
    const gaddr at = s_disc->hero + kHeroGroundMoby;
    const gaddr ground = ref<gaddr>(at);
    ref<gaddr>(at) = pad;
    update();
    if (ref<gaddr>(at) == pad) {
        ref<gaddr>(at) = ground;
    }
}

gaddr s_puzzle_lock = 0;  // the lock the Trespasser is in

template <class Original>
void lock_update(Original original, uint32_t lock) {
    if (in_puzzle()) {
        s_puzzle_lock = lock;
    }
    // The rings' exit waits for the help box, which they keep asking for: in a headset the tool comes
    // out at once (Rerac-vr's tracked items). The game's other way out is the tool not being in hand,
    // so for this update it is not; the rest of its exit, the camera and his stance, is its own.
    if (s_withdraw) {
        s_withdraw = false;
        if (in_puzzle()) {
            const gaddr item = s_disc->hero + kHeroSlot0 + kSlotItem;
            const int32_t held = ref<int32_t>(item);
            ref<int32_t>(item) = 0;
            original(lock);
            ref<int32_t>(item) = held;
            log_line(1, ("[vr-socket] the tool withdrawn: he is in state " + std::to_string(hero_state())).c_str());
            return;
        }
    }
    if (socket_pressed() && s_socket.target == lock) {
        const gaddr tool = hand_moby();
        if (tool != 0) {
            ref<uint8_t>(tool + kMobyCommand) = 1;
        }
        as_on_pad(lock, [&] { original(lock); });
        return;
    }
    original(lock);
}

template <class Original>
void hydro_update(Original original, uint32_t moby) {
    if (socket_pressed() && moby == hand_moby() && in_ram(s_socket.target)) {
        as_on_pad(s_socket.target, [&] { aimed_update(original, moby); });
        return;
    }
    aimed_update(original, moby);
}

// ---- Bolt cranks ----
//
// Rerac-vr's (vr/embodied.rs crank_wrench, moby_update/classes/bolt_crank.rs): the wrench's head brought
// to a bolt seats it, without the game's latch or its camera, and the player walks around the bolt to
// turn it: only the body's way round counts, stick or room, never a wrist. The hand may go 2.4 from the
// bolt and the body 3.4, for a moment. The bolt keeps its own update, told the turning; its end is the
// game's: its update runs once with Ratchet in the crank's state, which lets go and finishes it.

constexpr int16_t kBoltClass = 280;
constexpr int32_t kStateCrank = 0x3B;
constexpr uint8_t kBoltIdle = 1;
constexpr uint8_t kBoltTurning = 3;
constexpr uint32_t kBoltProgress = 0x00u, kBoltIdleTicks = 0x14u, kBoltAngle = 0x18u, kBoltFall = 0x24u;
constexpr uint32_t kBoltTurns = 0x30u, kBoltTop = 0x3Cu;
constexpr float kBoltNutUp = 0.9f;            // the hex head's middle above the bolt's origin
constexpr float kCrankReach = 3.2f, kCrankBeside = 0.55f, kCrankHeight = 0.55f;
constexpr float kCrankHandFar = 2.4f, kCrankBodyFar = 3.4f;
constexpr float kCrankOrbitMost = 0.30f, kCrankRadiusLeast = 0.60f, kCrankRadiusMost = 3.0f;
const Vec3 kCrankJaws{10670.0f, -930.0f, -351.0f};  // the wrench's model, on the bolt's axis

struct Crank {
    gaddr bolt = 0;
    float turn = 0;          // radians he went round it this tick
    bool seen = false;
    Vec3 last;               // his feet, a tick ago
    double far_since = 0;
    float radius = 0;        // how far he stood from it, seating it
};
Crank s_crank;

// Forward on the stick goes round the bolt (Rerac-vr's game_input.rs): along the circle he seated it
// from, whichever way he faces, and held on that circle.
void crank_stick(Controls& c) {
    if (s_crank.bolt == 0) {
        return;
    }
    const Vec3 feet = read_vec(s_disc->hero + kHeroPosition), axis = read_vec(s_crank.bolt + kMobyPosition);
    const float out_x = feet.x - axis.x, out_y = feet.y - axis.y, distance = std::hypot(out_x, out_y);
    if (distance <= 0.1f) {
        return;
    }
    const float radial_x = out_x / distance, radial_y = out_y / distance;
    const float along = c.left_y + c.left_x * 0.7f;
    const float back = std::clamp(s_crank.radius - distance, -0.25f, 0.25f) * 1.3f;
    const float want_x = -radial_y * along + radial_x * back, want_y = radial_x * along + radial_y * back;
    const float forward_x = std::cos(s_head_yaw), forward_y = std::sin(s_head_yaw);
    c.left_x = want_x * forward_y - want_y * forward_x;
    c.left_y = want_x * forward_x + want_y * forward_y;
}

// At the Trespasser's rings: the tool in the hand, the game's hold on him.
bool in_puzzle() {
    constexpr int32_t kStateHold = 0x72;
    return s_have_hero && ref<int32_t>(s_disc->hero + kHeroState) == kStateHold
           && ref<int32_t>(s_disc->hero + kHeroSlot0 + kSlotItem) == kTrespasserItemId;
}

bool wrench_on_bolt() { return s_crank.bolt != 0; }

Vec3 bolt_nut(gaddr bolt) { return read_vec(bolt + kMobyPosition) + Vec3{0, 0, kBoltNutUp}; }

bool bolt_usable(gaddr bolt) {
    if (bolt == 0 || s_mod->moby_slot_of(bolt) < 0 || ref<int16_t>(bolt + kMobyOclass) != kBoltClass) {
        return false;
    }
    const uint8_t state = ref<uint8_t>(bolt + kMobyState);
    return (state == kBoltIdle || state == kBoltTurning) && in_ram(ref<gaddr>(bolt + kMobyPvars));
}

void let_go_of_bolt(const char* why) {
    if (s_crank.bolt != 0) {
        char line[80];
        std::snprintf(line, sizeof line, "[vr-crank] let go: %s", why);
        log_line(1, line);
    }
    s_crank = {};
}

Vec3 wrench_head_now(gaddr wrench) {
    HandFrame hand;
    const Mount mount = hand_mount(wrench, hand);
    const float scale = s_grown == wrench ? s_grown_scale : get_float(wrench + kMobyScale);
    const JointFrame& head = class_joint(kWrenchClass, wrench, 1);
    return head.valid ? mount.position + mount.axes * (head.anchor * (scale * kWrenchScale / 1024.0f))
                      : hand.at + hand.forward * (0.5f * s_rig.scale);
}

// Once a tick.
void crank_tick() {
    const gaddr wrench = hand_moby();
    if (s_health <= 0) {
        let_go_of_bolt("he is down");  // Rerac-vr's: death lets go of the crank
        return;
    }
    if (!wrench_swung_now() || wrench_put_down() || wrench == 0 || wrench_flying(wrench)) {
        let_go_of_bolt("the wrench left it");
        return;
    }
    if (s_crank.bolt != 0 && !bolt_usable(s_crank.bolt)) {
        let_go_of_bolt("it is done");
        return;
    }
    const Vec3 body = s_hero_after;
    if (s_crank.bolt == 0) {
        const Vec3 head = wrench_head_now(wrench);
        const int slot = s_mod->moby_slot_of(wrench);
        const gaddr base = wrench - static_cast<gaddr>(slot) * kMobySize;
        float best = 1e9f;
        for (const openrac_moby& m : s_mod->moby_list()) {
            if (m.oclass != kBoltClass) {
                continue;
            }
            const gaddr bolt = base + static_cast<gaddr>(m.slot) * kMobySize;
            if (!bolt_usable(bolt)) {
                continue;
            }
            const Vec3 nut = bolt_nut(bolt);
            const float beside = std::hypot(head.x - nut.x, head.y - nut.y);
            if (length(nut - body) < kCrankReach * s_rig.scale && beside < kCrankBeside * s_rig.scale
                && std::abs(head.z - nut.z) < kCrankHeight * s_rig.scale && length(nut - head) < best) {
                best = length(nut - head);
                s_crank.bolt = bolt;
            }
        }
        if (s_crank.bolt != 0) {
            log_line(1, "[vr-crank] the wrench sits on the bolt");
            const Vec3 feet = read_vec(s_disc->hero + kHeroPosition), axis = read_vec(s_crank.bolt + kMobyPosition);
            s_crank.radius = std::hypot(feet.x - axis.x, feet.y - axis.y);
            if (s_options.haptics && s_headset) {
                s_headset->haptic(kRight, 0.5f, 0.05f, 0);
            }
        }
    }
    if (s_crank.bolt == 0) {
        return;
    }
    const Vec3 nut = bolt_nut(s_crank.bolt);
    const double now = now_seconds();
    if (length(hand_at(kRight) - nut) > kCrankHandFar * s_rig.scale || length(body - nut) > kCrankBodyFar * s_rig.scale) {
        if (s_crank.far_since == 0) {
            s_crank.far_since = now;
        } else if (now - s_crank.far_since >= 0.25) {
            let_go_of_bolt("pulled away");
            return;
        }
    } else {
        s_crank.far_since = 0;
    }
    s_crank.turn = 0;
    if (s_crank.seen) {
        const float bx = s_crank.last.x - nut.x, by = s_crank.last.y - nut.y;
        const float ax = body.x - nut.x, ay = body.y - nut.y;
        const float before = std::hypot(bx, by), after = std::hypot(ax, ay);
        if (length(body - s_crank.last) < 0.30f * s_rig.scale && before >= kCrankRadiusLeast && after >= kCrankRadiusLeast
            && after < kCrankRadiusMost) {
            const float orbit = std::atan2(bx * ay - by * ax, bx * ax + by * ay);
            if (std::abs(orbit) <= kCrankOrbitMost) {
                s_crank.turn = orbit;
            }
        }
    }
    s_crank.last = body;
    s_crank.seen = true;
}

template <class Original>
void bolt_update(Original original, uint32_t bolt) {
    if (s_crank.bolt != bolt) {
        original(bolt);
        return;
    }
    const gaddr vars = ref<gaddr>(bolt + kMobyPvars);
    const uint8_t state = ref<uint8_t>(bolt + kMobyState);
    if (state == kBoltIdle) {
        const float progress = get_float(vars + kBoltProgress);
        const bool ready = ref<uint32_t>(vars + kBoltIdleTicks) > static_cast<uint32_t>(0.5f / tick_seconds())
                           && get_float(bolt + kMobyPosition + 8u) == get_float(vars + kBoltTop) && progress < 1.0f;
        if (!ready) {
            original(bolt);
            return;
        }
        ref<uint32_t>(vars + kBoltIdleTicks) = 0;
        put_float(vars + kBoltFall, 0.0f);
        put_float(vars + kBoltAngle, progress * get_float(vars + kBoltTurns) * kTwoPi);
        ref<uint8_t>(bolt + kMobyState) = kBoltTurning;
        return;
    }
    if (state != kBoltTurning) {
        original(bolt);
        return;
    }
    const float angle = get_float(vars + kBoltAngle) + std::abs(s_crank.turn);
    put_float(vars + kBoltAngle, angle);
    const float progress = std::clamp(angle / (std::max(get_float(vars + kBoltTurns), 0.01f) * kTwoPi), 0.0f, 1.0f);
    put_float(vars + kBoltProgress, progress);
    // Its hexagon follows him round: toward 15 degrees past him, a sixth of a turn at a time.
    const Vec3 at = read_vec(bolt + kMobyPosition);
    const float target = wrap_angle(std::atan2(s_hero_after.y - at.y, s_hero_after.x - at.x) + kPi / 12.0f);
    float rot = get_float(bolt + kMobyEuler + 8u);
    const float step = tick_seconds() * kPi;
    rot = wrap_angle(rot + std::clamp(wrap_angle(target - rot), -step, step));
    if (wrap_angle(rot - target) > kPi / 3.0f) {
        rot = wrap_angle(rot - kPi / 3.0f);
    } else if (wrap_angle(rot - target) < -kPi / 3.0f) {
        rot = wrap_angle(rot + kPi / 3.0f);
    }
    put_float(bolt + kMobyEuler + 8u, rot);
    static uint64_t logged = 0;
    if (s_tick >= logged && s_crank.turn != 0.0f) {
        logged = s_tick + 25;
        char line[80];
        std::snprintf(line, sizeof line, "[vr-crank] turned %.0f%%", progress * 100.0f);
        log_line(1, line);
    }
    if (progress >= 1.0f) {
        log_line(1, "[vr-crank] done: the game finishes it");
        ref<int32_t>(s_disc->hero + kHeroState) = kStateCrank;
        original(bolt);
        s_crank = {};
    }
}

// The wrench seated: its jaws on the bolt, its shaft to the hand, lengthened or shortened about them.
bool seat_wrench(gaddr wrench) {
    if (s_crank.bolt == 0) {
        return false;
    }
    HandFrame hand;
    Mount mount = hand_mount(wrench, hand);
    const float base = s_grown == wrench ? s_grown_scale : get_float(wrench + kMobyScale);
    const float k = base * kWrenchScale / 1024.0f;
    const Vec3 nut = bolt_nut(s_crank.bolt);
    const Vec3 to = nut - hand.at;
    const float flat = std::hypot(to.x, to.y);
    float cx = 1, sy = 0;
    if (flat > 0.05f) {
        cx = to.x / flat;
        sy = to.y / flat;
    }
    const float pitch = std::clamp(std::atan2(to.z, flat), -0.35f, 0.35f);
    const Vec3 x{cx * std::cos(pitch), sy * std::cos(pitch), std::sin(pitch)};
    Vec3 z = Vec3{0, 0, 1} - x * x.z;
    z = z * (1.0f / length(z));
    const Vec3 y{z.y * x.z - z.z * x.y, z.z * x.x - z.x * x.z, z.x * x.y - z.y * x.x};
    const float ratio = std::clamp(dot(to, x) / ((kCrankJaws.x - kCrankGripX) * k), 0.55f, 2.5f);
    mount.axes = Mat3{x * ratio, y, z};
    mount.position = nut - (x * (ratio * kCrankJaws.x * k) + y * (kCrankJaws.y * k) + z * (kCrankJaws.z * k));
    put_mount(wrench, mount, base);
    return true;
}

// ---- The Visibomb's missile ----
//
// Rerac-vr's (vr/weapon_screen.rs): the game flies it from its own camera, which the player's eyes do
// not use. While it flies, the game's picture is shown on a monitor on the weapon in the hand, and the
// stick steers it as it comes.

constexpr int32_t kVisibombItem = 13;
const Vec3 kMonitorAt{0.0f, 0.15f, -0.12f};       // from the hand, metres
constexpr float kMonitorWidth = 0.27f;
constexpr float kMonitorTilt = 0.35f;             // radians back toward the eyes

uint64_t s_missile_tick = 0;

#if !defined(__ANDROID__)
// PC test aid (QUEST_MISSILE_TRACE=1): what the flight's start changes in the weapon's and the body's mobys and the
// hero, word by word, from the last tick before it.
void missile_diff_tick() {
    static const bool trace = std::getenv("QUEST_MISSILE_TRACE") != nullptr;
    if (!trace || !s_have_hero) {
        return;
    }
    constexpr int kMobyWords = 64, kHeroWords = 0x2300 / 4;
    static uint32_t before[2][kMobyWords];
    static std::vector<uint32_t> hero_before(kHeroWords);
    static bool flying_was = false;
    const bool flying = s_tick <= s_missile_tick + 2;
    const gaddr mobys[2] = {ref<gaddr>(s_disc->hero + kHeroSlot0), ref<gaddr>(s_disc->hero + kHeroBody)};
    if (!flying) {
        for (int m = 0; m < 2; ++m) {
            for (int w = 0; w < kMobyWords && mobys[m] != 0 && s_mod->moby_slot_of(mobys[m]) >= 0; ++w) {
                before[m][w] = ref<uint32_t>(mobys[m] + 4u * static_cast<uint32_t>(w));
            }
        }
        for (int w = 0; w < kHeroWords; ++w) {
            hero_before[w] = ref<uint32_t>(s_disc->hero + 4u * static_cast<uint32_t>(w));
        }
    } else if (!flying_was) {
        for (int m = 0; m < 2; ++m) {
            std::string line = m == 0 ? "[vr-missile] the weapon's moby changed:" : "[vr-missile] the body's moby changed:";
            for (int w = 0; w < kMobyWords && mobys[m] != 0; ++w) {
                const uint32_t now = ref<uint32_t>(mobys[m] + 4u * static_cast<uint32_t>(w));
                if (now != before[m][w]) {
                    char word[32];
                    std::snprintf(word, sizeof word, " +%02X %X>%X", w * 4, before[m][w], now);
                    line += word;
                }
            }
            log_line(1, line.c_str());
        }
        std::string line = "[vr-missile] the hero changed:";
        int shown = 0;
        for (int w = 0; w < kHeroWords && shown < 70; ++w) {
            const uint32_t now = ref<uint32_t>(s_disc->hero + 4u * static_cast<uint32_t>(w));
            if (now != hero_before[w]) {
                char word[32];
                std::snprintf(word, sizeof word, " +%X %X>%X", w * 4, hero_before[w], now);
                line += word;
                ++shown;
            }
        }
        log_line(1, line.c_str());
    }
    flying_was = flying;
}
#endif

bool missile_flying() {
    return s_tick <= s_missile_tick + 2 && s_steers && s_have_frame && s_frame.hand_valid[kRight]
           && ref<int32_t>(s_disc->hero + kHeroSlot0 + kSlotItem) == kVisibombItem;
}

// The missile's picture on the weapon's own screen (Rerac-vr's weapon_screen.rs monitor_transform): the quad of the
// decorative screen's four vertices, which joint 9 carries as the gun folds and fires, a little toward the eye so the
// screen's own face does not cover it. Through the port's moby_points_at and a world quad of the window's picture;
// without them the panel at the hand stays. Placed before each capture, read by the frame's quads.
constexpr int16_t kVisibombClass = 163;
constexpr int kVisibombScreenJoint = 9;
constexpr float kVisibombScreen[4][3] = {
    {2080.0f, -2179.0f, -588.0f}, {2080.0f, -2179.0f, 183.0f}, {1537.0f, -1958.0f, -588.0f}, {1537.0f, -1958.0f, 183.0f}};
constexpr float kWeaponScreenLift = 0.004f;

struct WeaponScreen {
    bool shown = false;
    Vec3 corner[4];
};
WeaponScreen s_weapon_screen[2];
std::atomic<int> s_weapon_screen_side{0};

void place_weapon_screen() {
    const int side = 1 - s_weapon_screen_side.load(std::memory_order_relaxed);
    WeaponScreen& screen = s_weapon_screen[side];
    screen.shown = false;
    const openrac_api& api = s_mod->api();
    const gaddr gun = s_have_hero ? hand_moby() : 0;
    float xyz[12];
    if (missile_flying() && gun != 0 && ref<int16_t>(gun + kMobyOclass) == kVisibombClass
        && api.size >= offsetof(openrac_api, moby_points_at) + sizeof(void*) && api.moby_points_at != nullptr
        && api.moby_points_at(gun, kVisibombScreenJoint, &kVisibombScreen[0][0], 4, xyz) == OPENRAC_HOOK_OK) {
        for (int i = 0; i < 4; ++i) {
            screen.corner[i] = {xyz[3 * i], xyz[3 * i + 1], xyz[3 * i + 2]};
        }
        Vec3 normal = cross(screen.corner[1] - screen.corner[0], screen.corner[2] - screen.corner[0]);
        const float size = length(normal);
        if (size > 1e-8f && std::isfinite(size)) {
            normal = normal * (1.0f / size);
            if (dot(normal, world_from_head() - screen.corner[0]) < 0.0f) {
                normal = normal * -1.0f;
            }
            for (Vec3& c : screen.corner) {
                c = c + normal * (kWeaponScreenLift * s_rig.scale);
            }
            screen.shown = true;
        }
    }
    static bool shown_was = false;
    if (screen.shown != shown_was) {
        shown_was = screen.shown;
        char line[120];
        std::snprintf(line, sizeof line, "[vr-missile] the picture %s the weapon's screen (%.2f wide)",
                      screen.shown ? "on" : "off", screen.shown ? length(screen.corner[1] - screen.corner[0]) : 0.0f);
        log_line(1, line);
    }
    s_weapon_screen_side.store(side, std::memory_order_release);
}

const WeaponScreen& weapon_screen() { return s_weapon_screen[s_weapon_screen_side.load(std::memory_order_acquire)]; }

template <class Original>
void missile_update(Original original, uint32_t moby) {
    s_missile_tick = s_tick + 1;
#if !defined(__ANDROID__)
    // PC test aid (QUEST_MISSILE_TRACE=1): how the game keeps the weapon and the hands while it flies.
    static uint64_t traced = 0;
    if (std::getenv("QUEST_MISSILE_TRACE") != nullptr && s_tick >= traced) {
        traced = s_tick + 50;
        const gaddr item = ref<gaddr>(s_disc->hero + kHeroSlot0);
        char line[200];
        std::snprintf(line, sizeof line, "[vr-missile] hero state 0x%X hand hidden %d, item 0x%X state %d mode 0x%X, steers %d arms %d",
                      ref<int32_t>(s_disc->hero + kHeroState), ref<uint8_t>(s_disc->hero + kHeroHandHidden), item,
                      item != 0 ? ref<uint8_t>(item + kMobyState) : -1, item != 0 ? ref<uint16_t>(item + kMobyMode) : 0, s_steers ? 1 : 0,
                      arms_drawn() ? 1 : 0);
        log_line(1, line);
    }
#endif
    original(moby);
}

// ---- Clank's and Giant Clank's fists ----
//
// Rerac-vr's (vr/body_fists.rs, hero/bodies.rs): a real punch of either hand strikes where that hand is,
// with the body's own attack: fast (2 m/s), over a stroke of 12 cm, not a flick of the wrist, for 10
// ticks, ended by the arm pulling back. Giant Clank's also sweeps the ground below it. The game's punch
// is not played.

constexpr float kFistSpeed = 2.0f, kFistStroke = 0.12f, kFistStill = 0.65f;
constexpr int kFistWindow = 10;
struct FistHit {
    float radius, damage;
    uint32_t mask;
};
constexpr FistHit kClankFist{0.25f, 1.0f, 0x10000u};
constexpr FistHit kGiantFist{2.0f, 4.0f, 0xB0000u};
constexpr FistHit kGiantSweep{2.2f, 40.0f, 0x30000u};

struct Fist {
    bool from_seen = false;
    Vec3 from;
    Vec3 direction;
    int active = 0;
    bool spent = false;
};
Fist s_fists[2];

bool strike(Vec3 centre, const FistHit& fist, Vec3 way) {
    const gaddr body = ref<gaddr>(s_disc->hero + kHeroBody);
    const int own = body != 0 ? s_mod->moby_slot_of(body) : -1;
    if (own < 0) {
        return false;
    }
    const gaddr base = body - static_cast<gaddr>(own) * kMobySize;
    const int hero_slot = s_mod->api().figure_slot(s_mod->handle(), -1);
    bool landed = false;
    for (const openrac_moby& m : s_mod->moby_list()) {
        if (m.slot == own || m.slot == hero_slot || (m.flags & (OPENRAC_MOBY_HIDDEN | OPENRAC_MOBY_DEAD)) != 0 || m.state >= 0x80) {
            continue;
        }
        const gaddr address = base + static_cast<gaddr>(m.slot) * kMobySize;
        if ((ref<uint16_t>(address + kMobyMode) & kMobyHittable) == 0) {
            continue;
        }
        const auto last = s_hit_at.find(m.slot);
        if (last != s_hit_at.end() && s_tick - last->second < kHitWait) {
            continue;
        }
        const Vec3 them = moby_centre(address);
        const float radius = std::min(get_float(address + 12u) / 1024.0f, 3.0f);
        if (length(them - centre) > fist.radius + 0.75f * radius) {
            continue;
        }
        openrac_hit hit{};
        hit.size = sizeof hit;
        hit.victim = m.slot;
        hit.attacker = own;
        hit.mask = fist.mask;
        hit.damage = fist.damage;
        hit.position[0] = centre.x;
        hit.position[1] = centre.y;
        hit.position[2] = centre.z;
        hit.velocity[0] = way.x;
        hit.velocity[1] = way.y;
        hit.velocity[2] = 1.0f;
        if (s_mod->api().moby_hit(s_mod->handle(), &hit) == OPENRAC_HOOK_OK) {
            s_hit_at[m.slot] = s_tick;
            landed = true;
        }
    }
    return landed;
}

// Once a tick.
// Giant Clank's missiles leave from the hands drawn (Rerac-vr's giant.rs): the game makes them at his arms' joints, 1.4
// along his heading, and his body is not drawn. Each new one goes to the hand on its side, as far along.
void giant_missiles() {
    constexpr int kMissileClass = 0x100;
    constexpr int32_t kStateMissiles = 0x5F;
    constexpr float kMissileAhead = 1.4f, kMissileNear = 12.0f;
    static std::vector<int> known;
    static bool firing = false;
    const gaddr hero = s_disc->hero;
    const gaddr body = ref<gaddr>(hero + kHeroBody);
    if (!s_have_hero || !s_steers || ref<uint8_t>(hero + kHeroVehicleMode) != kBodyGiant || hero_state() != kStateMissiles) {
        firing = false;
        return;
    }
    const int slot = s_mod->moby_slot_of(body);
    if (slot < 0) {
        return;
    }
    const gaddr base = body - static_cast<gaddr>(slot) * kMobySize;
    const float yaw = get_float(hero + kHeroFacing);
    const Vec3 ahead{std::cos(yaw), std::sin(yaw), 0}, left{-std::sin(yaw), std::cos(yaw), 0};
    if (!firing) {
        firing = true;
        known.clear();
        for (const openrac_moby& m : s_mod->moby_list()) {
            if (m.oclass == kMissileClass) {
                known.push_back(m.slot);  // an earlier volley's
            }
        }
        return;
    }
    for (const openrac_moby& m : s_mod->moby_list()) {
        if (m.oclass != kMissileClass || std::find(known.begin(), known.end(), m.slot) != known.end()) {
            continue;
        }
        known.push_back(m.slot);
        const gaddr missile = base + static_cast<gaddr>(m.slot) * kMobySize;
        const Vec3 at = read_vec(missile + kMobyPosition);
        const int side = dot(at - s_hero_after, left) > 0.0f ? kLeft : kRight;
        if (length(at - s_hero_after) > kMissileNear || !s_frame.hand_valid[side]) {
            continue;
        }
        const Vec3 from = hand_at(side) + ahead * kMissileAhead;
        put_float(missile + kMobyPosition, from.x);
        put_float(missile + kMobyPosition + 4u, from.y);
        put_float(missile + kMobyPosition + 8u, from.z);
        char line[100];
        std::snprintf(line, sizeof line, "[vr-fist] a missile from the %s hand", side == kLeft ? "left" : "right");
        log_line(1, line);
    }
}

void fist_tick() {
    const uint8_t body = s_have_hero ? ref<uint8_t>(s_disc->hero + kHeroVehicleMode) : 0;
    if ((body != kBodyClank && body != kBodyGiant) || !s_steers || !s_have_frame) {
        s_fists[0] = {};
        s_fists[1] = {};
        return;
    }
    for (int side = 0; side < 2; ++side) {
        Fist& f = s_fists[side];
        if (!s_frame.hand_valid[side]) {
            f = {};
            continue;
        }
        const Vec3 at = s_frame.hand_grip[side].position;
        const Vec3 v = s_hand_stroke[side];
        const float speed = length(v);
        if (speed < kFistStill || !f.from_seen) {
            f.from = at;
            f.from_seen = true;
            f.spent = false;
        }
        if (f.active == 0 && !f.spent && speed >= kFistSpeed && length(at - f.from) >= kFistStroke) {
            f.active = kFistWindow;
            f.spent = true;
            f.direction = v * (1.0f / speed);
            buzz(static_cast<Hand>(side), {0.22f, 0.025f, 160.0f});
            log_line(1, side == 0 ? "[vr-fist] left punch" : "[vr-fist] right punch");
        }
        if (f.active == 0) {
            continue;
        }
        --f.active;
        if (dot(v, f.direction) < -kFistStill) {  // pulling back ends it
            f.active = 0;
            continue;
        }
        const Vec3 centre = hand_at(side);
        Vec3 way = frame_to_world(rotate(yaw_quat(s_rig.yaw), f.direction));
        way.z = 0;
        way = way * (1.0f / std::max(length(way), 0.001f));
        bool landed = strike(centre, body == kBodyGiant ? kGiantFist : kClankFist, way);
        if (body == kBodyGiant) {
            for (int i = 0; i < kGiantSweepPoints; ++i) {
                landed = strike(giant_sweep_point(s_hero_after, way, i), kGiantSweep, way) || landed;
            }
        }
        if (landed) {
            buzz(static_cast<Hand>(side), {0.92f, 0.085f, 75.0f});
            buzz_later(0.09f, static_cast<Hand>(side), {0.32f, 0.030f, 135.0f});
            log_line(1, "[vr-fist] it landed");
        }
    }
}

// ---- The vibration of what is in the hand and on the back ----

constexpr int32_t kPackHeli = 2, kPackThruster = 3, kPackHydro = 4;
constexpr int32_t kStateGlide = 8, kStateHeliLong = 10, kStateHeliHigh = 0xF, kStateStomp = 0x22, kStateHover = 0x81;

// The Thruster-Pack's flame is lit (Rerac-vr's thruster_flame.rs thrusting, the game's own test): its glide and
// hover; a jump's states (0xB to 0xE) while rising; the first 44 ticks of 0x10; the stomp's first 15 ticks and,
// past 33, while in the air. Its flip (0x1C) lights frames 6 to 16 of the body's sequence: not followed here.
bool thrusting(int32_t state) {
    const int32_t timer = ref<int32_t>(s_disc->hero + kHeroStateTimer);
    if (state == kStateGlide || state == kStateHover) {
        return true;
    }
    if (state >= 0xB && state <= 0xE) {
        return get_float(s_disc->hero + kHeroVelocityZ) >= 0.0f;
    }
    if (state == 0x10) {
        return timer < game_ticks(44.0f);
    }
    if (state == kStateStomp) {
        return timer < game_ticks(15.0f) || (timer > game_ticks(33.0f) && airborne());
    }
    return false;
}
constexpr int32_t kStateHydroBoost = 0x35;

template <class Original>
int32_t take_ammo(Original original, int32_t item, int32_t count) {
    const int32_t taken = original(item, count);
    if (taken != 0) {
        const int32_t id = item >= 0 ? item : ref<int32_t>(s_disc->hero + kHeroSlot0 + kSlotItem);
        feel_round(s_left_active ? static_cast<Hand>(s_off_side) : kRight, id);
        if (s_support.held && !s_left_active) {
            feel_round(kLeft, id, 0.45f);
        }
    }
    return taken;
}

// Once a tick.
// A second beat, a moment after the first.
struct LaterPulse {
    uint64_t tick;
    Hand hand;
    Pulse pulse;
};
std::vector<LaterPulse> s_later;

void buzz_later(float seconds, Hand hand, const Pulse& pulse) {
    s_later.push_back({s_tick + static_cast<uint64_t>(std::max(1.0f, seconds / tick_seconds())), hand, pulse});
}

// ---- What the items and gadgets do (Rerac-vr's haptics.rs events) ----
//
// Each read from the game's own state: the hero block's words as Rerac-vr names them (its NTSC address less the
// hero's, 0x13F350) and the items' pvars. Each cue is logged once ([vr-feel]) so the PC tests can tell them apart.

constexpr uint32_t kHeroMeleeHit = 0xA64u;       // 0x13fdb4: the swing or the lunge hit something (once a stroke)
constexpr uint32_t kHeroSwingHooked = 0x99Cu;    // 0x13fcec: the Swingshot's hook holds on a swing target
constexpr uint32_t kHeroRopeSpeed = 0x9A8u;      // 0x13fcf8: the rope's spring velocity
constexpr uint32_t kHeroRail = 0x560u, kHeroOffRail = 0x570u, kHeroRailSpeed = 0x574u;  // 0x13f8b0, c0, c4
constexpr uint32_t kHeroNearestCache = 0x2040u, kHeroCacheDistance = 0x2044u;          // 0x141390, 0x141394
constexpr uint32_t kHeroHydroFull = 0x20B0u;     // 0x141400: the Hydrodisplacer holds water
constexpr int32_t kStatePlugged = 0x72;          // the tool in its lock or tap
bool pda_shopping();

void feel_log(const char* what) { log_line(1, (std::string("[vr-feel] ") + what).c_str()); }

// The Trespasser's lock in use (class 615, Rerac-vr's lock_snapshot): on (+0x12) and working (states 1 to 3), or
// just solved (4); its chosen ring (+0xE0), the rings' turns (+0x14, +0x18, +0x1C), the outputs powered (+0xB0,
// twelve words, 2 powered).
struct LockFeel {
    gaddr moby = 0;
    int state = 0;
    int32_t ring = 0;
    int32_t turn[3]{};
    int powered = 0;
};

LockFeel lock_now(gaddr tool, const LockFeel& was) {
    LockFeel lock;
    const int slot = tool != 0 ? s_mod->moby_slot_of(tool) : -1;
    if (slot < 0) {
        return lock;
    }
    const gaddr base = tool - static_cast<gaddr>(slot) * kMobySize;
    for (const openrac_moby& m : s_mod->moby_list()) {
        if (m.oclass != kLockClass) {
            continue;
        }
        const gaddr moby = base + static_cast<gaddr>(m.slot) * kMobySize;
        const gaddr vars = ref<gaddr>(moby + kMobyPvars);
        const bool finishing = was.moby == moby && was.state != 4 && m.state == 4;
        if (!readable(vars, 0xE4u) || ((ref<int16_t>(vars + 0x12u) == 0 || m.state < 1 || m.state > 3) && !finishing)) {
            continue;
        }
        lock.moby = moby;
        lock.state = m.state;
        lock.ring = ref<int32_t>(vars + 0xE0u);
        for (uint32_t i = 0; i < 3; ++i) {
            lock.turn[i] = ref<int32_t>(vars + 0x14u + 4u * i);
        }
        for (uint32_t i = 0; i < 12; ++i) {
            lock.powered += ref<int32_t>(vars + 0xB0u + 4u * i) == 2 ? 1 : 0;
        }
        break;
    }
    return lock;
}

// Once a tick, before the hero's state is noted as seen.
void item_feel(int32_t state, int32_t state_was) {
    const gaddr hero = s_disc->hero;
    const int32_t item = ref<int32_t>(hero + kHeroSlot0 + kSlotItem);
    const Hand lead = s_leading ? static_cast<Hand>(s_off_side) : kRight;  // the hand whose item the hero uses
    static int level_was = -1;
    const int level = s_mod->loaded_level();
    const bool fresh = level != level_was;  // a level's first tick: nothing it sets up is felt
    level_was = level;
    const bool feel = !fresh && (phase_now() == OPENRAC_PLAY_PLAYING || s_shopping);

    // The Walloper: each lunge begun (a chained one too), and its hit, once a lunge.
    static int32_t timer_was = 0;
    static int32_t hit_was = 0;
    const int32_t timer = ref<int32_t>(hero + kHeroStateTimer);
    const int32_t hit = ref<int32_t>(hero + kHeroMeleeHit);
    if (feel && state == kStateLunge && (state_was != kStateLunge || timer < timer_was)) {
        buzz(lead, {0.50f, 0.065f, 75.0f});
        feel_log("the Walloper lunges");
    }
    if (feel && state == kStateLunge && hit != 0 && hit_was == 0) {
        buzz(lead, {0.85f, 0.085f, 50.0f});
        feel_log("the Walloper hits");
    }
    timer_was = timer;
    hit_was = hit;

    // The Morph-o-Ray's target morphed: its meter (+0x18) run out and the target (+0x10) let go. The hand item and
    // the second item each.
    static gaddr morph_target[2] = {0, 0};
    static float morph_meter[2] = {0, 0};
    const gaddr morph_guns[2] = {item == 21 ? hand_moby() : 0, s_left_moby != 0 && s_off_item == 21 && !s_leading ? s_left_moby : 0};
    for (int gun = 0; gun < 2; ++gun) {
        const gaddr vars = morph_guns[gun] != 0 ? ref<gaddr>(morph_guns[gun] + kMobyPvars) : 0;
        const bool seen = readable(vars, 0x1Cu);
        const gaddr target = seen ? ref<gaddr>(vars + 0x10u) : 0;
        const float meter = seen ? get_float(vars + 0x18u) : 0.0f;
        if (feel && morph_target[gun] != 0 && target == 0 && morph_meter[gun] <= 0.2f) {
            buzz_double(gun == 0 ? kRight : static_cast<Hand>(s_off_side), 0.55f);
            feel_log("a creature morphed");
        }
        morph_target[gun] = target;
        morph_meter[gun] = meter;
    }

    // The Swingshot: its hook taking hold of a swing target, the item's state turning to 2 or 9, and letting go.
    static int16_t hooked_was = 0;
    static uint8_t swing_was = 0;
    const int16_t hooked = ref<int16_t>(hero + kHeroSwingHooked);
    const gaddr swing = item == 12 ? hand_moby() : 0;
    if (swing != 0) {
        const uint8_t swing_state = ref<uint8_t>(swing + kMobyState);
        if (feel && hooked != 0 && hooked_was == 0) {
            buzz(lead, {0.70f, 0.070f, 95.0f});
            feel_log("the Swingshot's hook holds");
        } else if (feel && swing_state != swing_was && (swing_state == 2 || swing_state == 9)) {
            buzz(lead, {0.35f, 0.030f, 155.0f});
            feel_log(swing_state == 2 ? "the Swingshot's state 2" : "the Swingshot's state 9");
        }
        if (feel && hooked == 0 && hooked_was != 0) {
            buzz(lead, {0.22f, 0.025f, 140.0f});
            feel_log("the Swingshot lets go");
        }
        swing_was = swing_state;
    }
    hooked_was = hooked;

    // The Hydrodisplacer's tank filling or emptying, the Hologuise put on or off.
    static int hydro_was = -1, body_was = -1;
    const int hydro = ref<uint8_t>(hero + kHeroHydroFull) != 0 ? 1 : 0;
    const int body = ref<uint8_t>(hero + kHeroVehicleMode);
    if (feel && hydro_was >= 0 && hydro != hydro_was) {
        buzz_double(kRight, 0.48f);
        feel_log(hydro != 0 ? "the Hydrodisplacer is full" : "the Hydrodisplacer is empty");
    }
    if (feel && body_was >= 0 && body != body_was && (body == kBodyDisguise || body_was == kBodyDisguise)) {
        buzz_double(kRight, 0.28f);
        feel_log(body == kBodyDisguise ? "the Hologuise on" : "the Hologuise off");
    }
    hydro_was = hydro;
    body_was = body;

    // The PDA's vendor opening, and each buy from it.
    static bool pda_was = false;
    static int32_t bolts_was = 0;
    const bool pda = pda_shopping();
    const int32_t bolts = ref<int32_t>(s_disc->bolts);
    if (pda && !pda_was) {
        buzz_double(kRight, 0.22f);
        feel_log("the PDA's vendor opens");
    } else if (pda && pda_was && bolts < bolts_was) {
        buzz_double(kRight, 0.40f);
        feel_log("a buy from the PDA");
    }
    pda_was = pda;

    // The Metal Detector's dig word (its pvar +0x08 set to 0x7FFF, Rerac-vr's detector_dig).
    static int32_t dig_was = 0;
    const gaddr detector = item == 27 ? hand_moby() : 0;
    const gaddr detector_vars = detector != 0 ? ref<gaddr>(detector + kMobyPvars) : 0;
    const int32_t dig = readable(detector_vars, 0x0Cu) ? ref<int32_t>(detector_vars + 0x08u) : 0;
    if (feel && detector != 0 && dig == 0x7FFF && dig_was != 0x7FFF) {
        buzz_double(kRight, 0.50f);
        feel_log("the Metal Detector digs");
    }
    dig_was = dig;

    // The Trespasser: its lock's ring chosen, turned, an output powered, solved; and the tool going in.
    static LockFeel lock_was;
    const LockFeel lock = item == kTrespasserItem ? lock_now(hand_moby(), lock_was) : LockFeel{};
    if (feel && lock.moby != 0 && lock.moby == lock_was.moby) {
        if (lock.ring != lock_was.ring) {
            buzz(kRight, {0.15f, 0.018f, 180.0f});
            feel_log("the lock's ring chosen");
        }
        if (std::memcmp(lock.turn, lock_was.turn, sizeof lock.turn) != 0) {
            buzz(kRight, {0.26f, 0.025f, 145.0f});
            feel_log("the lock's ring turned");
        }
        if (lock.powered > lock_was.powered) {
            buzz(kRight, {0.40f, 0.035f, 190.0f});
            feel_log("the lock's output powered");
        }
        if (lock.state == 4 && lock_was.state != 4) {
            buzz_double(kRight, 0.55f);
            feel_log("the lock solved");
        }
    }
    if (feel && item == kTrespasserItem && state == kStatePlugged && state_was != kStatePlugged) {
        buzz(kRight, {0.30f, 0.040f, 120.0f});
        feel_log("the Trespasser goes in");
    }
    lock_was = lock;

    // The Visibomb's missile gone: its blast.
    static bool flying_was = false;
    const bool flying = missile_flying();
    if (feel && flying_was && !flying && item == 13) {
        buzz(kRight, {0.65f, 0.085f, 50.0f});
        feel_log("the Visibomb's blast");
    }
    flying_was = flying;

    // The Suck Cannon's vortex taking bolts in (Rerac-vr's vacuumed): a light tick.
    const gaddr suck = item == 9 ? hand_moby() : 0;
    const uint8_t sucking = suck != 0 ? ref<uint8_t>(suck + kMobyState) : 0;
    if (feel && (sucking == 2 || sucking == 3) && bolts > bolts_was) {
        buzz(kRight, {0.20f, 0.020f, 140.0f});
        feel_log("the vacuum takes bolts");
    }
    bolts_was = bolts;

    // A bomb, mine or Decoy come into the palm.
    static bool palm_was[2] = {false, false};
    for (int side = 0; side < 2; ++side) {
        if (feel && s_throw[side].held && !palm_was[side]) {
            buzz(static_cast<Hand>(side), {0.20f, 0.022f, 150.0f});
            feel_log(side == kLeft ? "a bomb in the left palm" : "a bomb in the right palm");
        }
        palm_was[side] = s_throw[side].held;
    }

    // A grind rail hums low in both hands, a little more the faster; a rail taken, or the boots taking hold of a
    // surface, knocks.
    static uint64_t rail_next = 0;
    static int32_t rail_was = -1;
    static uint8_t frame_was = 0;
    const bool on_rail = hero_mode() == kModeRail && ref<int32_t>(hero + kHeroOffRail) == 0;
    const int32_t rail = on_rail ? ref<int32_t>(hero + kHeroRail) : -1;
    const uint8_t frame_mode = ref<uint8_t>(hero + kHeroFrameMode);
    if (feel && on_rail && s_tick >= rail_next) {
        rail_next = s_tick + static_cast<uint64_t>(std::max(1.0f, 0.085f / tick_seconds()));
        const Pulse hum{std::clamp(0.055f + std::fabs(get_float(hero + kHeroRailSpeed)) * 0.004f, 0.055f, 0.14f), 0.055f, 90.0f};
        buzz(kLeft, hum);
        buzz(kRight, hum);
    }
    if (feel && ((frame_mode != 0 && frame_was == 0) || (on_rail && rail != rail_was))) {
        buzz(kLeft, {0.35f, 0.060f, 80.0f});
        buzz(kRight, {0.35f, 0.060f, 80.0f});
        feel_log(on_rail && rail != rail_was ? "a rail taken" : "the boots take hold");
    }
    rail_was = rail;
    frame_was = frame_mode;
}

// Rerac-vr's haptics.rs, what the game's own state tells: bolts collected, the Suck Cannon's load and
// its shot, the Hyper Strike's start and landing, the climb's heave.
void feel_events() {
    for (std::size_t i = 0; i < s_later.size();) {
        if (s_tick >= s_later[i].tick) {
            buzz(s_later[i].hand, s_later[i].pulse);
            s_later.erase(s_later.begin() + static_cast<std::ptrdiff_t>(i));
        } else {
            ++i;
        }
    }
    const gaddr hero = s_disc->hero;
    const int32_t state = hero_state();
    static int32_t state_was = 0;
    item_feel(state, state_was);
    if (state != state_was) {
        if (state == kStateJumpAttack) {
            buzz(kRight, {0.28f, 0.030f, 110.0f});
        } else if (state_was == kStateJumpAttack) {
            buzz(kRight, {0.95f, 0.105f, 55.0f});
            buzz(kLeft, {0.48f, 0.075f, 55.0f});
            buzz_later(0.12f, kRight, {0.30f, 0.035f, 125.0f});
        }
        if (climbing(state)) {
            for (const Hand hand : {kLeft, kRight}) {
                buzz(hand, {0.65f, 0.075f, 85.0f});
                buzz_later(0.095f, hand, {0.24f, 0.030f, 145.0f});
            }
        }
        state_was = state;
    }
    // A bolt is a small tap in both hands, however many come at once.
    static int32_t bolts_was = -1;
    static uint64_t bolt_next = 0;
    const int32_t bolts = ref<int32_t>(s_disc->bolts);
    if (bolts_was >= 0 && bolts > bolts_was && s_tick >= bolt_next && phase_now() == OPENRAC_PLAY_PLAYING) {
        bolt_next = s_tick + static_cast<uint64_t>(std::max(1.0f, 0.09f / tick_seconds()));
        buzz(kLeft, {0.10f, 0.016f, 185.0f});
        buzz(kRight, {0.10f, 0.016f, 185.0f});
    }
    bolts_was = bolts;
    // The Suck Cannon's rounds are what it holds: one in is a heavy gulp, one out a heavier kick.
    constexpr int32_t kSuckCannon = 9;
    static int32_t load_was = -1;
    const int32_t load = ref<int32_t>(s_disc->ammo + 4u * static_cast<uint32_t>(kSuckCannon));
    if (load_was >= 0 && ref<int32_t>(hero + kHeroSlot0 + kSlotItem) == kSuckCannon) {
        const float left = left_supports() ? 1.0f : 0.0f;
        if (load > load_was) {
            buzz(kRight, {0.95f, 0.115f, 60.0f});
            buzz_later(0.13f, kRight, {0.55f, 0.065f, 100.0f});
            buzz(kLeft, {0.50f, 0.090f, 60.0f}, left);
        } else if (load < load_was) {
            buzz(kRight, {1.0f, 0.145f, 45.0f});
            buzz_later(0.16f, kRight, {0.45f, 0.055f, 90.0f});
            buzz(kLeft, {0.65f, 0.110f, 45.0f}, left);
        }
    }
    load_was = load;
    // The weapons and gadgets that beat softly while they run (Rerac-vr's haptics.rs continuous): the Suck Cannon's
    // vacuum (its states 2 and 3), the Pyrocitor's flame (state 3), the Tesla Claw firing (warmed up, more with each
    // target), the Morph-o-Ray's beam (more on a target), the Taunter's whistle (its sequence 3), the Swingshot while
    // its hook holds him (more as the rope swings), the Hydrodisplacer draining or filling (his states 0x38, 0x39) and
    // the Metal Detector by a buried cache (stronger and quicker nearer). The hand's item while ready, and the second
    // item in its own hand; a supporting left hand shares the Suck Cannon's and the Pyrocitor's. Each loop's start is
    // logged once.
    constexpr uint32_t kHeld = 0x1C0u, kFire = 0x20u;  // the pad's held word, and Circle
    constexpr uint32_t kReadyTicks = 0x20u;            // a slot's ticks ready
    const float wave = std::sin(static_cast<float>(now_seconds()) * 21.0f) * 0.05f;
    static uint64_t loop_next[2] = {0, 0};
    static int32_t loop_was[2] = {0, 0};
    const auto run_on = [&](int side, gaddr moby, int32_t item, bool fire_held) {
        if (moby == 0) {
            loop_was[side] = 0;
            return;
        }
        const uint8_t running = ref<uint8_t>(moby + kMobyState);
        const gaddr vars = ref<gaddr>(moby + kMobyPvars);
        Pulse beat{0, 0, 0};
        float every = 0.0f;
        bool shared = false;
        if (item == kSuckCannon && (running == 2 || running == 3)) {
            beat = {0.22f, 0.075f, 55.0f};
            every = 0.11f;
            shared = true;
        } else if (item == 16 && running == 3) {
            beat = {0.28f + wave, 0.070f, 70.0f};
            every = 0.08f;
            shared = true;
        } else if (item == 19 && fire_held && (running == 1 || running == 3) && state != kStatePlugged && readable(vars, 0x3Cu)
                   && ref<int32_t>(vars + 0x2Cu) == 0) {
            const int targets = (ref<gaddr>(vars + 0x20u) != 0 ? 1 : 0) + (ref<gaddr>(vars + 0x38u) != 0 ? 1 : 0);
            beat = {0.36f + wave + 0.06f * static_cast<float>(targets), 0.032f, 185.0f};
            every = 0.055f;
        } else if (item == 21 && fire_held && running != 0 && running != 4 && readable(vars, 0x14u)) {
            beat = {ref<gaddr>(vars + 0x10u) != 0 ? 0.30f : 0.18f, 0.080f, 125.0f};
            every = 0.10f;
        } else if (item == 14 && ref<uint8_t>(moby + 0x52u) == 3) {
            beat = {0.24f, 0.040f, 155.0f};
            every = 0.16f;
        } else if (item == 12 && (state == 0x25 || state == 0x26 || state == 0x2C || ref<int16_t>(hero + kHeroSwingHooked) != 0)) {
            beat = {std::clamp(0.14f + std::fabs(get_float(hero + kHeroRopeSpeed)) * 0.05f, 0.14f, 0.38f), 0.045f, 65.0f};
            every = 0.15f;
        } else if (item == kHydroItem && (state == 0x38 || state == 0x39)) {
            beat = {0.25f + wave, 0.090f, 55.0f};
            every = 0.12f;
        } else if (item == 27 && fire_held && ref<int32_t>(hero + kHeroSlot0 + kReadyTicks) > game_ticks(15.0f)
                   && ref<gaddr>(hero + kHeroNearestCache) != 0 && get_float(hero + kHeroCacheDistance) < 10.0f) {
            const float near = 1.0f - std::clamp(get_float(hero + kHeroCacheDistance) / 10.0f, 0.0f, 1.0f);
            beat = {0.16f + near * 0.25f, 0.025f, 175.0f};
            every = 0.45f - near * 0.36f;
        }
        if (every <= 0.0f) {
            loop_was[side] = 0;
            return;
        }
        if (loop_was[side] != item) {
            loop_was[side] = item;
            loop_next[side] = s_tick;
            log_line(1, ("[vr-feel] item " + std::to_string(item) + " runs in the " + (side == kLeft ? "left" : "right") + " hand").c_str());
        }
        if (s_tick < loop_next[side]) {
            return;
        }
        loop_next[side] = s_tick + static_cast<uint64_t>(std::max(1.0f, every / tick_seconds()));
        buzz(static_cast<Hand>(side), beat);
        if (shared && side == kRight && left_supports()) {
            buzz(kLeft, beat, 0.45f);
        }
    };
    if (ref<int32_t>(hero + kHeroSlot0 + kSlotState) == 2 && !s_shopping) {
        run_on(kRight, hand_moby(), ref<int32_t>(hero + kHeroSlot0 + kSlotItem), (ref<uint32_t>(s_disc->pad_state + kHeld) & kFire) != 0);
    } else {
        loop_was[kRight] = 0;
    }
    if (s_left_moby != 0 && !s_leading && !s_shopping) {
        const float trigger = s_off_side == kLeft ? s_frame.controls.left_trigger : s_frame.controls.right_trigger;
        run_on(s_off_side, s_left_moby, s_off_item, trigger > kTriggerOn);
    } else {
        loop_was[kLeft] = 0;
    }
}

// The packs (Rerac-vr's haptics.rs PackUse): what the pack on the back is doing, by the hero's state.
enum class PackUse { kNone, kHeliLift, kHeliGlide, kThrusterBoost, kThrusterGlide, kThrusterHover, kThrusterStompRise,
                     kThrusterStompDive, kHydroCruise, kHydroBoost };
constexpr uint32_t kHeroStompDown = 0x4B8u, kHeroStompGround = 0x4C4u;  // Rerac-vr's 0x13f808, 0x13f814
constexpr uint32_t kHeroVelocity = 0xE0u;                              // motion.velocity, per tick
constexpr uint32_t kPadR1R2 = 0x0Au;

int pack_of(PackUse use) {
    return use == PackUse::kHeliLift || use == PackUse::kHeliGlide ? kPackHeli
           : use == PackUse::kHydroCruise || use == PackUse::kHydroBoost ? kPackHydro : kPackThruster;
}

// Each phase's first kick: strength, seconds, hertz.
Pulse pack_start(PackUse use) {
    switch (use) {
        case PackUse::kHeliLift: return {0.32f, 0.038f, 165.0f};
        case PackUse::kHeliGlide: return {0.23f, 0.030f, 175.0f};
        case PackUse::kThrusterBoost:
        case PackUse::kThrusterStompRise: return {0.68f, 0.065f, 65.0f};
        case PackUse::kThrusterStompDive: return {0.52f, 0.060f, 50.0f};
        case PackUse::kThrusterGlide: return {0.38f, 0.045f, 80.0f};
        case PackUse::kThrusterHover: return {0.30f, 0.050f, 90.0f};
        case PackUse::kHydroCruise: return {0.27f, 0.070f, 50.0f};
        case PackUse::kHydroBoost: return {0.43f, 0.085f, 60.0f};
        default: return {0, 0, 0};
    }
}

const char* pack_use_name(PackUse use) {
    switch (use) {
        case PackUse::kHeliLift: return "the Heli-Pack lifts";
        case PackUse::kHeliGlide: return "the Heli-Pack glides";
        case PackUse::kThrusterBoost: return "the Thruster-Pack boosts";
        case PackUse::kThrusterGlide: return "the Thruster-Pack glides";
        case PackUse::kThrusterHover: return "the Thruster-Pack hovers";
        case PackUse::kThrusterStompRise: return "the Thruster-Pack's stomp rises";
        case PackUse::kThrusterStompDive: return "the Thruster-Pack's stomp dives";
        case PackUse::kHydroCruise: return "the Hydro-Pack cruises";
        case PackUse::kHydroBoost: return "the Hydro-Pack boosts";
        default: return "the pack stops";
    }
}

// Once a tick. Rerac-vr's lift windows (the high jump's curve, the long jump's take-off) are read here as his
// rising (velocity up).
void pack_feel() {
    const gaddr hero = s_disc->hero;
    const int32_t state = hero_state();
    const int32_t timer = ref<int32_t>(hero + kHeroStateTimer);
    const bool on_foot = ref<uint8_t>(hero + kHeroVehicleMode) == 0;
    const int32_t pack = on_foot ? ref<int32_t>(hero + kHeroSlot0 + 3u * kHeroSlotSize + kSlotItem) : -1;
    const float dt = tick_seconds();
    const float vz = get_float(hero + kHeroVelocityZ);
    const float speed = std::hypot(get_float(hero + kHeroVelocity), get_float(hero + kHeroVelocity + 4u)) / dt;
    PackUse use = PackUse::kNone;
    if (pack == kPackHeli) {
        if (state == kStateGlide) {
            use = PackUse::kHeliGlide;
        } else if ((state == kStateHeliHigh || state == kStateHeliLong || state == 0xE) && vz >= 0.0f && airborne()) {
            use = PackUse::kHeliLift;
        }
    } else if (pack == kPackThruster && thrusting(state)) {
        use = state == kStateGlide ? PackUse::kThrusterGlide
              : state == kStateHover ? PackUse::kThrusterHover
              : state == kStateStomp ? (ref<uint8_t>(hero + kHeroStompDown) != 0 ? PackUse::kThrusterStompDive : PackUse::kThrusterStompRise)
                                     : PackUse::kThrusterBoost;
    } else if (pack == kPackHydro && state == kStateHydroBoost && speed > 0.3f) {
        use = (ref<uint32_t>(s_disc->pad_state + 0x1C0u) & kPadR1R2) != 0 ? PackUse::kHydroBoost : PackUse::kHydroCruise;
    }
    const float load = state == kStateStomp ? std::clamp(std::fabs(vz) / dt / 20.0f, 0.0f, 1.0f) : std::clamp(speed / 7.0f, 0.0f, 1.0f);

    static int32_t pack_was = -1, state_was = 0, timer_was = 0, ground_was = 0;
    static PackUse use_was = PackUse::kNone;
    static double next = 0, started = 0;
    static uint64_t beat = 0;
    const double now = now_seconds();
    if (pack != pack_was) {
        use_was = PackUse::kNone;  // a pack put on or off: no release
    }
    const bool again = use == PackUse::kThrusterBoost && (state != state_was || timer < timer_was);
    if (use != use_was || again) {
        const PackUse previous = use_was;
        use_was = use;
        started = now;
        beat = 0;
        if (use != PackUse::kNone) {
            Pulse kick = pack_start(use);
            if (!again && previous != PackUse::kNone && pack_of(previous) == pack_of(use)) {
                kick.strength *= 0.65f;
            }
            for (const Hand hand : {kLeft, kRight}) {
                buzz(hand, kick);
                buzz_later(kick.seconds + 0.025f, hand, {kick.strength * 0.42f, 0.028f, kick.hertz * 1.25f});
            }
            next = now + kick.seconds + 0.055;  // the beats after the kick's tail
            log_line(1, (std::string("[vr-feel] ") + pack_use_name(use)).c_str());
        } else if (previous != PackUse::kNone && pack == pack_was) {
            buzz(kLeft, {0.10f, 0.025f, 95.0f});  // a gentle release, only after real use
            buzz(kRight, {0.10f, 0.025f, 95.0f});
            log_line(1, "[vr-feel] the pack stops");
        }
    }
    const int32_t ground = ref<int32_t>(hero + kHeroStompGround);
    if (pack == kPackThruster && ground > 0 && ground_was == 0 && state == kStateStomp) {
        for (const Hand hand : {kLeft, kRight}) {
            buzz(hand, {0.92f, 0.105f, 50.0f});
            buzz_later(0.13f, hand, {0.32f, 0.045f, 85.0f});
        }
        next = now + 0.20;
        log_line(1, "[vr-feel] the stomp lands");
    }
    pack_was = pack;
    state_was = state;
    timer_was = timer;
    ground_was = ground;
    if (use == PackUse::kNone || now < next) {
        return;
    }
    const float ramp = static_cast<float>(std::clamp((now - started) / 0.18, 0.0, 1.0));
    const float wave = std::sin(static_cast<float>(beat) * 1.3f) * 0.025f;
    Pulse pulse{0, 0, 0};
    double every = 0.1;
    switch (use) {
        case PackUse::kHeliLift: pulse = {0.18f + 0.08f * ramp, 0.032f, 165.0f}; every = 0.055; break;
        case PackUse::kHeliGlide: pulse = {0.12f + 0.04f * ramp, 0.028f, 180.0f}; every = 0.065; break;
        case PackUse::kThrusterBoost: pulse = {0.33f + load * 0.10f + wave, 0.065f, 65.0f}; every = 0.085; break;
        case PackUse::kThrusterGlide: pulse = {0.23f + wave, 0.060f, 75.0f}; every = 0.09; break;
        case PackUse::kThrusterHover: pulse = {0.16f + load * 0.07f + wave, 0.085f, 85.0f}; every = 0.11; break;
        case PackUse::kThrusterStompRise: pulse = {0.38f + wave, 0.055f, 65.0f}; every = 0.075; break;
        case PackUse::kThrusterStompDive: pulse = {0.24f + load * 0.24f, 0.060f, 50.0f}; every = 0.08; break;
        case PackUse::kHydroCruise: pulse = {0.10f + load * 0.14f + wave * 0.4f, 0.100f, 50.0f}; every = 0.12; break;
        case PackUse::kHydroBoost: pulse = {0.25f + load * 0.16f + wave * 0.4f, 0.110f, 65.0f}; every = 0.11; break;
        default: break;
    }
    const bool rotor = use == PackUse::kHeliLift || use == PackUse::kHeliGlide;
    for (const Hand hand : {kLeft, kRight}) {
        // The Heli-Pack's rotor: the hands by turns, the other a little less.
        buzz(hand, pulse, rotor && static_cast<uint64_t>(hand) != beat % 2 ? 0.60f : 1.0f);
    }
    next = now + every;
    ++beat;
}

void vibration_tick() {
    if (!s_have_hero) {
        return;
    }
    feel_events();
    const gaddr hero = s_disc->hero;
    static gaddr moby_was = 0;
    const gaddr moby = hand_moby();
    if (moby != moby_was) {
        moby_was = moby;
        if (moby != 0) {
            const int32_t item = ref<int32_t>(hero + kHeroSlot0 + kSlotItem);
            const Pulse click = item_comes_out(item);
            const std::vector<float> accents = item_accents(item);
            for (std::size_t i = 0; i < accents.size(); ++i) {
                const Pulse step{click.strength * accents[i], click.seconds, click.hertz};
                if (i == 0) {
                    buzz(kRight, step);
                } else {
                    buzz_later(0.085f * static_cast<float>(i), kRight, step);
                }
                if (left_supports()) {
                    buzz_later(0.085f * static_cast<float>(i) + 0.001f, kLeft, {step.strength * 0.4f, step.seconds, step.hertz});
                }
            }
        }
    }
    pack_feel();
}

// ---- Manned turrets ----
//
// Rerac-vr's (vr/turret.rs): Gaspar's cannons, Hoven's turret and Batalia's anti-aircraft turret. The
// eye sits at the seat and the view stays as it was while the turret turns beneath it, which turns
// toward where the right hand points (the gaze without it, the stick while it is pushed) at Hoven's
// speeds (turret.h); the trigger fires, and the HUD hangs along the barrels, whose reticle is at its
// centre. Each turret's update reads the stick and turns itself: the hand's turn is made first and
// the update then gets no stick and no speed to add, so all it does besides stays the game's.

constexpr uint32_t kHeroStickX = 0x1D20u;
constexpr uint32_t kHeroStickY = 0x1D24u;
constexpr uint8_t kHovenManned = 4;
constexpr uint8_t kCannonManned = 5;
constexpr uint8_t kCommandManned = 1;
constexpr int kHovenLevel = 12;
constexpr uint32_t kCameraType = 0x86u;       // a camera record's: 5 the script camera
constexpr uint32_t kCameraState = 0x70u;
constexpr uint32_t kCameraTarget = 0x80u;     // in its state: set_camera_position's
constexpr int16_t kCameraScript = 5;
constexpr uint64_t kSeatLost = 30;            // ticks in play with no word from the turret

TurretKind s_manned_kind = TurretKind::kHoven;
int s_manned_level = -1;
uint64_t s_manned_tick = 0;
Vec3 s_seat_before, s_seat_after;
bool s_seat_seen = false;

Vec3 read_vec(gaddr at) { return {get_float(at), get_float(at + 4u), get_float(at + 8u)}; }

float turret_yaw_of(gaddr moby) { return turret_heading(s_manned_kind, get_float(moby + kMobyEuler + 8u)); }

// The seat now: Gaspar's and Batalia's where the game's script camera is told to be, which follows
// the barrels.
Vec3 seat_now(gaddr moby) {
    const Vec3 position = read_vec(moby + kMobyPosition);
    if (s_manned_kind != TurretKind::kHoven) {
        const gaddr record = ref<gaddr>(level0(s_disc->follow_cam));
        if (in_ram(record) && ref<int16_t>(record + kCameraType) == kCameraScript) {
            const gaddr state = ref<gaddr>(record + kCameraState);
            const Vec3 target = in_ram(state) ? read_vec(state + kCameraTarget) : Vec3{};
            if (in_ram(state) && std::isfinite(target.x + target.y + target.z) && length(target - position) < 20.0f) {
                return target;
            }
        }
    }
    return turret_seat(s_manned_kind, position, get_float(moby + kMobyEuler + 8u));
}

// Once a tick, before the frame.
void sample_seat() {
    if (s_manned == 0 || s_mod->moby_slot_of(s_manned) < 0) {
        s_seat_seen = false;
        return;
    }
    const Vec3 seat = seat_now(s_manned);
    s_seat_before = s_seat_seen ? s_seat_after : seat;
    s_seat_after = seat;
    s_seat_seen = true;
}

void leave_seat() {
    if (s_manned != 0) {
        s_manned = 0;
        s_seat_seen = false;
        s_recentre_wanted = true;
        log_line(1, "[vr-turret] left");
    }
}

// A turret whose update has stopped, or another level: the player is not in it any more.
void watch_seat() {
    if (s_manned != 0 && (s_mod->loaded_level() != s_manned_level
                          || (phase_now() == OPENRAC_PLAY_PLAYING && s_tick > s_manned_tick + kSeatLost))) {
        leave_seat();
    }
}

bool turret_is_manned(gaddr moby, TurretKind kind) {
    if (ref<int32_t>(s_disc->hero + kHeroState) != kHeroStateMounted) {
        return false;
    }
    return kind == TurretKind::kHoven ? ref<uint8_t>(moby + kMobyState) == kHovenManned
                                      : ref<uint8_t>(moby + kMobyCommand) == kCommandManned;
}

// What the player points along, as world yaw and pitch up; false for nothing to steer by.
bool turret_aim(float& yaw, float& pitch) {
    if (!s_have_frame || !first_person_now(phase_now())) {
        return false;
    }
    const float x = get_float(s_disc->hero + kHeroStickX);
    const float y = get_float(s_disc->hero + kHeroStickY);
    if (std::sqrt(x * x + y * y) >= kTurretStick) {
        return false;
    }
    const bool hand = s_options.hand_aim && s_frame.hand_valid[kRight];
    const Quat pointing = hand ? s_frame.hand_grip[kRight].orientation : s_frame.head.orientation;
    const Vec3 d = frame_to_world(rotate(yaw_quat(s_rig.yaw) * pointing, {0, 0, -1}));
    const float flat = std::sqrt(d.x * d.x + d.y * d.y);
    if (flat <= 0.05f) {
        return false;
    }
    yaw = std::atan2(d.y, d.x);
    pitch = std::atan2(d.z, flat);
    return true;
}

// The turn the hand makes this tick. The turret's speeds are left at 0 for its update and given back
// after it, to go on from tick to tick (Rerac-vr keeps them in the same words).
struct TurretSpeeds {
    uint32_t yaw_at = 0, pitch_at = 0;
    float yaw = 0, pitch = 0;
    bool steered = false;
    float stick_x = 0, stick_y = 0;
};

TurretSpeeds steer_turret(gaddr moby, TurretKind kind) {
    TurretSpeeds out;
    const gaddr vars = ref<gaddr>(moby + kMobyPvars);
    float yaw = 0, pitch = 0;
    if (!in_ram(vars) || !turret_aim(yaw, pitch)) {
        return out;
    }
    const uint8_t state = ref<uint8_t>(moby + kMobyState);
    const float tick = tick_seconds();
    const gaddr rot_z = moby + kMobyEuler + 8u;
    const float heading = get_float(rot_z);
    if (kind == TurretKind::kHoven) {
        if (state != kHovenManned || ref<int32_t>(s_mod->relocate_level(kHovenLevel, s_disc->hoven_death_cam)) != 0) {
            return out;
        }
        out.yaw_at = 0x7Cu;
        out.pitch_at = 0x78u;
    } else if (state != kCannonManned) {
        return out;
    } else if (kind == TurretKind::kGaspar) {
        out.yaw_at = 0x60u;
        out.pitch_at = 0x64u;
    } else {
        out.yaw_at = 0x4Cu;
        out.pitch_at = 0x48u;
    }
    out.yaw = get_float(vars + out.yaw_at);
    out.pitch = get_float(vars + out.pitch_at);
    turret_yaw_speed(out.yaw, wrap_angle(yaw - turret_heading(kind, heading)), tick);
    put_float(rot_z, wrap_angle(heading + out.yaw));

    // The barrels' pitch: a var of Hoven's and Gaspar's, up positive; Batalia's whole moby, down positive.
    if (kind == TurretKind::kBatalia) {
        constexpr float kLowest = -0.7853982f, kHighest = 0.34906584f;
        const gaddr rot_y = moby + kMobyEuler + 4u;
        const float now = get_float(rot_y);
        turret_pitch_speed(out.pitch, std::clamp(-pitch, kLowest, kHighest) - now, tick);
        put_float(rot_y, std::clamp(wrap_angle(now + out.pitch), kLowest, kHighest));
    } else {
        const bool hoven = kind == TurretKind::kHoven;
        const float lowest = hoven ? -0.7853982f : -0.5235988f;
        const float highest = hoven ? 0.61086524f : 0.5235988f;
        const gaddr at = vars + (hoven ? 0x74u : 0x44u);
        const float now = get_float(at);
        turret_pitch_speed(out.pitch, std::clamp(pitch, lowest, highest) - now, tick);
        const float next = wrap_angle(now + out.pitch);
        if (!hoven && (next < lowest || next > highest)) {
            out.pitch = 0;
        }
        put_float(at, std::clamp(next, lowest, highest));
    }
    out.stick_x = get_float(s_disc->hero + kHeroStickX);
    out.stick_y = get_float(s_disc->hero + kHeroStickY);
    put_float(vars + out.yaw_at, 0.0f);
    put_float(vars + out.pitch_at, 0.0f);
    put_float(s_disc->hero + kHeroStickX, 0.0f);
    put_float(s_disc->hero + kHeroStickY, 0.0f);
    out.steered = true;
    return out;
}

template <TurretKind kind, class Original>
void turret_update(Original original, uint32_t moby) {
    if (s_mod->moby_slot_of(moby) < 0 || !turret_is_manned(moby, kind)) {
        if (s_manned == moby) {
            leave_seat();
        }
        original(moby);
        return;
    }
    s_manned_tick = s_tick;
    if (s_manned != moby) {
        s_manned = moby;
        s_manned_kind = kind;
        s_manned_level = s_mod->loaded_level();
        s_seat_seen = false;
        s_recentre_wanted = true;
        char line[120];
        std::snprintf(line, sizeof line, "[vr-turret] manned: class %d, hero on foot %d", ref<int16_t>(moby + kMobyOclass),
                      hero_on_foot() ? 1 : 0);
        log_line(1, line);
    }
    const TurretSpeeds speeds = steer_turret(moby, kind);
    original(moby);
    if (speeds.steered) {
        const gaddr vars = ref<gaddr>(moby + kMobyPvars);
        put_float(vars + speeds.yaw_at, speeds.yaw);
        put_float(vars + speeds.pitch_at, speeds.pitch);
        put_float(s_disc->hero + kHeroStickX, speeds.stick_x);
        put_float(s_disc->hero + kHeroStickY, speeds.stick_y);
    }
    static uint64_t logged = 0;
    if (s_tick >= logged) {
        logged = s_tick + 25;
        const gaddr vars = ref<gaddr>(moby + kMobyPvars);
        const float barrels = kind == TurretKind::kBatalia ? -get_float(moby + kMobyEuler + 4u)
                              : kind == TurretKind::kHoven ? get_float(vars + 0x74u)
                                                           : get_float(vars + 0x44u);
        char line[160];
        float yaw = 0, pitch = 0;
        const bool aimed = turret_aim(yaw, pitch);
        std::snprintf(line, sizeof line,
                      "[vr-turret] barrels yaw %.2f pitch %.2f, the player %s yaw %.2f pitch %.2f, seat (%.2f %.2f %.2f)",
                      turret_heading(kind, get_float(moby + kMobyEuler + 8u)), barrels, aimed ? "points" : "steers by stick",
                      yaw, pitch, s_seat_after.x, s_seat_after.y, s_seat_after.z);
        log_line(1, line);
    }
}

template <TurretKind kind, class HookPoint>
void turret_hook(openrac::Mod& mod) {
    mod.hook<HookPoint>([](auto original, uint32_t moby) { turret_update<kind>(original, moby); });
}

// ---- The helmet seen from inside ----
//
// Rerac-vr's (moby_attach.rs tracked_head): the O2 Mask or the Pilot's Helmet worn is on the player's own
// head, the eye behind its visor.

constexpr int16_t kHelmetClasses[2] = {1289, 1290};
const Vec3 kHelmetEye{0.16f, 0.0f, 0.22f};
constexpr float kHelmetStretch = 1.15f;

bool worn_on_head(gaddr moby) {
    const int16_t oclass = ref<int16_t>(moby + kMobyOclass);
    return oclass == kHelmetClasses[0] || oclass == kHelmetClasses[1];
}

void place_helmet() {
    if (!s_vr.helmet || !hands_shown() || !s_have_frame) {
        return;
    }
    for (uint32_t i = 1; i < kHeroSlotCount; ++i) {
        const gaddr at = s_disc->hero + kHeroSlots + i * kHeroSlotSize;
        for (uint32_t offset : kSlotMobys) {
            const gaddr moby = ref<gaddr>(at + offset);
            if (moby == 0 || s_mod->moby_slot_of(moby) < 0 || !worn_on_head(moby)) {
                continue;
            }
            const HandFrame head = hand_in_world(in_frame(s_rig, s_frame.head, anchor_now(1.0f)));
            Mount mount;
            mount.axes = Mat3{head.forward, head.left, head.up * kHelmetStretch};
            mount.position = head.at - mount.axes * kHelmetEye;
            put_mount(moby, mount, get_float(moby + kMobyScale));
            static gaddr told = 0;
            if (moby != told) {
                told = moby;
                char line[100];
                std::snprintf(line, sizeof line, "[vr-helmet] class %d worn, seen from inside", ref<int16_t>(moby + kMobyOclass));
                log_line(1, line);
            }
            return;
        }
    }
}

// ---- Ratchet's hands ----
//
// Rerac-vr's (moby_attach.rs tracked_hands, vr/body.rs): in first person the player sees Ratchet's own
// forearms and hands at the controllers, cut at the elbows. A free hand has its palm on the controller
// and closes into a fist with its grip; a hand on a weapon keeps the game's own fingers on it, the
// joint the game hangs the item from being where the item is. A glove and the Walloper are the hand.
// The port draws the body as those two arms alone (openrac_api.moby_arms, patches/).

// The bodies the player can be, each with the same two arms (Rerac-vr's BODY_HANDS, measured on the
// game's classes): where each arm is cut, its palm, its fingers and thumb, how far a fist curls them.
struct BodyArms {
    int16_t oclass;
    int forearm[2];
    int shoulder[2];
    int palm[2];
    int fingers[2];
    int finger_joints;
    int thumb[2];
    float curl;
};
inline constexpr BodyArms kBodyArms[] = {
    {0, {74, 54}, {72, 52}, {76, 56}, {77, 57}, 12, {89, 69}, 1.0f},    // Ratchet
    {87, {43, 33}, {43, 33}, {44, 34}, {47, 37}, 4, {45, 35}, 1.3f},    // Clank
    {419, {36, 56}, {36, 56}, {38, 58}, {0, 0}, 0, {0, 0}, 0.0f},       // Giant Clank: open hands
    {634, {28, 47}, {28, 47}, {29, 48}, {30, 49}, 12, {42, 61}, 1.0f},  // the Hologuise disguise
};
constexpr int16_t kRatchetClass = 0;
constexpr int kHeadJoint = 7;  // Ratchet's: all of his head is below it
constexpr int kThumbJoints = 2;
constexpr float kThumbCurl = 0.6f;

const BodyArms* body_arms(gaddr body) {
    const int16_t oclass = ref<int16_t>(body + kMobyOclass);
    for (const BodyArms& arms : kBodyArms) {
        if (arms.oclass == oclass) {
            return &arms;
        }
    }
    return nullptr;
}
constexpr uint32_t kItemDefSize = 0x4Cu;
constexpr uint32_t kItemDefJoint = 0x0Cu;
constexpr int32_t kItemDefJoints = 9;
constexpr uint32_t kHeroWaterHidden = 0x20AEu;  // the items put away (Rerac-vr's f13fe), for the wrench
constexpr uint32_t kHeroHandHidden = 0x20AFu;   // ... for all of them (its f13ff)

bool s_arms_api = false;  // the port has moby_arms (patches/)
bool s_arms_on = false;

bool arms_drawn() {
    if (!s_arms_api || !s_options.show_hands || !s_options.hide_body || !hands_shown() || !s_have_hero || s_health <= 0) {
        return false;
    }
    const gaddr body = ref<gaddr>(s_disc->hero + kHeroBody);
    return body != 0 && s_mod->moby_slot_of(body) >= 0 && body_arms(body) != nullptr;
}

// The wrench is out but the hand holds nothing: shown on the grip only. In flight it is its own.
bool wrench_put_down() {
    if (!swings_by_hand()) {
        return false;
    }
    return !wrench_flying(hand_moby()) && s_frame.controls.right_grip < kTriggerOff;
}

// The joint of Ratchet's the item `item` hangs from: its definition's list in the game's table, that
// list's end in his class. -1 for none. As class_joint, each word is checked.
int hanging_joint(gaddr body, int32_t item) {
    if (item <= 0 || item >= 40) {
        return -1;
    }
    const int32_t index = ref<int32_t>(level0(s_disc->item_defs) + kItemDefSize * static_cast<uint32_t>(item) + kItemDefJoint);
    if (index < 0 || index >= kItemDefJoints) {
        return -1;
    }
    const int32_t list_index = ref<int32_t>(level0(s_disc->joint_lists) + 4u * static_cast<uint32_t>(index));
    const gaddr cls = ref<gaddr>(body + kMobyClass);
    if (!in_ram(cls)) {
        return -1;
    }
    const gaddr joints = ref<gaddr>(cls + kClassJoints);
    if (!in_ram(joints) || list_index < 0 || list_index >= ref<int32_t>(joints)) {
        return -1;
    }
    const gaddr list = ref<gaddr>(joints + 4u + 4u * static_cast<uint32_t>(list_index));
    if (!in_ram(list)) {
        return -1;
    }
    const int chain = ref<int16_t>(list);
    return chain > 0 ? ref<uint8_t>(list + 4u + static_cast<uint32_t>(chain - 1)) : -1;
}

void set_rows(openrac_moby_arm& arm, Vec3 x, Vec3 y, Vec3 z, Vec3 at) {
    const Vec3 rows[3] = {x, y, z};
    for (int r = 0; r < 3; ++r) {
        arm.rows[r][0] = rows[r].x;
        arm.rows[r][1] = rows[r].y;
        arm.rows[r][2] = rows[r].z;
    }
    arm.position[0] = at.x;
    arm.position[1] = at.y;
    arm.position[2] = at.z;
}

void add_curl(openrac_moby_arm& arm, int joint, Quat turn) {
    if (arm.curl_count < OPENRAC_ARM_CURLS) {
        openrac_arm_curl& c = arm.curl[arm.curl_count++];
        c.joint = joint;
        c.quat[0] = turn.x;
        c.quat[1] = turn.y;
        c.quat[2] = turn.z;
        c.quat[3] = turn.w;
    }
}

// A hand with nothing in it: its palm at the controller, closing with the grip.
// A free hand stops at the world as what is held does (Rerac-vr's solid_contact.rs): only what is drawn
// moves, the controller's own path stays the gestures'. Asked of the game once a tick, for each hand.
Vec3 free_hand_shift(int side, const HandFrame& hand) {
    static uint64_t asked[2] = {~0ull, ~0ull};
    static Vec3 shift[2];
    if (asked[side] == s_tick) {
        return shift[side];
    }
    asked[side] = s_tick;
    shift[side] = {};
    Vec3 point;
    const Vec3 eye = world_from_head();
    const float reach = length(hand.at - eye);
    if (s_options.solid_hands && s_have_hero && reach > 0.0f && line_blocked(eye, hand.at, point)) {
        shift[side] = point + (hand.at - eye) * (-kContactBack * s_rig.scale / reach) - hand.at;
    }
    return shift[side];
}

void free_arm(openrac_moby_arm& arm, const BodyArms& body, int side, float grip) {
    const Pose pose = in_frame(s_rig, s_frame.hand_grip[side], anchor_now(1.0f));
    HandFrame hand = hand_in_world(pose);
    hand.at = hand.at + free_hand_shift(side, hand);
    arm.shown = 1;
    arm.joint = body.palm[side];
    if (side == kLeft) {
        set_rows(arm, hand.up, hand.forward, hand.left, hand.at);
    } else {
        set_rows(arm, hand.up, hand.forward * -1.0f, hand.left * -1.0f, hand.at);
    }
    const float amount = std::clamp((grip - 0.1f) / 0.8f, 0.0f, 1.0f);
    if (amount <= 0.0f || body.finger_joints == 0) {
        return;
    }
    const float sign = side == kRight ? -1.0f : 1.0f;
    const float finger = 0.5f * sign * body.curl * amount;
    const float thumb = -0.5f * sign * kThumbCurl * amount;
    for (int i = 0; i < body.finger_joints; ++i) {
        add_curl(arm, body.fingers[side] + i, Quat{std::sin(finger), 0, 0, std::cos(finger)});
    }
    for (int i = 0; i < kThumbJoints; ++i) {
        add_curl(arm, body.thumb[side] + i, Quat{0, 0, std::sin(thumb), std::cos(thumb)});
    }
}

// Before each capture, the item being in the hand by now.
void place_arms() {
    if (!s_arms_api) {
        return;
    }
    if (!arms_drawn()) {
        if (s_arms_on) {
            s_arms_on = false;
            s_mod->api().moby_arms(s_mod->handle(), -1, nullptr);
        }
        return;
    }
    const gaddr hero = s_disc->hero;
    const gaddr body = ref<gaddr>(hero + kHeroBody);
    const BodyArms& shape = *body_arms(body);
    openrac_moby_arm arms[2]{};
    arms[0].cut_only = s_vr.body == 2 ? kHeadJoint : 0;
    for (int side = 0; side < 2; ++side) {
        arms[side].root = shape.forearm[side];
        arms[side].shoulder = shape.shoulder[side];
        arms[side].joint = shape.palm[side];
    }

    const gaddr moby = shape.oclass == kRatchetClass ? hand_moby() : 0;
    const int32_t item = moby != 0 ? ref<int32_t>(hero + kHeroSlot0 + kSlotItem) : 0;
    const bool glove = glove_item(item);
    const bool wrench = moby != 0 && ref<int16_t>(moby + kMobyOclass) == kWrenchClass;
    const bool hidden = ref<uint8_t>(hero + kHeroHandHidden) != 0
                        || (wrench && ref<uint8_t>(hero + kHeroWaterHidden) != 0);
    const bool holding = moby != 0 && !hidden && !glove
                         && (!wrench || (!wrench_flying(moby) && !wrench_put_down()));
    const int joint = holding ? hanging_joint(body, item) : -1;
    const auto on_item = [&](openrac_moby_arm& arm) {
        arm.shown = 1;
        arm.joint = joint;
        const auto row = [&](uint32_t r) { return read_vec(moby + kMobyBasis + kMobyRowSize * r); };
        set_rows(arm, row(0), row(1), row(2), read_vec(moby + kMobyPosition));
    };

    const auto on_ledge = [&](int side) {
        openrac_moby_arm& arm = arms[side];
        const Vec3 tangent = ledge_tangent();
        const Vec3 anchor = ledge_anchor();
        arm.shown = 1;
        arm.slide = 1;
        set_rows(arm, tangent, {}, {}, anchor + tangent * s_ledge.along[side]);
    };
    // The second item, when it is a glove or the Walloper, is that hand.
    const auto off_replaces = [&](int side) {
        return s_left_moby != 0 && s_off_side == side && (glove_item(s_off_item) || s_off_item == kWalloperItem);
    };
    if (s_leading) {
        // The left hand leads with the hand slot's item, the right holds its own aside: both closed on
        // them, and the Walloper is the left hand.
        if (s_frame.hand_valid[kRight]) {
            free_arm(arms[kRight], shape, kRight, 1.0f);
        }
        if (s_frame.hand_valid[kLeft] && item != kWalloperItem) {
            free_arm(arms[kLeft], shape, kLeft, 1.0f);
        }
    } else if (s_ledge.grabbed[kRight]) {
        on_ledge(kRight);
    } else if (s_frame.hand_valid[kRight] && !glove && !(holding && item == kWalloperItem) && !off_replaces(kRight)) {
        if (holding && joint >= 0) {
            on_item(arms[kRight]);
        } else {
            free_arm(arms[kRight], shape, kRight, s_left_moby != 0 && s_off_side == kRight ? 1.0f : s_frame.controls.right_grip);
        }
    }
    if (s_leading) {
    } else if (s_ledge.grabbed[kLeft]) {
        on_ledge(kLeft);
    } else if (s_frame.hand_valid[kLeft] && !off_replaces(kLeft)) {
        if (s_support_on && s_support.held && joint >= 0) {
            on_item(arms[kLeft]);
        } else {
            free_arm(arms[kLeft], shape, kLeft, s_left_moby != 0 && s_off_side == kLeft ? 1.0f : s_frame.controls.left_grip);
        }
    }
    const int slot = s_mod->moby_slot_of(body);
    if (s_mod->api().moby_arms(s_mod->handle(), slot, arms) == OPENRAC_HOOK_OK) {
        s_arms_on = true;
    }
    static int32_t logged_item = -1;
    if (item != logged_item) {
        logged_item = item;
        char line[120];
        std::snprintf(line, sizeof line, "[vr-hand] item %d: right arm %s (joint %d)", item,
                      arms[kRight].shown == 0 ? "gone: the item is the hand"
                      : holding ? "on it"
                                : "free",
                      arms[kRight].joint);
        log_line(1, line);
    }
}

// ---- The quick select's ring ----
//
// Rerac-vr's (game_input.rs hand_select_menu, keep_stick_with_hero). The ring reads the left stick and
// locks the pad, the console's player having picked with that thumb. Here the right hand picks, so the
// ring is given the hand for its update and the stick and the buttons stay Ratchet's. All that is owned
// is shown, eight to a page, and the same hand's trigger turns the page; each opening starts on the
// first page.

constexpr int32_t kRingItems[] = {8,  10, 15, 16, 11, 13, 9,  18, 19, 20, 17,
                                  21, 23, 14, 24, 25, 12, 22, 26, 27, 31, 32};
constexpr int kRingSlots = 8;
constexpr uint32_t kRingSlotBytes = 0x1Cu;
constexpr uint32_t kRingSlotItem = 0x18u;
constexpr uint32_t kRingPick = 0x74u;     // the slot picked, -1 none
constexpr uint32_t kRingStage = 0x78u;
constexpr int32_t kRingPicking = 0x10000FF;
constexpr uint32_t kItemDefIcon = 0x38u;
constexpr uint32_t kPadRingX = 0x148u, kPadRingY = 0x14Cu;  // the left stick's copy: y down
constexpr uint32_t kPadPressedRaw = 0x1C4u;
constexpr uint32_t kPadMode = 0x1CCu;
constexpr int32_t kPadDirections = 0xF000;

std::vector<int32_t> owned_ring_items() {
    std::vector<int32_t> owned;
    for (const int32_t item : kRingItems) {
        if (ref<uint8_t>(s_disc->owned + static_cast<uint32_t>(item)) != 0) {
            owned.push_back(item);
        }
    }
    return owned;
}

void put_ring_slot(gaddr slots, int slot, int32_t item) {
    const gaddr entry = slots + static_cast<uint32_t>(slot) * kRingSlotBytes;
    ref<int32_t>(entry + kRingSlotItem) = item;
    ref<int32_t>(entry) = ref<uint16_t>(level0(s_disc->item_defs) + kItemDefSize * static_cast<uint32_t>(item) + kItemDefIcon);
}

void show_ring_page(gaddr ring, int page) {
    const std::vector<int32_t> owned = owned_ring_items();
    const int pages = std::max(1, (static_cast<int>(owned.size()) + kRingSlots - 1) / kRingSlots);
    s_ring_page = page % pages;
    const gaddr slots = level0(s_disc->quick_slots);
    for (int slot = 0; slot < kRingSlots; ++slot) {
        const std::size_t at = static_cast<std::size_t>(s_ring_page * kRingSlots + slot);
        const int32_t item = at < owned.size() ? owned[at] : 0;
        ref<int32_t>(s_disc->quick_items + 4u * static_cast<uint32_t>(slot)) = item;
        put_ring_slot(slots, slot, item);
    }
    ref<int32_t>(ring + kRingPick) = -1;
    ref<int32_t>(ring + kRingStage) = kRingPicking;
    log_line(1, ("[vr-ring] page " + std::to_string(s_ring_page + 1) + " of " + std::to_string(pages) + ", "
             + std::to_string(owned.size()) + " owned").c_str());
}

// A ring's own update, with that hand for its stick and the pad left alone after it.
template <typename Update>
void ring_of_hand(int side, Update update) {
    const gaddr pad = s_disc->pad_state;
    const float stick_x = get_float(pad + kPadRingX);
    const float stick_y = get_float(pad + kPadRingY);
    const int32_t pressed = ref<int32_t>(pad + kPadPressedRaw);
    // The hand's reach from where it was when the ring opened, in the head's view plane then (Rerac-vr's HandSelect):
    // past 6 cm it is a full stick's way, which the game's ring needs (over 0.9), until back within 3.5 cm.
    float stick_to_x = 0.0f, stick_to_y = 0.0f;
    if (s_frame.hand_valid[side]) {
        const Vec3 reach = rotate(conjugate(s_pick_head[side]), s_frame.hand_grip[side].position - s_pick_from[side]);
        const float distance = std::sqrt(reach.x * reach.x + reach.y * reach.y);
        if (distance > kPickStart) {
            s_pick_selecting[side] = true;
        } else if (distance < kPickStop) {
            s_pick_selecting[side] = false;
        }
        if (s_pick_selecting[side] && distance > 1e-4f) {
            stick_to_x = reach.x / distance;
            stick_to_y = reach.y / distance;
        }
    } else {
        s_pick_selecting[side] = false;
    }
    put_float(pad + kPadRingX, stick_to_x);
    put_float(pad + kPadRingY, -stick_to_y);
    ref<int32_t>(pad + kPadPressedRaw) = pressed & ~kPadDirections;  // the walking stick's
    update();
    put_float(pad + kPadRingX, stick_x);
    put_float(pad + kPadRingY, stick_y);
    ref<int32_t>(pad + kPadPressedRaw) = pressed;
    ref<int32_t>(pad + kPadMode) = 0;
}

template <typename Update>
void ring_by_hand(Update update) {
    if (!s_quick_select && !s_left_ring) {
        update();
        return;
    }
    ring_of_hand(s_left_ring ? kLeft : kRight, update);
}

// ---- Akimbo's two rings ----
//
// Rerac-vr's OffHandRing: the left stick's click opens the left hand's ring beside the right hand's, which goes to the
// right edge; both can be open. The game has one ring, the object these hooks get. Each hand keeps its own copy of what
// using a ring changes (its fades, pick and stage, the object's 0x70 to 0x7B) and its page, and the game's update and
// draw run once for each hand, on that copy, with that hand's reach and its click for Triangle. The object goes when
// the last ring has faded. Page 0 is the game's own layout of the player's quick select, the others all that is owned.
constexpr uint32_t kRingState = 0x70u, kRingStateBytes = 12u;
constexpr uint32_t kRingX = 0x50u;
constexpr uint32_t kPadAllHeld = 0x1C0u, kPadHeldTriangle = 0x10u;
constexpr int32_t kRingEdge = 8, kCanvasPixels = 512;

// The ring's width as the game draws it (level 0's quick_select_width), or a sane stand-in.
int32_t ring_width() {
    const int32_t width = ref<int32_t>(level0(s_disc->ring_size));
    return width >= 64 && width <= kCanvasPixels / 2 ? width : 0xC0;
}

struct RingHand {
    bool alive = false;  // held, or fading
    uint8_t state[kRingStateBytes]{};
    int page = 0;
};
RingHand s_ring_hand[2];
uint8_t s_ring_layout[kRingSlots * kRingSlotBytes];
void left_ring_done();

void show_hand_page(int page) {
    const gaddr slots = level0(s_disc->quick_slots);
    if (page == 0) {
        std::memcpy(s_mod->memory().pointer(slots), s_ring_layout, sizeof s_ring_layout);
        return;
    }
    // The pages after the layout: all that is owned, eight a page, from the first.
    const std::vector<int32_t> items = owned_ring_items();
    for (int slot = 0; slot < kRingSlots; ++slot) {
        const std::size_t at = static_cast<std::size_t>((page - 1) * kRingSlots + slot);
        put_ring_slot(slots, slot, at < items.size() ? items[at] : 0);
    }
}

// A hand's ring's pages: the layout, then all that is owned.
int hand_ring_pages() { return 1 + (static_cast<int>(owned_ring_items().size()) + kRingSlots - 1) / kRingSlots; }

void load_hand_ring(gaddr ring, const RingHand& hand) {
    std::memcpy(s_mod->memory().pointer(ring + kRingState), hand.state, kRingStateBytes);
    show_hand_page(hand.page);
}

template <typename Original>
void two_rings(Original original, gaddr ring) {
    static uint64_t last = 0;
    if (s_tick > last + 1) {
        s_ring_hand[kLeft].alive = s_ring_hand[kRight].alive = false;
        std::memcpy(s_ring_layout, s_mod->memory().pointer(level0(s_disc->quick_slots)), sizeof s_ring_layout);
    }
    last = s_tick;
    s_ring_seen = s_tick;
    const gaddr pad = s_disc->pad_state;
    const uint32_t held = ref<uint32_t>(pad + kPadAllHeld);
    const gaddr pending = s_disc->hero + kHeroPendingItem;
    bool ran = false;
    for (const int side : {kRight, kLeft}) {
        RingHand& hand = s_ring_hand[side];
        const bool clicked = side == kLeft ? s_left_ring : s_quick_select;
        if (clicked && !hand.alive) {
            ref<uint16_t>(ring + kRingState) = 0;
            ref<int32_t>(ring + kRingPick) = -1;
            ref<int32_t>(ring + kRingStage) = kRingPicking;
            std::memcpy(hand.state, s_mod->memory().pointer(ring + kRingState), kRingStateBytes);
            hand.page = 0;
            hand.alive = true;
        }
        if (!hand.alive) {
            continue;
        }
        ran = true;
        bool& page_wanted = side == kLeft ? s_left_page_wanted : s_page_wanted;
        load_hand_ring(ring, hand);
        if (page_wanted && clicked) {
            const int pages = hand_ring_pages();
            hand.page = (hand.page + 1) % pages;
            show_hand_page(hand.page);
            ref<int32_t>(ring + kRingPick) = -1;
            ref<int32_t>(ring + kRingStage) = kRingPicking;
            log_line(1, ("[vr-ring] " + std::string(side == kLeft ? "left" : "right") + " hand's page " + std::to_string(hand.page + 1)
                         + " of " + std::to_string(pages)).c_str());
        }
        page_wanted = false;
        // Its last tick of fading, with the other's still up: the object stays for that one.
        const bool last_fade = !clicked && ref<uint8_t>(ring + kRingState) <= 1 && s_ring_hand[1 - side].alive;
        if (last_fade) {
            ref<uint8_t>(ring + kRingState) = 2;
        }
        ref<uint32_t>(pad + kPadAllHeld) = clicked ? held | kPadHeldTriangle : held & ~kPadHeldTriangle;
        const int32_t before = ref<int32_t>(pending);
        ring_of_hand(side, [&] { original(ring); });
        if (side == kLeft && ref<int32_t>(pending) != before) {
            s_left_pick = ref<int32_t>(pending);
            ref<int32_t>(pending) = before;
        }
        std::memcpy(hand.state, s_mod->memory().pointer(ring + kRingState), kRingStateBytes);
        // The slot the hand is on, as the ring lights it: said when it changes.
        static int32_t pick_was[2] = {-2, -2};
        const int32_t pick = ref<int32_t>(ring + kRingPick);
        if (pick != pick_was[side]) {
            pick_was[side] = pick;
            const gaddr slots = level0(s_disc->quick_slots);
            const int32_t on = pick >= 0 && pick < kRingSlots ? ref<int32_t>(slots + static_cast<uint32_t>(pick) * kRingSlotBytes + kRingSlotItem) : 0;
            log_line(1, ("[vr-ring] the " + std::string(side == kLeft ? "left" : "right") + " hand on slot " + std::to_string(pick) + ", item "
                         + std::to_string(on)).c_str());
        }
        if (last_fade || (!clicked && ref<uint8_t>(ring + kRingState) == 0)) {
            hand.alive = false;
            if (side == kLeft) {
                left_ring_done();
            }
        }
    }
    ref<uint32_t>(pad + kPadAllHeld) = held;
    if (!ran) {
        original(ring);  // a tap of the click: the game's own
    }
}

// The left ring greys what the left hand cannot take (Rerac-vr's OffHandRing): its icons' frames, drawn dimmer by
// hud_frame while that ring is drawn. The Drone Device is not greyed: from the left ring it launches its drones (the
// game's own flag, which the left hand's pick leaves alone).
constexpr int32_t kDroneDevice = 24;
bool left_takes(int32_t item);
bool s_ring_dimming = false;
std::vector<int32_t> s_ring_dim;

void dim_left_ring_icons() {
    s_ring_dim.clear();
    using IconFrame = int (*)(int, int);
    static const auto icon_frame = reinterpret_cast<IconFrame>(s_mod->api().function(s_disc->icon_frame));
    if (icon_frame == nullptr) {
        return;
    }
    const gaddr slots = level0(s_disc->quick_slots);
    std::string greyed;
    for (int slot = 0; slot < kRingSlots; ++slot) {
        const gaddr entry = slots + static_cast<uint32_t>(slot) * kRingSlotBytes;
        const int32_t item = ref<int32_t>(entry + kRingSlotItem);
        if (item > 0 && !left_takes(item) && item != kDroneDevice) {
            for (const int variant : {0, 4}) {  // plain and gold, as the ring's draw picks
                s_ring_dim.push_back(icon_frame(ref<int32_t>(entry), variant));
            }
            greyed += " " + std::to_string(item);
        }
    }
    static std::string greyed_was = "-";
    if (greyed != greyed_was) {
        greyed_was = greyed;
        log_line(1, ("[vr-ring] the left ring greys:" + (greyed.empty() ? std::string(" nothing") : greyed)).c_str());
    }
}

template <class Original>
void hud_frame(Original original, uint32_t element, int32_t id, int32_t x, int32_t y, int32_t flags, int32_t alpha) {
    if (s_ring_dimming && std::find(s_ring_dim.begin(), s_ring_dim.end(), id) != s_ring_dim.end()) {
        alpha /= 3;
    }
    original(element, id, x, y, flags, alpha);
}

template <typename Original>
int32_t ring_draw(Original original, gaddr ring) {
    if (!s_vr.akimbo || (!s_ring_hand[kLeft].alive && !s_ring_hand[kRight].alive)) {
        return original(ring);
    }
    uint8_t kept[kRingStateBytes];
    std::memcpy(kept, s_mod->memory().pointer(ring + kRingState), sizeof kept);
    const int32_t x = ref<int32_t>(ring + kRingX);
    int32_t result = 0;
    for (const int side : {kLeft, kRight}) {
        if (s_ring_hand[side].alive) {
            load_hand_ring(ring, s_ring_hand[side]);
            ref<int32_t>(ring + kRingX) = side == kLeft ? kRingEdge : kCanvasPixels - ring_width() - kRingEdge;
            if (side == kLeft) {
                dim_left_ring_icons();
                s_ring_dimming = true;
            }
            result = original(ring);
            s_ring_dimming = false;
        }
    }
    ref<int32_t>(ring + kRingX) = x;
    std::memcpy(s_mod->memory().pointer(ring + kRingState), kept, sizeof kept);
    return result;
}

template <typename Original>
void ring_update(Original original, gaddr ring) {
    if (s_vr.akimbo) {
        two_rings(original, ring);
        return;
    }
    static uint64_t last = 0;
    s_ring_seen = s_tick;
    const bool opened = s_tick > last + 1;
    last = s_tick;
    if ((opened && s_ring_page != 0) || s_page_wanted) {
        show_ring_page(ring, opened ? 0 : s_ring_page + 1);
    }
    s_page_wanted = false;
    ring_by_hand([&] { original(ring); });
}

#if !defined(__ANDROID__)
// PC test aid (QUEST_RING_TRACE=1): each tick the stick is clicked, what the game's ring opening
// tests (hero_items_and_hud.c): the hero's state, the slot's switch lock, the item locks, the
// vehicle, the quick swap window, and the pad's held, pressed and lock words.
void ring_trace() {
    static const bool on = std::getenv("QUEST_RING_TRACE") != nullptr;
    const bool running = s_ring_seen + 1 >= s_tick && s_ring_seen != 0;
    if (!on || !s_have_hero || !s_have_frame || !(s_frame.controls.right_click || running)) {
        return;
    }
    const gaddr hero = s_disc->hero;
    const gaddr pad = s_disc->pad_state;
    // Novalis's help selector (D_L01_0015F674, PAL), which also gates the ring there.
    const int help = s_mod->api().loaded_level() == 1 ? ref<int32_t>(s_mod->relocate_level(1, 0x0015F674u)) : -1;
    char line[240];
    std::snprintf(line, sizeof line,
                  "[vr-ring] trace %llu: ring %s state %d lock %d use %d select %d vehicle %d swap %d help %d held 0x%X "
                  "pressed 0x%X unmasked 0x%X/0x%X mode %d",
                  static_cast<unsigned long long>(s_tick), running ? "runs" : "off", ref<int32_t>(hero + kHeroState),
                  ref<uint8_t>(hero + kHeroSlot0 + 0x1Cu), ref<uint8_t>(hero + 0x20ACu), ref<uint8_t>(hero + 0x20A7u),
                  ref<uint8_t>(hero + 0x20A4u), ref<int16_t>(hero + 0x22D6u), help, ref<int32_t>(pad + 0x1A0u),
                  ref<int32_t>(pad + 0x1A4u), ref<int32_t>(pad + 0x1C0u), ref<int32_t>(pad + 0x1C4u),
                  ref<int32_t>(pad + kPadMode));
    log_line(1, line);
}
#endif

// ---- The controller's buttons in the game's texts ----
//
// glyphs.h: the pad icons of the three fonts painted as the headset's controls, in the font
// textures in memory (effect textures 1, 2 and 3: 8-bit texels and their 256 colours in the
// level's texture payload). Painted again whenever a font texture is somewhere else or no longer
// as painted (a load brings the disc's back). The port keys effect textures by their content, so
// the new cells show from the next frame.

constexpr uint32_t kEffectDefBytes = 0x10u;  // tex0, texel and palette offsets (quadwords), log2 sizes
constexpr uint32_t kGlyphRecord = 4u;        // u, v, y offset, advance
constexpr uint8_t kFirstIcon = 0x10u;

struct FontPainted {
    gaddr pixels = 0;
    gaddr table = 0;
    uint32_t sum = 0;  // the first icon's cell, as painted
};
FontPainted s_fonts_painted[3];

uint32_t cell_sum(const uint8_t* pixels, uint32_t width, uint32_t u, uint32_t v) {
    uint32_t sum = 2166136261u;
    for (int y = 0; y < glyphs::kCellH; ++y) {
        for (int x = 0; x < glyphs::kCellW; ++x) {
            sum = (sum ^ pixels[(v + static_cast<uint32_t>(y)) * width + u + static_cast<uint32_t>(x)]) * 16777619u;
        }
    }
    return sum;
}

void paint_glyphs() {
    if (!s_options.glyphs) {
        return;
    }
    // Only where the textures stay put: the title, play and the menus, settled for a moment. A load
    // reads the next level's archive into memory the last level's textures were in (painting there
    // in a load broke the loader on PC).
    static int last_phase = -1;
    static int settled = 0;
    const int phase = phase_now();
    settled = phase == last_phase ? std::min(settled + 1, 1000) : 0;
    last_phase = phase;
    if ((phase != OPENRAC_PLAY_FRONT_END && phase != OPENRAC_PLAY_PLAYING && phase != OPENRAC_PLAY_MENU)
        || settled < 15) {
        return;
    }
    // The title prints with the executable's tables, a level with its program's (each level keeps
    // them at its own address), both on the effect textures the executable's table describes. The
    // executable's globals move with the level loaded: they are asked for each time.
    const bool title = phase == OPENRAC_PLAY_FRONT_END;
    if (!title && !s_have_hero) {
        return;
    }
    const gaddr defs = s_mod->relocate(s_disc->effect_defs);
    const gaddr payload = ref<uint32_t>(s_mod->relocate(s_disc->texture_payload));
    if (!in_ram(defs) || !in_ram(payload)) {
        return;
    }
    openrac::Memory memory = s_mod->memory();
    for (int font = 0; font < 3; ++font) {
        const gaddr def = defs + kEffectDefBytes * static_cast<uint32_t>(font + 1);
        const int width_log2 = ref<int16_t>(def + 12u), height_log2 = ref<int16_t>(def + 14u);
        if (width_log2 < 5 || width_log2 > 10 || height_log2 < 4 || height_log2 > 10) {
            continue;
        }
        const uint32_t width = 1u << width_log2, height = 1u << height_log2;
        const gaddr pixels = payload + 16u * ref<uint16_t>(def + 8u);
        const gaddr clut = payload + 16u * ref<uint16_t>(def + 10u);
        if (!in_ram(pixels) || !in_ram(pixels + width * height) || !in_ram(clut) || !in_ram(clut + 1024u)) {
            continue;
        }
        const gaddr table = title ? s_mod->relocate(s_disc->fonts[font])
                                  : level0(s_disc->level_fonts + 0x3A0u * static_cast<uint32_t>(font));
        if (!in_ram(table) || !in_ram(table + kGlyphRecord * 0xE8u)) {
            continue;
        }
        // The letter A's advance and the Cross icon's cell and advance a glyph's: anything else at
        // that address is not a glyph table, and is left alone. (An icon a font lacks has no
        // advance, and is skipped below, as Rerac-vr does.)
        const int a_advance = ref<int8_t>(table + kGlyphRecord * 'A' + 3u);
        const gaddr cross = table + kGlyphRecord * kFirstIcon;
        const int cross_advance = ref<int8_t>(cross + 3u);
        const bool plausible = a_advance > 0 && a_advance <= 32 && cross_advance > 0 && cross_advance <= 32
                               && ref<uint8_t>(cross) + static_cast<uint32_t>(glyphs::kCellW) <= width
                               && ref<uint8_t>(cross + 1u) + static_cast<uint32_t>(glyphs::kCellH) <= height;
        if (!plausible) {
            static int told_phase = -1;
            if (told_phase != phase) {
                told_phase = phase;
                char line[160];
                std::snprintf(line, sizeof line,
                              "[vr-glyphs] font %d: no glyph table at 0x%X (phase %d: A advance %d, Cross %u,%u advance %d)",
                              font, table, phase, a_advance, ref<uint8_t>(cross), ref<uint8_t>(cross + 1u), cross_advance);
                log_line(2, line);
            }
            continue;
        }
        uint8_t* texels = static_cast<uint8_t*>(memory.pointer(pixels));
        const uint8_t* palette = static_cast<const uint8_t*>(memory.pointer(clut));
        const uint32_t first_u = ref<uint8_t>(table + kGlyphRecord * kFirstIcon);
        const uint32_t first_v = ref<uint8_t>(table + kGlyphRecord * kFirstIcon + 1u);
        if (first_u + glyphs::kCellW > width || first_v + glyphs::kCellH > height) {
            continue;
        }
        FontPainted& done = s_fonts_painted[font];
        if (done.pixels == pixels && done.table == table && done.sum == cell_sum(texels, width, first_u, first_v)) {
            continue;
        }
        int painted = 0;
        for (const glyphs::Glyph& glyph : glyphs::kGlyphs) {
            const gaddr record = table + kGlyphRecord * glyph.code;
            const uint32_t u = ref<uint8_t>(record), v = ref<uint8_t>(record + 1u);
            if (ref<int8_t>(record + 3u) == 0 || u + glyphs::kCellW > width || v + glyphs::kCellH > height) {
                continue;
            }
            glyphs::Cell cell;
            const int advance = glyphs::paint(cell, glyph);
            for (int y = 0; y < glyphs::kCellH; ++y) {
                for (int x = 0; x < glyphs::kCellW; ++x) {
                    texels[(v + static_cast<uint32_t>(y)) * width + u + static_cast<uint32_t>(x)] =
                        glyphs::nearest(palette, cell.rgba[y][x]);
                }
            }
            ref<int8_t>(record + 3u) = static_cast<int8_t>(advance);
            ++painted;
        }
        done = {pixels, table, cell_sum(texels, width, first_u, first_v)};
        char line[120];
        std::snprintf(line, sizeof line, "[vr-glyphs] font %d: %d icons painted in its %ux%u texture at 0x%X (table 0x%X)", font,
                      painted, width, height, pixels, table);
        log_line(1, line);
    }
}

// ---- The surface underfoot ----
//
// Rerac-vr's gravity_frame: on Magneboots the player's up is the wall's. It carries everything from
// tracking space to the frame: the eyes and the hands.

Quat s_up_turn;

Pose in_frame(const Rig& rig, const Pose& pose, Vec3 anchor) {
    Pose out = rig.to_frame(pose, anchor);
    if (s_up_turn.w < 0.99999f) {
        out.position = anchor + rotate(s_up_turn, out.position - anchor);
        out.orientation = s_up_turn * out.orientation;
    }
    return out;
}

// Once a frame.
void follow_surface() {
    Vec3 up{0, 1, 0};
    const gaddr body = s_have_hero ? ref<gaddr>(s_disc->hero + kHeroBody) : 0;
    if (body != 0 && s_mod->moby_slot_of(body) >= 0 && ref<uint8_t>(s_disc->hero + kHeroFrameMode) != 0) {
        const Vec3 wall = world_to_frame(read_vec(body + kMobyBasis + 2u * kMobyRowSize));
        if (length(wall) > 0.5f) {
            up = wall * (1.0f / length(wall));
        }
    }
    const Vec3 old = rotate(s_up_turn, {0, 1, 0});
    const Vec3 axis{old.y * up.z - old.z * up.y, old.z * up.x - old.x * up.z, old.x * up.y - old.y * up.x};
    const float w = 1.0f + dot(old, up);
    if (w > 0.001f) {
        Quat arc{axis.x, axis.y, axis.z, w};
        const float size = std::sqrt(arc.x * arc.x + arc.y * arc.y + arc.z * arc.z + arc.w * arc.w);
        arc = {arc.x / size, arc.y / size, arc.z / size, arc.w / size};
        s_up_turn = arc * s_up_turn;
        const float n = std::sqrt(s_up_turn.x * s_up_turn.x + s_up_turn.y * s_up_turn.y + s_up_turn.z * s_up_turn.z
                                  + s_up_turn.w * s_up_turn.w);
        s_up_turn = {s_up_turn.x / n, s_up_turn.y / n, s_up_turn.z / n, s_up_turn.w / n};
    }
}

// ---- The hoverboard ----
//
// Rerac-vr's (vr/game.rs): he rides side-on, so the player faces the way his shoulders do and looks over
// one to see ahead; the view turns with the board. How far side-on is measured on him, from his shoulder
// joints. The stick steers it as it comes, the left trigger boosts, the right stick holds a trick's
// shoulder (pad.h).

constexpr float kBoardFollowSeconds = 0.12f, kBoardFollowRate = 4.0f;
constexpr int kShoulders[2] = {72, 52};

bool joints_of(gaddr moby, const int* joints, int count, Vec3* out) {
    const openrac_api& api = s_mod->api();
    if (api.size < offsetof(openrac_api, moby_joints) + sizeof(void*) || api.moby_joints == nullptr) {
        return false;
    }
    float xyz[12];
    return count <= 4 && api.moby_joints(s_mod->moby_slot_of(moby), joints, count, xyz) == OPENRAC_HOOK_OK
           && (std::memcpy(out, xyz, static_cast<std::size_t>(count) * sizeof(Vec3)), true);
}

bool s_board_heading_kept = false;  // the first ride's heading, until a recentre forgets it
float s_board_heading = 0;

// Rerac-vr's hoverboard.rs. A ride starts along the course where he stands, as a respawn does: the game's own start
// (Rilgar's yaw -2.1) faces back up it. And a landing tilted or turned away from the board, a spin the player cannot see
// from the eyes, straightens along the board instead of throwing him off (an unfinished trick still does): the game tests
// it as he lands, so that is done on the ticks just before.
constexpr uint32_t kHeroYawRate = 0x184u, kHeroBasis = 0x640u;
constexpr uint32_t kHeroBoardYaw = 0x874u, kHeroBoardJump = 0x89Cu, kHeroRacePath = 0x8B4u;
constexpr float kBoardTiltMost = 1.0471976f, kBoardTurnMost = 1.3962634f;
constexpr int kBoardPathWait = 60;  // ticks: the game finds the course on the ride's first ticks
bool s_board_started = false;

void board_face(float yaw, float tilt_kept) {
    const gaddr hero = s_disc->hero;
    put_float(hero + kHeroFacing - 8u, tilt_kept);
    put_float(hero + kHeroFacing - 4u, tilt_kept);
    put_float(hero + kHeroFacing, yaw);
    // The ride's slope alignment rebuilds the angles from these rows: left, it would turn him back on the next tick.
    const Vec3 rows[3] = {{std::cos(yaw), std::sin(yaw), 0.0f}, {-std::sin(yaw), std::cos(yaw), 0.0f}, {0.0f, 0.0f, 1.0f}};
    for (uint32_t i = 0; i < 3u; ++i) {
        put_float(hero + kHeroBasis + i * 16u, rows[i].x);
        put_float(hero + kHeroBasis + i * 16u + 4u, rows[i].y);
        put_float(hero + kHeroBasis + i * 16u + 8u, rows[i].z);
    }
}

void board_start() {
    static int waited = 0;
    const gaddr hero = s_disc->hero;
    const gaddr path = ref<gaddr>(hero + kHeroRacePath);
    const int nodes = readable(path, 16u) ? ref<int32_t>(path) : 0;
    if ((nodes < 2 || nodes > 4096 || !readable(path + 16u, 16u * static_cast<uint32_t>(nodes))) && ++waited < kBoardPathWait) {
        return;
    }
    waited = 0;
    s_board_started = true;
    if (nodes < 2 || nodes > 4096) {
        log_line(1, "[vr-board] no course to start along");
        return;
    }
    const auto node = [&](int i) { return read_vec(path + 16u + 16u * static_cast<uint32_t>(i)); };
    const Vec3 at = read_vec(hero + kHeroPosition);
    int nearest = 0;
    float best = 1e12f;
    for (int i = 0; i < nodes; ++i) {
        const Vec3 d = node(i) - at;
        const float distance = d.x * d.x + d.y * d.y + d.z * d.z;
        if (distance < best) {
            best = distance;
            nearest = i;
        }
    }
    const Vec3 along = node((nearest + 1) % nodes) - node(nearest);
    const float native = get_float(hero + kHeroFacing);
    const float yaw = std::atan2(along.y, along.x);
    board_face(yaw, 0.0f);
    put_float(hero + kHeroBoardYaw, yaw);
    char line[100];
    std::snprintf(line, sizeof line, "[vr-board] the start along the course: yaw %.2f (the game's %.2f), node %d of %d", yaw, native, nearest, nodes);
    log_line(1, line);
}

void board_landing() {
#if !defined(__ANDROID__)
    if (std::getenv("QUEST_BOARD_OLD") != nullptr) {  // PC test aid: the game's own landing, to set against this
        return;
    }
#endif
    const gaddr hero = s_disc->hero;
    const float falling = -get_float(hero + kHeroVelocity + 8u);
    if (ref<int16_t>(hero + kHeroOffGround) == 0 || ref<int16_t>(hero + kHeroBoardJump) == 0 || falling <= 0.0f) {
        return;
    }
    // Two ticks from the ground, and the rest of the way a tick's fall can take.
    if (get_float(hero + kHeroGroundDistance) > falling * 2.0f + 0.1f) {
        return;
    }
    const float board = get_float(hero + kHeroBoardYaw);
    const bool askew = std::fabs(get_float(hero + kHeroFacing - 4u)) > kBoardTiltMost || std::fabs(get_float(hero + kHeroFacing - 8u)) > kBoardTiltMost
                       || std::fabs(wrap_angle(get_float(hero + kHeroFacing) - board)) > kBoardTurnMost;
    if (askew) {
        board_face(board, 0.0f);
        put_float(hero + kHeroYawRate, 0.0f);
        log_line(1, "[vr-board] a crooked landing straightened along the board");
    }
}

// Once a tick, before the hero's state runs.
void board_tick() {
    if (!s_have_hero || !s_steers || hero_mode() != kModeBoard) {
        s_board_started = false;
        return;
    }
    if (!s_board_started) {
        board_start();
    }
#if !defined(__ANDROID__)
    // PC test aid (QUEST_BOARD_TWIST=1): each jump's way down held 2 radians off the board, as a spin leaves him.
    static bool twisted = false;
    if (std::getenv("QUEST_BOARD_TWIST") != nullptr) {
        const bool falling = ref<int16_t>(s_disc->hero + kHeroBoardJump) != 0 && ref<int16_t>(s_disc->hero + kHeroOffGround) != 0
                             && get_float(s_disc->hero + kHeroVelocity + 8u) < 0.0f;
        if (falling) {
            board_face(wrap_angle(get_float(s_disc->hero + kHeroBoardYaw) + 2.0f), 0.0f);
        }
        if (falling && !twisted) {
            log_line(1, "[sim] the board's jump twisted 2 radians");
        }
        twisted = falling;
    }
#endif
    board_landing();
#if !defined(__ANDROID__)
    static uint64_t next = 0;
    if (std::getenv("QUEST_BOARD_TRACE") != nullptr && s_tick >= next) {
        next = s_tick + 30u;
        const Vec3 at = read_vec(s_disc->hero + kHeroPosition);
        char line[140];
        std::snprintf(line, sizeof line, "[vr-board] state 0x%X at (%.1f %.1f %.1f) yaw %.2f board %.2f camera %.2f", hero_state(), at.x, at.y, at.z,
                      get_float(s_disc->hero + kHeroFacing), get_float(s_disc->hero + kHeroBoardYaw), camera_yaw());
        log_line(1, line);
    }
#endif
}

void follow_board(const Pose& head, bool steering, float seconds) {
    static bool riding = false;
    static float heading = 0;
    if (!steering || hero_mode() != kModeBoard || !s_board_started) {
        riding = false;
        return;
    }
    float facing = get_float(s_disc->hero + kHeroFacing);
    const gaddr body = ref<gaddr>(s_disc->hero + kHeroBody);
    Vec3 shoulders[2];
    static float stance = 0;
    if (body != 0 && joints_of(body, kShoulders, 2, shoulders)) {
        const Vec3 line = shoulders[0] - shoulders[1];
        if (std::hypot(line.x, line.y) > 0.01f) {
            stance = wrap_angle(std::atan2(line.y, line.x) - facing - kPi * 0.5f);
        }
    }
    facing = wrap_angle(facing + stance);
    if (!riding) {
        riding = true;
        heading = facing;
        // A rider stands side-on and may be looking over a shoulder as a ride starts: the first ride's heading stays
        // for the later starts and respawns, so the posture holds (Rerac-vr's).
        const float yaw_now = tracking_yaw(head.orientation);
        if (!s_board_heading_kept) {
            s_board_heading_kept = true;
            s_board_heading = yaw_now;
        }
        s_rig.recentre(head, wrap_angle(facing + yaw_now - s_board_heading));
        char line[80];
        std::snprintf(line, sizeof line, "[vr-board] riding, stance %.2f", stance);
        log_line(1, line);
        return;
    }
    const float error = wrap_angle(facing - heading);
    const float most = seconds * kBoardFollowRate;
    const float change = std::clamp(error * (1.0f - std::exp(-seconds / kBoardFollowSeconds)), -most, most);
    s_rig.turn(change, head.position);
    heading = wrap_angle(heading + change);
}

// ---- Cutscenes through his eyes ----
//
// Rerac-vr's (scene_render.rs ratchet_eye, vr/body.rs ratchet_hands_only): in a scene with Ratchet in it
// the player sees from between his eyes, the head free, his forearms and hands as the scene moves them.
// A scene without him stays at the game's camera.

constexpr int kEyeJoints[2] = {19, 20};
bool s_scene_eyes = false;
Vec3 s_scene_before, s_scene_after, s_scene_origin;
int s_scene_actor = -1;

// Once a tick while a scene plays.
// The last joint of `moby`'s class's joint list `list_index`, -1 for none.
int list_end_joint(gaddr moby, int list_index) {
    const gaddr cls = ref<gaddr>(moby + kMobyClass);
    if (!in_ram(cls)) {
        return -1;
    }
    const gaddr joints = ref<gaddr>(cls + kClassJoints);
    if (!in_ram(joints) || ref<int32_t>(joints) <= list_index) {
        return -1;
    }
    const gaddr list = ref<gaddr>(joints + 4u + 4u * static_cast<uint32_t>(list_index));
    if (!in_ram(list)) {
        return -1;
    }
    const int chain = ref<int16_t>(list);
    const int last = chain > 0 ? ref<uint8_t>(list + 4u + static_cast<uint32_t>(chain - 1)) : -1;
    return last < ref<uint8_t>(cls + kClassJointCount) ? last : -1;
}

// In a body, a scene without Ratchet is seen through Clank's scene actor (classes 10 and 0x555), from his
// noggin joint (list 2) raised once to his eye height in play; anyone else's scene from its own camera
// (Rerac-vr's scene_render.rs). Its slot, or -1.
constexpr int16_t kClankScene[2] = {10, 0x555};
constexpr int kClankHeadList = 2;

void sample_scene() {
    int found = -1;
    int clank = -1;
    if (s_options.scene_eyes && phase_now() == OPENRAC_PLAY_SCENE && !s_options.screen) {
        const bool in_body = ref<uint8_t>(s_disc->hero + kHeroVehicleMode) != 0;
        for (const openrac_moby& m : s_mod->moby_list()) {
            if ((m.flags & OPENRAC_MOBY_HIDDEN) != 0 || m.state >= 0x80) {
                continue;
            }
            if (m.oclass == kRatchetClass) {
                found = m.slot;
                if ((m.flags & OPENRAC_MOBY_SCENE) != 0) {
                    break;
                }
            } else if (in_body && (m.oclass == kClankScene[0] || m.oclass == kClankScene[1])) {
                clank = m.slot;
            }
        }
    }
    const bool through_clank = found < 0 && clank >= 0;
    if (through_clank) {
        found = clank;
    }
    const gaddr any = ref<gaddr>(s_disc->hero + kHeroBody);
    const int any_slot = any != 0 ? s_mod->moby_slot_of(any) : -1;
    if (found < 0 || any_slot < 0) {
        if (s_scene_eyes) {
            s_scene_eyes = false;
            s_mod->api().moby_arms(s_mod->handle(), -1, nullptr);
            s_recentre_wanted = true;
        }
        return;
    }
    const gaddr actor = any - static_cast<gaddr>(any_slot - found) * kMobySize;
    Vec3 eye;
    if (through_clank) {
        const int head = list_end_joint(actor, kClankHeadList);
        static float lift = -1.0f;
        if (head < 0 || !joints_of(actor, &head, 1, &eye)) {
            return;
        }
        if (lift < 0.0f) {
            lift = std::clamp(kEyeClank - (eye.z - get_float(actor + kMobyPosition + 8u)), 0.0f, 0.6f);
        }
        eye.z += lift;
    } else {
        Vec3 eyes[2];
        if (!joints_of(actor, kEyeJoints, 2, eyes)) {
            return;
        }
        eye = (eyes[0] + eyes[1]) * 0.5f;
    }
    if (!std::isfinite(eye.x + eye.y + eye.z)) {
        return;
    }
    s_scene_before = s_scene_eyes ? s_scene_after : eye;
    s_scene_after = eye;
    if (!s_scene_eyes) {
        s_scene_eyes = true;
        s_scene_actor = found;
        // It starts looking where his head looks (Rerac-vr's), from his head joint to his eyes, else where his body faces.
        const float body_yaw = std::atan2(get_float(actor + kMobyBasis + 4u), get_float(actor + kMobyBasis));
        float yaw = body_yaw;
        Vec3 head;
        if (!through_clank && joints_of(actor, &kHeadJoint, 1, &head)) {
            const Vec3 look = eye - head;
            if (std::hypot(look.x, look.y) > 0.03f && std::isfinite(look.x + look.y)) {
                yaw = std::atan2(look.y, look.x);
            }
        }
        if (s_have_frame) {
            s_rig.recentre(s_frame.head, yaw);
            s_scene_origin = s_frame.head.position;
        }
        char line[120];
        std::snprintf(line, sizeof line, "[vr-scene] through %s eyes, starting along his head (yaw %.2f, his body %.2f)",
                      through_clank ? "Clank's" : "Ratchet's", yaw, body_yaw);
        log_line(1, line);
    }
    // Only his arms, where the scene has them. Clank's scene bodies have no table of arms: none of him.
    openrac_moby_arm arms[2]{};
    if (through_clank) {
        s_mod->api().moby_arms(s_mod->handle(), found, arms);
        return;
    }
    const BodyArms& shape = kBodyArms[0];
    arms[0].cut_only = s_vr.body >= 1 ? kHeadJoint : 0;
    for (int side = 0; side < 2; ++side) {
        arms[side].shown = 1;
        arms[side].slide = 1;
        arms[side].root = shape.forearm[side];
        arms[side].shoulder = shape.shoulder[side];
        arms[side].joint = shape.palm[side];
    }
    s_mod->api().moby_arms(s_mod->handle(), found, arms);
}

// ---- The camera flip ----
//
// Rerac-vr's update_flip (the option is off by default): through a flip or a double jump the view turns
// about the eyes as Ratchet's head does, measured from his head joint to his eyes, and fades back.

constexpr int32_t kFlipStates[5] = {0x0B, 0x0E, 0x11, 0x29, 0x2A};
constexpr float kFlipFade = 0.15f;
constexpr int kHeadAndEye[2] = {7, 19};

float s_flip_angle = 0;  // about his right, radians
Vec3 s_flip_axis{1, 0, 0};

void update_flip(float seconds) {
    static bool flipping = false;
    static float start = 0;
    static float weight = 0;
    static float turned = 0;
    const int32_t state = hero_state();
    bool now = s_vr.camera_flip && s_steers && s_have_hero;
    if (now) {
        now = false;
        for (const int32_t flip : kFlipStates) {
            now = now || state == flip;
        }
    }
    const gaddr body = s_have_hero ? ref<gaddr>(s_disc->hero + kHeroBody) : 0;
    Vec3 joints[2];
    if (now && body != 0 && joints_of(body, kHeadAndEye, 2, joints)) {
        const Vec3 facing = read_vec(body + kMobyBasis);
        const Vec3 up = read_vec(body + kMobyBasis + 2u * kMobyRowSize);
        const Vec3 look = joints[1] - joints[0];
        const float angle = std::atan2(dot(look, up), dot(look, facing));
        if (!flipping) {
            start = angle;
            turned = 0;
        }
        turned += wrap_angle(angle - start - turned);
        s_flip_axis = world_to_frame(read_vec(body + kMobyBasis + kMobyRowSize));
    }
    flipping = now;
    weight = std::clamp(weight + (now ? seconds : -seconds) / kFlipFade, 0.0f, 1.0f);
    s_flip_angle = turned * weight * weight * (3.0f - 2.0f * weight);
}

// ---- The double jump's roll, not drawn ----
//
// Rerac-vr's drawn_state (vr/body.rs): the double jump (hero state 0x0E) rolls Ratchet's body through
// sequence 0x16. With the camera flip off the view stays level, so what is drawn of him (his body,
// his hands) holds the pose he had before it until the roll's frame 35, then blends into the live
// pose over 5 frames. Only what is drawn: the keys and the blend the port reads (moby +0x50..+0x6C)
// are swapped in before the frame's capture and given back right after it (on_frame follows the
// capture), so the game's animation, the roll's sound and the jump's timing are untouched.

constexpr int32_t kStateDoubleJump = 0x0E;
constexpr uint8_t kRollSeq = 0x16;
constexpr uint8_t kSnapshotSeq = 0xFF;  // key A from a snapshot buffer the game reuses
constexpr float kRollEnd = 35.0f;
constexpr float kRollBlend = 5.0f;
constexpr uint32_t kMobyKeys = 0x50u;  // frames A, B, sequences A, B, the blend; the keys' data at +0x68, +0x6C
constexpr uint32_t kMobyKeysBytes = 0x20u;

struct HeldKey {
    bool valid = false;
    uint8_t seq = 0, frame = 0;
    gaddr data = 0;
};
HeldKey s_held_key;
gaddr s_roll_moby = 0;  // the body whose keys are swapped for this frame, to give back
uint8_t s_roll_saved[kMobyKeysBytes];

// Before the capture.
void hold_roll() {
    s_roll_moby = 0;
    if (!s_capture_hook || !s_have_hero || !s_steers || s_vr.camera_flip || !s_held_key.valid
        || hero_state() != kStateDoubleJump) {
        return;
    }
    const gaddr body = ref<gaddr>(s_disc->hero + kHeroBody);
    if (body == 0 || s_mod->moby_slot_of(body) < 0) {
        return;
    }
    const uint8_t seq_a = ref<uint8_t>(body + 0x52u), seq_b = ref<uint8_t>(body + 0x53u);
    if (seq_a != kRollSeq && seq_b != kRollSeq) {
        return;
    }
    const float at = seq_b == kRollSeq ? static_cast<float>(ref<uint8_t>(body + 0x51u)) : 0.0f;
    const float t = std::clamp((at - kRollEnd) / kRollBlend, 0.0f, 1.0f);
    if (t >= 1.0f) {
        return;
    }
    openrac::Memory memory = s_mod->memory();
    std::memcpy(s_roll_saved, memory.pointer(body + kMobyKeys), kMobyKeysBytes);
    s_roll_moby = body;
    ref<uint8_t>(body + 0x50u) = s_held_key.frame;
    ref<uint8_t>(body + 0x52u) = s_held_key.seq;
    ref<gaddr>(body + 0x68u) = s_held_key.data;
    if (t <= 0.0f) {
        ref<uint8_t>(body + 0x51u) = s_held_key.frame;
        ref<uint8_t>(body + 0x53u) = s_held_key.seq;
        ref<gaddr>(body + 0x6Cu) = s_held_key.data;
    }
    put_float(body + 0x54u, t);
    static bool told = false;
    if (!told) {
        told = true;
        char line[120];
        std::snprintf(line, sizeof line, "[vr-body] the double jump's roll not drawn: held at sequence %d frame %d",
                      s_held_key.seq, s_held_key.frame);
        log_line(1, line);
    }
}

// After the capture: the keys back, then the pose to hold next, from before a double jump.
void release_roll() {
    if (s_roll_moby != 0) {
        if (s_mod->moby_slot_of(s_roll_moby) >= 0) {
            std::memcpy(s_mod->memory().pointer(s_roll_moby + kMobyKeys), s_roll_saved, kMobyKeysBytes);
        }
        s_roll_moby = 0;
    }
    if (!s_have_hero) {
        s_held_key.valid = false;
        return;
    }
    const gaddr body = ref<gaddr>(s_disc->hero + kHeroBody);
    if (body == 0 || s_mod->moby_slot_of(body) < 0) {
        s_held_key.valid = false;
        return;
    }
    const uint8_t seq_b = ref<uint8_t>(body + 0x53u);
    if (hero_state() != kStateDoubleJump && seq_b != kSnapshotSeq && seq_b != kRollSeq) {
        s_held_key = {true, seq_b, ref<uint8_t>(body + 0x51u), ref<gaddr>(body + 0x6Cu)};
    }
}

// ---- Flown ships ----
//
// Rerac-vr's (vr/game.rs ship_view): Ratchet flying a ship is in its canopy, the view along its heading
// and pitch, easing after it, its roll left to the head, and the tracking scale the ship's flight size
// (they fly at a quarter of their class's). The stick flies it as it comes, the trigger is its lasers, B
// and X its missiles, and the HUD hangs along the nose.

const Vec3 kShipSeat{0.8f, 0.0f, 1.3f};
constexpr float kShipEase = 0.08f;
constexpr float kShipNear = 8.0f;             // his body rides within this of the ship
constexpr uint32_t kMobyClassScale = 0x24u;   // in the class

gaddr s_ship = 0;
uint64_t s_ship_tick = 0;
Quat s_ship_view;                             // frame space, eased
bool s_ship_view_seen = false;
Vec3 s_ship_eye_before, s_ship_eye_after;
float s_ship_ratio = 1;

bool in_ship() { return s_ship != 0; }

void leave_ship() {
    if (s_ship != 0) {
        s_ship = 0;
        s_ship_view_seen = false;
        s_recentre_wanted = true;
        s_rig.scale = s_options.world_scale;
        log_line(1, "[vr-ship] out of the ship");
    }
}

template <class Original>
void ship_update(Original original, uint32_t moby) {
    original(moby);
    const bool flown = ref<int32_t>(s_disc->hero + kHeroState) == kHeroStateMounted
                       && length(read_vec(moby + kMobyPosition) - s_hero_after) < kShipNear;
    if (flown) {
        s_ship_tick = s_tick;
        if (s_ship != moby) {
            s_ship = moby;
            s_ship_view_seen = false;
            s_recentre_wanted = true;
            char line[80];
            std::snprintf(line, sizeof line, "[vr-ship] flying class %d", ref<int16_t>(moby + kMobyOclass));
            log_line(1, line);
        }
    }
}

void watch_ship() {
    if (s_ship != 0 && (s_mod->moby_slot_of(s_ship) < 0
                        || (phase_now() == OPENRAC_PLAY_PLAYING && s_tick > s_ship_tick + kSeatLost))) {
        leave_ship();
    }
}

// Before the capture.
void sample_ship() {
    if (s_ship == 0) {
        return;
    }
    const auto axis = [&](uint32_t row) {
        const Vec3 v = read_vec(s_ship + kMobyBasis + kMobyRowSize * row);
        return v * (1.0f / std::max(length(v), 0.001f));
    };
    const Vec3 forward = axis(0), left = axis(1), up = axis(2);
    const gaddr cls = ref<gaddr>(s_ship + kMobyClass);
    const float own = in_ram(cls) ? get_float(cls + kMobyClassScale) : 0.0f;
    s_ship_ratio = own > 0.0f ? get_float(s_ship + kMobyScale) / own : 1.0f;
    const Vec3 eye = read_vec(s_ship + kMobyPosition)
                     + (forward * kShipSeat.x + left * kShipSeat.y + up * kShipSeat.z) * s_ship_ratio;
    s_ship_eye_before = s_ship_view_seen ? s_ship_eye_after : eye;
    s_ship_eye_after = eye;
}

Quat ship_turn(float seconds) {
    const Vec3 forward = world_to_frame(read_vec(s_ship + kMobyBasis));
    const Quat target = looking_along(forward * (1.0f / std::max(length(forward), 0.001f)));
    if (!s_ship_view_seen) {
        s_ship_view = target;
        s_ship_view_seen = true;
    }
    // Between two near rotations: normalised, along the shorter way.
    const float t = 1.0f - std::exp(-seconds / kShipEase);
    const float sign = s_ship_view.x * target.x + s_ship_view.y * target.y + s_ship_view.z * target.z + s_ship_view.w * target.w < 0.0f ? -1.0f : 1.0f;
    Quat q{s_ship_view.x + (target.x * sign - s_ship_view.x) * t, s_ship_view.y + (target.y * sign - s_ship_view.y) * t,
           s_ship_view.z + (target.z * sign - s_ship_view.z) * t, s_ship_view.w + (target.w * sign - s_ship_view.w) * t};
    const float size = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    s_ship_view = {q.x / size, q.y / size, q.z / size, q.w / size};
    return s_ship_view;
}

// ---- Akimbo ----
//
// Rerac-vr's (vr/akimbo.rs, rc-game hero/off_hand.rs), on the game's own code: a gun in each hand, off by
// default. The port's second hand item is a moby the moby loop leaves alone, in the decode buffer the
// hand's is not in. Each tick its own update runs with the hero's hand slot its own, which is all of a
// gun's state the game keeps on the hero, and the left trigger for its fire button: it aims, spends the
// same ammo and shoots as the hand's does. Ratchet's animation stays the right hand's.
//
// The left stick's click opens the same ring, for the left hand to pick from; picking what it holds
// empties it. It takes the guns whose whole use is their update: the Swingshot and the Walloper are hero
// states and the gloves are thrown, and those stay in the right hand. One ring at a time here.

constexpr int32_t kLeftGuns[] = {9, 11, 14, 15, 16, 19, 21, 23,  // Suck Cannon, Devastator, Taunter, Blaster,
                                                                  // Pyrocitor, Tesla Claw, Morph-o-Ray, RYNO,
                                  10, 17, 20, 25,                 // the gloves, thrown by hand,
                                  12, 18};                        // and the Swingshot and Walloper, which lead
constexpr uint32_t kItemDefClass = 0x10u;
constexpr uint32_t kMobyUpdateFn = 0x74u;
constexpr uint32_t kSlotBytes = 0x50u, kSlotButton = 0x10u, kSlotReadyTicks = 0x20u;
constexpr int32_t kSlotReady = 2, kCircleButton = 0x20;
constexpr uint32_t kPadHeldBits[] = {0x1A0u, 0x1C0u}, kPadPressedBits[] = {0x1A4u, 0x1C4u};

constexpr uint32_t kHeroArmOut = 0x20A8u, kHeroArmRaised = 0x20AAu, kHeroArmTimer = 0x1BCu;
constexpr uint32_t kHeroBytes = 0x2304u;
constexpr uint32_t kHeroNoItemFlags[3] = {0x20A7u, 0x20ACu, 0x20AEu};
constexpr float kLeftGunSphere = 1.5f;  // about a gun at its size, world units
constexpr int32_t kRynoItem = 23;
constexpr uint8_t kRynoDrawing = 1, kRynoReady = 2;

uint8_t s_left_slot[kSlotBytes] = {};
int32_t s_left_arm_ticks = 0;

void put_word_at(uint8_t* bytes, uint32_t at, int32_t value) { std::memcpy(bytes + at, &value, sizeof value); }
bool s_left_fire_was = false;

bool left_takes(int32_t item) {
    return std::find(std::begin(kLeftGuns), std::end(kLeftGuns), item) != std::end(kLeftGuns);
}

// The items whose use is a hero state (Rerac-vr's off_hand.rs hero_item): in the left hand they lead.
bool leads_hero(int32_t item) { return item == 12 || item == kWalloperItem; }

void left_ring_frame(const FrameInfo& frame, const Pose& head, bool first_person) {
    const Controls& c = frame.controls;
    const bool ring = first_person && s_vr.akimbo && !s_shopping && c.left_click && frame.hand_valid[kLeft];
    if (ring && !s_left_ring) {
        s_left_pick = 0;
        s_left_ring_open = true;
        s_pick_from[kLeft] = frame.hand_grip[kLeft].position;
        s_pick_head[kLeft] = head.orientation;
        s_pick_selecting[kLeft] = false;
        s_left_page_trigger = c.left_trigger > kTriggerOn;
    }
    if (ring) {
        const bool trigger = c.left_trigger > kTriggerOn;
        s_left_page_wanted = s_left_page_wanted || (trigger && !s_left_page_trigger);
        s_left_page_trigger = trigger;
    }
    s_left_ring = ring;
}

// The game makes the ring's pick after the button is let go, again each tick as it fades, so the pick
// is the left hand's once the ring has gone.
void left_ring_closed() {
    if (!s_left_ring_open || s_left_ring || s_tick <= s_ring_seen + 2) {
        return;
    }
    left_ring_done();
}

void left_ring_done() {
    s_left_ring_open = false;
    if (s_left_pick == 0) {
        return;
    }
    if (left_takes(s_left_pick)) {
        s_left_item = s_left_item == s_left_pick ? 0 : s_left_pick;
    }
    log_line(1, ("[vr-akimbo] left hand: " + std::to_string(s_left_item) + " (picked " + std::to_string(s_left_pick) + ")").c_str());
}

// Runs the gun's own update as the hand's, its hand's trigger its fire. The hero block is put back
// and his body's moby after it but for the gun's own slot: what the gun asks of Ratchet (an arm layer, a
// stance, a state) is dropped, as his animation stays the hand's (Rerac-vr's off_hand.rs). A hand on a ledge or a bolt holds
// his state and the no-item flags, which are that hand's: for this update he stands.
void run_left_gun(void (*update)(gaddr)) {
    const gaddr hero = s_disc->hero;
    const gaddr pad = s_disc->pad_state;
    const float trigger = s_off_side == kLeft ? s_frame.controls.left_trigger : s_frame.controls.right_trigger;
    // A glove's bomb goes by the hand's throw (glove_update), not the game's: its trigger holds it. The
    // Swingshot and the Walloper fire as the hand's item, leading (lead_tick).
    const bool fire = trigger > kTriggerOn && !s_left_ring && !s_shopping && !(glove_item(s_off_item) && s_options.throw_by_hand)
                      && !leads_hero(s_off_item);
    const bool fresh = fire && !s_left_fire_was;
    s_left_fire_was = fire;
    s_left_arm_ticks = fire ? s_left_arm_ticks + 1 : 0;

    openrac::Memory memory = s_mod->memory();
    static uint8_t kept[kHeroBytes];
    static uint8_t kept_body[kMobySize];
    const gaddr body = ref<gaddr>(hero + kHeroBody);
    std::memcpy(kept, memory.pointer(hero), kHeroBytes);
    if (body != 0) {
        std::memcpy(kept_body, memory.pointer(body), kMobySize);
    }
    std::memcpy(memory.pointer(hero + kHeroSlot0), s_left_slot, kSlotBytes);
    ref<int32_t>(hero + kHeroSlot0 + kSlotReadyTicks) += 1;  // how long it has been out: the hero counts the hand's

    int32_t held[2], pressed[2];
    for (int i = 0; i < 2; ++i) {
        held[i] = ref<int32_t>(pad + kPadHeldBits[i]);
        pressed[i] = ref<int32_t>(pad + kPadPressedBits[i]);
        ref<int32_t>(pad + kPadHeldBits[i]) = (held[i] & ~kCircleButton) | (fire ? kCircleButton : 0);
        ref<int32_t>(pad + kPadPressedBits[i]) = (pressed[i] & ~kCircleButton) | (fresh ? kCircleButton : 0);
    }
    // What the hero's weapon check does for the hand's gun: the arm out while it fires, and how long.
    ref<uint8_t>(hero + kHeroArmOut) = fire ? 1 : 0;
    ref<uint8_t>(hero + kHeroArmRaised) = fire ? 1 : 0;
    ref<int32_t>(hero + kHeroArmTimer) = s_left_arm_ticks;
    if (hanging_on() || wrench_on_bolt()) {
        ref<int32_t>(hero + kHeroState) = 0;
        ref<int32_t>(hero + kHeroMode) = 0;
        for (const uint32_t flag : kHeroNoItemFlags) {
            ref<uint8_t>(hero + flag) = 0;
        }
    }
    // The hero's update advances a hand item's animation; this one's is here. The port starts it on
    // its held sequence, past the draw, which the RYNO's own state waits for.
    static const auto advance = reinterpret_cast<void (*)(gaddr)>(s_mod->api().function(s_disc->anim_advance));
    if (advance != nullptr) {
        advance(s_left_moby);
    }
    if (s_off_item == kRynoItem && ref<uint8_t>(s_left_moby + kMobyState) == kRynoDrawing) {
        ref<uint8_t>(s_left_moby + kMobyState) = kRynoReady;
    }
    s_left_active = true;
    update(s_left_moby);
    s_left_active = false;
    for (int i = 0; i < 2; ++i) {
        ref<int32_t>(pad + kPadHeldBits[i]) = held[i];
        ref<int32_t>(pad + kPadPressedBits[i]) = pressed[i];
    }
    std::memcpy(s_left_slot, memory.pointer(hero + kHeroSlot0), kSlotBytes);
    // An item that leads stays ready in the left hand: its idle update can leave the copy at state 3 (a
    // swap committed), which the hand slot would finish by putting it away.
    if (leads_hero(s_off_item)) {
        put_word_at(s_left_slot, kSlotState, kSlotReady);
    }
    std::memcpy(memory.pointer(hero), kept, kHeroBytes);
    if (body != 0) {
        std::memcpy(memory.pointer(body), kept_body, kMobySize);
    }
}

// ---- The left hand leads ----
//
// Rerac-vr's off_hand.rs lead: the Swingshot's pull and swing and the Walloper's lunge are hero states,
// which the hero update runs for the hand slot's item. The left hand's leads when its trigger (or, the
// Walloper's, a punch of it) asks while Ratchet's hands are free: its slot and the hand's change places
// for the use, the left trigger the game's fire (pad.h lead), and back when it ends. Meanwhile the right
// hand's item is kept aside, in the right hand (place_left_gun), the moby loop not running it as it never
// runs the second item; the left one, now the hand's, it still does not run: it is run here.

constexpr uint16_t kMobyNotRun = 0x2u;   // the moby loop passes it by
uint16_t s_lead_kept_flags = 0;          // the right item's own, before it was kept aside
bool s_lead_used = false;                // the lead's hero state was seen
uint64_t s_lead_ticks = 0;
// The hand's item globals (Rerac-vr's ItemGlobals, NTSC 0x141408 and 0x141660 with the hero at 0x13F350):
// the request, and the saved item Circle brings back when another is held (items.rs update_hand_selected),
// which would put the leading item away at its first press. The lead has its own: none asked, its item saved.
constexpr uint32_t kHeroSavedItem = 0x2310u;
int32_t s_lead_kept_request = 0, s_lead_kept_saved = 0;

// Ratchet's hands free for a lead: none of the hand's no-item flags, on his feet, not hanging or cranking.
bool lead_free() {
    const gaddr hero = s_disc->hero;
    for (const uint32_t flag : kHeroNoItemFlags) {
        if (ref<uint8_t>(hero + flag) != 0) {
            return false;
        }
    }
    return ref<uint8_t>(hero + kHeroHandHidden) == 0 && !hanging_on() && !wrench_on_bolt() && !s_quick_select
           && !s_left_ring && hand_moby() != 0 && hero_on_foot() && phase_now() == OPENRAC_PLAY_PLAYING;
}

void swap_lead_slots() {
    openrac::Memory memory = s_mod->memory();
    uint8_t hand[kSlotBytes];
    std::memcpy(hand, memory.pointer(s_disc->hero + kHeroSlot0), kSlotBytes);
    std::memcpy(memory.pointer(s_disc->hero + kHeroSlot0), s_left_slot, kSlotBytes);
    std::memcpy(s_left_slot, hand, kSlotBytes);
}

// The right hand's item kept aside while the left leads: its moby, or 0.
gaddr kept_right_moby() {
    gaddr moby = 0;
    std::memcpy(&moby, s_left_slot, sizeof moby);
    return moby != 0 && s_mod->moby_slot_of(moby) >= 0 && ref<uint8_t>(moby + kMobyState) < 0x80 ? moby : 0;
}

void start_lead() {
    put_word_at(s_left_slot, kSlotState, kSlotReady);
    swap_lead_slots();
    const gaddr hero = s_disc->hero;
    s_lead_kept_request = ref<int32_t>(hero + kHeroPendingItem);
    s_lead_kept_saved = ref<int32_t>(hero + kHeroSavedItem);
    ref<int32_t>(hero + kHeroPendingItem) = 0;
    ref<int32_t>(hero + kHeroSavedItem) = s_off_item;
    s_leading = true;
    s_lead_used = false;
    s_lead_ticks = 0;
    if (const gaddr right = kept_right_moby()) {
        s_lead_kept_flags = ref<uint16_t>(right + kMobyMode);
        ref<uint16_t>(right + kMobyMode) = static_cast<uint16_t>(s_lead_kept_flags | kMobyNotRun);
    }
    char line[120];
    std::snprintf(line, sizeof line, "[vr-akimbo] the left hand leads: item %d (the hand's saved item %d, request %d)",
                  s_off_item, s_lead_kept_saved, s_lead_kept_request);
    log_line(1, line);
}

void end_lead() {
    const gaddr right = kept_right_moby();
    if (right != 0) {
        const uint16_t flags = ref<uint16_t>(right + kMobyMode);
        ref<uint16_t>(right + kMobyMode) = static_cast<uint16_t>((flags & ~kMobyNotRun) | (s_lead_kept_flags & kMobyNotRun));
    }
    swap_lead_slots();
    ref<int32_t>(s_disc->hero + kHeroPendingItem) = s_lead_kept_request;
    ref<int32_t>(s_disc->hero + kHeroSavedItem) = s_lead_kept_saved;
    s_leading = false;
    char line[140];
    std::snprintf(line, sizeof line, "[vr-akimbo] the left hand's lead ends after %llu ticks: %s, the right item %s",
                  static_cast<unsigned long long>(s_lead_ticks), s_lead_used ? "its hero state ran" : "its hero state never began",
                  right != 0 ? "back in the hand" : "gone");
    log_line(1, line);
}

// Once a tick: while the left hand leads, true (the second item's own run waits).
bool lead_tick() {
    if (!s_leading) {
        return false;
    }
    if (!s_have_hero) {
        // A load made the hand's slot anew, and the kept item went with the level.
        s_leading = false;
        s_left_moby = 0;
        log_line(1, "[vr-akimbo] the left hand's lead dropped by a load");
        return false;
    }
    if (phase_now() != OPENRAC_PLAY_PLAYING) {
        return true;
    }
    ++s_lead_ticks;
    const int32_t state = hero_state();
    const gaddr moby = hand_moby();
    const bool walloper = ref<int32_t>(s_disc->hero + kHeroSlot0 + kSlotItem) == kWalloperItem;
#if !defined(__ANDROID__)
    static const bool trace = std::getenv("QUEST_AKIMBO_TRACE") != nullptr;
    if (trace) {
        const gaddr slot_moby = ref<gaddr>(s_disc->hero + kHeroSlot0);
        char line[200];
        std::snprintf(line, sizeof line, "[vr-akimbo] lead tick %llu: hero state 0x%X, slot moby 0x%X (state %d, class %d), slot item %d state %d",
                      static_cast<unsigned long long>(s_lead_ticks), state, slot_moby,
                      slot_moby != 0 ? ref<uint8_t>(slot_moby + kMobyState) : -1, slot_moby != 0 ? ref<int16_t>(slot_moby + kMobyOclass) : -1,
                      ref<int32_t>(s_disc->hero + kHeroSlot0 + kSlotItem), ref<int32_t>(s_disc->hero + kHeroSlot0 + kSlotState));
        log_line(1, line);
    }
#endif
    // In use: the lunge; the Swingshot's pull and swing, or its hook out and back.
    const bool using_it = walloper ? state == kStateLunge
                                   : state == 0x24 || state == 0x25 || state == 0x26 || state == 0x2C
                                         || (moby != 0 && ref<uint8_t>(moby + kMobyState) > 1);
    s_lead_used = s_lead_used || using_it;
    const bool asked = s_have_frame && (s_frame.controls.left_trigger > kTriggerOn || (punching() && s_punch_side == kLeft));
    const bool can = s_vr.akimbo && s_steers && s_have_frame && s_frame.hand_valid[kLeft] && hero_on_foot() && !s_quick_select;
    if (can && moby != 0 && (asked || using_it)) {
        const gaddr address = ref<gaddr>(moby + kMobyUpdateFn);
        const auto update = reinterpret_cast<void (*)(gaddr)>(address != 0 ? s_mod->api().function_at(address) : nullptr);
        if (update != nullptr) {
            update(moby);
        }
        return true;
    }
    end_lead();
    return false;
}

// Once a tick.
void akimbo_tick() {
    left_ring_closed();
    const openrac_api& api = s_mod->api();
    if (api.size < offsetof(openrac_api, second_item_drop) + sizeof(void*) || api.second_item == nullptr) {
        return;
    }
#if !defined(__ANDROID__)
    if (const char* left = std::getenv("QUEST_SIM_LEFT")) {
        s_vr.akimbo = true;
        s_left_item = s_left_item != 0 ? s_left_item : std::atoi(left);
    }
#endif
    if (lead_tick()) {
        return;
    }
    // One hand on a ledge, the other keeps its weapon (Rerac-vr's off_hand.rs): the game puts the hand's
    // item away for the hang, so a hand left free has it here, as a second gun. A right hand hanging
    // alone leaves a left gun as it is.
    // The hand's slot empties as he hangs, so it is the gun he last had out that stays.
    static int32_t last_gun = 0;
    const int32_t in_hand = s_have_hero ? ref<int32_t>(s_disc->hero + kHeroSlot0 + kSlotItem) : 0;
    const bool hanging = s_have_hero && hanging_on();
    if (!hanging) {
        last_gun = left_takes(in_hand) ? in_hand : 0;
    }
    const bool left_hangs = hanging && s_ledge.grabbed[kLeft] && !s_ledge.grabbed[kRight] && last_gun != 0;
    const int side = left_hangs ? kRight : kLeft;
    const int32_t item = left_hangs ? last_gun : s_left_item;
    const bool wanted = (left_hangs || s_vr.akimbo) && item != 0 && s_have_hero && s_steers && s_have_frame && s_frame.hand_valid[side]
                        && hero_on_foot() && hero_mode() != kModeBoard && phase_now() == OPENRAC_PLAY_PLAYING
                        && ref<uint8_t>(s_disc->owned + static_cast<uint32_t>(item)) != 0;
    if (!wanted) {
        if (s_left_moby != 0) {
            api.second_item_drop();
            s_left_moby = 0;
        }
        return;
    }
    const bool changed = item != s_off_item || side != s_off_side;
    s_off_item = item;
    s_off_side = side;
    const uint32_t def = kItemDefSize * static_cast<uint32_t>(item);
    const int32_t oclass = ref<int32_t>(level0(s_disc->item_defs) + def + kItemDefClass);
    const gaddr moby = api.second_item(oclass);
    if (moby != s_left_moby || changed) {
        char line[100];
        std::snprintf(line, sizeof line, "[vr-akimbo] the %s gun: item %d, class %d, moby 0x%X", side == kLeft ? "left" : "right", item, oclass, moby);
        log_line(1, line);
        std::memset(s_left_slot, 0, sizeof s_left_slot);
        put_word_at(s_left_slot, 0, moby);
        put_word_at(s_left_slot, kSlotButton, kCircleButton);
        put_word_at(s_left_slot, kSlotState, kSlotReady);
        put_word_at(s_left_slot, kSlotItem, item);
    }
    s_left_moby = moby;
    if (moby == 0) {
        return;
    }
    const gaddr address = ref<gaddr>(moby + kMobyUpdateFn);
    const auto update = reinterpret_cast<void (*)(gaddr)>(address != 0 ? api.function_at(address) : nullptr);
    if (update != nullptr) {
        const uint32_t ammo = s_disc->ammo + 4u * static_cast<uint32_t>(item);
        const int32_t rounds = ref<int32_t>(ammo);
        run_left_gun(update);
        if (ref<int32_t>(ammo) != rounds) {
            log_line(1, ("[vr-akimbo] the " + std::string(side == kLeft ? "left" : "right") + " gun fired: " + std::to_string(ref<int32_t>(ammo)) + " rounds left").c_str());
        }
        // The Taunter and the Suck Cannon spend no rounds as they work: their own states say it (Rerac-vr's
        // taunter.rs: the whistle is sequence 3; suck_cannon.rs: state 2 starting, 3 sucking, 5 firing).
        static int working_was = 0;
        int working = 0;
        if (item == 14) {
            working = ref<uint8_t>(moby + 0x52u) == 3 ? 3 : 0;
        } else if (item == 9) {
            const uint8_t state = ref<uint8_t>(moby + kMobyState);
            working = state == 2 || state == 3 || state == 5 ? state : 0;
        }
        if (working != working_was) {
            working_was = working;
            char line[120];
            std::snprintf(line, sizeof line, "[vr-akimbo] the %s %s: %s", side == kLeft ? "left" : "right",
                          item == 14 ? "Taunter" : "Suck Cannon",
                          working == 0 ? "stopped" : item == 14 ? "whistles" : working == 2 ? "starts" : working == 3 ? "sucks" : "fires");
            if (item == 14 || item == 9) {
                log_line(1, line);
            }
        }
    }
    if (side == kLeft && leads_hero(item) && lead_free()
        && (s_frame.controls.left_trigger > kTriggerOn || (punching() && s_punch_side == kLeft))) {
        start_lead();
    }
}

void place_left_gun() {
    // While the left hand leads, the right hand's item kept aside stays in it.
    if (s_leading) {
        const gaddr right = kept_right_moby();
        if (right != 0 && s_have_frame && s_frame.hand_valid[kRight]) {
            int32_t item = 0;
            std::memcpy(&item, s_left_slot + kSlotItem, sizeof item);
            HandFrame hand;
            const Mount mount = mount_in(right, hand, kRight, item, false);
            put_mount(right, mount, get_float(right + kMobyScale));
            put_float(right, mount.position.x * 1024.0f);
            put_float(right + 4u, mount.position.y * 1024.0f);
            put_float(right + 8u, mount.position.z * 1024.0f);
            put_float(right + 12u, kLeftGunSphere * 1024.0f);
        }
        return;
    }
    if (s_left_moby == 0 || !s_have_frame || !s_frame.hand_valid[s_off_side]) {
        return;
    }
    s_left_active = true;
    HandFrame hand;
    const Mount mount = hand_mount(s_left_moby, hand);
    put_mount(s_left_moby, mount, get_float(s_left_moby + kMobyScale));
    s_left_active = false;
    centre_held_decoy(s_left_moby, hand.at, s_off_side, s_off_item);
    // The bounding sphere the draw culls by: the hero's update keeps the hand item's, nothing this one's.
#if !defined(__ANDROID__)
    static const bool trace = std::getenv("QUEST_AKIMBO_TRACE") != nullptr;
    static uint64_t logged = 0;
    if (trace && s_tick >= logged) {
        logged = s_tick + 300;
        const gaddr right = hand_moby();
        if (right != 0) {
            std::string diff = "[vr-akimbo] left/right differ:";
            for (uint32_t at = 0; at < 0x100u; at += 4u) {
                const uint32_t a = ref<uint32_t>(s_left_moby + at), b = ref<uint32_t>(right + at);
                if (a != b && (at < 0xC0u || at >= 0xF0u) && at != 0x10u && at != 0x14u && at != 0x18u && at != 0x0u && at != 0x4u && at != 0x8u) {
                    char word[40];
                    std::snprintf(word, sizeof word, " +%02X %X/%X", at, a, b);
                    diff += word;
                }
            }
            log_line(1, diff.c_str());
        }
        for (const gaddr m : {s_left_moby, hand_moby()}) {
            if (m == 0) {
                continue;
            }
            char line[170];
            std::snprintf(line, sizeof line, "[vr-akimbo] %s class %d: frame %d/%d seq %d/%d t %.2f speed %.2f state %d flags 0x%X scale %.3f",
                          m == s_left_moby ? "left" : "right", ref<int16_t>(m + kMobyOclass), ref<uint8_t>(m + 0x50u), ref<uint8_t>(m + 0x51u),
                          ref<uint8_t>(m + 0x52u), ref<uint8_t>(m + 0x53u), get_float(m + 0x54u), get_float(m + 0x58u), ref<uint8_t>(m + kMobyState),
                          ref<uint16_t>(m + kMobyMode), get_float(m + kMobyScale));
            log_line(1, line);
        }
    }
#endif
    put_float(s_left_moby, mount.position.x * 1024.0f);
    put_float(s_left_moby + 4u, mount.position.y * 1024.0f);
    put_float(s_left_moby + 8u, mount.position.z * 1024.0f);
    put_float(s_left_moby + 12u, kLeftGunSphere * 1024.0f);
}

// ---- The frame ----

Vec3 world_from_head() { return frame_to_world(in_frame(s_rig, s_frame.head, anchor_now(1.0f)).position); }

// The eyes' anchor, where the head at the last recentre stands in the frame: above the hero as the
// scene draws him, part way between his last two ticks.
Vec3 anchor_now(float alpha) {
    if (!s_have_hero) {
        return world_to_frame(s_hero_after);
    }
    if (s_manned != 0 && s_seat_seen) {
        return world_to_frame(s_seat_before + (s_seat_after - s_seat_before) * alpha);
    }
    if (s_eye_parked) {
        return world_to_frame(s_eye_parked_at + Vec3{0, 0, s_eye_height * s_options.world_scale});
    }
    const Vec3 hero = s_hero_before + (s_hero_after - s_hero_before) * alpha;
    return world_to_frame(hero + Vec3{0, 0, s_eye_height * s_options.world_scale});
}

Rig first_person_rig(float alpha);

// How far toward the panel the level leaves room (Rerac-vr's spatial menu clearance): lines from the eye to its centre,
// corners and bottom edge against the level's collision; the nearest hit, less a tenth, as a share of the way. A
// fifth at least; 1 with nothing in the way.
float panel_clearance(const Pose& head, const Pose& panel) {
    constexpr float kLeast = 0.2f, kMargin = 0.9f;
    const Rig rig = first_person_rig(s_view_alpha);
    const Vec3 anchor = anchor_now(s_view_alpha);
    const auto world = [&](Vec3 room) { return frame_to_world(in_frame(rig, Pose{room, {}}, anchor).position); };
    const Vec3 eye = world(head.position);
    const float half_wide = kPanelWidth * 0.5f, half_high = kPanelWidth * 0.375f;
    const Vec3 points[] = {{0, 0, 0},   {-half_wide, half_high, 0}, {half_wide, half_high, 0}, {-half_wide, -half_high, 0},
                           {half_wide, -half_high, 0}, {0, -half_high, 0}};
    float share = 1.0f;
    for (const Vec3& local : points) {
        const Vec3 to = world(panel.position + rotate(panel.orientation, local));
        const float whole = length(to - eye);
        Vec3 hit;
        if (whole > 1e-4f && line_blocked(eye, to, hit)) {
            share = std::min(share, std::clamp(length(hit - eye) / whole * kMargin, kLeast, 1.0f));
        }
    }
    return share;
}

void place_panel(const Pose& head) {
    const float yaw = tracking_yaw(head.orientation);
    s_panel_pose.orientation = yaw_quat(yaw);
    s_panel_pose.position =
        head.position + rotate(yaw_quat(yaw), {0, 0, -kPanelDistance}) + Vec3{0, 0.0f, 0};
    s_panel_scale = 1.0f;
    if (s_world_menu) {
        s_panel_scale = panel_clearance(head, s_panel_pose);
#if !defined(__ANDROID__)
        // PC test aid (QUEST_PANEL_FAR=1): its whole distance whatever stands there, for the sims' fixed aims.
        const char* far = std::getenv("QUEST_PANEL_FAR");
        s_panel_scale = far != nullptr && far[0] == '1' ? 1.0f : s_panel_scale;
#endif
        s_panel_pose.position = head.position + rotate(yaw_quat(yaw), {0, 0, -kPanelDistance * s_panel_scale});
        if (s_panel_scale < 1.0f) {
            char line[96];
            std::snprintf(line, sizeof line, "[vr-menu] a wall before the menu: it comes to %.0f%% of its distance", 100.0f * s_panel_scale);
            log_line(1, line);
        }
    }
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

// On a rail the view follows it gradually, averaging out its small turns, and never fast (Rerac-vr's
// vr/game.rs): only the slow changes of the path turn the player, and the gaze of the moment stays.
constexpr float kRailFollowSeconds = 0.48f;
constexpr float kRailTurnMost = 0.85f;

void follow_rail(const Pose& head, bool steering, float seconds) {
    static bool riding = false;
    static float heading = 0;
    if (!steering || hero_mode() != kModeRail) {
        riding = false;
        return;
    }
    const float yaw = get_float(s_disc->hero + kHeroFacing);
    if (!riding) {
        riding = true;
        heading = yaw;
        return;
    }
    const float error = wrap_angle(yaw - heading);
    const float most = seconds * kRailTurnMost;
    const float change = std::clamp(error * (1.0f - std::exp(-seconds / kRailFollowSeconds)), -most, most);
    s_rig.turn(change, head.position);
    heading = wrap_angle(heading + change);
}

// One eye's view, world pose or relative to the game's camera.
// ---- Heard from the head ----
//
// Rerac-vr's (orientation.rs, "Headset heading, spatial sound"): the game mixes its voices each tick from
// its camera, which stays behind Ratchet for its own uses. In a headset they are heard from the eyes, in
// play and through an actor's eyes in a scene: the camera's eye and rows are the head's for that step.

struct Heard {
    bool valid = false;
    HandFrame head;
};
Heard s_heard;

template <typename Original>
int32_t sound_update(Original original) {
    if (!s_heard.valid || s_mod->loaded_level() < 0) {
        return original();
    }
    const gaddr camera = camera_block();
    float eye[3], rows[12];
    std::memcpy(eye, s_mod->memory().pointer(camera + kCameraEye), sizeof eye);
    std::memcpy(rows, s_mod->memory().pointer(camera + kCameraRows), sizeof rows);
    const auto put = [](gaddr at, Vec3 v) {
        put_float(at, v.x);
        put_float(at + 4u, v.y);
        put_float(at + 8u, v.z);
    };
    put(camera + kCameraEye, s_heard.head.at);
    put(camera + kCameraRows, s_heard.head.forward);
    put(camera + kCameraRows + 16u, s_heard.head.left);
    put(camera + kCameraRows + 32u, s_heard.head.up);
    const int32_t result = original();
    std::memcpy(s_mod->memory().pointer(camera + kCameraEye), eye, sizeof eye);
    std::memcpy(s_mod->memory().pointer(camera + kCameraRows), rows, sizeof rows);
    static uint64_t logged = 0;
    if (s_tick >= logged) {
        logged = s_tick + 600;
        char line[160];
        std::snprintf(line, sizeof line, "[vr-sound] heard from (%.2f %.2f %.2f) along (%.2f %.2f %.2f), the camera at (%.2f %.2f %.2f)",
                      s_heard.head.at.x, s_heard.head.at.y, s_heard.head.at.z, s_heard.head.forward.x, s_heard.head.forward.y,
                      s_heard.head.forward.z, eye[0], eye[1], eye[2]);
        log_line(1, line);
    }
    return result;
}

// The rig the eyes see through in first person: the neck between its ticks. What stands in the world by the room goes
// by this one too, or it slides against the eyes.
Rig first_person_rig(float alpha) {
    Rig rig = s_rig;
    if (s_neck_seen) {
        rig.origin = s_origin_before + (s_origin_after - s_origin_before) * alpha;
    }
    return rig;
}

void fill_eye(openrac_view& view, int eye, const FrameInfo& frame, bool first_person, float alpha) {
    int width = 0, height = 0;
    s_headset->target_size(static_cast<Target>(kTargetLeftEye + eye), width, height);
    view.kind = OPENRAC_VIEW_WORLD;
    view.width = width;
    view.height = height;
    Rig rig = first_person ? first_person_rig(alpha) : s_rig;
    const bool actor = s_scene_eyes && !first_person;
    if (actor) {
        rig.origin = s_scene_origin;
    }
    Pose pose = first_person ? in_frame(rig, frame.eye[eye], anchor_now(alpha))
                : actor      ? rig.to_frame(frame.eye[eye], world_to_frame(s_scene_before + (s_scene_after - s_scene_before) * alpha))
                             : rig.to_camera(frame.eye[eye], s_recentre_yaw);
    // Third person keeps a level horizon (rerac-vr-3p): the game's camera gives the view's place and heading, the head its
    // pitch and roll; a follow camera's own tilt would tip the whole world in a headset.
    if (!first_person && !actor && s_have_hero && !s_options.screen && phase_now() == OPENRAC_PLAY_PLAYING) {
        const Vec3 along = read_vec(camera_block() + kCameraRows);
        const float size = length(along);
        if (size > 0.001f) {
            const float half = -std::asin(std::clamp(along.z / size, -1.0f, 1.0f)) * 0.5f;
            const Quat level{std::sin(half), 0.0f, 0.0f, std::cos(half)};
            pose.position = rotate(level, pose.position);
            pose.orientation = level * pose.orientation;
        }
    }
    if (first_person && s_flip_angle != 0.0f && !in_ship()) {
        const Vec3 at = anchor_now(alpha);
        const float half = s_flip_angle * 0.5f;
        const Quat turn{s_flip_axis.x * std::sin(half), s_flip_axis.y * std::sin(half), s_flip_axis.z * std::sin(half), std::cos(half)};
        pose.position = at + rotate(turn, pose.position - at);
        pose.orientation = turn * pose.orientation;
    }
    if (first_person && in_ship()) {
        static Quat view_turn;
        if (eye == 0) {
            view_turn = ship_turn(static_cast<float>(frame.frame_seconds)) * yaw_quat(-s_recentre_yaw);
        }
        const Vec3 at = s_ship_eye_before + (s_ship_eye_after - s_ship_eye_before) * alpha;
        pose.position = world_to_frame(at) + rotate(view_turn, (frame.eye[eye].position - frame.head.position) * s_ship_ratio);
        pose.orientation = view_turn * frame.eye[eye].orientation;
    }
    s_heard.valid = first_person || actor;
    if (s_heard.valid) {
        const HandFrame seen = hand_in_world(pose);
        if (eye == 0) {
            s_heard.head = seen;
        } else {
            s_heard.head.at = (s_heard.head.at + seen.at) * 0.5f;
        }
    }
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
    view.reserved[0] = first_person || actor ? kViewFlagWorldPose : 0;
    // A Visibomb in flight takes the game's camera far off: the port keeps what is near the eyes, the weapon and
    // the hands among it, and the sky the game stops drawing (Rerac-vr keeps both).
    if ((view.reserved[0] & kViewFlagWorldPose) != 0 && (missile_flying() || s_shopping)) {
        view.reserved[0] |= kViewFlagNearEye;
    }
    // The title's menu frames are mobys with the menu's camera: they stand in the window's picture, not about the eyes.
    if (s_title_world) {
        view.reserved[0] |= kViewFlagNoOwnCameras;
    }
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

// The game's sprites and glows face its camera, which the player's eyes are not (Rerac-vr's: glow billboards face the
// headset). A PC test aid, QUEST_SPRITES_OFF=1, leaves them to the game's camera, to compare.
void face_sprites() {
    const openrac_api& api = s_mod->api();
    bool mine = s_have_frame && (s_steers || s_world_menu || s_scene_eyes);
#if !defined(__ANDROID__)
    static const bool off = std::getenv("QUEST_SPRITES_OFF") != nullptr;
    mine = mine && !off;
#endif
    if (!mine) {
        api.sprites_face(nullptr, nullptr, nullptr, nullptr);
        return;
    }
    const Quat turn = yaw_quat(s_rig.yaw) * s_frame.head.orientation;
    const Vec3 left = frame_to_world(rotate(turn, {-1, 0, 0})), up = frame_to_world(rotate(turn, {0, 1, 0}));
    const Vec3 eye = world_from_head(), camera = read_vec(camera_block() + kCameraEye);
    const float l[3] = {left.x, left.y, left.z}, u[3] = {up.x, up.y, up.z};
    const float e[3] = {eye.x, eye.y, eye.z}, c[3] = {camera.x, camera.y, camera.z};
    api.sprites_face(l, u, e, c);
}

// Y in a scene: the game's own skip, pressed for its next ticks, as a scene takes none in its first (Rerac-vr's
// SkipInput). A cutscene's skip is pressed for the port to pulse, so Y stays out of the pad, and does nothing else until
// let go: play comes back under it held. A movie, or a ship's scene, is the game's own Start.
void skip_scene(bool y, int phase) {
    static bool y_was = false;
    const openrac_api& api = s_mod->api();
    const bool scene = !s_options.screen && phase == OPENRAC_PLAY_SCENE;
    if (scene && y && !y_was) {
        api.play_skip(s_mod->handle());
        log_line(1, "[vr-skip] Y in a scene");
    }
    int frame = 0;
    s_skip_held = y && (s_skip_held || (scene && api.scene_playing(&frame) >= 0));
    y_was = y;
}

// What the head and hands do to the rig, once a frame.
void steer(const FrameInfo& frame, int phase) {
    const Pose& head = frame.head;
    const Controls& c = frame.controls;

    // The left menu button, the headset's own recentre, the settle of a new phase's first frame, or the session's first.
    static bool menu_was = false;
    const bool menu = c.menu && !s_options.screen;
    if ((menu && !menu_was) || frame.recentred || s_recentre_wanted) {
        log_line(1, menu && !menu_was ? "[vr-rig] recentred: the left menu button"
                    : frame.recentred ? "[vr-rig] recentred: the headset's own recentre"
                                      : "[vr-rig] recentred: a new phase or the session's start");
        s_board_heading_kept = s_board_heading_kept && !(menu && !menu_was) && !frame.recentred;
        // A turret's seat starts looking along its barrels, where the head looks then.
        s_rig.recentre(head, s_manned != 0 ? turret_yaw_of(s_manned) : s_have_hero ? s_facing : 0.0f);
        s_rig.origin.y -= s_vr.seated ? kSeatedLift : 0.0f;
        s_neck_seen = false;
        s_recentre_yaw = tracking_yaw(head.orientation);
        s_recentre_wanted = false;
        s_panel_placed = false;
    }
    menu_was = menu;

    // In a turret's seat the head is the player's and the hands are the turret's.
    const bool first_person = first_person_now(phase) && s_manned == 0 && !in_ship();
    skip_scene(c.y, phase);
    // The turn is the player's in first person; in third the right stick is the game's camera.
    const float angle = s_turn.update(c.right_x, static_cast<float>(frame.frame_seconds), s_options.turn,
                                      !first_person || s_quick_select || in_puzzle() || s_shopping);  // that hand is picking, or the rings have the stick
    if (first_person) {
        if (angle != 0.0f) {
            s_rig.turn(angle, head.position);
        }

        // Without the room's steps, walking round it leaves the view ahead of Ratchet's body; moving
        // him closes it.
        if (!s_options.room_scale && std::abs(c.left_x) + std::abs(c.left_y) > 0.2f) {
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
    follow_head(head, first_person);
    update_eye_height(static_cast<float>(frame.frame_seconds));
    follow_rail(head, first_person, static_cast<float>(frame.frame_seconds));
    follow_board(head, first_person, static_cast<float>(frame.frame_seconds));
    follow_surface();
    update_flip(static_cast<float>(frame.frame_seconds));
    // Where he faces and the stick's forward: the body's heading, or the gaze. The weapons aim by the
    // hand whatever he faces (aimed_update).
    static BodyHeading body;
    if (first_person && s_vr.direction_body) {
        const Pose* left = frame.hand_valid[kLeft] ? &frame.hand_grip[kLeft] : nullptr;
        const Pose* right = frame.hand_valid[kRight] ? &frame.hand_grip[kRight] : nullptr;
        const float heading = body.update(head, left, right, static_cast<float>(frame.frame_seconds));
        s_head_yaw = wrap_angle(s_rig.yaw + heading + kPi * 0.5f);
    } else {
        body.reset();
        s_head_yaw = world_yaw(s_rig.yaw, head.orientation);
    }
    s_aim_yaw = s_head_yaw;

    // How fast the hand goes makes a swing of the wrench.
    float speed = 0;
    if (frame.hand_valid[kRight]) {
        const Vec3 at = frame.hand_grip[kRight].position;
        if (s_hand_seen && frame.frame_seconds > 0.0) {
            speed = length(at - s_hand_before) / static_cast<float>(frame.frame_seconds);
        }
        s_hand_before = at;
        s_hand_seen = true;
    } else {
        s_hand_seen = false;
    }
    s_swinging = s_swing.update(speed, first_person && !s_options.swing_wrench && c.right_grip > kTriggerOn,
                                static_cast<float>(frame.frame_seconds));

    // The quick select is opened by the right stick's click; where the hand then goes picks.
    const bool picking = first_person && c.right_click && frame.hand_valid[kRight] && !s_shopping;
    left_ring_frame(frame, head, first_person);
    if (picking && !s_quick_select) {
        s_pick_from[kRight] = frame.hand_grip[kRight].position;
        s_pick_head[kRight] = head.orientation;
        s_pick_selecting[kRight] = false;
        s_page_trigger = c.right_trigger > kTriggerOn;  // held as it opened: that is not a page
    }
    if (picking) {
        const bool trigger = c.right_trigger > kTriggerOn;
        s_page_wanted = s_page_wanted || (trigger && !s_page_trigger);
        s_page_trigger = trigger;
    }
    s_quick_select = picking;
    s_menu_input = phase == OPENRAC_PLAY_FRONT_END || phase == OPENRAC_PLAY_MENU
                   || phase == OPENRAC_PLAY_NONE || phase == OPENRAC_PLAY_LOADING;
}

// What the controllers send, whenever it changes, and a word when a hand is missing: the log and the
// screen say at once what used to take a guess from the game's behaviour.
void trace_input(const FrameInfo& frame) {
    const Controls& c = frame.controls;
    const unsigned buttons = (c.a ? 1u : 0u) | (c.b ? 2u : 0u) | (c.x ? 4u : 0u) | (c.y ? 8u : 0u)
                             | (c.left_click ? 16u : 0u) | (c.right_click ? 32u : 0u) | (c.menu ? 64u : 0u)
                             | (c.right_trigger > 0.5f ? 128u : 0u) | (c.left_trigger > 0.5f ? 256u : 0u)
                             | (c.right_grip > 0.5f ? 512u : 0u) | (c.left_grip > 0.5f ? 1024u : 0u)
                             | (std::abs(c.left_x) + std::abs(c.left_y) > 0.5f ? 2048u : 0u)
                             | (std::abs(c.right_x) + std::abs(c.right_y) > 0.5f ? 4096u : 0u);
    static unsigned shown = 0;
    if (buttons != shown) {
        shown = buttons;
        char line[200];
        std::snprintf(line, sizeof line,
                      "[vr-input] A%d B%d X%d Y%d L3%d R3%d RT%.2f LT%.2f RG%.2f LG%.2f left %.2f %.2f right %.2f %.2f "
                      "sticks %d%d bound %d%d",
                      c.a ? 1 : 0, c.b ? 1 : 0, c.x ? 1 : 0, c.y ? 1 : 0, c.left_click ? 1 : 0, c.right_click ? 1 : 0,
                      c.right_trigger, c.left_trigger, c.right_grip, c.left_grip, c.left_x, c.left_y, c.right_x,
                      c.right_y, frame.stick_active[0] ? 1 : 0, frame.stick_active[1] ? 1 : 0,
                      frame.hand_bound[0] ? 1 : 0, frame.hand_bound[1] ? 1 : 0);
        log_line(1, line);
    }

    // Three seconds with a controller gone, in focus: said on the game's picture, until it is back.
    static int missing = 0;
    static const char* said = nullptr;
    const bool left = frame.stick_active[0] || frame.hand_valid[0];
    const bool right = frame.stick_active[1] || frame.hand_valid[1];
    missing = frame.focused && !(left && right) ? missing + 1 : 0;
    const char* text = nullptr;
    if (missing > 216) {
        text = !left && !right ? "CONTROLLERS NOT FOUND  CHECK THEIR BATTERIES"
               : !left         ? "LEFT CONTROLLER NOT FOUND  CHECK ITS BATTERY"
                               : "RIGHT CONTROLLER NOT FOUND  CHECK ITS BATTERY";
    }
    if (text != said) {
        said = text;
        s_headset->show_notice(text);
        log_line(2, text != nullptr ? text : "the controllers are back");
    }
}

// Rerac-vr's VR_PERF, what a headset log needs: each five seconds, how long the frames took on this
// thread from one to the next, on average and at worst, and how many were late for the refresh.
void trace_timing(const FrameInfo& frame) {
    static double since = 0, worst = 0, total = 0, last = 0;
    static int frames = 0, late = 0;
    // Measured: frame.frame_seconds is the display's period, the same however the game is doing.
    const double now = now_seconds();
    const double seconds = last > 0.0 ? now - last : 0.0;
    last = now;
    if (seconds <= 0.0) {
        return;
    }
    total += seconds;
    worst = std::max(worst, seconds);
    ++frames;
    if (seconds > frame.frame_seconds * 1.5) {
        ++late;
    }
    since += seconds;
    if (since >= 5.0) {
        char line[120];
        std::snprintf(line, sizeof line, "[vr-perf] %d frames: %.1f ms on average, %.1f ms at worst, %d late", frames,
                      total * 1000.0 / frames, worst * 1000.0, late);
        log_line(1, line);
        since = worst = total = 0;
        frames = late = 0;
    }
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

void menu_pointer();

int views_begin(void*, openrac_view* views, int max) {
    const bool picture = views[0].reserved[kViewHintPicture] != 0;
    float alpha = 1.0f;
    std::memcpy(&alpha, &views[0].reserved[kViewHintAlpha], sizeof alpha);
    alpha = std::clamp(alpha, 0.0f, 1.0f);
    s_view_alpha = alpha;
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
        if ((phase == OPENRAC_PLAY_PLAYING || phase == OPENRAC_PLAY_SCENE || phase == OPENRAC_PLAY_FRONT_END)
            && s_last_phase != OPENRAC_PLAY_MENU) {
            s_recentre_wanted = true;
        }
        s_last_phase = phase;
        s_panel_placed = false;
    }
    if (s_frame.views_valid) {
        steer(s_frame, phase);
    }
    support_step();
    throw_gesture();
    wrench_gesture();
    punch_gesture();
    disguise_gesture();
    menu_pointer();
    ledge_gesture();
    trace_input(s_frame);
    trace_timing(s_frame);
    static int frames = 0;
    if (changed || ++frames >= 360) {
        frames = 0;
        trace_state(phase, s_frame, changed ? "phase" : "tick");
    }
    if (!s_frame.should_render || !s_frame.views_valid) {
        return 0;
    }

    int count = 0;
    s_world_menu = !picture && world_menu_now(phase);
    // The title stands around the player (Rerac-vr's title_world): its space scene from the game's camera in the eyes,
    // its menus at their place in the room, not a picture of both on a panel.
    s_title_world = !picture && phase == OPENRAC_PLAY_FRONT_END && s_options.menu_in_world && !s_options.screen;
    const bool world_phase = !picture && !s_options.screen
                             && (phase == OPENRAC_PLAY_PLAYING || s_world_menu || s_title_world
                                 || (phase == OPENRAC_PLAY_SCENE && s_options.scenes_in_vr));
    if (world_phase) {
        const bool first_person = first_person_now(phase) || s_world_menu;
        fill_eye(views[count++], 0, s_frame, first_person, alpha);
        fill_eye(views[count++], 1, s_frame, first_person, alpha);
        s_eyes_shown = true;
        // With a missile flying the panel is the weapon's monitor, with the game's own picture.
        s_panel_is_monitor = missile_flying() && !s_world_menu;
        fill_panel(views[count++], s_panel_is_monitor || s_title_world ? kViewKindWindow : OPENRAC_VIEW_2D);
        s_panel_shown = true;
        s_panel_is_hud = !s_panel_is_monitor && !s_world_menu && !s_title_world;
        // The title's menus are drawn with mobys the 2D has not, so the window's picture gives them, and the 2D its
        // logo and prompts; the two share the panel's target, which is not shown.
        if (s_title_world) {
            views[count] = views[count - 1];
            views[count++].kind = OPENRAC_VIEW_2D;
        }
        if ((s_world_menu || s_title_world) && !s_panel_placed) {
            place_panel(s_frame.head);
        }
    } else {
        s_panel_is_monitor = false;
        s_heard.valid = false;
        if (!s_panel_placed) {
            place_panel(s_frame.head);
        }
        fill_panel(views[count++], kViewKindWindow);
        s_panel_shown = true;
        s_panel_is_hud = false;
    }
    return count;
}

// The Trespasser's rings hang over the plugged tool, as Rerac-vr's spatial_menu.rs anchor: once a socket, 55%
// along from the hand to the prong tips and 0.42 above them, at least 0.6 from the eyes and facing them, the
// lock's upright plate three quarters of the way to the tips. Its numbers are game units, a metre at the
// default world size, so scaled with it here by the rig. The rings are the game's picture, so the same panel.
// The game brings him to the pad over the hold's first ticks, so it waits for him to stop ("at the final socket,
// not at the first reach key"), on the face till then. The hand is the tool's, wherever the real one has gone.
bool puzzle_panel(PanelLayer& layer) {
    constexpr float kAlong = 0.55f, kRise = 0.42f, kNearest = 0.6f, kPlateAt = 0.75f;
    constexpr int kSettledFrames = 20;
    static gaddr anchored = 0;
    static Vec3 centre, facing, hero_was;
    static int settled = 0;
#if !defined(__ANDROID__)
    // PC test aid (QUEST_SOCKET_TRACE=1): what keeps the panel from its place, about once a second.
    static uint64_t traced = 0;
    if (std::getenv("QUEST_SOCKET_TRACE") != nullptr && s_have_hero && s_tick >= traced
        && ref<int32_t>(s_disc->hero + kHeroState) == 0x72) {
        traced = s_tick + 50;
        const Vec3 hero = read_vec(s_disc->hero + kHeroPosition);
        char line[200];
        std::snprintf(line, sizeof line, "[vr-socket] panel gate: option %d puzzle %d lock 0x%X hand moby 0x%X item %d settled %d hero (%.3f %.3f %.3f)",
                      s_options.puzzle_over_tool ? 1 : 0, in_puzzle() ? 1 : 0, s_puzzle_lock, hand_moby(),
                      ref<int32_t>(s_disc->hero + kHeroSlot0 + kSlotItem), settled, hero.x, hero.y, hero.z);
        log_line(1, line);
    }
#endif
    if (!s_options.puzzle_over_tool || !in_puzzle() || s_puzzle_lock == 0 || !s_have_frame || hand_moby() == 0) {
        anchored = 0;
        settled = 0;
        return false;
    }
    if (anchored != s_puzzle_lock) {
        const Vec3 hero = read_vec(s_disc->hero + kHeroPosition);
        settled = length(hero - hero_was) < 0.001f ? settled + 1 : 0;
        hero_was = hero;
        if (settled < kSettledFrames) {
            return false;
        }
        anchored = s_puzzle_lock;
        const Vec3 hand = read_vec(hand_moby() + kMobyPosition);
        const Vec3 tip = hand + (moby_centre(s_puzzle_lock) - hand) * (1.0f / kPlateAt);
        const Vec3 eye = world_from_head();
        centre = hand + (tip - hand) * kAlong;
        centre.z = tip.z + kRise * s_rig.scale;
        facing = centre - eye;
        if (length(facing) < kNearest * s_rig.scale) {
            centre = eye + facing * (kNearest * s_rig.scale / std::max(length(facing), 0.001f));
            facing = centre - eye;
        }
        char line[140];
        std::snprintf(line, sizeof line, "[vr-socket] the rings' panel at (%.2f %.2f %.2f), %.2f from the eyes, the plate at (%.2f %.2f %.2f)", centre.x, centre.y,
                      centre.z, length(facing), moby_centre(s_puzzle_lock).x, moby_centre(s_puzzle_lock).y, moby_centre(s_puzzle_lock).z);
        log_line(1, line);
    }
    const Vec3 at = s_rig.from_frame(world_to_frame(centre), anchor_now(1.0f));
    const float distance = length(at - s_frame.head.position);
    layer.follows_head = false;
    layer.pose.position = at;
    layer.pose.orientation = looking_along(rotate(yaw_quat(-s_rig.yaw), world_to_frame(facing * (1.0f / length(facing)))));
    layer.width = distance * 1.05f * s_options.hud_scale;
    layer.height = layer.width * 3.0f / 4.0f;
    return true;
}

void views_end(void*, const openrac_view* views, int count) {
    if (!s_frame_open) {
        return;
    }
    s_frame_open = false;

    // Nothing drawn at this view's size yet means nothing to show.
    bool eyes = s_eyes_shown;
    bool panel = s_panel_shown;
    bool eyes_kept = false;
    for (int i = 0; i < count; ++i) {
        if (views[i].picture == 0 && views[i].drawn == 0) {
            (views[i].kind == OPENRAC_VIEW_WORLD ? eyes : panel) = false;
        }
        eyes_kept = eyes_kept || (views[i].kind == OPENRAC_VIEW_WORLD && views[i].drawn == 0);
    }

    PanelLayer layer;
    layer.visible = panel;
    if (s_panel_is_hud) {
        layer.follows_head = true;
        layer.blend = true;
        layer.pose.position = {0, 0, -kHudDistance};
        layer.width = kHudDistance * 1.05f * s_options.hud_scale;
        layer.height = layer.width * 3.0f / 4.0f;
        // In a seat the HUD hangs along the aim, not the gaze: its reticle, at the picture's centre,
        // marks where the barrels point.
        Vec3 aim = rotate(yaw_quat(-s_rig.yaw), world_to_frame(read_vec(camera_block() + kCameraRows)));
        if (in_ship()) {
            aim = rotate(yaw_quat(s_recentre_yaw) * conjugate(s_ship_view), world_to_frame(read_vec(s_ship + kMobyBasis)));
        }
        if ((s_manned != 0 || in_ship()) && first_person_now(phase_now()) && length(aim) > 0.5f) {
            aim = aim * (1.0f / length(aim));
            layer.follows_head = false;
            layer.pose.position = s_frame.head.position + aim * kHudDistance;
            layer.pose.orientation = looking_along(aim);
        }
        puzzle_panel(layer);
        // At the vendor the 2D is shown by pieces, in the world: on its monitors, and the counters at the head.
#if !defined(__ANDROID__)
        // PC test aid (QUEST_SHOP_PANEL=1): the whole of it, for the test's shots.
        static const bool whole = std::getenv("QUEST_SHOP_PANEL") != nullptr;
        layer.visible = layer.visible && (!s_shopping || whole);
#else
        layer.visible = layer.visible && !s_shopping;
#endif
    } else if (s_panel_is_monitor) {
        const Pose& grip = s_frame.hand_grip[kRight];
        layer.pose.position = grip.position + rotate(grip.orientation, kMonitorAt);
        layer.pose.orientation = grip.orientation * Quat{std::sin(kMonitorTilt * 0.5f), 0, 0, std::cos(kMonitorTilt * 0.5f)};
        layer.width = kMonitorWidth;
        layer.height = kMonitorWidth * 3.0f / 4.0f;
        // It is on the weapon: no pose, no monitor; and none by the hand while the weapon's own screen shows it.
        layer.visible = layer.visible && s_frame.hand_valid[kRight] && !weapon_screen().shown;
    } else {
        layer.pose = s_panel_pose;
        layer.width = panel_width();
        layer.height = panel_width() * 3.0f / 4.0f;
#if !defined(__ANDROID__)
        // PC test aid (QUEST_SHOP_PANEL=1): the whole of it, for the test's shots.
        static const bool whole = std::getenv("QUEST_SHOP_PANEL") != nullptr;
        layer.visible = layer.visible && ((!s_world_menu && !s_title_world) || whole);
        layer.blend = layer.blend || ((s_world_menu || s_title_world) && whole);
#else
        layer.visible = layer.visible && !s_world_menu && !s_title_world;  // shown by parts, in the world
#endif
    }
    s_headset->finish_frame(eyes, eyes_kept, layer);
}

void views_closing(void*) {
    if (s_headset) {
        s_headset->stop();
    }
}

// ---- The menu pointer ----
//
// pointer.h: in a menu on the panel the right hand points, and the row of a list under its ray takes
// the game's own cursor (the page's focus given to that list on the game's next update); the trigger
// is Cross as ever, so pointing and pulling picks the row. As Rerac-vr's pause/pointer.rs: the lists
// (the ship's planet list too, whose cursor is the destination), the item grids, the option rows, the
// Sound page's rows and bars (the trigger held sets the volume where it points), the galaxy map's
// planets (as their rows of the planet list), the quit box's two lines (the trigger is their key), the
// memory card's slots and the icon lists.

constexpr float kBeamStart = 0.025f, kBeamFree = 2.0f;  // metres from the hand
constexpr float kDotLift = 0.01f;                       // the dot off the panel
bool s_pointer_shown = false;
constexpr float kCanvasWidth = 512.0f;
constexpr float kCanvasHeight = 416.0f;
constexpr uint32_t kMenuState = 0x00u, kMenuCurrent = 0x04u;
constexpr int32_t kMenuSwitching = 1;
constexpr uint32_t kPageFocus = 0x40u, kPageScreens = 0x44u, kPagePendingFocus = 0x80u;
constexpr int kPageScreenCount = 14;
constexpr uint32_t kWidgetFlags = 0x10u, kWidgetRect = 0x18u;
constexpr uint32_t kWidgetHidden = 4u;
constexpr uint32_t kListFlags = 0x30u, kListItems = 0x34u, kListCursor = 0x40u, kListScroll = 0x44u;
constexpr uint32_t kMenuItemBytes = 12u, kMenuItemEnabled = 2u, kMenuItemSecond = 8u;

struct PointedRow {
    gaddr list = 0;
    int row = -1;
    uint32_t cursor_at = 0x40u;  // where this widget keeps its cursor, 0 for none
    uint16_t key = 0;            // the key the trigger presses there, 0 for Cross
    int volume_row = -1;         // a Sound bar, and the volume at the point
    int volume = 0;
    int32_t dest = 0;            // a planet of the galaxy map (its level)
};
PointedRow s_pointed;

uint16_t pointed_key() { return s_pointed.key; }

// ---- The message boxes' key labels ----
//
// Rerac-vr's menu_pointer.rs: the labels of the game's message boxes (the memory card's, the confirmations':
// Yes, No, OK) answer their own keys, not Cross. Where a box puts them follows its text's height, so they are
// known as the game draws them (font_print_center, centred at x): each tick's are kept, a box about each.

struct DrawnLabel {
    float x = 0, y = 0;
    uint16_t key = 0;
};
constexpr int kMostLabels = 8;
constexpr float kLabelHalfWidth = 48.0f, kLabelAbove = 5.0f, kLabelBelow = 21.0f;
DrawnLabel s_labels[kMostLabels];
int s_label_count = 0;
uint64_t s_label_tick = 0;
struct LabelKey {
    int32_t message;
    uint16_t key;
};
// The US disc's ids, Rerac-vr's; those from here on sit quest_disc.label_shift higher on another disc.
constexpr int32_t kShiftedLabels = 0x5200;
constexpr LabelKey kLabelKeys[] = {{0x524A, kPadCross},    {0x524E, kPadCross},    {0x524F, kPadCircle}, {0x524B, kPadTriangle},
                                   {0x4FA8, kPadTriangle}, {0x5249, kPadCross},    {0x5248, kPadCircle}};

void record_label(int32_t x, int32_t y, uint32_t text) {
    constexpr std::size_t kKeys = sizeof kLabelKeys / sizeof kLabelKeys[0];
    static uint64_t looked_up = ~0ull;
    static uint32_t texts[kKeys] = {};
    if (text == 0) {
        return;
    }
    if (looked_up != s_tick) {
        // Each tick: a level's load or a change of language moves the texts.
        looked_up = s_tick;
        using Message = int (*)(int);
        static const auto message = reinterpret_cast<Message>(s_mod->api().function(s_disc->message_text));
        for (std::size_t i = 0; i < kKeys; ++i) {
            const int32_t id = kLabelKeys[i].message + (kLabelKeys[i].message >= kShiftedLabels ? s_disc->label_shift : 0);
            texts[i] = message != nullptr ? static_cast<uint32_t>(message(id)) : 0u;
        }
    }
#if !defined(__ANDROID__)
    // PC test aid (QUEST_POINTER_TRACE=1): each centred text once, with the labels it was compared to.
    static const bool trace = std::getenv("QUEST_POINTER_TRACE") != nullptr;
    static uint32_t traced[32] = {};
    if (trace) {
        bool seen = false;
        for (uint32_t& t : traced) {
            seen = seen || t == text;
            if (!seen && t == 0) {
                t = text;
                char word[24] = {};
                std::memcpy(word, s_mod->memory().pointer(text), sizeof word - 1);
                char line[170];
                std::snprintf(line, sizeof line, "[vr-pointer] centred text at (%d %d) 0x%X \"%s\"; labels 0x%X 0x%X 0x%X 0x%X tick %llu", x, y, text, word,
                              texts[0], texts[2], texts[3], texts[6], static_cast<unsigned long long>(s_tick));
                log_line(1, line);
                seen = true;
            }
        }
    }
#endif
    for (std::size_t i = 0; i < kKeys; ++i) {
        if (texts[i] != 0 && text == texts[i]) {
            if (s_label_tick != s_tick) {
                s_label_tick = s_tick;
                s_label_count = 0;
            }
            if (s_label_count < kMostLabels) {
                s_labels[s_label_count++] = {static_cast<float>(x), static_cast<float>(y), kLabelKeys[i].key};
            }
            return;
        }
    }
}

// The centred lines the game draws this tick: their rows and how wide the widest is (its characters, at most 64),
// what a box drawn without a frame is found by (menu_quads).
struct CentredLines {
    float top = 0, bottom = 0, half = 0;
    uint64_t tick = 0;
};
CentredLines s_centred;

void note_centred(int32_t x, int32_t y, uint32_t text) {
    constexpr float kCharWidth = 7.0f, kSideMargin = 24.0f, kLeastHalf = 104.0f;  // canvas units
    int chars = 0;
    while (chars < 64 && readable(text + static_cast<uint32_t>(chars), 1u) && ref<uint8_t>(text + static_cast<uint32_t>(chars)) != 0) {
        ++chars;
    }
    const float half = std::max(kLeastHalf, std::fabs(static_cast<float>(x) - kCanvasWidth * 0.5f) + kCharWidth * 0.5f * static_cast<float>(chars) + kSideMargin);
    const float row = static_cast<float>(y);
    if (s_centred.tick != s_tick) {
        s_centred = {row, row, half, s_tick};
        return;
    }
    s_centred.top = std::min(s_centred.top, row);
    s_centred.bottom = std::max(s_centred.bottom, row);
    s_centred.half = std::max(s_centred.half, half);
}

template <class Original>
int32_t print_center(Original original, int32_t x, int32_t y, int32_t color, int32_t text, int32_t length) {
    record_label(x, y, static_cast<uint32_t>(text));
    note_centred(x, y, static_cast<uint32_t>(text));
    return original(x, y, color, text, length);
}

// The label under (`cx`, `cy`) of a box drawn this tick or the last, as a pointed row (its key); none, row -1.
PointedRow label_at(float cx, float cy) {
    PointedRow out;
    if (s_label_tick + 2 < s_tick) {
        return out;
    }
    for (int i = 0; i < s_label_count; ++i) {
        const DrawnLabel& label = s_labels[i];
        if (std::abs(cx - label.x) < kLabelHalfWidth && cy >= label.y - kLabelAbove && cy < label.y + kLabelBelow) {
            out.row = i;
            out.cursor_at = 0;
            out.key = label.key;
            return out;
        }
    }
    return out;
}

constexpr int32_t kListPlanetCursor = 0x20;  // a list whose cursor is the ship's destination

// The widgets beyond the game's text lists (Rerac-vr's pause/pointer.rs): their update says which.
constexpr uint32_t kGridCursor = 0x3Cu, kGridRows = 0x40u, kGridCols = 0x44u, kGridCells = 0x48u;
constexpr uint32_t kGridCellBytes = 10u, kGridCellKind = 4u, kGridCellItem = 6u;
constexpr uint32_t kOptionEntries = 0x34u, kOptionCursor = 0x38u;
constexpr uint32_t kToggleBytes = 0x14u, kChoiceBytes = 0x18u;
constexpr uint32_t kSoundCursor = 0x40u;
constexpr uint32_t kSlotCursor = 0x40u;                                       // the memory card slot, 0..4
constexpr uint32_t kIconCursor = 0x3Cu, kIconCount = 0x40u, kIconFirstY = 0x5Cu;
constexpr int kSoundRows = 3, kMaxOptionRows = 32;
constexpr float kOptionRowSpan = 24.0f;

bool widget_runs(gaddr widget, const char* name) {
    const openrac_api& api = s_mod->api();
    const gaddr update = ref<gaddr>(widget);
    void* const wanted = api.function(name);
    return update != 0 && wanted != nullptr && api.function_at(update) == wanted;
}

bool widget_draws(gaddr widget, const char* name) {
    const openrac_api& api = s_mod->api();
    const gaddr draw = ref<gaddr>(widget + 4u);
    void* const wanted = api.function(name);
    return draw != 0 && wanted != nullptr && api.function_at(draw) == wanted;
}

// A level's planet on the galaxy map: visited (the game draws it lit) or unlocked (blinking).
bool planet_known(int level) {
    return ref<uint8_t>(s_disc->planets_visited + static_cast<uint32_t>(level)) != 0
           || ref<uint8_t>(s_disc->planets_unlocked + static_cast<uint32_t>(level)) != 0;
}

int option_rows(gaddr widget, uint32_t entry_bytes) {
    const gaddr entries = ref<gaddr>(widget + kOptionEntries);
    int rows = 0;
    while (rows < kMaxOptionRows && readable(entries + entry_bytes * static_cast<uint32_t>(rows), 4u)
           && ref<uint32_t>(entries + entry_bytes * static_cast<uint32_t>(rows)) != 0) {
        ++rows;
    }
    return rows;
}

bool read_list(gaddr widget, MenuList& out, int& x, int& y);
bool is_picked_list(gaddr widget);

// What of `widget` is under the point, by what kind it is; its row -1 for nothing, or not one of these.
PointedRow widget_at(gaddr widget, float x, float y, float width, float height) {
    PointedRow out;
    out.list = widget;
    if (widget_runs(widget, s_disc->menu_grid)) {
        const int rows = ref<int32_t>(widget + kGridRows), cols = ref<int32_t>(widget + kGridCols);
        const gaddr cells = ref<gaddr>(widget + kGridCells);
        if (rows <= 0 || cols <= 0 || rows * cols > 64 || !readable(cells, kGridCellBytes * static_cast<uint32_t>(rows * cols))) {
            return out;
        }
        const int cell = grid_cell_at(rows, cols, width, height, x, y);
        if (cell >= 0) {
            const gaddr at = cells + kGridCellBytes * static_cast<uint32_t>(cell);
            // A weapon not had yet shows nothing, and takes no cursor.
            const int16_t item = ref<int16_t>(at + kGridCellItem);
            if (ref<int16_t>(at + kGridCellKind) != 0 || (item >= 0 && ref<uint8_t>(s_disc->owned + static_cast<uint32_t>(item)) != 0)) {
                out.row = cell;
                out.cursor_at = kGridCursor;
            }
        }
    } else if (widget_runs(widget, s_disc->menu_toggles)) {
        out.row = spread_row_at(option_rows(widget, kToggleBytes), height, kOptionRowSpan, y);
        out.cursor_at = kOptionCursor;
    } else if (widget_runs(widget, s_disc->menu_choices)) {
        out.row = spread_row_at(option_rows(widget, kChoiceBytes), height, kOptionRowSpan, y);
        out.cursor_at = kOptionCursor;
    } else if (widget_runs(widget, s_disc->menu_sound)) {
        out.row = spread_row_at(kSoundRows, height, kOptionRowSpan, y);
        out.cursor_at = kSoundCursor;
        int row = 0, value = 0;
        if (volume_at(static_cast<int>(width), static_cast<int>(height), x, y, row, value)) {
            out.row = row;
            out.volume_row = row;
            out.volume = value;
        }
    } else if (widget_draws(widget, s_disc->menu_quit)) {
        const int line = quit_line_at(static_cast<int>(width), static_cast<int>(height), x, y);
        if (line != 0) {
            out.row = line - 1;
            out.cursor_at = 0;
            out.key = line == 1 ? kPadTriangle : kPadCircle;
        }
    } else if (widget_draws(widget, s_disc->menu_slots)) {
        out.row = save_slot_at(ref<int32_t>(s_disc->pal_mode) != 0, static_cast<int>(height), y);
        out.cursor_at = kSlotCursor;
    } else if (widget_draws(widget, s_disc->menu_icons)) {
        // A row past the list's foot scrolls the next one under the hand, each tick if let: one a step.
        constexpr uint64_t kIconStepTicks = 15;
        static int32_t first_was = 0;
        static uint64_t scrolled = 0;
        if (ref<int32_t>(widget + kIconFirstY) != first_was) {
            first_was = ref<int32_t>(widget + kIconFirstY);
            scrolled = s_tick;
        }
        if (s_tick < scrolled + kIconStepTicks) {
            return out;
        }
        out.row = icon_at(static_cast<int>(width), ref<int32_t>(widget + kIconCount), ref<int32_t>(widget + kIconFirstY), s_disc->icon_step, x, y);
        out.cursor_at = kIconCursor;
    } else if (widget_draws(widget, s_disc->menu_galaxy)) {
        // A planet, 24 pixels about its marker (the map's box's own place), visited or unlocked.
        const gaddr markers = s_mod->relocate(s_disc->planet_markers);
        for (int level = 1; level < 20 && readable(markers + 16u * static_cast<uint32_t>(level), 8u); ++level) {
            const gaddr marker = markers + 16u * static_cast<uint32_t>(level);
            const float px = static_cast<float>(ref<int32_t>(marker)), py = static_cast<float>(ref<int32_t>(marker + 4u));
            if (px != 0.0f && planet_known(level) && std::abs(x - px) < 12.0f && std::abs(y - py) < 12.0f) {
                out.dest = level;
                break;
            }
        }
    }
    return out;
}

// The ship's planet list on `page` and its row for `level`, as a pointed row; none when it has none.
PointedRow planet_list_row(gaddr page, int32_t level) {
    const gaddr order = ref<gaddr>(s_mod->relocate(s_disc->planet_order));
    for (int i = 0; i < kPageScreenCount; ++i) {
        const gaddr widget = ref<gaddr>(page + kPageScreens + 4u * static_cast<uint32_t>(i));
        MenuList list;
        int x = 0, y = 0;
        if (!readable(widget, 0x58u) || !read_list(widget, list, x, y) || (list.flags & kListPlanetCursor) == 0) {
            continue;
        }
        for (int row = 0; row < list.rows && readable(order + 4u * static_cast<uint32_t>(row), 4u); ++row) {
            if (ref<int32_t>(order + 4u * static_cast<uint32_t>(row)) == level) {
                PointedRow out;
                out.list = widget;
                out.row = row;
                out.dest = level;
                return out;
            }
        }
    }
    return {};
}

// The Sound page's volume `row` (0 the effects, 1 the music) set to `value`, and given to the mixer as the
// page's own left and right give it (pause.c's Sound update: it gives them only on its own change).
void set_volume(int row, int value) {
    const uint32_t at = s_disc->volumes[row];
    if (ref<int32_t>(at) == value) {
        return;
    }
    ref<int32_t>(at) = value;
    const int32_t sound = ref<int32_t>(s_disc->volumes[0]), music = ref<int32_t>(s_disc->volumes[1]);
    const gaddr mixer = s_disc->mixer;
    ref<int32_t>(mixer + 0x48u) = sound * 8 / 10;
    ref<int32_t>(mixer + 0x4Cu) = music;
    ref<int32_t>(mixer + 0x50u) = music;
    ref<int32_t>(mixer + 0x54u) = sound * 7 / 10;
    ref<int32_t>(mixer + 0x58u) = sound * 7 / 10;
    ref<int32_t>(mixer + 0x5Cu) = sound;
    static uint64_t logged = 0;
    if (s_tick >= logged) {
        logged = s_tick + 25;
        char line[80];
        std::snprintf(line, sizeof line, "[vr-pointer] %s volume %d", row == 0 ? "effects" : "music", value);
        log_line(1, line);
    }
}

// A list the player picks in: its update and draw the game's list's (wherever the level placed
// them; a footer has no update, a text box another draw).
bool is_picked_list(gaddr widget) {
    const openrac_api& api = s_mod->api();
    static void* const update = api.function(s_disc->list_update);
    static void* const draw = api.function(s_disc->list_draw);
    static void* const planets = api.function(s_disc->menu_planet_list);
    const gaddr own_update = ref<gaddr>(widget), own_draw = ref<gaddr>(widget + 4u);
    if (update == nullptr || draw == nullptr || own_update == 0 || own_draw == 0 || api.function_at(own_update) != update) {
        return false;
    }
    void* const drawn = api.function_at(own_draw);
    return drawn == draw || (planets != nullptr && drawn == planets);
}

// `widget` as a list, if it is one: the game's list, its box on the canvas sane, its items readable
// and ended (a 0 text) within 40.
bool read_list(gaddr widget, MenuList& out, int& x, int& y) {
    if (!is_picked_list(widget)) {
        return false;
    }
    x = ref<int32_t>(widget + kWidgetRect);
    y = ref<int32_t>(widget + kWidgetRect + 4u);
    out.width = ref<int32_t>(widget + kWidgetRect + 8u);
    out.height = ref<int32_t>(widget + kWidgetRect + 12u);
    if (out.width <= 8 || out.height <= 8 || out.width > 600 || out.height > 600 || x < -600 || x > 600 || y < -600
        || y > 600) {
        return false;
    }
    const gaddr items = ref<gaddr>(widget + kListItems);
    if (!readable(items, kMenuItemBytes * 41u)) {
        return false;
    }
    out.flags = ref<int32_t>(widget + kListFlags);
    out.scroll = ref<int32_t>(widget + kListScroll);
    out.rows = 0;
    while (out.rows < 40 && ref<int16_t>(items + kMenuItemBytes * static_cast<uint32_t>(out.rows)) != 0) {
        out.second_line[out.rows] = ref<int16_t>(items + kMenuItemBytes * static_cast<uint32_t>(out.rows) + kMenuItemSecond) != 0;
        ++out.rows;
    }
    const int cursor = ref<int32_t>(widget + kListCursor);
    return out.rows > 0 && out.rows < 40 && cursor >= -1 && cursor < out.rows && std::abs(out.scroll) < 2000;
}

// The beam from the hand to where it lands on the panel, a little before it, or two metres out; the
// dot only on the panel.
void show_beam(bool on_panel, float u, float v) {
    PointerLayers layers;
    const Pose& hand = s_frame.hand_aim[kRight];
    const Vec3 from = hand.position + rotate(hand.orientation, {0, 0, -kBeamStart});
    Vec3 to = hand.position + rotate(hand.orientation, {0, 0, -kBeamFree});
    if (on_panel) {
        const Vec3 local{(u - 0.5f) * panel_width(), (0.5f - v) * panel_width() * 0.75f, kDotLift};
        to = s_panel_pose.position + rotate(s_panel_pose.orientation, local);
        layers.dot = true;
        layers.dot_pose = {to, s_panel_pose.orientation};
    }
    layers.beam = beam_pose(from, to, s_frame.head.position, layers.beam_pose, layers.beam_length);
    s_headset->show_pointer(layers);
}

void point_at_vr_rows(gaddr page, float cx, float cy);

// The memory card's question: Yes on the left of the screen's middle and No on the right, the game's
// Circle and Triangle (Menu_ConfirmPromptInput), on the box's last line. The widget's own rectangle is
// not where the box is drawn, so these are the screen's: the box is centred, its labels at x 202 and 309.
PointedRow prompt_answer_at(gaddr widget, float cx, float cy) {
    constexpr float kLabelsLeft = 150.0f, kLabelsRight = 362.0f, kLabelsTop = 240.0f, kLabelsBottom = 290.0f;
    PointedRow out;
    if (cx < kLabelsLeft || cx >= kLabelsRight || cy < kLabelsTop || cy >= kLabelsBottom) {
        return out;
    }
    const bool yes = cx < kCanvasWidth * 0.5f;
    out.list = widget;
    out.row = yes ? 0 : 1;
    out.cursor_at = 0;
    out.key = yes ? kPadCircle : kPadTriangle;
    return out;
}

// ---- The vendor ----
//
// Rerac-vr's vendor.rs has the pointer on the screens of the machine, in the world; here the game's own
// vendor is the panel, its six screens flat where the game draws them, so these are canvas boxes. It is game
// mode 5, no page: without this the trigger is Cross wherever the hand is, and Exit would buy. On a box the
// trigger is that box's key, and off them nothing: A stays Cross, as the screen says. A pick on the strip
// walks the game's own keys there, which is what changes the ticker and the HUD's ammo with the selection.

constexpr int32_t kGameModeVendor = 5, kGameModeMenu = 3, kGameModeFreeze = 4;
constexpr int32_t kVendorMenuOpen = 1, kVendorLeaving = 2;  // its substates: the screens on, and going
constexpr uint32_t kVendorState = 0x00u, kVendorTimer = 0x04u, kVendorScroll = 0x4Cu, kVendorSelected = 0x58u, kVendorBuy = 0x5Cu;
constexpr uint32_t kVendorEntries = 0xD0u, kVendorEntryBytes = 0x14u, kVendorEntryAmmo = 4u, kVendorCount = 0x210u;

struct CanvasBox {
    float x, y, width, height;
    bool holds(float cx, float cy) const { return cx >= x && cy >= y && cx < x + width && cy < y + height; }
};
constexpr CanvasBox kVendorButtons{39.0f, 234.0f, 109.0f, 72.0f};
constexpr CanvasBox kVendorPrompt{364.0f, 234.0f, 109.0f, 72.0f};
constexpr CanvasBox kVendorPopup{202.0f, 169.0f, 108.0f, 113.0f};
constexpr float kVendorStripTop = 318.0f, kVendorStripBottom = 398.0f;

int s_vendor_wanted = -1;  // the strip's pick the keys are walking to

bool vendor_open() {
    return s_mod->loaded_level() >= 0 && ref<int32_t>(s_mod->relocate(s_disc->game_mode)) == kGameModeVendor;
}

// Once a tick: a key, every other tick, toward the strip's pick.
uint16_t vendor_pulse() {
    static bool rest = false;
    if (s_vendor_wanted < 0 || !vendor_open()) {
        s_vendor_wanted = -1;
        return 0;
    }
    rest = !rest;
    if (rest) {
        return 0;
    }
    const gaddr vendor = level0(s_disc->vendor_menu);
    const int step = vendor_strip_step(ref<int32_t>(vendor + kVendorCount), ref<int32_t>(vendor + kVendorSelected), s_vendor_wanted);
    if (step == 0 || ref<int32_t>(vendor + kVendorBuy) != 0) {
        s_vendor_wanted = -1;
        return 0;
    }
    return step > 0 ? kPadRight : kPadLeft;
}

// What the hand is on, and the trigger's pick of the strip.
void point_at_vendor(float cx, float cy) {
    const gaddr vendor = level0(s_disc->vendor_menu);
    static bool held = false;
    const bool was_held = held;
    held = s_frame.controls.right_trigger > kTriggerOn;
    PointedRow over;
    const char* name = nullptr;
    if (ref<int32_t>(vendor + kVendorState) == kVendorMenuOpen) {
        const int count = ref<int32_t>(vendor + kVendorCount), selected = ref<int32_t>(vendor + kVendorSelected);
        const bool buying = ref<int32_t>(vendor + kVendorBuy) != 0;
        if (buying && kVendorPopup.holds(cx, cy)) {
            const float across = (cx - kVendorPopup.x) / kVendorPopup.width, down = (cy - kVendorPopup.y) / kVendorPopup.height;
            const bool ammo = selected >= 0 && selected < 16
                              && ref<int32_t>(vendor + kVendorEntries + kVendorEntryBytes * static_cast<uint32_t>(selected) + kVendorEntryAmmo) == 1;
            over.key = !ammo ? (down > 0.5f ? kPadTriangle : kPadCross) : across < 0.25f ? kPadLeft : across > 0.75f ? kPadRight : kPadCross;
            name = "popup";
        } else if (kVendorButtons.holds(cx, cy)) {
            over.key = kPadCross;
            name = "Buy";
        } else if (kVendorPrompt.holds(cx, cy)) {
            over.key = kPadTriangle;
            name = "Exit";
        } else if (!buying && cy >= kVendorStripTop && cy < kVendorStripBottom) {
            const int item = vendor_strip_at(count, selected, ref<int32_t>(vendor + kVendorScroll), cx);
            if (item >= 0) {
                name = "strip";
                over.dest = item + 1;
                if (held && !was_held) {
                    s_vendor_wanted = item;
                }
            }
        }
    }
    if (name != nullptr) {
        over.list = vendor;
        over.row = 0;
    }
#if !defined(__ANDROID__)
    // PC test aid (QUEST_POINTER_TRACE=1): the game's own selection and bolts as they change.
    static int selected_was = -1, bolts_was = -1;
    if (std::getenv("QUEST_POINTER_TRACE") != nullptr
        && (ref<int32_t>(vendor + kVendorSelected) != selected_was || ref<int32_t>(s_disc->bolts) != bolts_was)) {
        selected_was = ref<int32_t>(vendor + kVendorSelected);
        bolts_was = ref<int32_t>(s_disc->bolts);
        char line[100];
        std::snprintf(line, sizeof line, "[vr-pointer] the vendor has item %d of %d selected, %d bolts", selected_was,
                      ref<int32_t>(vendor + kVendorCount), bolts_was);
        log_line(1, line);
    }
#endif
    if (over.key != s_pointed.key || over.dest != s_pointed.dest) {
        char line[120];
        std::snprintf(line, sizeof line, "[vr-pointer] the vendor's %s (canvas %.0f %.0f) key 0x%X item %d", name != nullptr ? name : "nothing", cx, cy,
                      over.key, over.dest - 1);
        log_line(1, line);
    }
    s_pointed = over;
}

// ---- Shopping on foot ----
//
// Rerac-vr's spatial vendor (vr/vendor.rs, interact_render.rs): the machine's screens are on its monitors, in the
// world, and the player keeps walking, with the shop's input apart. Here it is the game's own vendor, all of it, its
// mode included. What changes is around it. Ratchet comes out of the hold it puts him in. In its menu's state, where
// the game stops the world, what its own fly-in runs goes on, the hero's update among it. The picture it would keep
// is the level drawn again, so the eyes see the world as ever, the game's camera staying on the machine: that is what
// keeps the game's screens where they are in the 2D, and the port keeps what is near the eyes. Each screen then
// stands on its monitor, a rectangle of the 2D drawn in the eye's pass, so the world can hide it.
// Walking off, or dying, leaves as the keys do; leaving does not move him, and no weapon's scene takes the view.

constexpr uint32_t kVendorMoby = 0x1Cu;         // the machine
constexpr uint32_t kVendorRemote = 0x40u;        // the PDA's: its list and prices, and it goes without moving him
constexpr uint32_t kVendorEye = 0x60u, kVendorLook = 0x70u;  // the camera its screens are drawn for
constexpr uint32_t kVendorBoughtWeapon = 0x48u;  // the weapon whose scene plays on leaving, -1 none
constexpr int kShopScreens = 6;
constexpr int kShopPopup = kShopScreens;  // the seventh: the buy popup, on a moby of its own
constexpr uint32_t kVendorMobys = 0x20u;  // the vendor's own mobys, the popup's 0x400 in
constexpr uint32_t kPopupMoby = 0x400u;
constexpr int32_t kBuyPopupOpen = 2;
constexpr float kShopFarAcross = 7.0f, kShopFarUp = 4.0f;  // walked this far, the shop closes (too_far)
constexpr float kScreenLift = 0.05f;   // the joints' plane is inside the monitor, behind its face (vr/vendor.rs)
constexpr uint32_t kHeroLocked = 0x20A5u;

struct ShopScreen {
    bool shown = false;
    Vec3 corner, across, down;    // on the monitor, in the world
    float x = 0, y = 0, width = 0, height = 0;  // on the canvas
};
// Filled by the game's tick, read by the frame's: the two sides of it, the frame taking the last one finished.
ShopScreen s_shop_screens[2][kShopScreens + 1];
std::atomic<int> s_shop_side{0};
bool s_shop_start = false;     // the vendor opened for the player on foot
bool s_shop_leaving = false;   // walked off, or dead: the keys leave
bool s_shop_remote = false;    // the PDA's vendor
bool pda_shopping() { return s_shopping && s_shop_remote; }

// Where the game's camera puts a point of the world on the canvas (project_graphics_bounds_float).
bool canvas_point(Vec3 world, float& x, float& y) {
    const gaddr matrix = s_mod->relocate(s_disc->camera_matrix);
    const Vec3 eye = read_vec(matrix + 0x40u);
    const Vec3 p = (world - eye) * 1024.0f;
    float out[4];
    for (uint32_t i = 0; i < 4u; ++i) {
        out[i] = get_float(matrix + 4u * i) * p.x + get_float(matrix + 0x10u + 4u * i) * p.y
                 + get_float(matrix + 0x20u + 4u * i) * p.z + get_float(matrix + 0x30u + 4u * i);
    }
    if (out[3] <= 0.0f) {
        return false;
    }
    const gaddr view = s_mod->relocate(s_disc->view_context);
    const gaddr extent = s_mod->relocate(s_disc->screen_extent);
    x = out[0] / out[3] * get_float(view + 0x190u) * 0.25f + static_cast<float>(ref<int32_t>(extent + 8u));
    y = out[1] / out[3] * get_float(view + 0x194u) * 0.25f + static_cast<float>(ref<int32_t>(extent + 12u));
    return true;
}

// A screen from its three points, less its margins: on the monitor, off it toward the camera, and the rectangle the
// game draws it in.
bool place_shop_screen(ShopScreen& screen, Vec3 p0, Vec3 p1, Vec3 p2, float inset_a, float inset_b, float less_a, float less_b, Vec3 eye) {
    const Vec3 a = p1 - p0, b = p2 - p0;
    const float along_a = length(a), along_b = length(b);
    if (along_a < 1e-3f || along_b < 1e-3f) {
        return false;
    }
    const Vec3 unit_a = a * (1.0f / along_a), unit_b = b * (1.0f / along_b);
    const Vec3 corner = p0 + unit_a * inset_a + unit_b * inset_b;
    const Vec3 across = unit_a * (along_a - less_a);
    const Vec3 down = unit_b * (along_b - less_b);
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    if (!canvas_point(corner, x0, y0) || !canvas_point(corner + across + down, x1, y1) || x1 <= x0 || y1 <= y0) {
        return false;
    }
    Vec3 normal = normalized(cross(across, down));
    if (dot(normal, eye - corner) < 0.0f) {
        normal = normal * -1.0f;
    }
    screen.corner = corner + normal * kScreenLift;
    screen.across = across;
    screen.down = down;
    screen.x = x0;
    screen.y = y0;
    screen.width = x1 - x0;
    screen.height = y1 - y0;
    return true;
}

// Each screen where the game places it (render_vendor_capture_pass_sequence): three joints of the machine, less the
// screen's margins, and the canvas rectangle the game draws it in.
void place_shop_screens() {
    const gaddr vendor = level0(s_disc->vendor_menu);
    const gaddr moby = ref<gaddr>(vendor + kVendorMoby);
    const int slot = moby != 0 ? s_mod->moby_slot_of(moby) : -1;
    if (slot < 0) {
        return;
    }
    int joints[kShopScreens * 3];
    float xyz[kShopScreens * 3 * 3];
    // The game's ids are the class's joint lists; the point of one is its last joint.
    for (int i = 0; i < kShopScreens * 3; ++i) {
        joints[i] = list_end_joint(moby, i / 3 * 4 + i % 3);
        if (joints[i] < 0) {
            return;
        }
    }
    if (s_mod->api().moby_joints(slot, joints, kShopScreens * 3, xyz) != OPENRAC_HOOK_OK) {
        return;
    }
    const gaddr margins = s_mod->relocate(s_disc->screen_margins);
    const Vec3 eye = read_vec(s_mod->relocate(s_disc->camera_matrix) + 0x40u);
    const int side = 1 - s_shop_side.load(std::memory_order_relaxed);
    for (int s = 0; s < kShopScreens; ++s) {
        ShopScreen& screen = s_shop_screens[side][s];
        const float* at = xyz + s * 9;
        const Vec3 p0{at[0], at[1], at[2]}, p1{at[3], at[4], at[5]}, p2{at[6], at[7], at[8]};
        const gaddr margin = margins + 16u * static_cast<uint32_t>(s);
        screen.shown = place_shop_screen(screen, p0, p1, p2, get_float(margin), get_float(margin + 4u), 2.0f * get_float(margin + 8u),
                                         2.0f * get_float(margin + 12u), eye);
    }
    // The popup, a moby the game does not list: its first three joint lists, the margins of
    // draw_vendor_purchase_panel, while it is up.
    ShopScreen& popup = s_shop_screens[side][kShopPopup];
    popup.shown = false;
    const gaddr own = ref<gaddr>(vendor + kVendorMobys);
    if (ref<int32_t>(vendor + kVendorBuy) >= kBuyPopupOpen && in_ram(own)) {
        const gaddr panel = own + kPopupMoby;
        const int lists[3] = {list_end_joint(panel, 0), list_end_joint(panel, 1), list_end_joint(panel, 2)};
        float corners[9];
        if (lists[0] >= 0 && lists[1] >= 0 && lists[2] >= 0 && s_mod->api().moby_joints_at(panel, lists, 3, corners) == OPENRAC_HOOK_OK) {
            popup.shown = place_shop_screen(popup, Vec3{corners[0], corners[1], corners[2]}, Vec3{corners[3], corners[4], corners[5]},
                                            Vec3{corners[6], corners[7], corners[8]}, 0.03f, 0.04f, 0.06f, 0.04f, eye);
        }
    }
#if !defined(__ANDROID__)
    static uint64_t told = 0;
    if (s_tick >= told && std::getenv("QUEST_POINTER_TRACE") != nullptr) {
        told = s_tick + 120;
        const gaddr pad = s_disc->pad_state;
        char head[170];
        std::snprintf(head, sizeof head, "[vr-shop] state %d, pad mode %d stick %.2f %.2f, hero state %d at (%.2f %.2f %.2f), camera at (%.2f %.2f %.2f)",
                      ref<int32_t>(vendor), ref<int32_t>(pad + 0x1CCu), get_float(pad + 0x100u), get_float(pad + 0x104u), hero_state(), s_hero_after.x,
                      s_hero_after.y, s_hero_after.z, eye.x, eye.y, eye.z);
        log_line(1, head);
        const Vec3 along = read_vec(camera_block() + kCameraRows);
        std::snprintf(head, sizeof head, "[vr-shop] camera along (%.3f %.3f %.3f), look (%.3f %.3f %.3f)", along.x, along.y, along.z,
                      get_float(vendor + 0x70u), get_float(vendor + 0x74u), get_float(vendor + 0x78u));
        log_line(1, head);
        for (int s = 0; s < kShopScreens; ++s) {
            const ShopScreen& screen = s_shop_screens[side][s];
            char line[170];
            std::snprintf(line, sizeof line, "[vr-shop] screen %d %s: canvas (%.0f %.0f %.0f %.0f), at (%.2f %.2f %.2f) %.2f wide %.2f high", s,
                          screen.shown ? "shown" : "not shown", screen.x, screen.y, screen.width, screen.height, screen.corner.x,
                          screen.corner.y, screen.corner.z, length(screen.across), length(screen.down));
            log_line(1, line);
        }
    }
#endif
    s_shop_side.store(side, std::memory_order_release);
}

// On foot the trigger buys and A jumps, so the prompts' Cross icon is drawn as the trigger's, the Circle's (Rerac-vr's
// vendor.rs cross_icon): its record in the level's three fonts, swapped for the shop's time.
void shop_cross_icon(bool on) {
    static uint8_t kept[3][kGlyphRecord];
    static bool swapped = false;
    if (on == swapped) {
        return;
    }
    swapped = on;
    openrac::Memory memory = s_mod->memory();
    for (uint32_t font = 0; font < 3u; ++font) {
        const gaddr table = level0(s_disc->level_fonts + 0x3A0u * font);
        if (!in_ram(table)) {
            continue;
        }
        const gaddr cross = table + kGlyphRecord * kFirstIcon;
        if (on) {
            std::memcpy(kept[font], memory.pointer(cross), kGlyphRecord);
            std::memcpy(memory.pointer(cross), memory.pointer(cross + kGlyphRecord), kGlyphRecord);
        } else {
            std::memcpy(memory.pointer(cross), kept[font], kGlyphRecord);
        }
    }
}

// Ratchet out of the hold the vendor's opening puts him in, what its closing does for him, and the sounds it stopped:
// the world goes on.
void start_shopping() {
    using SetState = int (*)(int, int);
    using Step = void (*)();
    using Group = void (*)(int);
    constexpr int kSoundsOfPlay = 0x1D;
    const openrac_api& api = s_mod->api();
    if (void* set_state = api.function_called(0, s_disc->vendor_close, s_disc->vendor_close_size, s_disc->hero_set_state)) {
        reinterpret_cast<SetState>(set_state)(0, 1);
    }
    ref<uint8_t>(s_disc->hero + kHeroLocked) = 0;
    if (void* show = api.function(s_disc->hero_show)) {
        reinterpret_cast<Step>(show)();
    }
    if (void* sounds = api.function(s_disc->sounds_on)) {
        reinterpret_cast<Group>(sounds)(kSoundsOfPlay);
    }
    if (void* music = api.function(s_disc->music_on)) {
        reinterpret_cast<Step>(music)();
    }
    s_shopping = true;
    s_shop_leaving = false;
    shop_cross_icon(true);
    log_line(1, "[vr-shop] on foot at the vendor");
}

// What the shop's update gets as its pad: the right stick pages, its click leaves, the trigger is what the hand is on.
uint32_t shop_keys() {
    const Controls& c = s_frame.controls;
    constexpr float kStickPages = 0.7f;
    uint16_t held = vendor_pulse();
    if (c.right_click) {
        held = static_cast<uint16_t>(held | kPadTriangle);
    }
    if (c.right_x > kStickPages) {
        held = static_cast<uint16_t>(held | kPadRight);
    }
    if (c.right_x < -kStickPages) {
        held = static_cast<uint16_t>(held | kPadLeft);
    }
    if (c.right_trigger > kTriggerOn) {
        held = static_cast<uint16_t>(held | s_pointed.key);
    }
    if (s_shop_leaving && s_tick % 2u == 0u) {
        held = static_cast<uint16_t>(held | kPadTriangle);
    }
    // The game's words have the two bytes the other way about.
    return static_cast<uint32_t>(static_cast<uint16_t>((held << 8) | (held >> 8)));
}

// The PDA's vendor, on foot, is a machine like the others (its draw above all, which would show a machine standing in
// a place of its own), but for what it sells and how it goes: the word says so to the shop's own passes alone. Its
// camera keeps the tilt of the one it took over, where a machine's is level, and its screens are drawn for that.
void as_remote(gaddr vendor, bool remote) {
    if (!s_shop_remote) {
        return;
    }
    ref<int32_t>(vendor + kVendorRemote) = remote ? 1 : 0;
    if (remote) {
        return;
    }
    const float yaw = get_float(vendor + kVendorLook + 8u);
    const Vec3 f{std::cos(yaw), std::sin(yaw), 0}, l{-std::sin(yaw), std::cos(yaw), 0}, u{0, 0, 1};
    const gaddr camera = camera_block();
    const auto put3 = [](gaddr at, Vec3 v) {
        put_float(at, v.x);
        put_float(at + 4u, v.y);
        put_float(at + 8u, v.z);
    };
    put3(camera + kCameraRows, f);
    put3(camera + kCameraRows + 0x10u, l);
    put3(camera + kCameraRows + 0x20u, u);
    put3(camera + kCameraMatrix, {-l.x, -u.x, f.x});
    put3(camera + kCameraMatrix + 0x10u, {-l.y, -u.y, f.y});
    put3(camera + kCameraMatrix + 0x20u, {-l.z, -u.z, f.z});
}

template <class Original>
void vendor_update(Original original) {
    if (s_shop_start) {
        s_shop_start = false;
        start_shopping();
    }
    if (!s_shopping) {
        original();
        return;
    }
    const gaddr vendor = level0(s_disc->vendor_menu);
    ref<int32_t>(vendor + kVendorBoughtWeapon) = -1;
    if (ref<int32_t>(vendor + kVendorState) != kVendorMenuOpen) {
        as_remote(vendor, true);
        original();
        as_remote(vendor, false);
        place_shop_screens();
        return;
    }
    // The world's tick is the one its fly-in and leaving run: the game's own code, so each level's own hero. One pass
    // of the update as leaving, not far enough along to leave, in play's mode (the hero's code asks it: a hit counts
    // only in mode 0); then its menu's pass.
    const gaddr mode = s_mod->relocate(s_disc->game_mode);
    const int32_t timer = ref<int32_t>(vendor + kVendorTimer);
    ref<int32_t>(vendor + kVendorState) = kVendorLeaving;
    ref<int32_t>(vendor + kVendorTimer) = 0;
    ref<int32_t>(mode) = 0;
    original();
    ref<int32_t>(mode) = kGameModeVendor;
    ref<int32_t>(vendor + kVendorState) = kVendorMenuOpen;
    ref<int32_t>(vendor + kVendorTimer) = timer;
    const gaddr moby = ref<gaddr>(vendor + kVendorMoby);
    if (moby != 0 && !s_shop_leaving) {
        const Vec3 at = read_vec(moby + kMobyPosition);
        const bool far = std::hypot(at.x - s_hero_after.x, at.y - s_hero_after.y) > kShopFarAcross || std::abs(at.z - s_hero_after.z) > kShopFarUp;
        if ((far && !s_shop_remote) || s_health <= 0) {
            s_shop_leaving = true;
            log_line(1, "[vr-shop] walked off: the shop closes");
        }
    }
    // The hero has had his pad; the shop's update reads the same words, so for it they are the shop's.
    constexpr uint32_t kHeld = 0x1A0u, kPressed = 0x1A4u, kRawHeld = 0x1B0u, kRawPressed = 0x1B4u, kAllHeld = 0x1C0u, kAllPressed = 0x1C4u;
    const gaddr pad = s_disc->pad_state;
    const uint32_t kept[6] = {ref<uint32_t>(pad + kHeld),    ref<uint32_t>(pad + kPressed),   ref<uint32_t>(pad + kRawHeld),
                              ref<uint32_t>(pad + kRawPressed), ref<uint32_t>(pad + kAllHeld), ref<uint32_t>(pad + kAllPressed)};
    static uint32_t held_was = 0;
    const uint32_t held = shop_keys(), pressed = held & ~held_was;
    held_was = held;
    ref<uint32_t>(pad + kHeld) = ref<uint32_t>(pad + kRawHeld) = ref<uint32_t>(pad + kAllHeld) = held;
    ref<uint32_t>(pad + kPressed) = ref<uint32_t>(pad + kRawPressed) = ref<uint32_t>(pad + kAllPressed) = pressed;
    as_remote(vendor, true);
    original();
    as_remote(vendor, false);
    ref<uint32_t>(pad + kHeld) = kept[0];
    ref<uint32_t>(pad + kPressed) = kept[1];
    ref<uint32_t>(pad + kRawHeld) = kept[2];
    ref<uint32_t>(pad + kRawPressed) = kept[3];
    ref<uint32_t>(pad + kAllHeld) = kept[4];
    ref<uint32_t>(pad + kAllPressed) = kept[5];
    place_shop_screens();
}

// The PDA's vendor: the game stands its own machine by the camera, for the screen. For the eyes it stands ahead of the
// player, at their feet's height, facing them (Rerac-vr's remote_vendor), and the game's camera goes where a machine's
// does, the same offset from it, for the screens to be drawn from.
void stand_remote_vendor() {
    constexpr float kRemoteAhead = 3.8f, kRemoteEyeUp = 1.5f;  // vendor_camera_eye_local_offset
    const gaddr vendor = level0(s_disc->vendor_menu);
    const gaddr moby = ref<gaddr>(vendor + kVendorMoby);
    if (moby == 0) {
        return;
    }
    const Vec3 gaze = frame_to_world(rotate(yaw_quat(s_rig.yaw) * s_frame.head.orientation, {0, 0, -1}));
    const float yaw = std::atan2(gaze.y, gaze.x);
    put_float(moby + kMobyPosition, s_hero_after.x + std::cos(yaw) * kRemoteAhead);
    put_float(moby + kMobyPosition + 4u, s_hero_after.y + std::sin(yaw) * kRemoteAhead);
    put_float(moby + kMobyPosition + 8u, s_hero_after.z);
    put_float(moby + kMobyEuler + 8u, yaw + kPi);
    const openrac_api& api = s_mod->api();
    if (void* matrix = api.function(s_disc->moby_matrix)) {
        reinterpret_cast<void (*)(uint32_t)>(matrix)(moby);
    }
    put_float(vendor + kVendorEye, s_hero_after.x);
    put_float(vendor + kVendorEye + 4u, s_hero_after.y);
    put_float(vendor + kVendorEye + 8u, s_hero_after.z + kRemoteEyeUp);
    put_float(vendor + kVendorLook + 8u, yaw);
    if (void* script = api.function(s_disc->camera_script)) {
        reinterpret_cast<void (*)(uint32_t, uint32_t, int32_t, int32_t, int32_t)>(script)(vendor + kVendorEye, vendor + kVendorLook, 1, 0, 0);
    }
    if (void* position = api.function(s_disc->camera_position)) {
        reinterpret_cast<void (*)(uint32_t)>(position)(vendor + kVendorEye);
    }
    if (void* rotation = api.function(s_disc->camera_rotation)) {
        reinterpret_cast<void (*)(uint32_t)>(rotation)(vendor + kVendorLook);
    }
    s_shop_remote = true;
}

template <class Original>
void vendor_opened(Original original, uint32_t moby) {
    original(moby);
    s_shop_start = s_options.shop_on_foot && s_options.first_person && !s_options.screen;
    if (s_shop_start) {
        s_shopping = true;  // the player is in play from this tick on
        if (moby == 0) {
            stand_remote_vendor();
        }
    }
}

template <class Original>
void vendor_closed(Original original, int32_t after_scene) {
    if (!s_shopping) {
        original(after_scene);
        return;
    }
    // The game puts him in front of the machine: on foot he stays where he walked to.
    const gaddr hero = s_disc->hero;
    uint32_t place[8];
    std::memcpy(place, s_mod->memory().pointer(hero + kHeroPosition), sizeof place);
    original(after_scene);
    std::memcpy(s_mod->memory().pointer(hero + kHeroPosition), place, sizeof place);
    if (void* script_end = s_mod->api().function(s_disc->camera_script_end); s_shop_remote && script_end != nullptr) {
        reinterpret_cast<void (*)(int32_t)>(script_end)(2);
    }
    s_shop_remote = false;
    s_shopping = false;
    s_shop_start = false;
    shop_cross_icon(false);
    log_line(1, "[vr-shop] left the vendor where he stands");
}

// The picture the vendor keeps under its menu is, on foot, the level drawn anew: the mask its fly-in left (world, no
// HUD, which it draws itself), and game mode 0 for the billboards, which are drawn in that mode alone.
template <class Original>
void menu_backdrop(Original original, int32_t source, int32_t target, int32_t bytes) {
    if (!s_shopping) {
        original(source, target, bytes);
        return;
    }
    using Step = void (*)();
    const gaddr mode = s_mod->relocate(s_disc->game_mode);
    const int32_t kept = ref<int32_t>(mode);
    ref<int32_t>(mode) = 0;
    if (void* draw = s_mod->api().function(s_disc->draw_world)) {
        reinterpret_cast<Step>(draw)();
    }
    ref<int32_t>(mode) = kept;
}

// The hand's ray on the screens: as a point of the canvas, what the flat vendor's boxes answer.
void shop_pointer() {
    if (!s_have_frame || !s_frame.hand_valid[kRight]) {
        s_pointed = {};
        return;
    }
    const ShopScreen* screens = s_shop_screens[s_shop_side.load(std::memory_order_acquire)];
    const Pose aim = in_frame(s_rig, s_frame.hand_aim[kRight], anchor_now(1.0f));
    const Vec3 from = frame_to_world(aim.position);
    const Vec3 along = frame_to_world(rotate(aim.orientation, Vec3{0, 0, -1}));
    float nearest = 1e9f, cx = -1.0f, cy = -1.0f;
    for (int s = 0; s <= kShopPopup; ++s) {
        const ShopScreen& screen = screens[s];
        float far = 0, u = 0, v = 0;
        if (screen.shown && ray_on_quad(from, along, screen.corner, screen.across, screen.down, far, u, v) && far < nearest) {
            nearest = far;
            cx = screen.x + u * screen.width;
            cy = screen.y + v * screen.height;
        }
    }
    point_at_vendor(cx, cy);
#if !defined(__ANDROID__)
    // PC test aid (QUEST_POINTER_TRACE=1): the hand's yaw and pitch that would point at the middle of each screen.
    static uint64_t aimed = 0;
    if (s_tick >= aimed && std::getenv("QUEST_POINTER_TRACE") != nullptr) {
        aimed = s_tick + 240;
        for (int s = 0; s <= kShopPopup; ++s) {
            if (screens[s].shown) {
                const Vec3 middle = screens[s].corner + (screens[s].across + screens[s].down) * 0.5f;
                const Vec3 to = s_rig.from_frame(world_to_frame(middle), anchor_now(1.0f)) - s_frame.hand_aim[kRight].position;
                char line[100];
                std::snprintf(line, sizeof line, "[vr-shop] aim at screen %d: hand_yaw %.1f hand_pitch %.1f", s,
                              std::atan2(-to.x, -to.z) * 57.29578f, std::atan2(to.y, std::hypot(to.x, to.z)) * 57.29578f);
                log_line(1, line);
            }
        }
    }
#endif
    PointerLayers layers;
    const Pose& hand = s_frame.hand_aim[kRight];
    const Vec3 start = hand.position + rotate(hand.orientation, Vec3{0, 0, -kBeamStart});
    Vec3 to = hand.position + rotate(hand.orientation, Vec3{0, 0, -kBeamFree});
    if (nearest < 1e8f) {
        to = s_rig.from_frame(world_to_frame(from + along * nearest), anchor_now(1.0f));
        layers.dot = true;
        layers.dot_pose = {to, looking_along(normalized(to - s_frame.head.position))};
    }
    layers.beam = beam_pose(start, to, s_frame.head.position, layers.beam_pose, layers.beam_length);
    s_headset->show_pointer(layers);
    s_pointer_shown = true;
}

// Whether the page up has something to point at (Rerac-vr: then the sticks leave its cursor alone, wherever the hand
// is): a port page, a list, a grid, toggles, choices or bars, or a widget the beam has taken a target from. A message, a prompt and the Select
// map have none, and keep their sticks.
bool page_has_targets() {
    const gaddr menu = s_mod->relocate(s_disc->menu_system);
    const gaddr page = in_ram(menu) ? ref<gaddr>(menu + kMenuCurrent) : 0;
    if (vendor_open() || !readable(page, 0x88u) || ref<int32_t>(menu + kMenuState) == kMenuSwitching) {
        return false;
    }
    if (in_port_statics(page) || page == s_target_page) {
        return true;
    }
    const gaddr prompt = s_mod->relocate(s_disc->prompt_widget);
    for (int i = 0; i < kPageScreenCount; ++i) {
        const gaddr widget = ref<gaddr>(page + kPageScreens + 4u * static_cast<uint32_t>(i));
        if (widget == prompt || !readable(widget, 0x58u) || (ref<uint32_t>(widget + kWidgetFlags) & kWidgetHidden) != 0) {
            continue;
        }
        MenuList list;
        int x = 0, y = 0;
        if (read_list(widget, list, x, y) || widget_runs(widget, s_disc->menu_grid) || widget_runs(widget, s_disc->menu_toggles)
            || widget_runs(widget, s_disc->menu_choices) || widget_runs(widget, s_disc->menu_sound)) {
            return true;
        }
    }
    return false;
}

// Once a frame.
void menu_pointer() {
    const int phase = phase_now();
    s_page_targets = false;
    if (s_shopping) {
        shop_pointer();
        return;
    }
    float u = 0, v = 0;
    const bool in_panel_menu = s_have_frame && s_frame.hand_valid[kRight] && !s_panel_is_hud && !s_panel_is_monitor
                               && s_panel_placed && (phase == OPENRAC_PLAY_MENU || phase == OPENRAC_PLAY_FRONT_END);
    s_pointer_on_panel = in_panel_menu && panel_point(s_panel_pose, panel_width(), panel_width() * 0.75f, s_frame.hand_aim[kRight], u, v);
    s_page_targets = in_panel_menu && page_has_targets();
    if (in_panel_menu) {
        show_beam(s_pointer_on_panel, u, v);
        s_pointer_shown = true;
    } else if (s_pointer_shown) {
        s_pointer_shown = false;
        s_headset->show_pointer({});
    }
    if (!s_have_frame || !s_frame.hand_valid[kRight] || s_panel_is_hud || s_panel_is_monitor || !s_panel_placed
        || (phase != OPENRAC_PLAY_MENU && phase != OPENRAC_PLAY_FRONT_END)
        || !panel_point(s_panel_pose, panel_width(), panel_width() * 0.75f, s_frame.hand_aim[kRight], u, v)) {
        s_pointed = {};
        return;
    }
    const float cx = u * kCanvasWidth, cy = v * kCanvasHeight;
    if (vendor_open()) {
        point_at_vendor(cx, cy);
        return;
    }
    // A message box's labels first: the box is over the page.
    const PointedRow label = label_at(cx, cy);
    if (label.row >= 0) {
        if (label.key != s_pointed.key || s_pointed.list != 0) {
            char line[120];
            std::snprintf(line, sizeof line, "[vr-pointer] the message box's %s label (canvas %.0f %.0f, drawn at %.0f %.0f)",
                          label.key == kPadCross ? "Cross" : label.key == kPadCircle ? "Circle" : "Triangle", cx, cy,
                          s_labels[label.row].x, s_labels[label.row].y);
            log_line(1, line);
        }
        s_pointed = label;
        return;
    }
    const gaddr menu = s_mod->relocate(s_disc->menu_system);
    const gaddr page = in_ram(menu) ? ref<gaddr>(menu + kMenuCurrent) : 0;  // the port's pages are in its statics
    if (!readable(page, 0x88u) || ref<int32_t>(menu + kMenuState) == kMenuSwitching) {
        s_pointed = {};
        return;
    }
    // The port's pages of rows (the mods' pages, VR Options among them, and Video) show a window of eight rows the port
    // fills, a widget the list reader does not take: the hand writes that window's row. A port page whose widget 3 is a
    // list as the game's are (the title's Exit box: Yes and No, 2 rows) is pointed at as the game's pages are: taken
    // as rows, its two lines took a cursor of up to four.
    if (in_port_statics(page)) {
        const gaddr rows_list = ref<gaddr>(page + kPageScreens + 4u * 3u);
        MenuList rows;
        int rows_x = 0, rows_y = 0;
        const bool listed = readable(rows_list, 0x58u) && read_list(rows_list, rows, rows_x, rows_y);
        const bool rows_page = !listed;
        static gaddr told_port_page = 0;
        if (page != told_port_page) {
            told_port_page = page;
            char line[200];
            std::snprintf(line, sizeof line, "[vr-pointer] the port's page 0x%X %s: widget 3 0x%X a list %d, %d rows, %dx%d at (%d %d)", page,
                          rows_page ? "of rows" : "of its own, not rows", rows_list, listed ? 1 : 0, rows.rows, rows.width, rows.height, rows_x,
                          rows_y);
            log_line(1, line);
        }
        if (rows_page) {
            point_at_vr_rows(page, cx, cy);
            return;
        }
    }
    PointedRow over;
    static gaddr told_page = 0, told_other = 0;
    bool told = false;
    const gaddr prompt = s_mod->relocate(s_disc->prompt_widget);
    for (int i = 0; i < kPageScreenCount; ++i) {
        const gaddr widget = ref<gaddr>(page + kPageScreens + 4u * static_cast<uint32_t>(i));
        if (!readable(widget, 0x58u) || (ref<uint32_t>(widget + kWidgetFlags) & kWidgetHidden) != 0) {
            continue;
        }
        if (widget == prompt) {
#if !defined(__ANDROID__)
            static bool told_prompt = false;
            if (!told_prompt && std::getenv("QUEST_POINTER_TRACE") != nullptr) {
                told_prompt = true;
                char line[120];
                std::snprintf(line, sizeof line, "[vr-pointer] the prompt widget %d: rect (%d %d %d %d)", i, ref<int32_t>(widget + kWidgetRect),
                              ref<int32_t>(widget + kWidgetRect + 4u), ref<int32_t>(widget + kWidgetRect + 8u), ref<int32_t>(widget + kWidgetRect + 12u));
                log_line(1, line);
            }
#endif
            const PointedRow answer = prompt_answer_at(widget, cx, cy);
            if (answer.row >= 0) {
                over = answer;
            }
            continue;
        }
        MenuList list;
        int x = 0, y = 0;
        if (!read_list(widget, list, x, y)) {
            const int box_x = ref<int32_t>(widget + kWidgetRect), box_y = ref<int32_t>(widget + kWidgetRect + 4u);
            const int box_w = ref<int32_t>(widget + kWidgetRect + 8u), box_h = ref<int32_t>(widget + kWidgetRect + 12u);
            if (box_w > 8 && box_h > 8 && box_w <= 600 && box_h <= 600 && cx >= static_cast<float>(box_x) && cy >= static_cast<float>(box_y)
                && cx < static_cast<float>(box_x + box_w) && cy < static_cast<float>(box_y + box_h)) {
                const PointedRow other = widget_at(widget, cx - static_cast<float>(box_x), cy - static_cast<float>(box_y),
                                                   static_cast<float>(box_w), static_cast<float>(box_h));
                if (other.row >= 0 || other.dest > 0) {
                    over = other;
                }
            }
            if (page != told_other && std::getenv("QUEST_POINTER_TRACE") != nullptr) {
                char line[240];
                std::snprintf(line, sizeof line,
                              "[vr-pointer] page 0x%X widget %d: not a list: rect (%d %d %d %d) flags 0x%X items 0x%X cursor %d "
                              "scroll %d update 0x%X draw 0x%X",
                              page, i, ref<int32_t>(widget + kWidgetRect), ref<int32_t>(widget + kWidgetRect + 4u),
                              ref<int32_t>(widget + kWidgetRect + 8u), ref<int32_t>(widget + kWidgetRect + 12u),
                              ref<int32_t>(widget + kListFlags), ref<gaddr>(widget + kListItems),
                              ref<int32_t>(widget + kListCursor), ref<int32_t>(widget + kListScroll), ref<gaddr>(widget),
                              ref<gaddr>(widget + 4u));
                log_line(1, line);
            }
            continue;
        }
        if (page != told_page) {
            told = true;
            char line[160];
            std::snprintf(line, sizeof line, "[vr-pointer] page 0x%X: a list at (%d %d) %dx%d, %d rows, flags 0x%X, update 0x%X draw 0x%X", page, x,
                          y, list.width, list.height, list.rows, list.flags, ref<gaddr>(widget), ref<gaddr>(widget + 4u));
            log_line(1, line);
        }
        if (cx < static_cast<float>(x) || cy < static_cast<float>(y) || cx >= static_cast<float>(x + list.width)
            || cy >= static_cast<float>(y + list.height)) {
            continue;
        }
        static void* const planets = s_mod->api().function(s_disc->menu_planet_list);
        const bool planet_list = planets != nullptr && s_mod->api().function_at(ref<gaddr>(widget + 4u)) == planets;
        const int row = planet_list ? planet_row_at(list, cx - static_cast<float>(x), cy - static_cast<float>(y))
                                    : list_row_at(list, cx - static_cast<float>(x), cy - static_cast<float>(y));
        const gaddr items = ref<gaddr>(widget + kListItems);
        if (row >= 0 && ref<int16_t>(items + kMenuItemBytes * static_cast<uint32_t>(row) + kMenuItemEnabled) != 0) {
            over = {widget, row};
        }
    }
    if (told) {
        told_page = page;
    }
    told_other = page;
    // A planet of the galaxy map is its row of the ship's planet list.
    if (over.dest > 0 && over.row < 0) {
        over = planet_list_row(page, over.dest);
    }
    // A slot page's other widgets take no input, and its slot list updates only with the focus (pointer.rs).
    const gaddr focused = ref<gaddr>(page + kPageFocus);
    if (over.list != 0 && over.list != focused && readable(focused, 8u) && widget_draws(focused, s_disc->menu_slots)
        && !widget_draws(over.list, s_disc->menu_slots)) {
        over = {};
    }
    if (over.list != 0 && over.row >= 0 && (over.list != s_pointed.list || over.row != s_pointed.row)) {
        if (over.cursor_at != 0 && ref<int32_t>(over.list + over.cursor_at) != over.row) {
            ref<int32_t>(over.list + over.cursor_at) = over.row;
        }
        // A slot list's cursor is also the pages' shared slot: its update starts from that each tick.
        if (widget_draws(over.list, s_disc->menu_slots)) {
            ref<int32_t>(s_disc->memcard_slot) = over.row;
        }
        if (ref<gaddr>(page + kPageFocus) != over.list) {
            ref<gaddr>(page + kPagePendingFocus) = over.list;
        }
        // The planet list's cursor is the ship's destination, which the game sets only as its keys move it.
        int32_t dest = 0;
        if (over.cursor_at == kListCursor && is_picked_list(over.list) && (ref<int32_t>(over.list + kListFlags) & kListPlanetCursor) != 0) {
            const gaddr order = ref<gaddr>(s_mod->relocate(s_disc->planet_order));
            if (readable(order + 4u * static_cast<uint32_t>(over.row), 4u)) {
                dest = ref<int32_t>(order + 4u * static_cast<uint32_t>(over.row));
                ref<int32_t>(s_mod->relocate(s_disc->planet_dest)) = dest;
            }
        }
        char line[160];
        // An item grid's cell: the item it holds.
        std::string held;
        if (over.cursor_at == kGridCursor) {
            const gaddr cell = ref<gaddr>(over.list + kGridCells) + kGridCellBytes * static_cast<uint32_t>(over.row);
            if (readable(cell, kGridCellBytes)) {
                held = ", item " + std::to_string(ref<int16_t>(cell + kGridCellItem));
            }
        }
        std::snprintf(line, sizeof line, "[vr-pointer] row %d of the list 0x%X (canvas %.0f %.0f)%s%s%s", over.row, over.list, cx, cy,
                      over.key == kPadTriangle ? ", Triangle's line" : over.key == kPadCircle ? ", Circle's line" : "",
                      dest > 0 ? (", the destination level " + std::to_string(dest)).c_str() : "", held.c_str());
        log_line(1, line);
    }
    // The trigger held on a Sound bar sets its volume where the hand points.
    if (over.list != 0 && over.volume_row >= 0 && s_frame.controls.right_trigger > kTriggerOn) {
        set_volume(over.volume_row, over.volume);
    }
    if (over.list != 0) {
        s_target_page = page;
    }
    s_pointed = over;
}

// A rectangle of the 2D, from the canvas, on the panel the HUD would be: the quad over the world, at the head.
void head_quad(openrac_world_quad& quad, float x, float y, float width, float height) {
    const float wide = kHudDistance * 1.05f * s_options.hud_scale, high = wide * 3.0f / 4.0f;
    const Rig rig = first_person_rig(s_view_alpha);
    const Vec3 anchor = anchor_now(s_view_alpha);
    const auto at = [&](float cx, float cy) {
        const Vec3 local{(cx / kCanvasWidth - 0.5f) * wide, (0.5f - cy / kCanvasHeight) * high, -kHudDistance};
        const Pose point{s_frame.head.position + rotate(s_frame.head.orientation, local), {}};
        return frame_to_world(in_frame(rig, point, anchor).position);
    };
    const Vec3 corners[4] = {at(x, y), at(x + width, y), at(x, y + height), at(x + width, y + height)};
    for (int i = 0; i < 4; ++i) {
        quad.corner[i][0] = corners[i].x;
        quad.corner[i][1] = corners[i].y;
        quad.corner[i][2] = corners[i].z;
    }
    quad.u0 = x / kCanvasWidth;
    quad.v0 = y / kCanvasHeight;
    quad.u1 = (x + width) / kCanvasWidth;
    quad.v1 = (y + height) / kCanvasHeight;
    quad.opacity = 1.0f;
    quad.over = 1;
    quad.solid = 0;
}

// ---- The menu in the world ----
//
// Rerac-vr's spatial menu (vr/spatial_menu.rs): the pause menu's primitives at a world anchor, the level around them.
// Here the port draws the level under the page without running it (keep_running 2, world.c), so the eyes see it as
// it stopped, and the page's widgets stand on the panel's plane in the room, each its rectangle of the 2D as a quad
// over the world: the world shows between them, and the one the player is on stands nearer. A compositor layer
// needs none of this; quads drawn in the eye do it with the level's own depth left to them.

constexpr float kTileLift = 0.03f, kTileFocused = 0.07f;  // metres off the plane, toward the player
constexpr float kTileMargin = 5.0f;                       // the frames stand this far outside their widgets
constexpr float kBoxLift = 0.11f;                         // a message box stands before the page that opened it
constexpr uint32_t kWidgetModels = 0x44u;                 // a widget's moby handles (menu.h, MenuItemPreviewData)

// The frame the game drew last: over a page, the message box's, which it draws after the page.
struct BoxFrame {
    int32_t top, bottom, left, right;
    uint64_t tick;
};
BoxFrame s_box_frame{};

// A rectangle of the 2D, from the canvas, at its place on the room's panel.
void panel_quad(openrac_world_quad& quad, float x, float y, float width, float height, float lift) {
    const float high = panel_width() * 3.0f / 4.0f;
    const Rig rig = s_title_world ? s_rig : first_person_rig(s_view_alpha);
    const Vec3 anchor = s_title_world ? Vec3{} : anchor_now(s_view_alpha);
    const auto at = [&](float cx, float cy) {
        const Vec3 local{(cx / kCanvasWidth - 0.5f) * panel_width(), (0.5f - cy / kCanvasHeight) * high, lift * s_panel_scale};
        const Pose point{s_panel_pose.position + rotate(s_panel_pose.orientation, local), {}};
        return s_title_world ? rig.to_camera(point, s_recentre_yaw).position : frame_to_world(in_frame(rig, point, anchor).position);
    };
    const Vec3 corners[4] = {at(x, y), at(x + width, y), at(x, y + height), at(x + width, y + height)};
    for (int i = 0; i < 4; ++i) {
        quad.corner[i][0] = corners[i].x;
        quad.corner[i][1] = corners[i].y;
        quad.corner[i][2] = corners[i].z;
    }
    quad.u0 = x / kCanvasWidth;
    quad.v0 = y / kCanvasHeight;
    quad.u1 = (x + width) / kCanvasWidth;
    quad.v1 = (y + height) / kCanvasHeight;
    quad.opacity = 1.0f;
    quad.over = 1;
    quad.solid = 0;
}

// The current page's parts, or the message box it opened, which the game draws in its place; none between pages, when
// it is shown whole.
int menu_quads(openrac_world_quad* quads) {
    const gaddr menu = s_mod->relocate(s_disc->menu_system);
    const gaddr page = in_ram(menu) ? ref<gaddr>(menu + kMenuCurrent) : 0;
    int used = 0;
    const int32_t mode = ref<int32_t>(s_mod->relocate(s_disc->game_mode));
    if ((mode == kGameModeMenu || s_title_world) && readable(page, 0x88u) && ref<int32_t>(menu + kMenuState) != kMenuSwitching) {
        const gaddr focus = ref<gaddr>(page + kPageFocus);
        for (int i = 0; i < kPageScreenCount; ++i) {
            const gaddr widget = ref<gaddr>(page + kPageScreens + 4u * static_cast<uint32_t>(i));
            if (!readable(widget, 0x58u) || (ref<uint32_t>(widget + kWidgetFlags) & kWidgetHidden) != 0) {
                continue;
            }
            float x = static_cast<float>(ref<int32_t>(widget + kWidgetRect)) - kTileMargin;
            float y = static_cast<float>(ref<int32_t>(widget + kWidgetRect + 4u)) - kTileMargin;
            float width = static_cast<float>(ref<int32_t>(widget + kWidgetRect + 8u)) + 2.0f * kTileMargin;
            float height = static_cast<float>(ref<int32_t>(widget + kWidgetRect + 12u)) + 2.0f * kTileMargin;
            width = std::min(width + std::min(x, 0.0f), kCanvasWidth - std::max(x, 0.0f));
            height = std::min(height + std::min(y, 0.0f), kCanvasHeight - std::max(y, 0.0f));
            x = std::max(x, 0.0f);
            y = std::max(y, 0.0f);
            if (width >= 16.0f && height >= 16.0f && used < OPENRAC_WORLD_QUADS_MAX) {
                panel_quad(quads[used], x, y, width, height, widget == focus ? kTileFocused : kTileLift);
                quads[used].solid = 1;
                // Its turning model stands out of the tile (Rerac-vr's menu_models.rs): the widget's handles, which
                // the port takes only where one is a moby the game drew with the menu's own camera.
                if (readable(widget, kWidgetModels + sizeof quads[used].stage)) {
                    std::memcpy(quads[used].stage, s_mod->memory().pointer(widget + kWidgetModels), sizeof quads[used].stage);
                }
                ++used;
            }
        }
    }
    if (mode == kGameModeFreeze && s_tick <= s_box_frame.tick + 2u && used < OPENRAC_WORLD_QUADS_MAX) {
        const BoxFrame& box = s_box_frame;
        panel_quad(quads[used], static_cast<float>(box.left) - kTileMargin, static_cast<float>(box.top) - kTileMargin,
                   static_cast<float>(box.right - box.left) + 2.0f * kTileMargin,
                   static_cast<float>(box.bottom - box.top) + 2.0f * kTileMargin, kBoxLift);
        quads[used++].solid = 1;
    } else if (mode == kGameModeFreeze && s_tick <= s_centred.tick + 2u && used < OPENRAC_WORLD_QUADS_MAX) {
        // A box drawn without a bevelled frame (the race's, the help's Continue): the part around its lines, from a
        // little above the first to under the last (the Continue box: rows 100 to 160 about its line at 122).
        constexpr float kAbove = 30.0f, kBelow = 38.0f;
        const CentredLines& lines = s_centred;
        const float top = std::max(0.0f, lines.top - kAbove);
        panel_quad(quads[used], kCanvasWidth * 0.5f - lines.half, top, 2.0f * lines.half,
                   std::min(kCanvasHeight, lines.bottom + kBelow) - top, kBoxLift);
        quads[used++].solid = 1;
    }
    if (used == 0) {
        panel_quad(quads[used++], 0.0f, 0.0f, kCanvasWidth, kCanvasHeight, 0.0f);
    }
    // On the title the window's picture has the panels (the 2D, the logo and the prompts between pages); the corners
    // are about the game's camera, which the title's own scene flies.
    for (int i = 0; s_title_world && i < used; ++i) {
        quads[i].solid |= OPENRAC_WORLD_QUAD_CAMERA | ((quads[i].solid & 1) != 0 ? OPENRAC_WORLD_QUAD_WINDOW : 0);
    }
    return used;
}

// The rectangles of this frame's 2D (or of the window's picture) that stand in the world (openrac_view_source.world_quads).
const openrac_world_quad* views_quads(void*, int* count) {
    // The counters the game keeps in the canvas's top corners, to either side of the ticker's monitor.
    constexpr float kCounterWidth = 134.0f, kCounterTop = 21.0f, kCounterHeight = 27.0f;
    static openrac_world_quad quads[OPENRAC_WORLD_QUADS_MAX];
    std::memset(quads, 0, sizeof quads);
    int used = 0;
    if (s_shopping) {
        const ShopScreen* screens = s_shop_screens[s_shop_side.load(std::memory_order_acquire)];
        for (int s = 0; s <= kShopPopup; ++s) {
            if (!screens[s].shown) {
                continue;
            }
            const ShopScreen& screen = screens[s];
            openrac_world_quad& quad = quads[used++];
            const Vec3 corners[4] = {screen.corner, screen.corner + screen.across, screen.corner + screen.down,
                                     screen.corner + screen.across + screen.down};
            for (int i = 0; i < 4; ++i) {
                quad.corner[i][0] = corners[i].x;
                quad.corner[i][1] = corners[i].y;
                quad.corner[i][2] = corners[i].z;
            }
            quad.u0 = screen.x / kCanvasWidth;
            quad.v0 = screen.y / kCanvasHeight;
            quad.u1 = (screen.x + screen.width) / kCanvasWidth;
            quad.v1 = (screen.y + screen.height) / kCanvasHeight;
            quad.opacity = 1.0f;
            quad.over = 0;
            quad.solid = 1;
        }
        head_quad(quads[used++], 0.0f, kCounterTop, kCounterWidth, kCounterHeight);
        head_quad(quads[used++], kCanvasWidth - kCounterWidth, kCounterTop, kCounterWidth, kCounterHeight);
    } else if (s_world_menu || s_title_world) {
        used = menu_quads(quads);
    } else if (const WeaponScreen& screen = weapon_screen(); screen.shown) {
        // The Visibomb's screen: the window's picture whole, the missile's camera.
        openrac_world_quad& quad = quads[used++];
        for (int i = 0; i < 4; ++i) {
            quad.corner[i][0] = screen.corner[i].x;
            quad.corner[i][1] = screen.corner[i].y;
            quad.corner[i][2] = screen.corner[i].z;
        }
        quad.u0 = 0.0f;
        quad.v0 = 0.0f;
        quad.u1 = 1.0f;
        quad.v1 = 1.0f;
        quad.opacity = 1.0f;
        quad.over = 0;
        quad.solid = 1 | OPENRAC_WORLD_QUAD_WINDOW;
    }
    *count = used;
    return quads;
}

// ---- The pause menu's VR Options ----
//
// Rerac-vr's page, on the game's own pause menu (openrac_api.ui_pause_item): the rows it can change at
// once, kept beside the status file.

std::string s_vr_file;
openrac::UiPage s_vr_page;

enum VrRow { kRowHelmet, kRowBoots, kRowFlip, kRowDirection, kRowBody, kRowAkimbo, kRowPosition, kRowView };

// The pause menu's page of the mod: the port's window of eight rows, in the list's box, all of VR Options
// at once. ui_cursor only reads there, so the hand writes the row the game's cursor is on, as the d-pad's
// bits end up doing; the port's input then clamps it, and on a page longer than the window moves it. The
// game's own list on the title's page is pointed at as any other list.
void point_at_vr_rows(gaddr page, float cx, float cy) {
    constexpr int kListSlot = 3, kRowsShown = 8;
    constexpr int kSettleFrames = 8;        // the port's tick takes a write up, and the window with it
    constexpr uint32_t kRowCursor = 0x38u;  // the rows' list: the row under the cursor
    constexpr int kTitleRowTop = 4, kTitleRowStep = 20;  // the title's text list, as drawn: canvas pixels
    const gaddr list = ref<gaddr>(page + kPageScreens + 4u * static_cast<uint32_t>(kListSlot));
    static int settling = 0;
    if (settling > 0) {
        --settling;
        return;
    }
    const int item = s_vr_page.cursor();
    if (item < 0 || !readable(list, 0x58u)) {
        return;
    }
    const int x = ref<int32_t>(list + kWidgetRect), y = ref<int32_t>(list + kWidgetRect + 4u);
    const int width = ref<int32_t>(list + kWidgetRect + 8u), height = ref<int32_t>(list + kWidgetRect + 12u);
    if (width <= 8 || height <= 8 || cx < static_cast<float>(x) || cx >= static_cast<float>(x + width)
        || cy < static_cast<float>(y) || cy >= static_cast<float>(y + height)) {
        return;
    }
    // On the title the page's rows are a text list of the port's, with no update of its own: its cursor is the list's,
    // which the port takes up for the page, its rows the list's own spacing.
    if (ref<gaddr>(list) == 0) {
        static int told = -1;
        const int rows = kRowsShown;
        const int top = ref<int32_t>(list + kListScroll);
        const int step = kTitleRowStep;
        const int row = std::clamp((static_cast<int>(cy) - y - kTitleRowTop + top) / step, 0, rows - 1);
        if (row != ref<int32_t>(list + kListCursor)) {
            ref<int32_t>(list + kListCursor) = row;
            settling = kSettleFrames;
        }
        if (row != told) {
            told = row;
            char line[100];
            std::snprintf(line, sizeof line, "[vr-pointer] VR Options on the title: row %d (canvas y %.0f, box %d %d)", row, cy, y, height);
            log_line(1, line);
        }
        return;
    }
    const int cursor = ref<int32_t>(list + kRowCursor);
    const int row = std::clamp(window_row_at(kRowsShown, static_cast<float>(height), cy - static_cast<float>(y)), 0, kRowsShown - 1);
    if (row != cursor) {
        ref<int32_t>(list + kRowCursor) = row;
        settling = kSettleFrames;
        log_line(1, ("[vr-pointer] VR Options: row " + std::to_string(row) + " of the window at item " + std::to_string(item - cursor)).c_str());
    }
}

void show_vr_page() {
    openrac::UiContent content;
    content.title = "VR Options";
    content.items = {
        openrac::UiItem::choice_of("Helmet", {"On", "Off"}, s_vr.helmet ? 0u : 1u),
        openrac::UiItem::choice_of("Boots", {"On", "Off"}, s_vr.boots ? 0u : 1u),
        openrac::UiItem::choice_of("Camera flip", {"Off", "On"}, s_vr.camera_flip ? 1u : 0u),
        openrac::UiItem::choice_of("Direction", {"Body", "Gaze"}, s_vr.direction_body ? 0u : 1u),
        openrac::UiItem::choice_of("Player body", {"Off", "Cutscenes", "Always"}, static_cast<std::uint32_t>(s_vr.body)),
        openrac::UiItem::choice_of("Akimbo", {"Off", "On"}, s_vr.akimbo ? 1u : 0u),
        openrac::UiItem::choice_of("Play position", {"Standing", "Seated"}, s_vr.seated ? 1u : 0u),
        openrac::UiItem::choice_of("View", {"First person", "Third person"}, s_options.first_person ? 0u : 1u),
    };
    s_vr_page.set(content);
}

// The launcher's View as it was when the row last saved: changed there since, the launcher's wins.
bool s_launcher_first = true;

void save_vr_options() {
    if (s_vr_file.empty()) {
        return;
    }
    std::ofstream file(s_vr_file);
    file << "helmet " << (s_vr.helmet ? 1 : 0) << "\nboots " << (s_vr.boots ? 1 : 0) << "\ncamera_flip "
         << (s_vr.camera_flip ? 1 : 0) << "\ndirection_body " << (s_vr.direction_body ? 1 : 0) << "\nbody " << s_vr.body
         << "\nakimbo " << (s_vr.akimbo ? 1 : 0) << "\nseated " << (s_vr.seated ? 1 : 0) << "\nfirst_person "
         << (s_options.first_person ? 1 : 0) << "\nlauncher_first " << (s_launcher_first ? 1 : 0) << "\n";
}

void load_vr_options() {
    std::ifstream file(s_vr_file);
    std::string key;
    int value = 0;
    int first_person = -1, launcher_first = -1;
    while (file >> key >> value) {
        if (key == "first_person") {
            first_person = value;
        } else if (key == "launcher_first") {
            launcher_first = value;
        } else if (key == "helmet") {
            s_vr.helmet = value != 0;
        } else if (key == "boots") {
            s_vr.boots = value != 0;
        } else if (key == "camera_flip") {
            s_vr.camera_flip = value != 0;
        } else if (key == "body") {
            s_vr.body = std::clamp(value, 0, 2);
        } else if (key == "akimbo") {
            s_vr.akimbo = value != 0;
        } else if (key == "direction_body") {
            s_vr.direction_body = value != 0;
        } else if (key == "seated") {
            s_vr.seated = value != 0;
        }
    }
    if (first_person >= 0 && launcher_first == (s_launcher_first ? 1 : 0)) {
        s_options.first_person = first_person != 0;
    }
}

void vr_page_event(const openrac::UiEvent& event) {
    if (event.kind() == OPENRAC_UI_SHOW) {
        show_vr_page();
    } else if (event.kind() == OPENRAC_UI_CHANGE) {
        if (event.item() == kRowHelmet) {
            s_vr.helmet = event.value() == 0;
        } else if (event.item() == kRowBoots) {
            s_vr.boots = event.value() == 0;
        } else if (event.item() == kRowFlip) {
            s_vr.camera_flip = event.value() == 1;
        } else if (event.item() == kRowBody) {
            s_vr.body = event.value();
        } else if (event.item() == kRowAkimbo) {
            s_vr.akimbo = event.value() == 1;
        } else if (event.item() == kRowDirection) {
            s_vr.direction_body = event.value() == 0;
        } else if (event.item() == kRowView) {
            s_options.first_person = event.value() == 0;
            s_recentre_wanted = true;
            log_line(1, s_options.first_person ? "[vr-rig] the view is first person now" : "[vr-rig] the view is third person now");
        } else if (event.item() == kRowPosition) {
            const bool seated = event.value() == 1;
            if (seated != s_vr.seated) {
                s_rig.origin.y += seated ? -kSeatedLift : kSeatedLift;
                s_vr.seated = seated;
            }
        }
        save_vr_options();
        show_vr_page();
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

    s_options.first_person = mod.setting<std::string>("view") == "first";
    s_launcher_first = s_options.first_person;
    s_options.screen = mod.setting<std::string>("view") == "screen";
    s_options.scenes_in_vr = mod.setting<std::string>("cutscenes") != "screen";
    s_options.scene_eyes = mod.setting<std::string>("cutscenes") == "eyes";
    s_options.turn.smooth = mod.setting<std::string>("turn") == "smooth";
    s_options.turn.snap_angle = static_cast<float>(mod.setting<double>("snap_degrees")) * kPi / 180.0f;
    s_options.turn.smooth_rate = static_cast<float>(mod.setting<double>("smooth_degrees")) * kPi / 180.0f;
    s_options.world_scale = static_cast<float>(mod.setting<double>("world_scale"));
    s_options.hud_scale = static_cast<float>(mod.setting<double>("hud_scale"));
    s_options.haptics = mod.setting<bool>("haptics");
    s_options.hand_aim = mod.setting<bool>("hand_aim");
    s_options.hide_body = mod.setting<bool>("hide_body");
    s_options.hand_weapon = mod.setting<bool>("hand_weapon");
    s_options.throw_by_hand = mod.setting<bool>("throw_by_hand");
    s_options.swing_wrench = mod.setting<bool>("swing_wrench");
    s_options.two_handed = mod.setting<bool>("two_handed");
    s_options.glyphs = mod.setting<bool>("glyphs");
    s_options.show_hands = mod.setting<bool>("show_hands");
    s_options.grab_ledges = mod.setting<bool>("grab_ledges");
    s_options.puzzle_over_tool = mod.setting<bool>("puzzle_over_tool");
    s_options.shop_on_foot = mod.setting<bool>("shop_on_foot");
    s_options.menu_in_world = mod.setting<bool>("menu_in_world");
    s_options.room_scale = mod.setting<bool>("room_scale");
    s_options.swim_strokes = mod.setting<bool>("swim_strokes");
    s_options.solid_hands = mod.setting<bool>("solid_hands");
    s_xr.refresh_hz = static_cast<float>(mod.setting<std::int64_t>("refresh_hz"));
    s_xr.resolution = static_cast<float>(mod.setting<double>("resolution"));
    s_rig.scale = s_options.world_scale;

    if (const char* status = std::getenv("QUEST_VR_STATUS")) {
        s_status_file = status;
        const std::size_t slash = s_status_file.find_last_of("/\\");
        s_vr_file = (slash == std::string::npos ? std::string() : s_status_file.substr(0, slash + 1)) + "vr-options.txt";
        load_vr_options();
    }
    if (mod.has_ui() && mod.has_pause_items()) {
        s_vr_page = mod.ui_page([](const openrac::UiPage&, const openrac::UiEvent& event) { vr_page_event(event); });
        mod.ui_pause_item("VR Options", s_vr_page);
        mod.ui_main_item(OPENRAC_UI_OPTIONS, "VR Options", s_vr_page);
    }

    mod.on_frame([] { on_tick(); });
    // The Quest fork's entry (patches/): a port without it has a shorter table, so it is looked
    // for as the SDK looks for its newer entries, and the weapon stays where the game puts it.
    const openrac_api& api = mod.api();
    if (!s_options.screen && api.size >= offsetof(openrac_api, quiet_new_game) + sizeof(void*) && api.quiet_new_game != nullptr) {
        api.quiet_new_game(1);
    }
    if (api.size >= offsetof(openrac_api, on_before_capture) + sizeof(void*) && api.on_before_capture != nullptr
        && api.on_before_capture(mod.handle(), [](void*) {
               sample_hero();
               hide_body();
               hold_roll();
               change_items();
               place_weapon();
               place_weapon_screen();
               place_left_gun();
               place_arms();
               place_helmet();
               face_sprites();
           }, nullptr) == 0) {
        s_capture_hook = true;
        log_line(1, "weapon in the hand: placed before each frame's capture");
    } else {
        log_line(2, "this game build lacks the Quest patch's on_before_capture: the weapon stays in "
                    "the game's hand, not yours");
    }
    s_arms_api = api.size >= offsetof(openrac_api, moby_arms) + sizeof(void*) && api.moby_arms != nullptr;
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

    // Every hand item's update but the wrench's and the Walloper's (their swing and punch are the
    // hand's own): aimed by the hand (aimed_update). A function some classes share is filtered there,
    // by the moby in Ratchet's hand.
    aim_hook<rac1::Moby_Blaster_Update>(mod);
    aim_hook<rac1::Moby_Devastator_Update>(mod);
    aim_hook<rac1::Moby_TeslaClaw_Update>(mod);
    aim_hook<rac1::Moby_Swingshot_Update>(mod);
    aim_hook<rac1::Moby_Pyrocitor_Update>(mod);
    aim_hook<rac1::Moby_Ryno_Update>(mod);
    aim_hook<rac1::Moby_Suckcannon_Update>(mod);
    aim_hook<rac1::Moby_Taunter_Update>(mod);
    aim_hook<rac1::Moby_Morphoray_Update>(mod);
    aim_hook<rac1::Moby_Visibomb_Update>(mod);
    mod.hook<rac1::Moby_Wrench_Update>([](auto original, uint32_t moby) { wrench_update(original, moby); });
    mod.hook<rac1::Moby_BoltCrank_Update>([](auto original, uint32_t moby) { bolt_update(original, moby); });
    glove_hook<rac1::Moby_BombGlove_Update>(mod);
    glove_hook<rac1::Moby_MineGlove_Update>(mod);
    glove_hook<rac1::Moby_DecoyGlove_Update>(mod);
    glove_hook<rac1::Moby_GloveOfDoom_Update>(mod);
    held_projectile_hook<rac1::Moby_Class121_Update>(mod);  // the bomb
    held_projectile_hook<rac1::Moby_Mine_Update>(mod);
    held_projectile_hook<rac1::Moby_Decoy_Update>(mod);
    held_projectile_hook<rac1::Moby_Bomb_Update>(mod);      // the Doom canister
    mod.hook<rac1::Vendor_MenuUpdate>([](auto original) { vendor_update(original); });
    mod.hook<rac1::Vendor_Open>([](auto original, uint32_t moby) { vendor_opened(original, moby); });
    mod.hook<rac1::Vendor_Close>([](auto original, int32_t after_scene) { vendor_closed(original, after_scene); });
    mod.hook<rac1::Menu_Backdrop>([](auto original, int32_t source, int32_t target, int32_t bytes) { menu_backdrop(original, source, target, bytes); });
    mod.hook<rac1::Ui_Frame>([](auto original, int32_t top, int32_t bottom, int32_t left, int32_t right, int32_t alpha) {
        s_box_frame = {top, bottom, left, right, s_tick};
        original(top, bottom, left, right, alpha);
    });
    mod.hook<rac1::Font_PrintCenter>([](auto original, int32_t x, int32_t y, int32_t color, int32_t text, int32_t length) {
        return print_center(original, x, y, color, text, length);
    });
    mod.hook<rac1::Moby_Hydrodisplacer_Update>([](auto original, uint32_t moby) { hydro_update(original, moby); });
    mod.hook<rac1::Moby_TresspasserPad_Update>([](auto original, uint32_t moby) { lock_update(original, moby); });
    aim_hook<rac1::Moby_Trespasser_Update>(mod);
    aim_hook<rac1::Moby_MetalDetector_Update>(mod);
    // Eight levels' ledge probe, and the other eleven's.
    mod.hook<rac1::Moby_JetFighter_Update>([](auto original, uint32_t moby) { ship_update(original, moby); });
    mod.hook<rac1::Moby_FighterJet69_Update>([](auto original, uint32_t moby) { ship_update(original, moby); });
    mod.hook<rac1::Moby_FighterJet1379_Update>([](auto original, uint32_t moby) { ship_update(original, moby); });
    mod.hook<rac1::Moby_VisibombMissile_Update>([](auto original, uint32_t moby) { missile_update(original, moby); });
    mod.hook<rac1::Weapon_TakeAmmo>([](auto original, int32_t item, int32_t count) { return take_ammo(original, item, count); });
    mod.hook<rac1::Hud_QuickSelectUpdate>([](auto original, uint32_t ring) { ring_update(original, ring); });
    mod.hook<rac1::Hud_QuickSelectDraw>([](auto original, uint32_t ring) { return ring_draw(original, ring); });
    mod.hook<rac1::Hud_Frame>([](auto original, uint32_t element, int32_t id, int32_t x, int32_t y, int32_t flags, int32_t alpha) {
        hud_frame(original, element, id, x, y, flags, alpha);
    });
    mod.hook<rac1::Hero_LocalPoint>([](auto original, uint32_t out, float x, float y, float z) { local_point(original, out, x, y, z); });
    mod.hook<rac1::Hud_OrdersRingUpdate>([](auto original, uint32_t ring) { ring_by_hand([&] { original(ring); }); });
    mod.hook<rac1::Sound_Update>([](auto original) { return sound_update(original); });
    mod.hook<rac1::Hero_CheckLedgeGrab>([](auto original) { check_ledge(original); });
    mod.hook<rac1::Hero_CheckLedgeGrabB>([](auto original) { check_ledge(original); });
    turret_hook<TurretKind::kGaspar, rac1::Moby_Turret_Update>(mod);
    turret_hook<TurretKind::kHoven, rac1::Moby_HovenTurret_Update>(mod);
    turret_hook<TurretKind::kBatalia, rac1::Moby_Class440_Update>(mod);
    mod.hook<rac1::Blaster_SelectAutoAimTarget>(
        [](auto original, int32_t blaster, uint32_t from, uint32_t yaw, uint32_t pitch) {
            return blaster_auto_aim(original, blaster, from, yaw, pitch);
        }
    );

    openrac_view_source source{};
    source.size = sizeof source;
    source.begin = views_begin;
    source.end = views_end;
    source.closing = views_closing;
    source.world_quads = views_quads;
    mod.on_views(source);

    mod.info(std::string("Quest VR on the ") + std::string(mod.version()) + " disc");
}

}  // namespace

OPENRAC_MOD_CPP("quest-vr", "0.1.2", load)
