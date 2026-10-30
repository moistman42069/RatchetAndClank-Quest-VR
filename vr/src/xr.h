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
    float resolution = 1.0f;     // of the headset's recommended eye size
    int panel_width = 1024;
    int panel_height = 768;
};

struct FrameInfo {
    bool should_render = false;  // the headset will show this frame
    bool focused = false;        // the app has input (the Quest's system menu takes it away)
    bool views_valid = false;
    std::int64_t display_time = 0;
    double frame_seconds = 1.0 / 72.0;
    Pose eye[2];
    float fov[2][4]{};           // left, right, up, down, radians
    Pose head;
    Pose hand_grip[2];
    Pose hand_aim[2];
    bool hand_valid[2]{};
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
    virtual void finish_frame(bool eyes, const PanelLayer& panel) = 0;

    virtual void haptic(Hand hand, float strength, float seconds, float hertz) = 0;

    // The system ended the app's session for good: leave.
    virtual bool wants_exit() const = 0;

    virtual void stop() = 0;
};

// The backend this build has.
Headset* make_headset(void (*log)(int level, const char* text));

}  // namespace quest
