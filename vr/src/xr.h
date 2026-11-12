// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 the Ratchet & Clank Quest VR contributors
//
// The headset, as the mod sees it. Two backends share this: xr_openxr.cpp (OpenXR on the Quest) and
// xr_sim.cpp (scripted poses and PNGs on a desktop, for testing against the real game).
// Everything here runs on the game's thread, with its GL context current.

#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "pad.h"
#include "rig.h"

namespace quest {

enum Hand { kLeft = 0, kRight = 1 };

// The swapchains: one for each eye and the panel the 2D goes on.
enum Target { kTargetLeftEye = 0, kTargetRightEye = 1, kTargetPanel = 2, kTargetCount = 3 };

struct XrSettings {
    float refresh_hz = 0;        // 0: the headset's own
    bool keep_refresh = true;    // stay at it where it is not held; false: a step down (to 90, then 72) after two slow 10 s spells
    float resolution = 1.0f;     // of the headset's recommended eye size
    int panel_width = 1024;
    int panel_height = 768;
    // Smoother frames (Application SpaceWarp, XR_FB_space_warp, from 0.1.17): the game draws every other frame and the
    // headset makes the ones between from the last and how its pixels moved. Where the runtime has it (the Quest's).
    bool space_warp = true;
    // CPU and GPU boost (0.1.18, the owner's ask for Quest Games Optimizer's highest speeds): the processors at the
    // highest level the runtime lets an app ask for (XR_EXT_performance_settings' boost) rather than sustained high. Tools
    // such as Quest Games Optimizer go further through the developer shell, which no app can do itself.
    bool cpu_gpu_boost = true;
    // Foveated rendering (0.1.19, the owner's ask): each eye's picture drawn at a lower resolution towards its edges,
    // where the lenses blur it anyway (GL_QCOM_texture_foveated, the Quest's graphics chip). 0 off, 1 low, 2 medium,
    // 3 high.
    int foveation = 2;
};

// Smoother frames: an eye's motion and depth images to draw this frame (GL textures of the game's context).
struct MotionTarget {
    unsigned motion = 0;  // GL_RGBA16F: each pixel's movement since the last frame, in NDC
    unsigned depth = 0;   // GL_DEPTH24_STENCIL8: that pass's depth, 1 near and 0 far
    int width = 0;
    int height = 0;
};

// What the frame's motion pass drew, for the finish_frame after it.
struct SpaceWarpFrame {
    bool drawn[2]{};        // each eye's motion and depth were drawn this frame, from the frame before
    float depth_zero = 0;   // the distance in metres that depth 0 stands for (the far plane)
    float depth_one = 0;    // and depth 1 (the near plane)
};

struct FrameInfo {
    bool should_render = false;  // the headset will show this frame
    bool recentred = false;      // the runtime moved the room (the headset's own recentre, its button held)
    bool focused = false;        // the app has input (the Quest's system menu takes it away)
    bool views_valid = false;
    std::int64_t display_time = 0;
    double frame_seconds = 1.0 / 72.0;
    Pose eye[2];
    float fov[2][4]{};           // left, right, up, down, radians
    Pose head;
    float head_above_floor = -1.0f;  // over the floor the headset knows (Feet on the floor); below 0, it knows none
    Pose hand_grip[2];
    Pose hand_aim[2];
    bool hand_valid[2]{};
    bool hand_bound[2]{};        // a controller is bound to this hand, as OpenXR says
    bool stick_active[2]{};      // its stick reports: the system has given it to the app
    Controls controls;
};

// A flat picture in the world (tracking space) or following the head.
struct PanelLayer {
    bool visible = false;
    bool follows_head = false;
    Pose pose;                   // in tracking space, or the head's when follows_head
    float width = 1, height = 1; // metres
    bool blend = false;          // uses the picture's alpha
};

// The menu pointer's beam from the hand and the dot where it lands, in tracking space; a second pair for the other hand.
struct PointerLayers {
    bool beam = false;
    Pose beam_pose;
    float beam_length = 0;
    bool dot = false;
    Pose dot_pose;
    // The other hand's (0.1.19): with Akimbo both weapon wheels can be up at once, each hand pointing at its own.
    bool beam2 = false;
    Pose beam2_pose;
    float beam2_length = 0;
    bool dot2 = false;
    Pose dot2_pose;
};

class Headset {
public:
    virtual ~Headset() = default;

    // Starts with the GL context current. False: `why` says what is missing.
    virtual bool start(const XrSettings& settings, std::string& why) = 0;

    // Waits for the headset's next frame. False: no frame this time (the session is not running):
    // the caller draws nothing and calls neither finish_frame nor anything below.
    virtual bool begin_frame(FrameInfo& out) = 0;

    // The size of a target, and a GL texture for this frame's picture of it (0: none to draw to).
    virtual void target_size(Target target, int& width, int& height) const = 0;
    virtual unsigned acquire(Target target) = 0;

    // Ends the frame begin_frame began. `eyes`: the eyes were drawn (with the FrameInfo's poses).
    // `eyes_kept`: the eyes' pictures are the last drawn, not this frame's: shown from the poses they were drawn
    // from, so that the compositor holds the world still under the head.
    virtual void finish_frame(bool eyes, bool eyes_kept, const PanelLayer& panel) = 0;

    // Smoother frames: eye `eye`'s motion images for this frame, taken with it; false for none (the headset makes no
    // frames between, or it is off).
    virtual bool motion_target(int eye, MotionTarget& out) {
        (void)eye;
        (void)out;
        return false;
    }

    // What this frame's motion pass drew, told before its finish_frame; not told: nothing.
    virtual void space_warp(const SpaceWarpFrame& warp) { (void)warp; }

    // In how many nanoseconds the next frame begin_frame begins is predicted to be shown; 0: it cannot say.
    virtual std::int64_t next_display_in() { return 0; }

    // Foveated rendering: `texture` is the GL texture the port draws eye `eye`'s picture into, before it is copied to the
    // eye's target; drawn foveated from the next frame, as XrSettings::foveation says. Called each frame the eye is drawn.
    virtual void foveate(int eye, unsigned texture) {
        (void)eye;
        (void)texture;
    }

    virtual void haptic(Hand hand, float strength, float seconds, float hertz) = 0;

    // A line of words ahead of the head until it is cleared with null or "" (notice.h).
    virtual void show_notice(const char* text) = 0;
    // How much of the notice shows, 1 (all, as show_notice leaves it) to 0, for its fade (0.1.20).
    virtual void notice_alpha(float alpha) { (void)alpha; }

    // Drawn over the panel in the next finish_frame.
    virtual void show_pointer(const PointerLayers& pointer) = 0;

    // The system ended the app's session for good: leave.
    virtual bool wants_exit() const = 0;

    virtual void stop() = 0;
};

// The backend this build has.
Headset* make_headset(void (*log)(int level, const char* text));

}  // namespace quest
