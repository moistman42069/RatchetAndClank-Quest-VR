// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 the Ratchet & Clank Quest VR contributors
//
// A headset that is not there, for a desktop: the real game runs and the whole mod with it, and a
// script stands for the player. Nothing of this goes in the APK.
//
//   QUEST_SIM_SCRIPT=<file>  a line each: <frame> <name> <value>, values holding until changed.
//       a b x y left_click right_click: 0 or 1; left_x left_y right_x right_y left_trigger
//       right_trigger left_grip right_grip: -1..1; left_first_touch right_first_touch left_thumb_touch
//       right_thumb_touch left_rest_touch: 0 or 1; head_yaw hand_yaw (degrees, left positive);
//       head_pitch hand_pitch hand_roll (degrees, up positive); head_x head_y head_z (metres); shot <name>: the eyes and panel as
//       PNGs; quit 0: ends the run.
//   QUEST_SIM_OUT=<folder>   where the PNGs go.

#if !defined(__ANDROID__)

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

#include "notice.h"
#include "xr.h"

namespace quest {

// Set by the mod before make_headset: the port's GL.
void* (*sim_gl_proc)(const char*) = nullptr;
bool sim_left_grip = false;
// Set by the mod once the test's set-up is done (items given, Ratchet placed): QUEST_SIM_ALIGN's mark.
bool sim_ready = false;

namespace {

using GLuint = unsigned int;
using GLenum = unsigned int;
using GLint = int;
using GLsizei = int;
constexpr GLenum kTexture2D = 0x0DE1, kRgba = 0x1908, kRgba8 = 0x8058, kUnsignedByte = 0x1401;
constexpr GLenum kFramebuffer = 0x8D40, kColorAttachment0 = 0x8CE0, kLinear = 0x2601;
constexpr GLenum kMinFilter = 0x2801, kMagFilter = 0x2800;
// Smoother frames' motion and depth images, as the Quest's runtime has them.
constexpr GLenum kRgba16f = 0x881A, kFloat = 0x1406, kNearest = 0x2600;
constexpr GLenum kDepth24Stencil8 = 0x88F0, kDepthStencil = 0x84F9, kUnsignedInt248 = 0x84FA;

#if defined(_WIN32)
#define GLAPI __stdcall
#else
#define GLAPI
#endif

struct Gl {
    void(GLAPI* GenTextures)(GLsizei, GLuint*);
    void(GLAPI* BindTexture)(GLenum, GLuint);
    void(GLAPI* TexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*);
    void(GLAPI* TexParameteri)(GLenum, GLenum, GLint);
    void(GLAPI* GenFramebuffers)(GLsizei, GLuint*);
    void(GLAPI* BindFramebuffer)(GLenum, GLuint);
    void(GLAPI* FramebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint);
    void(GLAPI* ReadPixels)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*);
};

std::uint32_t crc_of(const unsigned char* data, std::size_t size, std::uint32_t crc = 0xFFFFFFFFu) {
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (int k = 0; k < 8; ++k) {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return crc;
}

void put32(std::vector<unsigned char>& out, std::uint32_t v) {
    for (int shift = 24; shift >= 0; shift -= 8) {
        out.push_back(static_cast<unsigned char>(v >> shift));
    }
}

void chunk(std::ofstream& file, const char* name, const std::vector<unsigned char>& body) {
    std::vector<unsigned char> head;
    put32(head, static_cast<std::uint32_t>(body.size()));
    file.write(reinterpret_cast<const char*>(head.data()), 4);
    std::vector<unsigned char> sum(name, name + 4);
    sum.insert(sum.end(), body.begin(), body.end());
    file.write(reinterpret_cast<const char*>(sum.data()), static_cast<std::streamsize>(sum.size()));
    std::vector<unsigned char> tail;
    put32(tail, ~crc_of(sum.data(), sum.size()));
    file.write(reinterpret_cast<const char*>(tail.data()), 4);
}

// RGBA rows, last row first (as GL reads them), as an uncompressed PNG. `alpha`: keep it.
void write_png(const std::string& path, const std::vector<unsigned char>& rgba, int width, int height, bool alpha) {
    const int channels = alpha ? 4 : 3;
    std::vector<unsigned char> raw;
    raw.reserve(static_cast<std::size_t>(height) * (static_cast<std::size_t>(width) * channels + 1));
    for (int y = height - 1; y >= 0; --y) {
        raw.push_back(0);
        const unsigned char* row = rgba.data() + static_cast<std::size_t>(y) * width * 4;
        for (int x = 0; x < width; ++x) {
            raw.insert(raw.end(), row + x * 4, row + x * 4 + channels);
        }
    }
    std::vector<unsigned char> z = {0x78, 0x01};
    std::uint32_t a = 1, b = 0;
    for (std::size_t at = 0; at < raw.size();) {
        const std::size_t size = std::min<std::size_t>(65535, raw.size() - at);
        z.push_back(at + size == raw.size() ? 1 : 0);
        z.push_back(static_cast<unsigned char>(size));
        z.push_back(static_cast<unsigned char>(size >> 8));
        z.push_back(static_cast<unsigned char>(~size));
        z.push_back(static_cast<unsigned char>(~size >> 8));
        z.insert(z.end(), raw.begin() + static_cast<std::ptrdiff_t>(at), raw.begin() + static_cast<std::ptrdiff_t>(at + size));
        for (std::size_t i = at; i < at + size; ++i) {
            a = (a + raw[i]) % 65521;
            b = (b + a) % 65521;
        }
        at += size;
    }
    put32(z, (b << 16) | a);

    std::ofstream file(path, std::ios::binary);
    const unsigned char signature[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    file.write(reinterpret_cast<const char*>(signature), 8);
    std::vector<unsigned char> header;
    put32(header, static_cast<std::uint32_t>(width));
    put32(header, static_cast<std::uint32_t>(height));
    header.push_back(8);
    header.push_back(alpha ? 6 : 2);
    header.push_back(0);
    header.push_back(0);
    header.push_back(0);
    chunk(file, "IHDR", header);
    chunk(file, "IDAT", z);
    chunk(file, "IEND", {});
}

struct Command {
    long frame;
    std::string name, value;
};

class Sim final : public Headset {
public:
    explicit Sim(void (*log)(int, const char*)) : m_log(log) {}

    bool start(const XrSettings& settings, std::string& why) override {
        const auto load = [](auto& fn, const char* name) {
            fn = reinterpret_cast<std::decay_t<decltype(fn)>>(sim_gl_proc(name));
        };
        if (sim_gl_proc == nullptr) {
            why = "the simulated headset needs the port's OpenGL";
            return false;
        }
        load(m_gl.GenTextures, "glGenTextures");
        load(m_gl.BindTexture, "glBindTexture");
        load(m_gl.TexImage2D, "glTexImage2D");
        load(m_gl.TexParameteri, "glTexParameteri");
        load(m_gl.GenFramebuffers, "glGenFramebuffers");
        load(m_gl.BindFramebuffer, "glBindFramebuffer");
        load(m_gl.FramebufferTexture2D, "glFramebufferTexture2D");
        load(m_gl.ReadPixels, "glReadPixels");
        const int eye = static_cast<int>(1000 * settings.resolution) & ~1;
        make_target(kTargetLeftEye, eye, eye);
        make_target(kTargetRightEye, eye, eye);
        make_target(kTargetPanel, settings.panel_width, settings.panel_height);
        // Smoother frames, as on the Quest when the launcher has it on: motion images a quarter of the eye's size.
        // QUEST_SIM_SPACE_WARP=0 leaves them out.
        const char* warp = std::getenv("QUEST_SIM_SPACE_WARP");
        m_space_warp = settings.space_warp && !(warp != nullptr && std::string(warp) == "0");
        if (m_space_warp) {
            m_motion_width = eye / 4;
            m_motion_height = eye / 4;
            for (int side = 0; side < 2; ++side) {
                m_gl.GenTextures(1, &m_motion[side]);
                m_gl.BindTexture(kTexture2D, m_motion[side]);
                m_gl.TexImage2D(kTexture2D, 0, static_cast<GLint>(kRgba16f), m_motion_width, m_motion_height, 0, kRgba, kFloat,
                                nullptr);
                m_gl.TexParameteri(kTexture2D, kMinFilter, static_cast<GLint>(kNearest));
                m_gl.TexParameteri(kTexture2D, kMagFilter, static_cast<GLint>(kNearest));
                m_gl.GenTextures(1, &m_motion_depth[side]);
                m_gl.BindTexture(kTexture2D, m_motion_depth[side]);
                m_gl.TexImage2D(kTexture2D, 0, static_cast<GLint>(kDepth24Stencil8), m_motion_width, m_motion_height, 0,
                                kDepthStencil, kUnsignedInt248, nullptr);
                m_gl.TexParameteri(kTexture2D, kMinFilter, static_cast<GLint>(kNearest));
                m_gl.TexParameteri(kTexture2D, kMagFilter, static_cast<GLint>(kNearest));
                m_gl.BindTexture(kTexture2D, 0);
                m_gl.GenFramebuffers(1, &m_motion_fbo[side]);
                m_gl.BindFramebuffer(kFramebuffer, m_motion_fbo[side]);
                m_gl.FramebufferTexture2D(kFramebuffer, kColorAttachment0, kTexture2D, m_motion[side], 0);
                m_gl.BindFramebuffer(kFramebuffer, 0);
            }
        }

        if (const char* path = std::getenv("QUEST_SIM_SCRIPT")) {
            std::ifstream script(path);
            std::string line;
            while (std::getline(script, line)) {
                std::istringstream words(line);
                Command command;
                if (line.empty() || line[0] == '#' || !(words >> command.frame >> command.name >> command.value)) {
                    continue;
                }
                m_script.push_back(command);
            }
        }
        if (const char* out = std::getenv("QUEST_SIM_OUT")) {
            m_out = out;
        }
        // QUEST_SIM_ALIGN=F: the script's frames from F on count from the frame the set-up was done, which is F where
        // the script was written (Novalis: about 2620), so that it runs as well on a level whose arrival gives control
        // at another, or a varying, frame (Eudora).
        if (const char* align = std::getenv("QUEST_SIM_ALIGN")) {
            m_align = std::atol(align);
        }
        // QUEST_SIM_GRIP_TILT=D: the hands' grip pose turned D degrees up from their aim pose, as the Quest's controllers
        // have it (the grip's forward up the handle); the script's hand angles stay the aim's, where the hand points.
        if (const char* tilt = std::getenv("QUEST_SIM_GRIP_TILT")) {
            m_grip_tilt = static_cast<float>(std::atof(tilt)) * kPi / 180.0f;
        }
        m_log(1, ("simulated headset: " + std::to_string(m_script.size()) + " script lines").c_str());
        return true;
    }

    bool begin_frame(FrameInfo& out) override {
        // A real runtime's frame wait paces each frame, and a movie has no other pace (movie.cpp's headset_paces). So a frame asked for
        // far too soon waits out a 60th here, which keeps a script's frame numbers in time through a movie. The game's own frames
        // come at their 60 a second and do not wait.
        using PaceClock = std::chrono::steady_clock;
        if (PaceClock::now() - m_frame_asked < std::chrono::milliseconds(10)) {
            std::this_thread::sleep_until(m_frame_asked + std::chrono::microseconds(16667));
        }
        m_frame_asked = PaceClock::now();
        out = FrameInfo{};
        ++m_frame;
        if (m_ready_frame < 0 && sim_ready) {
            m_ready_frame = m_frame;
            if (m_align >= 0) {
                m_log(1, ("[sim] the set-up done at frame " + std::to_string(m_frame) + ": frame "
                          + std::to_string(m_align) + " of the script").c_str());
            }
        }
        while (m_next < m_script.size() && due(m_script[m_next].frame)) {
            apply(m_script[m_next++]);
        }
        out.should_render = true;
        out.focused = true;
        out.views_valid = true;
        out.frame_seconds = 1.0 / 72.0;
        const Quat yaw = yaw_quat(number("head_yaw") * kPi / 180.0f);
        const float pitch = number("head_pitch") * kPi / 360.0f;
        const Quat look = yaw * Quat{std::sin(pitch), 0, 0, std::cos(pitch)};
        const Vec3 head{number("head_x"), 1.7f + number("head_y"), number("head_z")};
        out.head = {head, look};
        out.head_above_floor = head.y;
        // The Quest 3's eyes: 64 mm apart, each with about this field of view.
        const Vec3 half = rotate(look, {0.032f, 0, 0});
        out.eye[0] = {head - half, look};
        out.eye[1] = {head + half, look};
        // view_free 1: the eyes alone stand at view_x view_y view_z (metres from the head), turned view_yaw and
        // view_pitch: a look at the player's body from outside, the head that fits it staying where it is.
        if (number("view_free") != 0) {
            const float view_pitch = number("view_pitch") * kPi / 360.0f;
            const Quat view_look = yaw_quat(number("view_yaw") * kPi / 180.0f) * Quat{std::sin(view_pitch), 0, 0, std::cos(view_pitch)};
            const Vec3 view_at = head + Vec3{number("view_x"), number("view_y"), number("view_z")};
            const Vec3 view_half = rotate(view_look, {0.032f, 0, 0});
            out.eye[0] = {view_at - view_half, view_look};
            out.eye[1] = {view_at + view_half, view_look};
        }
        for (int eye = 0; eye < 2; ++eye) {
            out.fov[eye][0] = eye == 0 ? -0.95f : -0.75f;
            out.fov[eye][1] = eye == 0 ? 0.75f : 0.95f;
            out.fov[eye][2] = 0.85f;
            out.fov[eye][3] = -0.85f;
        }
        const float hand_pitch = number("hand_pitch") * kPi / 360.0f;
        const float hand_roll = number("hand_roll") * kPi / 360.0f;
        const Quat hand = yaw_quat(number("hand_yaw") * kPi / 180.0f)
                          * Quat{std::sin(hand_pitch), 0, 0, std::cos(hand_pitch)}
                          * Quat{0, 0, std::sin(hand_roll), std::cos(hand_roll)};
        for (int side = 0; side < 2; ++side) {
            out.hand_valid[side] = number(side == 0 ? "left_gone" : "right_gone") == 0;
            out.hand_bound[side] = out.hand_valid[side];
            out.stick_active[side] = out.hand_valid[side];
            const Vec3 at = head + rotate(look, {side == 0 ? -0.2f : 0.2f, -0.35f, -0.3f});
            out.hand_aim[side] = {at + Vec3{number(side == 0 ? "lh_x" : "rh_x"), number(side == 0 ? "lh_y" : "rh_y"),
                                            number(side == 0 ? "lh_z" : "rh_z")},
                                  hand};
            out.hand_grip[side] = {out.hand_aim[side].position,
                                   hand * Quat{std::sin(m_grip_tilt * 0.5f), 0, 0, std::cos(m_grip_tilt * 0.5f)}};
        }
        Controls& c = out.controls;
        c.a = number("a") != 0;
        c.b = number("b") != 0;
        c.x = number("x") != 0;
        c.y = number("y") != 0;
        c.menu = number("menu") != 0;
        c.left_click = number("left_click") != 0;
        c.right_click = number("right_click") != 0;
        c.left_x = number("left_x");
        c.left_y = number("left_y");
        c.right_x = number("right_x");
        c.right_y = number("right_y");
        c.left_trigger = number("left_trigger");
        c.right_trigger = number("right_trigger");
        c.left_grip = sim_left_grip ? 1.0f : number("left_grip");
        c.right_grip = number("right_grip");
        c.left_first_touch = number("left_first_touch") != 0;
        c.right_first_touch = number("right_first_touch") != 0;
        c.left_thumb_touch = number("left_thumb_touch") != 0;
        c.right_thumb_touch = number("right_thumb_touch") != 0;
        c.left_rest_touch = number("left_rest_touch") != 0;
        return true;
    }

    void target_size(Target target, int& width, int& height) const override {
        width = m_width[target];
        height = m_height[target];
    }

    unsigned acquire(Target target) override { return m_texture[target]; }

    bool motion_target(int eye, MotionTarget& out) override {
        if (!m_space_warp || eye < 0 || eye > 1) {
            return false;
        }
        out = {m_motion[eye], m_motion_depth[eye], m_motion_width, m_motion_height};
        return true;
    }

    void space_warp(const SpaceWarpFrame& warp) override {
        m_warp = warp;
        m_warp_set = true;
        m_warp_frames += warp.drawn[0] && warp.drawn[1] ? 1 : 0;
        ++m_warp_asked;
    }

    void finish_frame(bool eyes, bool eyes_kept, const PanelLayer& panel) override {
        char line[200];
        const bool warped = m_warp_set && eyes && !eyes_kept && m_warp.drawn[0] && m_warp.drawn[1];
        m_warp_set = false;
        if (m_frame % 300 == 0) {
            std::snprintf(line, sizeof line, "[sim] frame %ld eyes %d panel %d (hud %d)", m_frame, eyes ? 1 : 0,
                          panel.visible ? 1 : 0, panel.follows_head ? 1 : 0);
            m_log(1, line);
            if (m_space_warp) {
                std::snprintf(line, sizeof line, "[sim] smoother frames: motion given %ld of %ld frames asked (depth %.1f to %.3f m)",
                              m_warp_frames, m_warp_asked, m_warp.depth_zero, m_warp.depth_one);
                m_log(1, line);
                m_warp_frames = m_warp_asked = 0;
            }
        }
        if (!m_shot.empty()) {
            const std::string base = m_out + "/" + m_shot;
            if (eyes) {
                save(kTargetLeftEye, base + "-left.png", false);
                save(kTargetRightEye, base + "-right.png", false);
            }
            if (m_space_warp) {
                save_motion(base + "-motion.png", warped);
            }
            if (panel.visible) {
                save(kTargetPanel, base + "-panel.png", panel.blend);
            }
            std::snprintf(line, sizeof line, "[sim] shot %s: eyes %d panel %d blend %d", m_shot.c_str(),
                          eyes ? 1 : 0, panel.visible ? 1 : 0, panel.blend ? 1 : 0);
            m_log(1, line);
            m_shot.clear();
        }
    }

    void haptic(Hand hand, float strength, float seconds, float) override {
        char line[80];
        std::snprintf(line, sizeof line, "[sim] haptic hand %d %.2f for %.3f", static_cast<int>(hand), strength, seconds);
        m_log(1, line);
    }

    void show_pointer(const PointerLayers& pointer) override {
        if (pointer.dot != m_pointer_dot || pointer.beam != m_pointer_beam || pointer.dot2 != m_pointer_dot2 || pointer.beam2 != m_pointer_beam2) {
            m_pointer_dot = pointer.dot;
            m_pointer_beam = pointer.beam;
            m_pointer_dot2 = pointer.dot2;
            m_pointer_beam2 = pointer.beam2;
            char line[200];
            std::snprintf(line, sizeof line, "[sim] pointer: beam %d (%.2f m) dot %d at (%.2f %.2f %.2f); second beam %d dot %d at (%.2f %.2f %.2f)",
                          pointer.beam ? 1 : 0, pointer.beam_length, pointer.dot ? 1 : 0, pointer.dot_pose.position.x, pointer.dot_pose.position.y,
                          pointer.dot_pose.position.z, pointer.beam2 ? 1 : 0, pointer.dot2 ? 1 : 0, pointer.dot2_pose.position.x,
                          pointer.dot2_pose.position.y, pointer.dot2_pose.position.z);
            m_log(1, line);
        }
    }

    void show_notice(const char* text) override {
        if (text != nullptr && text[0] != 0) {
            write_png(m_out + "/notice.png", render_notice(text), kNoticeWidth, kNoticeHeight, false);
        }
    }

    bool wants_exit() const override { return m_quit; }
    void stop() override {}

private:
    // Whether the script's line for `frame` is due (QUEST_SIM_ALIGN above).
    bool due(long frame) const {
        if (m_align < 0 || frame < m_align) {
            return frame <= m_frame;
        }
        return m_ready_frame >= 0 && m_ready_frame + (frame - m_align) <= m_frame;
    }

    float number(const std::string& name) const {
        const auto found = m_values.find(name);
        return found == m_values.end() ? 0.0f : found->second;
    }

    void apply(const Command& command) {
        if (command.name == "shot") {
            m_shot = command.value;
        } else if (command.name == "quit") {
            m_quit = true;
        } else {
            m_values[command.name] = std::stof(command.value);
        }
    }

    void make_target(int target, int width, int height) {
        m_width[target] = width;
        m_height[target] = height;
        m_gl.GenTextures(1, &m_texture[target]);
        m_gl.BindTexture(kTexture2D, m_texture[target]);
        m_gl.TexImage2D(kTexture2D, 0, static_cast<GLint>(kRgba8), width, height, 0, kRgba, kUnsignedByte, nullptr);
        m_gl.TexParameteri(kTexture2D, kMinFilter, static_cast<GLint>(kLinear));
        m_gl.TexParameteri(kTexture2D, kMagFilter, static_cast<GLint>(kLinear));
        m_gl.BindTexture(kTexture2D, 0);
        m_gl.GenFramebuffers(1, &m_fbo[target]);
        m_gl.BindFramebuffer(kFramebuffer, m_fbo[target]);
        m_gl.FramebufferTexture2D(kFramebuffer, kColorAttachment0, kTexture2D, m_texture[target], 0);
        m_gl.BindFramebuffer(kFramebuffer, 0);
    }

    // The left eye's motion as a picture (red right, green up, from grey; full at 0.05 of the picture a frame, a twentieth
    // of its half-width), and its numbers in the log: the mean movement across and up, how much of it moves, the most.
    void save_motion(const std::string& path, bool warped) {
        const int w = m_motion_width, h = m_motion_height;
        std::vector<float> motion(static_cast<std::size_t>(w) * h * 4);
        m_gl.BindFramebuffer(kFramebuffer, m_motion_fbo[0]);
        m_gl.ReadPixels(0, 0, w, h, kRgba, kFloat, motion.data());
        m_gl.BindFramebuffer(kFramebuffer, 0);
        std::vector<unsigned char> picture(static_cast<std::size_t>(w) * h * 4);
        double sum_x = 0, sum_y = 0, sum_z = 0, most = 0;
        long moving = 0;
        const auto byte = [](float v) { return static_cast<unsigned char>(std::clamp(128.0f + v * 2560.0f, 0.0f, 255.0f)); };
        for (std::size_t i = 0; i < static_cast<std::size_t>(w) * h; ++i) {
            const float x = motion[i * 4], y = motion[i * 4 + 1], z = motion[i * 4 + 2];
            sum_x += x;
            sum_y += y;
            sum_z += z;
            const double size = std::sqrt(static_cast<double>(x) * x + static_cast<double>(y) * y);
            most = std::max(most, size);
            moving += size > 0.002 ? 1 : 0;
            picture[i * 4] = byte(x);
            picture[i * 4 + 1] = byte(y);
            picture[i * 4 + 2] = byte(z * 10.0f);
            picture[i * 4 + 3] = 255;
        }
        write_png(path, picture, w, h, false);
        const double count = static_cast<double>(w) * h;
        char line[240];
        std::snprintf(line, sizeof line,
                      "[sim] motion %s: given %d, mean across %.4f up %.4f depth %.5f, moving %.0f%%, most %.4f (of NDC, a frame)",
                      m_shot.c_str(), warped ? 1 : 0, sum_x / count, sum_y / count, sum_z / count, 100.0 * moving / count, most);
        m_log(1, line);
    }

    void save(int target, const std::string& path, bool alpha) {
        std::vector<unsigned char> pixels(static_cast<std::size_t>(m_width[target]) * m_height[target] * 4);
        m_gl.BindFramebuffer(kFramebuffer, m_fbo[target]);
        m_gl.ReadPixels(0, 0, m_width[target], m_height[target], kRgba, kUnsignedByte, pixels.data());
        m_gl.BindFramebuffer(kFramebuffer, 0);
        write_png(path, pixels, m_width[target], m_height[target], alpha);
    }

    void (*m_log)(int, const char*);
    Gl m_gl{};
    GLuint m_texture[kTargetCount]{};
    GLuint m_fbo[kTargetCount]{};
    int m_width[kTargetCount]{};
    int m_height[kTargetCount]{};
    std::vector<Command> m_script;
    std::size_t m_next = 0;
    std::map<std::string, float> m_values;
    std::string m_out = ".";
    std::string m_shot;
    long m_frame = 0;
    long m_align = -1;        // QUEST_SIM_ALIGN, -1 without
    float m_grip_tilt = 0;    // QUEST_SIM_GRIP_TILT, radians
    std::chrono::steady_clock::time_point m_frame_asked{};  // when begin_frame last ran
    long m_ready_frame = -1;  // the frame the set-up was done
    bool m_pointer_dot = false, m_pointer_beam = false, m_pointer_dot2 = false, m_pointer_beam2 = false;
    bool m_quit = false;
    // Smoother frames.
    bool m_space_warp = false;
    int m_motion_width = 0, m_motion_height = 0;
    GLuint m_motion[2]{}, m_motion_depth[2]{}, m_motion_fbo[2]{};
    SpaceWarpFrame m_warp;
    bool m_warp_set = false;
    long m_warp_frames = 0, m_warp_asked = 0;
};

}  // namespace

#if defined(_WIN32)
Headset* make_openxr_headset(void (*log)(int, const char*));
#endif

// A test (QUEST_SIM_SCRIPT, or QUEST_SIM=1 to stand still) gets this headset; on Windows a player's game gets
// the real one through OpenXR (xr_openxr.cpp, the PC release's).
Headset* make_headset(void (*log)(int, const char*)) {
#if defined(_WIN32)
    if (std::getenv("QUEST_SIM_SCRIPT") == nullptr && std::getenv("QUEST_SIM") == nullptr) {
        return make_openxr_headset(log);
    }
#endif
    return new Sim(log);
}

}  // namespace quest

#endif  // !__ANDROID__
