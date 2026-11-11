// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 the Ratchet & Clank Quest VR contributors
//
// OpenXR on the Quest, with the game's OpenGL ES context. The sequence is the one proven on the
// Quest 3 by the Halo CE Quest VR port's host_xr.c: the loader through the Java VM and activity,
// the session on the game's EGL context, sustained-high performance levels at session begin, an
// sRGB swapchain written without encoding (the game's picture is gamma-encoded already), and a
// refresh rate above 72 given up when it is not held (only with Keep the refresh rate off, from 0.1.14).
//
// And on a Windows PC (0.1.14, the PC release): the same, on the game's desktop OpenGL context through
// XR_KHR_opengl_enable, with the Khronos loader (openxr_loader.dll beside this library, as Rerac-vr's PC
// release has it inside its exe) opened at run time, so that a PC without one still runs the simulated
// headset's tests. Any runtime then: SteamVR (Index, Vive, Pico, PSVR2, DPVR, Steam Frame and every
// other headset it drives), Meta's for Quest Link, or another. Controllers: suggested bindings for every
// family OpenXR names (Rerac-vr's Touch, Index, Microsoft and simple, and more); SteamVR maps the rest
// onto them. A default runtime with no OpenGL (Windows Mixed Reality's) gives way to SteamVR's, for this
// game only.

#if defined(__ANDROID__) || defined(_WIN32)

#if defined(__ANDROID__)
#include <EGL/egl.h>
#include <GLES3/gl32.h>
#include <dirent.h>
#include <dlfcn.h>
#include <jni.h>
#include <sched.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <ctime>

#define XR_USE_PLATFORM_ANDROID
#define XR_USE_GRAPHICS_API_OPENGL_ES
#define XR_USE_TIMESPEC  // XR_KHR_convert_timespec_time: the runtime's clock against CLOCK_MONOTONIC
#else
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <unknwn.h>  // IUnknown, which openxr_platform.h names for Windows

#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_OPENGL
#define XR_NO_PROTOTYPES  // every call is found at run time, in the loader opened then
#endif
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include "notice.h"
#include "xr.h"

#ifndef GL_FRAMEBUFFER_SRGB_EXT
#define GL_FRAMEBUFFER_SRGB_EXT 0x8DB9
#endif

namespace quest {

#if defined(_WIN32)
// Set by the mod before make_headset (xr_sim.cpp's): the port's GL, whose calls past 1.1 Windows
// does not export.
extern void* (*sim_gl_proc)(const char*);
#endif

namespace {

#if defined(_WIN32)
// The few GL calls made here, taken from the port's GL.
using GLint = int;
using GLuint = unsigned int;
using GLenum = unsigned int;
using GLsizei = int;
using GLubyte = unsigned char;
constexpr GLenum GL_TEXTURE_2D = 0x0DE1, GL_RGBA = 0x1908, GL_RGBA8 = 0x8058, GL_SRGB8_ALPHA8 = 0x8C43;
constexpr GLenum GL_UNSIGNED_BYTE = 0x1401, GL_NUM_EXTENSIONS = 0x821D, GL_EXTENSIONS = 0x1F03;
constexpr GLenum GL_TEXTURE_BINDING_2D = 0x8069, GL_PIXEL_UNPACK_BUFFER = 0x88EC;
constexpr GLenum GL_PIXEL_UNPACK_BUFFER_BINDING = 0x88EF, GL_UNPACK_ALIGNMENT = 0x0CF5;
constexpr GLenum GL_MAJOR_VERSION = 0x821B, GL_MINOR_VERSION = 0x821C;
constexpr GLenum GL_PIXEL_PACK_BUFFER = 0x88EB, GL_PIXEL_PACK_BUFFER_BINDING = 0x88ED, GL_PACK_ALIGNMENT = 0x0D05;

struct GlCalls {
    void(__stdcall* Flush)() = nullptr;
    void(__stdcall* GetIntegerv)(GLenum, GLint*) = nullptr;
    const GLubyte*(__stdcall* GetStringi)(GLenum, GLuint) = nullptr;
    void(__stdcall* Disable)(GLenum) = nullptr;
    void(__stdcall* PixelStorei)(GLenum, GLint) = nullptr;
    void(__stdcall* BindTexture)(GLenum, GLuint) = nullptr;
    void(__stdcall* BindBuffer)(GLenum, GLuint) = nullptr;
    void(__stdcall* TexSubImage2D)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*) = nullptr;
};
GlCalls g_gl;

// Any function of this library: its address names the library to GetModuleHandleExW.
void module_marker() {}

bool load_gl(std::string& why) {
    if (sim_gl_proc == nullptr) {
        why = "the game gave no OpenGL";
        return false;
    }
    const auto take = [](auto& slot, const char* name) {
        slot = reinterpret_cast<std::remove_reference_t<decltype(slot)>>(sim_gl_proc(name));
        return slot != nullptr;
    };
    if (!take(g_gl.Flush, "glFlush") || !take(g_gl.GetIntegerv, "glGetIntegerv") || !take(g_gl.GetStringi, "glGetStringi")
        || !take(g_gl.Disable, "glDisable") || !take(g_gl.PixelStorei, "glPixelStorei")
        || !take(g_gl.BindTexture, "glBindTexture") || !take(g_gl.BindBuffer, "glBindBuffer")
        || !take(g_gl.TexSubImage2D, "glTexSubImage2D")) {
        why = "the game's OpenGL lacks a call the headset needs";
        return false;
    }
    return true;
}

void glFlush() { g_gl.Flush(); }
void glGetIntegerv(GLenum name, GLint* out) { g_gl.GetIntegerv(name, out); }
const GLubyte* glGetStringi(GLenum name, GLuint index) { return g_gl.GetStringi(name, index); }
void glDisable(GLenum cap) { g_gl.Disable(cap); }
void glPixelStorei(GLenum name, GLint value) { g_gl.PixelStorei(name, value); }
void glBindTexture(GLenum target, GLuint texture) { g_gl.BindTexture(target, texture); }
void glBindBuffer(GLenum target, GLuint buffer) { g_gl.BindBuffer(target, buffer); }
void glTexSubImage2D(GLenum target, GLint level, GLint x, GLint y, GLsizei width, GLsizei height, GLenum format,
                     GLenum type, const void* pixels) {
    g_gl.TexSubImage2D(target, level, x, y, width, height, format, type, pixels);
}

// The OpenXR calls, found through the loader's xrGetInstanceProcAddr: the first three before there
// is an instance, the rest once there is one.
PFN_xrGetInstanceProcAddr xrGetInstanceProcAddr = nullptr;
#define QUEST_XR_EARLY_CALLS(X) X(xrEnumerateInstanceExtensionProperties) X(xrCreateInstance)
#define QUEST_XR_CALLS(X)                                                                                              \
    X(xrDestroyInstance) X(xrResultToString) X(xrGetSystem) X(xrGetSystemProperties) X(xrCreateSession)                \
    X(xrDestroySession) X(xrCreateReferenceSpace) X(xrEnumerateSwapchainFormats) X(xrCreateSwapchain)                  \
    X(xrDestroySwapchain) X(xrEnumerateSwapchainImages) X(xrEnumerateViewConfigurationViews) X(xrStringToPath)         \
    X(xrPathToString) X(xrCreateActionSet) X(xrCreateAction) X(xrSuggestInteractionProfileBindings)                    \
    X(xrAttachSessionActionSets) X(xrCreateActionSpace) X(xrGetCurrentInteractionProfile) X(xrBeginSession)           \
    X(xrEndSession) X(xrPollEvent) X(xrLocateViews) X(xrLocateSpace) X(xrGetActionStateFloat)                         \
    X(xrGetActionStateVector2f) X(xrGetActionStateBoolean) X(xrSyncActions) X(xrAcquireSwapchainImage)                \
    X(xrWaitSwapchainImage) X(xrReleaseSwapchainImage) X(xrWaitFrame) X(xrBeginFrame) X(xrEndFrame)                   \
    X(xrApplyHapticFeedback) X(xrEnumerateBoundSourcesForAction)
#define QUEST_XR_DECLARE(name) PFN_##name name = nullptr;
QUEST_XR_EARLY_CALLS(QUEST_XR_DECLARE)
QUEST_XR_CALLS(QUEST_XR_DECLARE)
#undef QUEST_XR_DECLARE

using SwapchainImage = XrSwapchainImageOpenGLKHR;
constexpr XrStructureType kSwapchainImageType = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR;
#else
using SwapchainImage = XrSwapchainImageOpenGLESKHR;
constexpr XrStructureType kSwapchainImageType = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR;
#endif

constexpr const char* kRefreshExtension = "XR_FB_display_refresh_rate";
// Smoother frames (Application SpaceWarp, XR_FB_space_warp, from 0.1.17): the motion and depth images' formats, as
// Meta's guide has them.
constexpr int64_t kMotionFormat = 0x881A;       // GL_RGBA16F
constexpr int64_t kMotionDepthFormat = 0x88F0;  // GL_DEPTH24_STENCIL8
constexpr int kInfo = 1;
constexpr int kWarn = 2;
constexpr int kError = 3;
constexpr int kPointerSize = 32;
constexpr float kBeamWidth = 0.006f, kDotSize = 0.05f;  // metres

double now_seconds() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

Pose pose_of(const XrPosef& p) {
    return {{p.position.x, p.position.y, p.position.z},
            {p.orientation.x, p.orientation.y, p.orientation.z, p.orientation.w}};
}

XrPosef xr_pose(const Pose& p) {
    XrPosef out{};
    out.orientation = {p.orientation.x, p.orientation.y, p.orientation.z, p.orientation.w};
    out.position = {p.position.x, p.position.y, p.position.z};
    return out;
}

struct Swapchain {
    XrSwapchain handle = XR_NULL_HANDLE;
    int width = 0;
    int height = 0;
    std::vector<SwapchainImage> images;
    bool acquired = false;
    uint32_t index = 0;
};

class OpenXr final : public Headset {
public:
    explicit OpenXr(void (*log)(int, const char*)) : m_log(log) {}

    bool start(const XrSettings& settings, std::string& why) override {
        m_settings = settings;
#if defined(_WIN32)
        if (!load_gl(why)) {
            return false;
        }
#endif
        if (!initialize_loader(why) || !create_instance(why) || !create_session(why)
            || !choose_format(why) || !create_targets(why) || !create_actions(why)) {
            return false;
        }
        srgb_write_control();
        return true;
    }

    bool begin_frame(FrameInfo& out) override {
        out = FrameInfo{};
        poll_events();

        if (!m_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            return false;
        }

        XrFrameState state{XR_TYPE_FRAME_STATE};
        XrFrameWaitInfo wait{XR_TYPE_FRAME_WAIT_INFO};
        const double before = now_seconds();
        if (!check(xrWaitFrame(m_session, &wait, &state), "xrWaitFrame")) {
            return false;
        }
        XrFrameBeginInfo begin{XR_TYPE_FRAME_BEGIN_INFO};
        if (!check(xrBeginFrame(m_session, &begin), "xrBeginFrame")) {
            return false;
        }
        m_frame_begun = true;
        m_frame_time = state.predictedDisplayTime;
        // Displays this frame came too late for: more than the last frame's step since its display time (one period, two
        // with smoother frames, finish_frame's m_frame_periods).
        const XrDuration step = state.predictedDisplayPeriod * m_frame_periods;
        if (m_last_display != 0 && state.predictedDisplayPeriod > 0
            && state.predictedDisplayTime - m_last_display > step + state.predictedDisplayPeriod / 2) {
            m_displays_missed += static_cast<long>(
                (state.predictedDisplayTime - m_last_display + state.predictedDisplayPeriod / 2) / state.predictedDisplayPeriod
                - m_frame_periods);
        }
        note_display(state.predictedDisplayTime);
        const double after = now_seconds();
        m_wait_seconds += after - before;
        m_work_start = after;
        m_period_seconds = static_cast<double>(state.predictedDisplayPeriod) * 1e-9;
        ++m_frames;

        out.should_render = state.shouldRender == XR_TRUE;
        out.focused = m_focused;
        out.display_time = state.predictedDisplayTime;
        out.frame_seconds = static_cast<double>(state.predictedDisplayPeriod) * 1e-9;

        locate_views(out);
        sync_input(out);
        statistics(out);
        return true;
    }

    void target_size(Target target, int& width, int& height) const override {
        width = m_targets[target].width;
        height = m_targets[target].height;
    }

    unsigned acquire(Target target) override { return acquire_chain(m_targets[target]); }

    bool motion_target(int eye, MotionTarget& out) override {
        if (!m_space_warp_ready || eye < 0 || eye > 1) {
            return false;
        }
        const unsigned motion = acquire_chain(m_motion[eye]);
        const unsigned depth = acquire_chain(m_motion_depth[eye]);
        if (motion == 0 || depth == 0) {
            return false;
        }
        out = {motion, depth, m_motion[eye].width, m_motion[eye].height};
        return true;
    }

    void space_warp(const SpaceWarpFrame& warp) override {
        m_warp = warp;
        m_warp_set = true;
    }

    unsigned acquire_chain(Swapchain& chain) {
        if (!m_frame_begun || chain.handle == XR_NULL_HANDLE) {
            return 0;
        }
        if (chain.acquired) {
            return chain.images[chain.index].image;
        }
        XrSwapchainImageAcquireInfo acquire{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        if (!check(xrAcquireSwapchainImage(chain.handle, &acquire, &chain.index),
                   "xrAcquireSwapchainImage")) {
            return 0;
        }
        XrSwapchainImageWaitInfo wait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
        wait.timeout = XR_INFINITE_DURATION;
        if (!check(xrWaitSwapchainImage(chain.handle, &wait), "xrWaitSwapchainImage")) {
            return 0;
        }
        chain.acquired = true;
        return chain.images[chain.index].image;
    }

    void finish_frame(bool eyes, bool eyes_kept, const PanelLayer& panel) override {
        if (!m_frame_begun) {
            return;
        }
        m_frame_begun = false;

        // The window is not swapped in VR, so nothing else sends the frame's drawing on its way.
        glFlush();

        XrCompositionLayerProjectionView views[2]{};
        XrCompositionLayerProjection projection{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
        XrCompositionLayerQuad quad{XR_TYPE_COMPOSITION_LAYER_QUAD};
        XrCompositionLayerQuad notice{XR_TYPE_COMPOSITION_LAYER_QUAD};
        XrCompositionLayerQuad beam{XR_TYPE_COMPOSITION_LAYER_QUAD};
        XrCompositionLayerQuad dot{XR_TYPE_COMPOSITION_LAYER_QUAD};
        XrCompositionLayerQuad beam2{XR_TYPE_COMPOSITION_LAYER_QUAD};
        XrCompositionLayerQuad dot2{XR_TYPE_COMPOSITION_LAYER_QUAD};
        const XrCompositionLayerBaseHeader* layers[7];
        uint32_t count = 0;

        const bool have_eyes = eyes && m_views_valid && m_targets[kTargetLeftEye].acquired
                               && m_targets[kTargetRightEye].acquired;
        // Smoother frames: both eyes' motion and depth, drawn this frame from the last, beside their pictures. The app
        // space does not move under the motion (identity): the eyes' motion has the rig's movement in it already.
        XrCompositionLayerSpaceWarpInfoFB warp_info[2]{};
        const bool warped = have_eyes && !eyes_kept && m_warp_set && m_warp.drawn[0] && m_warp.drawn[1]
                            && m_motion[0].acquired && m_motion[1].acquired && m_motion_depth[0].acquired
                            && m_motion_depth[1].acquired && m_warp.depth_zero > m_warp.depth_one && m_warp.depth_one > 0.0f;
        m_warp_set = false;
        if (have_eyes) {
            for (int eye = 0; eye < 2; ++eye) {
                const Swapchain& chain = m_targets[kTargetLeftEye + eye];
                if (!eyes_kept) {
                    m_drawn_pose[eye] = m_views[eye].pose;
                    m_drawn_fov[eye] = m_views[eye].fov;
                }
                views[eye].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
                views[eye].pose = m_drawn_pose[eye];
                views[eye].fov = m_drawn_fov[eye];
                views[eye].subImage.swapchain = chain.handle;
                views[eye].subImage.imageRect.extent = {chain.width, chain.height};
                if (warped) {
                    XrCompositionLayerSpaceWarpInfoFB& w = warp_info[eye];
                    w.type = XR_TYPE_COMPOSITION_LAYER_SPACE_WARP_INFO_FB;
                    w.motionVectorSubImage.swapchain = m_motion[eye].handle;
                    w.motionVectorSubImage.imageRect.extent = {m_motion[eye].width, m_motion[eye].height};
                    w.appSpaceDeltaPose.orientation.w = 1.0f;
                    w.depthSubImage.swapchain = m_motion_depth[eye].handle;
                    w.depthSubImage.imageRect.extent = {m_motion_depth[eye].width, m_motion_depth[eye].height};
                    w.minDepth = 0.0f;
                    w.maxDepth = 1.0f;
                    // Reversed depth (the port's): depth 0 is the far plane, so nearZ, the distance at minDepth, is the
                    // larger, which says so.
                    w.nearZ = m_warp.depth_zero;
                    w.farZ = m_warp.depth_one;
                    views[eye].next = &w;
                }
            }
            if (warped) {
                ++m_warp_frames;
            }
            projection.space = m_local;
            projection.viewCount = 2;
            projection.views = views;
            layers[count++] = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&projection);
        }

        if (panel.visible && m_targets[kTargetPanel].acquired) {
            const Swapchain& chain = m_targets[kTargetPanel];
            quad.layerFlags = panel.blend ? XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT : 0;
            quad.space = panel.follows_head ? m_view : m_local;
            quad.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
            quad.subImage.swapchain = chain.handle;
            quad.subImage.imageRect.extent = {chain.width, chain.height};
            quad.pose = xr_pose(panel.pose);
            quad.size = {panel.width, panel.height};
            layers[count++] = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&quad);
        }

        if (!m_notice.empty() && draw_notice()) {
            notice.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
            notice.space = m_view;
            notice.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
            notice.subImage.swapchain = m_notice_chain.handle;
            notice.subImage.imageRect.extent = {m_notice_chain.width, m_notice_chain.height};
            notice.pose = xr_pose({{0, 0.28f, -1.5f}, {}});
            notice.size = {1.3f, 1.3f * static_cast<float>(kNoticeHeight) / static_cast<float>(kNoticeWidth)};
            layers[count++] = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&notice);
        }

        if ((m_pointer.beam || m_pointer.dot || m_pointer.beam2 || m_pointer.dot2) && draw_pointer()) {
            const auto fill = [&](XrCompositionLayerQuad& layer, const Pose& pose, float width, float height) {
                layer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
                layer.space = m_local;
                layer.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
                layer.subImage.swapchain = m_pointer_chain.handle;
                layer.subImage.imageRect.extent = {m_pointer_chain.width, m_pointer_chain.height};
                layer.pose = xr_pose(pose);
                layer.size = {width, height};
                layers[count++] = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&layer);
            };
            if (m_pointer.beam) {
                fill(beam, m_pointer.beam_pose, m_pointer.beam_length, kBeamWidth);
            }
            if (m_pointer.dot) {
                fill(dot, m_pointer.dot_pose, kDotSize, kDotSize);
            }
            if (m_pointer.beam2) {
                fill(beam2, m_pointer.beam2_pose, m_pointer.beam2_length, kBeamWidth);
            }
            if (m_pointer.dot2) {
                fill(dot2, m_pointer.dot2_pose, kDotSize, kDotSize);
            }
        }

#if defined(_WIN32)
        test_shots();
#endif
        release_targets();

        // With smoother frames the headset paces the game at half its rate: each picture has two periods, and the next
        // display is two periods on (begin_frame's count of missed displays).
        m_frame_periods = warped ? 2 : 1;
        if (m_work_start > 0.0) {
            const double work = now_seconds() - m_work_start;
            m_work_seconds += work;
            m_work_most = std::max(m_work_most, work);
            m_work_over += work > m_period_seconds * m_frame_periods ? 1 : 0;
            m_work_start = 0.0;
        }

        XrFrameEndInfo end{XR_TYPE_FRAME_END_INFO};
        end.displayTime = m_frame_time;
        end.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        end.layerCount = count;
        end.layers = count > 0 ? layers : nullptr;
        check(xrEndFrame(m_session, &end), "xrEndFrame");
    }

    void haptic(Hand hand, float strength, float seconds, float hertz) override {
        if (!m_running || !m_focused) {
            return;
        }
        XrHapticActionInfo info{XR_TYPE_HAPTIC_ACTION_INFO};
        info.action = m_haptic;
        info.subactionPath = m_hand_path[hand];
        XrHapticVibration vibration{XR_TYPE_HAPTIC_VIBRATION};
        vibration.amplitude = std::clamp(strength, 0.0f, 1.0f);
        vibration.duration = static_cast<XrDuration>(seconds * 1e9f);
        vibration.frequency = hertz > 0.0f ? hertz : XR_FREQUENCY_UNSPECIFIED;
        xrApplyHapticFeedback(m_session, &info,
                              reinterpret_cast<const XrHapticBaseHeader*>(&vibration));
    }

    void show_pointer(const PointerLayers& pointer) override { m_pointer = pointer; }

    void show_notice(const char* text) override {
        m_notice = text != nullptr ? text : "";
        m_notice_pixels.clear();
    }

    bool wants_exit() const override { return m_exiting; }

    void stop() override {
        if (m_session != XR_NULL_HANDLE) {
            release_targets();
            if (m_running) {
                xrEndSession(m_session);
                m_running = false;
            }
            for (Swapchain& chain : m_targets) {
                if (chain.handle != XR_NULL_HANDLE) {
                    xrDestroySwapchain(chain.handle);
                    chain.handle = XR_NULL_HANDLE;
                }
            }
            for (int eye = 0; eye < 2; ++eye) {
                for (Swapchain* chain : {&m_motion[eye], &m_motion_depth[eye]}) {
                    if (chain->handle != XR_NULL_HANDLE) {
                        xrDestroySwapchain(chain->handle);
                        chain->handle = XR_NULL_HANDLE;
                    }
                }
            }
            m_space_warp_ready = false;
            if (m_notice_chain.handle != XR_NULL_HANDLE) {
                xrDestroySwapchain(m_notice_chain.handle);
                m_notice_chain.handle = XR_NULL_HANDLE;
            }
            if (m_pointer_chain.handle != XR_NULL_HANDLE) {
                xrDestroySwapchain(m_pointer_chain.handle);
                m_pointer_chain.handle = XR_NULL_HANDLE;
            }
            xrDestroySession(m_session);
            m_session = XR_NULL_HANDLE;
        }
#if defined(_WIN32)
        if (m_instance != XR_NULL_HANDLE && xrDestroyInstance != nullptr) {
#else
        if (m_instance != XR_NULL_HANDLE) {
#endif
            xrDestroyInstance(m_instance);
            m_instance = XR_NULL_HANDLE;
        }
    }

private:
    void info(const std::string& text) const { m_log(kInfo, text.c_str()); }
    void warn(const std::string& text) const { m_log(kWarn, text.c_str()); }

    bool check(XrResult result, const char* what) {
        if (XR_SUCCEEDED(result)) {
            return true;
        }
        char name[XR_MAX_RESULT_STRING_SIZE] = "unknown";
#if defined(_WIN32)
        if (m_instance != XR_NULL_HANDLE && xrResultToString != nullptr) {
#else
        if (m_instance != XR_NULL_HANDLE) {
#endif
            xrResultToString(m_instance, result, name);
        }
        m_log(kError, (std::string("[openxr] ") + what + ": " + name + " ("
                       + std::to_string(static_cast<int>(result)) + ")").c_str());
        m_last_error = std::string(what) + ": " + name;
        return false;
    }

    template <class Fn>
    Fn proc(const char* name) {
        PFN_xrVoidFunction fn = nullptr;
        xrGetInstanceProcAddr(m_instance, name, &fn);
        return reinterpret_cast<Fn>(fn);
    }

#if defined(_WIN32)
    // The Khronos loader: beside this library first (the release puts it there), then where Windows looks.
    bool open_loader(std::string& why) {
        HMODULE self = nullptr;
        wchar_t path[MAX_PATH] = L"";
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(&module_marker), &self);
        const DWORD length = self != nullptr ? GetModuleFileNameW(self, path, MAX_PATH) : 0;
        std::wstring beside(path, length);
        beside = beside.substr(0, beside.find_last_of(L"\\/") + 1) + L"openxr_loader.dll";
        m_loader = length > 0 ? LoadLibraryW(beside.c_str()) : nullptr;
        if (m_loader == nullptr) {
            m_loader = LoadLibraryW(L"openxr_loader.dll");
        }
        if (m_loader == nullptr) {
            why = "openxr_loader.dll was not found beside quest_vr.dll";
            return false;
        }
        xrGetInstanceProcAddr =
            reinterpret_cast<PFN_xrGetInstanceProcAddr>(GetProcAddress(m_loader, "xrGetInstanceProcAddr"));
        bool found = xrGetInstanceProcAddr != nullptr;
#define QUEST_XR_FIND(name)                                                                                            \
    found = found && XR_SUCCEEDED(xrGetInstanceProcAddr(XR_NULL_HANDLE, #name,                                        \
                                                        reinterpret_cast<PFN_xrVoidFunction*>(&name)));
        QUEST_XR_EARLY_CALLS(QUEST_XR_FIND)
#undef QUEST_XR_FIND
        if (!found) {
            why = "openxr_loader.dll is not the OpenXR loader";
            return false;
        }
        return true;
    }

    // Once there is an instance: the rest of the calls.
    bool find_calls(std::string& why) {
        bool found = true;
#define QUEST_XR_FIND(name)                                                                                            \
    found = found && XR_SUCCEEDED(xrGetInstanceProcAddr(m_instance, #name, reinterpret_cast<PFN_xrVoidFunction*>(&name)));
        QUEST_XR_CALLS(QUEST_XR_FIND)
#undef QUEST_XR_FIND
        if (!found) {
            why = "the OpenXR runtime lacks a core call";
        }
        return found;
    }

    static std::string registry_text(const char* value) {
        char text[1024] = "";
        DWORD size = sizeof text;
        if (RegGetValueA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Khronos\\OpenXR\\1", value, RRF_RT_REG_SZ, nullptr, text, &size)
            != ERROR_SUCCESS) {
            return {};
        }
        return text;
    }

    // SteamVR's runtime manifest, from the runtimes Windows lists, or where Steam puts it.
    static std::string steamvr_runtime() {
        HKEY key = nullptr;
        std::string found;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Khronos\\OpenXR\\1\\AvailableRuntimes", 0, KEY_READ, &key)
            == ERROR_SUCCESS) {
            char name[1024];
            for (DWORD i = 0;; ++i) {
                DWORD size = sizeof name;
                if (RegEnumValueA(key, i, name, &size, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) {
                    break;
                }
                std::string path(name, size);
                std::string lower = path;
                std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });
                if (lower.find("steamxr") != std::string::npos && GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
                    found = path;
                    break;
                }
            }
            RegCloseKey(key);
        }
        const char* fallback = "C:\\Program Files (x86)\\Steam\\steamapps\\common\\SteamVR\\steamxr_win64.json";
        if (found.empty() && GetFileAttributesA(fallback) != INVALID_FILE_ATTRIBUTES) {
            found = fallback;
        }
        return found;
    }

    std::vector<XrExtensionProperties> runtime_extensions() {
        uint32_t count = 0;
        std::vector<XrExtensionProperties> available;
        if (XR_SUCCEEDED(xrEnumerateInstanceExtensionProperties(nullptr, 0, &count, nullptr))) {
            available.resize(count, XrExtensionProperties{XR_TYPE_EXTENSION_PROPERTIES});
            xrEnumerateInstanceExtensionProperties(nullptr, count, &count, available.data());
            available.resize(count);
        }
        return available;
    }

    static bool listed(const std::vector<XrExtensionProperties>& available, const char* name) {
        return std::any_of(available.begin(), available.end(),
                           [&](const auto& e) { return std::strcmp(e.extensionName, name) == 0; });
    }

    // The player's own runtime when it has OpenGL; when it has none (Windows Mixed Reality's) or none is set,
    // SteamVR's, for this game only (XR_RUNTIME_JSON in this process): Windows' setting is not touched. A
    // runtime the player set in XR_RUNTIME_JSON themselves is theirs.
    bool initialize_loader(std::string& why) {
        if (!open_loader(why)) {
            return false;
        }
        const std::string active = registry_text("ActiveRuntime");
        const bool chosen = std::getenv("XR_RUNTIME_JSON") != nullptr;
        info("[openxr] the PC's OpenXR runtime: " + (chosen ? std::string(std::getenv("XR_RUNTIME_JSON")) + " (XR_RUNTIME_JSON)"
                                                            : active.empty() ? std::string("none set") : active));
        if (listed(runtime_extensions(), XR_KHR_OPENGL_ENABLE_EXTENSION_NAME) || chosen) {
            return true;
        }
        const std::string steamvr = steamvr_runtime();
        if (steamvr.empty() || steamvr == active) {
            why = active.empty() ? "no OpenXR runtime is set on this PC: start SteamVR (or your headset's PC app) once"
                                 : "the PC's OpenXR runtime has no OpenGL, and SteamVR is not installed";
            return false;
        }
        warn("[openxr] that runtime has no OpenGL: SteamVR's is used for this game (" + steamvr + ")");
        FreeLibrary(m_loader);
        m_loader = nullptr;
        _putenv_s("XR_RUNTIME_JSON", steamvr.c_str());
        SetEnvironmentVariableA("XR_RUNTIME_JSON", steamvr.c_str());
        return open_loader(why);
    }
#else
    // SDL's own library holds the activity; it is loaded by Java, so it is opened by name.
    bool initialize_loader(std::string& why) {
        void* sdl = dlopen("libSDL3.so", RTLD_NOW | RTLD_NOLOAD);
        if (sdl == nullptr) {
            sdl = dlopen("libSDL3.so", RTLD_NOW);
        }
        auto get_env = sdl != nullptr
                           ? reinterpret_cast<void* (*)()>(dlsym(sdl, "SDL_GetAndroidJNIEnv"))
                           : nullptr;
        auto get_activity = sdl != nullptr
                                ? reinterpret_cast<void* (*)()>(dlsym(sdl, "SDL_GetAndroidActivity"))
                                : nullptr;
        if (get_env == nullptr || get_activity == nullptr) {
            why = "SDL's Android calls were not found";
            return false;
        }
        JNIEnv* env = static_cast<JNIEnv*>(get_env());
        jobject activity = env != nullptr ? static_cast<jobject>(get_activity()) : nullptr;
        if (activity == nullptr || env->GetJavaVM(&m_vm) != 0) {
            why = "no Java VM or activity";
            return false;
        }
        m_activity = env->NewGlobalRef(activity);
        env->DeleteLocalRef(activity);

        PFN_xrInitializeLoaderKHR initialize = nullptr;
        if (!check(xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR",
                                         reinterpret_cast<PFN_xrVoidFunction*>(&initialize)),
                   "xrGetInstanceProcAddr(xrInitializeLoaderKHR)")) {
            why = m_last_error;
            return false;
        }
        XrLoaderInitInfoAndroidKHR init{XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR};
        init.applicationVM = m_vm;
        init.applicationContext = m_activity;
        if (!check(initialize(reinterpret_cast<const XrLoaderInitInfoBaseHeaderKHR*>(&init)),
                   "xrInitializeLoaderKHR")) {
            why = m_last_error;
            return false;
        }
        return true;
    }
#endif

#if defined(_WIN32)
    // The controller families named by extensions, each enabled where the runtime has it, for its bindings.
    static constexpr const char* kControllerExtensions[] = {
        "XR_META_touch_controller_plus", "XR_FB_touch_controller_pro", "XR_BD_controller_interaction",
        "XR_HTC_vive_cosmos_controller_interaction", "XR_HTC_vive_focus3_controller_interaction",
        "XR_EXT_hp_mixed_reality_controller", "XR_EXT_samsung_odyssey_controller",
    };

    bool create_instance(std::string& why) {
        const std::vector<XrExtensionProperties> available = runtime_extensions();
        if (!listed(available, XR_KHR_OPENGL_ENABLE_EXTENSION_NAME)) {
            why = "the OpenXR runtime has no OpenGL support: make SteamVR the OpenXR runtime";
            return false;
        }
        std::vector<const char*> extensions = {XR_KHR_OPENGL_ENABLE_EXTENSION_NAME};
        m_refresh_extension = listed(available, kRefreshExtension);
        m_performance_extension = listed(available, XR_EXT_PERFORMANCE_SETTINGS_EXTENSION_NAME);
        if (m_refresh_extension) {
            extensions.push_back(kRefreshExtension);
        }
        if (m_performance_extension) {
            extensions.push_back(XR_EXT_PERFORMANCE_SETTINGS_EXTENSION_NAME);
        }
        m_time_extension = listed(available, XR_KHR_WIN32_CONVERT_PERFORMANCE_COUNTER_TIME_EXTENSION_NAME);
        if (m_time_extension) {
            extensions.push_back(XR_KHR_WIN32_CONVERT_PERFORMANCE_COUNTER_TIME_EXTENSION_NAME);
        }
        for (const char* name : kControllerExtensions) {
            if (listed(available, name)) {
                extensions.push_back(name);
                m_enabled.push_back(name);
            }
        }
        XrInstanceCreateInfo create{XR_TYPE_INSTANCE_CREATE_INFO};
        std::strncpy(create.applicationInfo.applicationName, "Ratchet & Clank VR", XR_MAX_APPLICATION_NAME_SIZE - 1);
        create.applicationInfo.applicationVersion = 1;
        std::strncpy(create.applicationInfo.engineName, "OpenRAC", XR_MAX_ENGINE_NAME_SIZE - 1);
        create.applicationInfo.apiVersion = XR_API_VERSION_1_0;
        create.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        create.enabledExtensionNames = extensions.data();
        if (!check(xrCreateInstance(&create, &m_instance), "xrCreateInstance")) {
            why = m_last_error;
            return false;
        }
        if (!find_calls(why)) {
            return false;
        }
        find_clock();
        auto properties_fn = proc<PFN_xrGetInstanceProperties>("xrGetInstanceProperties");
        XrInstanceProperties runtime{XR_TYPE_INSTANCE_PROPERTIES};
        if (properties_fn != nullptr && XR_SUCCEEDED(properties_fn(m_instance, &runtime))) {
            info(std::string("[openxr] runtime: ") + runtime.runtimeName + " "
                 + std::to_string(XR_VERSION_MAJOR(runtime.runtimeVersion)) + "."
                 + std::to_string(XR_VERSION_MINOR(runtime.runtimeVersion)) + "."
                 + std::to_string(XR_VERSION_PATCH(runtime.runtimeVersion)));
        }
        return find_system(why);
    }
#else
    bool create_instance(std::string& why) {
        uint32_t count = 0;
        std::vector<XrExtensionProperties> available;
        if (check(xrEnumerateInstanceExtensionProperties(nullptr, 0, &count, nullptr),
                  "xrEnumerateInstanceExtensionProperties")) {
            available.resize(count, XrExtensionProperties{XR_TYPE_EXTENSION_PROPERTIES});
            xrEnumerateInstanceExtensionProperties(nullptr, count, &count, available.data());
        }
        const auto has = [&](const char* name) {
            return std::any_of(available.begin(), available.end(), [&](const auto& e) {
                return std::strcmp(e.extensionName, name) == 0;
            });
        };
        if (!has(XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME)) {
            why = "the OpenXR runtime has no OpenGL ES support, or no runtime is installed";
            return false;
        }
        std::vector<const char*> extensions = {XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME,
                                               XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME};
        m_refresh_extension = has(kRefreshExtension);
        m_performance_extension = has(XR_EXT_PERFORMANCE_SETTINGS_EXTENSION_NAME);
        m_thread_extension = has(XR_KHR_ANDROID_THREAD_SETTINGS_EXTENSION_NAME);
        if (m_refresh_extension) {
            extensions.push_back(kRefreshExtension);
        }
        if (m_performance_extension) {
            extensions.push_back(XR_EXT_PERFORMANCE_SETTINGS_EXTENSION_NAME);
        }
        if (m_thread_extension) {
            extensions.push_back(XR_KHR_ANDROID_THREAD_SETTINGS_EXTENSION_NAME);
        }
        m_time_extension = has(XR_KHR_CONVERT_TIMESPEC_TIME_EXTENSION_NAME);
        if (m_time_extension) {
            extensions.push_back(XR_KHR_CONVERT_TIMESPEC_TIME_EXTENSION_NAME);
        }
        // Smoother frames, where the runtime has it and the launcher's option is on.
        m_space_warp_extension = m_settings.space_warp && has(XR_FB_SPACE_WARP_EXTENSION_NAME);
        if (m_space_warp_extension) {
            extensions.push_back(XR_FB_SPACE_WARP_EXTENSION_NAME);
        } else if (m_settings.space_warp) {
            info("[openxr] smoother frames: the runtime has no XR_FB_space_warp");
        }

        XrInstanceCreateInfoAndroidKHR android{XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR};
        android.applicationVM = m_vm;
        android.applicationActivity = m_activity;
        XrInstanceCreateInfo create{XR_TYPE_INSTANCE_CREATE_INFO};
        create.next = &android;
        std::strncpy(create.applicationInfo.applicationName, "Ratchet & Clank Quest VR",
                     XR_MAX_APPLICATION_NAME_SIZE - 1);
        create.applicationInfo.applicationVersion = 1;
        std::strncpy(create.applicationInfo.engineName, "OpenRAC", XR_MAX_ENGINE_NAME_SIZE - 1);
        create.applicationInfo.apiVersion = XR_API_VERSION_1_0;
        create.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        create.enabledExtensionNames = extensions.data();
        if (!check(xrCreateInstance(&create, &m_instance), "xrCreateInstance")) {
            why = m_last_error;
            return false;
        }
        find_clock();
        return find_system(why);
    }
#endif

    // The runtime's clock against this system's (XR_KHR_win32_convert_performance_counter_time on Windows,
    // XR_KHR_convert_timespec_time on Android), for when the next frame is shown (next_display_in).
    void find_clock() {
        if (m_time_extension) {
#if defined(_WIN32)
            m_clock = proc<PFN_xrConvertWin32PerformanceCounterToTimeKHR>("xrConvertWin32PerformanceCounterToTimeKHR");
#else
            m_clock = proc<PFN_xrConvertTimespecTimeToTimeKHR>("xrConvertTimespecTimeToTimeKHR");
#endif
        }
        info(m_clock != nullptr ? "[openxr] frame timing: the scene blended for the headset's display times"
                                : "[openxr] frame timing: the runtime does not convert its clock; the scene blended "
                                  "for the port's own guess");
    }

    // The runtime's clock now; 0 where it cannot say.
    XrTime xr_now() const {
        if (m_clock == nullptr) {
            return 0;
        }
        XrTime time = 0;
#if defined(_WIN32)
        LARGE_INTEGER counter;
        QueryPerformanceCounter(&counter);
        return XR_SUCCEEDED(m_clock(m_instance, &counter, &time)) ? time : 0;
#else
        timespec now{};
        clock_gettime(CLOCK_MONOTONIC, &now);
        return XR_SUCCEEDED(m_clock(m_instance, &now, &time)) ? time : 0;
#endif
    }

    // A frame's display time from xrWaitFrame: the steps between them lately, and how far ahead of the wait's return
    // it was (a frame begun late is shown no sooner than that from its start).
    void note_display(XrTime display) {
        if (m_last_display != 0 && display > m_last_display) {
            m_display_steps[m_display_step_at] = display - m_last_display;
            m_display_step_at = (m_display_step_at + 1) % kDisplaySteps;
        }
        m_last_display = display;
        const XrTime now = xr_now();
        if (now != 0 && display > now) {
            m_display_ahead[m_display_ahead_at] = display - now;
            m_display_ahead_at = (m_display_ahead_at + 1) % kDisplaySteps;
        }
    }

    // The next frame's display time: the last one and the steps' median (one refresh, or two while the headset paces
    // the game at half its rate), later by steps where the frame begins too late for it.
    std::int64_t next_display_in() override {
        if (!m_running || m_last_display == 0) {
            return 0;
        }
        const XrTime now = xr_now();
        XrDuration steps[kDisplaySteps];
        XrDuration aheads[kDisplaySteps];
        std::copy(std::begin(m_display_steps), std::end(m_display_steps), steps);
        std::copy(std::begin(m_display_ahead), std::end(m_display_ahead), aheads);
        std::sort(std::begin(steps), std::end(steps));
        std::sort(std::begin(aheads), std::end(aheads));
        const XrDuration step = steps[kDisplaySteps / 2];
        const XrDuration ahead = aheads[0];  // the least: 0 until the runs fill (then no later steps are added)
        if (now == 0 || step <= 0) {
            return 0;
        }
        XrTime next = m_last_display + step;
        for (int i = 0; i < 8 && next < now + ahead; ++i) {
            next += step;
        }
        const XrDuration in = next - now;
        return in > 0 && in < 500000000 ? in : 0;
    }

    bool find_system(std::string& why) {
        XrSystemGetInfo system{XR_TYPE_SYSTEM_GET_INFO};
        system.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
        if (!check(xrGetSystem(m_instance, &system, &m_system), "xrGetSystem")) {
            why = m_last_error;
            return false;
        }
        XrSystemProperties properties{XR_TYPE_SYSTEM_PROPERTIES};
        XrSystemSpaceWarpPropertiesFB warp{XR_TYPE_SYSTEM_SPACE_WARP_PROPERTIES_FB};
        if (m_space_warp_extension) {
            properties.next = &warp;
        }
        if (XR_SUCCEEDED(xrGetSystemProperties(m_instance, m_system, &properties))) {
            info(std::string("[openxr] system: ") + properties.systemName);
            if (m_space_warp_extension) {
                m_motion_width = static_cast<int>(warp.recommendedMotionVectorImageRectWidth);
                m_motion_height = static_cast<int>(warp.recommendedMotionVectorImageRectHeight);
            }
        }
        return true;
    }

    bool create_session(std::string& why) {
#if defined(_WIN32)
        auto requirements_fn = proc<PFN_xrGetOpenGLGraphicsRequirementsKHR>("xrGetOpenGLGraphicsRequirementsKHR");
        XrGraphicsRequirementsOpenGLKHR requirements{XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR};
        if (requirements_fn == nullptr
            || !check(requirements_fn(m_instance, m_system, &requirements), "xrGetOpenGLGraphicsRequirementsKHR")) {
            why = "the runtime gave no OpenGL requirements";
            return false;
        }
        GLint major = 0, minor = 0;
        glGetIntegerv(GL_MAJOR_VERSION, &major);
        glGetIntegerv(GL_MINOR_VERSION, &minor);
        info("[openxr] the game's OpenGL " + std::to_string(major) + "." + std::to_string(minor) + "; the runtime asks "
             + std::to_string(XR_VERSION_MAJOR(requirements.minApiVersionSupported)) + "."
             + std::to_string(XR_VERSION_MINOR(requirements.minApiVersionSupported)) + " or later");
        XrGraphicsBindingOpenGLWin32KHR binding{XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR};
        binding.hDC = wglGetCurrentDC();
        binding.hGLRC = wglGetCurrentContext();
        if (binding.hDC == nullptr || binding.hGLRC == nullptr) {
            why = "no OpenGL context is current";
            return false;
        }
#else
        auto requirements_fn =
            proc<PFN_xrGetOpenGLESGraphicsRequirementsKHR>("xrGetOpenGLESGraphicsRequirementsKHR");
        XrGraphicsRequirementsOpenGLESKHR requirements{XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR};
        if (requirements_fn == nullptr
            || !check(requirements_fn(m_instance, m_system, &requirements),
                      "xrGetOpenGLESGraphicsRequirementsKHR")) {
            why = "the runtime gave no OpenGL ES requirements";
            return false;
        }

        EGLDisplay display = eglGetCurrentDisplay();
        EGLContext context = eglGetCurrentContext();
        if (display == EGL_NO_DISPLAY || context == EGL_NO_CONTEXT) {
            why = "no OpenGL ES context is current";
            return false;
        }
        EGLint config_id = 0;
        EGLint configs = 0;
        EGLConfig config = nullptr;
        eglQueryContext(display, context, EGL_CONFIG_ID, &config_id);
        const EGLint attributes[] = {EGL_CONFIG_ID, config_id, EGL_NONE};
        eglChooseConfig(display, attributes, &config, 1, &configs);

        XrGraphicsBindingOpenGLESAndroidKHR binding{XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR};
        binding.display = display;
        binding.config = configs > 0 ? config : nullptr;
        binding.context = context;
#endif
        XrSessionCreateInfo create{XR_TYPE_SESSION_CREATE_INFO};
        create.next = &binding;
        create.systemId = m_system;
        if (!check(xrCreateSession(m_instance, &create, &m_session), "xrCreateSession")) {
            why = m_last_error;
            return false;
        }

        XrReferenceSpaceCreateInfo space{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
        space.poseInReferenceSpace.orientation.w = 1.0f;
        space.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
        if (!check(xrCreateReferenceSpace(m_session, &space, &m_local), "LOCAL space")) {
            why = m_last_error;
            return false;
        }
        space.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
        if (!check(xrCreateReferenceSpace(m_session, &space, &m_view), "VIEW space")) {
            why = m_last_error;
            return false;
        }
        // The floor, for Feet on the floor. A runtime may know none: then that option does nothing.
        space.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
        if (XR_FAILED(xrCreateReferenceSpace(m_session, &space, &m_stage))) {
            m_stage = XR_NULL_HANDLE;
        }
        return true;
    }

    // SRGB8_ALPHA8 first: the game's picture is gamma-encoded, and with encoding off it reaches
    // the compositor as the sRGB image it is. Without the switch to turn encoding off it would be
    // encoded twice, so RGBA8 is then the nearer of the two.
    bool choose_format(std::string& why) {
        uint32_t count = 0;
        if (!check(xrEnumerateSwapchainFormats(m_session, 0, &count, nullptr),
                   "xrEnumerateSwapchainFormats") || count == 0) {
            why = "the runtime offers no swapchain formats";
            return false;
        }
        std::vector<int64_t> formats(count);
        xrEnumerateSwapchainFormats(m_session, count, &count, formats.data());
        m_formats = formats;
#if defined(_WIN32)
        // Desktop OpenGL from 3.0 has the switch (GL_FRAMEBUFFER_SRGB, off unless turned on).
        GLint major = 0;
        glGetIntegerv(GL_MAJOR_VERSION, &major);
        m_srgb_control = major >= 3 || has_gl_extension("GL_ARB_framebuffer_sRGB");
#else
        m_srgb_control = has_gl_extension("GL_EXT_sRGB_write_control");
#endif
        const auto offered = [&](int64_t format) {
            return std::find(formats.begin(), formats.end(), format) != formats.end();
        };
        if (m_srgb_control && offered(GL_SRGB8_ALPHA8)) {
            m_format = GL_SRGB8_ALPHA8;
        } else if (offered(GL_RGBA8)) {
            m_format = GL_RGBA8;
        } else if (offered(GL_SRGB8_ALPHA8)) {
            m_format = GL_SRGB8_ALPHA8;
        } else {
            why = "the runtime offers neither SRGB8_ALPHA8 nor RGBA8";
            return false;
        }
        info("[openxr] swapchain format " + std::to_string(m_format) + ", sRGB write control "
             + (m_srgb_control ? "yes" : "no"));
        return true;
    }

    static bool has_gl_extension(const char* name) {
        GLint count = 0;
        glGetIntegerv(GL_NUM_EXTENSIONS, &count);
        for (GLint i = 0; i < count; ++i) {
            const auto* extension = reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, i));
            if (extension != nullptr && std::strcmp(extension, name) == 0) {
                return true;
            }
        }
        return false;
    }

    void srgb_write_control() const {
        if (m_srgb_control) {
            glDisable(GL_FRAMEBUFFER_SRGB_EXT);
        }
    }

    // A swapchain of the eyes' format, or of `format` with `usage` (the motion images').
    bool create_target(Swapchain& chain, int width, int height, std::string& why, int64_t format = 0,
                       XrSwapchainUsageFlags usage = 0) {
        XrSwapchainCreateInfo create{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        create.usageFlags = usage != 0 ? usage : XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
        create.format = format != 0 ? format : m_format;
        create.sampleCount = 1;
        create.width = static_cast<uint32_t>(width);
        create.height = static_cast<uint32_t>(height);
        create.faceCount = 1;
        create.arraySize = 1;
        create.mipCount = 1;
        if (!check(xrCreateSwapchain(m_session, &create, &chain.handle), "xrCreateSwapchain")) {
            why = m_last_error;
            return false;
        }
        chain.width = width;
        chain.height = height;
        uint32_t count = 0;
        xrEnumerateSwapchainImages(chain.handle, 0, &count, nullptr);
        chain.images.assign(count, SwapchainImage{kSwapchainImageType});
        if (!check(xrEnumerateSwapchainImages(
                       chain.handle, count, &count,
                       reinterpret_cast<XrSwapchainImageBaseHeader*>(chain.images.data())),
                   "xrEnumerateSwapchainImages") || count == 0) {
            why = m_last_error;
            return false;
        }
        return true;
    }

    bool create_targets(std::string& why) {
        XrViewConfigurationView views[2] = {{XR_TYPE_VIEW_CONFIGURATION_VIEW},
                                            {XR_TYPE_VIEW_CONFIGURATION_VIEW}};
        uint32_t count = 0;
        if (!check(xrEnumerateViewConfigurationViews(m_instance, m_system,
                                                     XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 2,
                                                     &count, views),
                   "xrEnumerateViewConfigurationViews") || count != 2) {
            why = "the headset does not offer a stereo view";
            return false;
        }
        for (int eye = 0; eye < 2; ++eye) {
            const auto size = [&](uint32_t recommended, uint32_t largest) {
                const int scaled = static_cast<int>(static_cast<float>(recommended) * m_settings.resolution);
                return std::clamp(scaled & ~1, 64, static_cast<int>(largest));
            };
            const int width = size(views[eye].recommendedImageRectWidth, views[eye].maxImageRectWidth);
            const int height = size(views[eye].recommendedImageRectHeight, views[eye].maxImageRectHeight);
            if (!create_target(m_targets[kTargetLeftEye + eye], width, height, why)) {
                return false;
            }
            info("[openxr] eye " + std::to_string(eye) + ": " + std::to_string(width) + " x "
                 + std::to_string(height) + " (recommended "
                 + std::to_string(views[eye].recommendedImageRectWidth) + " x "
                 + std::to_string(views[eye].recommendedImageRectHeight) + ")");
        }
        if (!create_target(m_targets[kTargetPanel], m_settings.panel_width, m_settings.panel_height, why)) {
            return false;
        }
        create_space_warp_targets();
        return true;
    }

    // Smoother frames: a motion and a depth swapchain an eye, at the size the runtime recommends. Without them the game
    // draws every frame, as before.
    void create_space_warp_targets() {
        if (!m_space_warp_extension) {
            return;
        }
        const auto offered = [&](int64_t format) {
            return std::find(m_formats.begin(), m_formats.end(), format) != m_formats.end();
        };
        if (m_motion_width < 16 || m_motion_height < 16 || !offered(kMotionFormat) || !offered(kMotionDepthFormat)) {
            warn("[openxr] smoother frames: off, the runtime gives a motion size of " + std::to_string(m_motion_width) + " x "
                 + std::to_string(m_motion_height) + (offered(kMotionFormat) ? "" : ", no RGBA16F")
                 + (offered(kMotionDepthFormat) ? "" : ", no DEPTH24_STENCIL8"));
            return;
        }
        std::string why;
        bool made = true;
        for (int eye = 0; eye < 2 && made; ++eye) {
            made = create_target(m_motion[eye], m_motion_width, m_motion_height, why, kMotionFormat,
                                 XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT)
                   && create_target(m_motion_depth[eye], m_motion_width, m_motion_height, why, kMotionDepthFormat,
                                    XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT);
        }
        if (!made) {
            warn("[openxr] smoother frames: off, " + why);
            return;
        }
        m_space_warp_ready = true;
        info("[openxr] smoother frames (Application SpaceWarp): on, motion " + std::to_string(m_motion_width) + " x "
             + std::to_string(m_motion_height) + " an eye");
    }

    XrPath path(const char* name) const {
        XrPath out = XR_NULL_PATH;
        xrStringToPath(m_instance, name, &out);
        return out;
    }

    XrAction make_action(XrActionType type, const char* name, const char* label, bool both_hands) {
        XrActionCreateInfo create{XR_TYPE_ACTION_CREATE_INFO};
        create.actionType = type;
        std::strncpy(create.actionName, name, XR_MAX_ACTION_NAME_SIZE - 1);
        std::strncpy(create.localizedActionName, label, XR_MAX_LOCALIZED_ACTION_NAME_SIZE - 1);
        if (both_hands) {
            create.countSubactionPaths = 2;
            create.subactionPaths = m_hand_path;
        }
        XrAction action = XR_NULL_HANDLE;
        check(xrCreateAction(m_set, &create, &action), name);
        return action;
    }

    bool create_actions(std::string& why) {
        m_hand_path[kLeft] = path("/user/hand/left");
        m_hand_path[kRight] = path("/user/hand/right");

        XrActionSetCreateInfo set{XR_TYPE_ACTION_SET_CREATE_INFO};
        std::strcpy(set.actionSetName, "gameplay");
        std::strcpy(set.localizedActionSetName, "Gameplay");
        if (!check(xrCreateActionSet(m_instance, &set, &m_set), "xrCreateActionSet")) {
            why = m_last_error;
            return false;
        }
        m_grip = make_action(XR_ACTION_TYPE_POSE_INPUT, "grip", "Hand", true);
        m_aim = make_action(XR_ACTION_TYPE_POSE_INPUT, "aim", "Aim", true);
        m_move = make_action(XR_ACTION_TYPE_VECTOR2F_INPUT, "move", "Walk", false);
        m_look = make_action(XR_ACTION_TYPE_VECTOR2F_INPUT, "look", "Turn", false);
        m_trigger_left = make_action(XR_ACTION_TYPE_FLOAT_INPUT, "trigger_left", "Left trigger", false);
        m_trigger_right = make_action(XR_ACTION_TYPE_FLOAT_INPUT, "trigger_right", "Right trigger", false);
        m_squeeze_left = make_action(XR_ACTION_TYPE_FLOAT_INPUT, "squeeze_left", "Left grip", false);
        m_squeeze_right = make_action(XR_ACTION_TYPE_FLOAT_INPUT, "squeeze_right", "Right grip", false);
        m_click_left = make_action(XR_ACTION_TYPE_BOOLEAN_INPUT, "click_left", "Left stick click", false);
        m_click_right = make_action(XR_ACTION_TYPE_BOOLEAN_INPUT, "click_right", "Right stick click", false);
        m_menu = make_action(XR_ACTION_TYPE_BOOLEAN_INPUT, "button_menu", "Menu", false);
        m_a = make_action(XR_ACTION_TYPE_BOOLEAN_INPUT, "button_a", "A", false);
        m_b = make_action(XR_ACTION_TYPE_BOOLEAN_INPUT, "button_b", "B", false);
        m_x = make_action(XR_ACTION_TYPE_BOOLEAN_INPUT, "button_x", "X", false);
        m_y = make_action(XR_ACTION_TYPE_BOOLEAN_INPUT, "button_y", "Y", false);
        m_first_left = make_action(XR_ACTION_TYPE_BOOLEAN_INPUT, "first_left", "Left first finger", false);
        m_first_right = make_action(XR_ACTION_TYPE_BOOLEAN_INPUT, "first_right", "Right first finger", false);
        m_thumb_left = make_action(XR_ACTION_TYPE_BOOLEAN_INPUT, "thumb_left", "Left thumb", false);
        m_thumb_right = make_action(XR_ACTION_TYPE_BOOLEAN_INPUT, "thumb_right", "Right thumb", false);
        m_rest_left = make_action(XR_ACTION_TYPE_BOOLEAN_INPUT, "rest_left", "Left thumb rest", false);
        m_haptic = make_action(XR_ACTION_TYPE_VIBRATION_OUTPUT, "haptic", "Vibration", true);

#if defined(_WIN32)
        if (!suggest_every_controller()) {
            why = "the runtime took no controller's bindings";
            return false;
        }
#else
        const std::vector<XrActionSuggestedBinding> touch = {
            {m_grip, path("/user/hand/left/input/grip/pose")},
            {m_grip, path("/user/hand/right/input/grip/pose")},
            {m_aim, path("/user/hand/left/input/aim/pose")},
            {m_aim, path("/user/hand/right/input/aim/pose")},
            {m_move, path("/user/hand/left/input/thumbstick")},
            {m_look, path("/user/hand/right/input/thumbstick")},
            {m_click_left, path("/user/hand/left/input/thumbstick/click")},
            {m_click_right, path("/user/hand/right/input/thumbstick/click")},
            {m_trigger_left, path("/user/hand/left/input/trigger/value")},
            {m_trigger_right, path("/user/hand/right/input/trigger/value")},
            {m_squeeze_left, path("/user/hand/left/input/squeeze/value")},
            {m_squeeze_right, path("/user/hand/right/input/squeeze/value")},
            {m_menu, path("/user/hand/left/input/menu/click")},
            {m_a, path("/user/hand/right/input/a/click")},
            {m_b, path("/user/hand/right/input/b/click")},
            {m_x, path("/user/hand/left/input/x/click")},
            {m_y, path("/user/hand/left/input/y/click")},
            {m_first_left, path("/user/hand/left/input/trigger/touch")},
            {m_first_right, path("/user/hand/right/input/trigger/touch")},
            {m_thumb_left, path("/user/hand/left/input/x/touch")},
            {m_thumb_left, path("/user/hand/left/input/y/touch")},
            {m_thumb_left, path("/user/hand/left/input/thumbstick/touch")},
            {m_thumb_left, path("/user/hand/left/input/thumbrest/touch")},
            {m_thumb_right, path("/user/hand/right/input/a/touch")},
            {m_thumb_right, path("/user/hand/right/input/b/touch")},
            {m_thumb_right, path("/user/hand/right/input/thumbstick/touch")},
            {m_thumb_right, path("/user/hand/right/input/thumbrest/touch")},
            {m_rest_left, path("/user/hand/left/input/thumbrest/touch")},
            {m_haptic, path("/user/hand/left/output/haptic")},
            {m_haptic, path("/user/hand/right/output/haptic")},
        };
        XrInteractionProfileSuggestedBinding suggested{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
        suggested.interactionProfile = path("/interaction_profiles/oculus/touch_controller");
        suggested.suggestedBindings = touch.data();
        suggested.countSuggestedBindings = static_cast<uint32_t>(touch.size());
        if (!check(xrSuggestInteractionProfileBindings(m_instance, &suggested),
                   "xrSuggestInteractionProfileBindings")) {
            why = m_last_error;
            return false;
        }
#endif

        XrSessionActionSetsAttachInfo attach{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
        attach.countActionSets = 1;
        attach.actionSets = &m_set;
        if (!check(xrAttachSessionActionSets(m_session, &attach), "xrAttachSessionActionSets")) {
            why = m_last_error;
            return false;
        }

        // After the attach, as Halo's host does it on the Quest.
        for (int hand = 0; hand < 2; ++hand) {
            XrActionSpaceCreateInfo space{XR_TYPE_ACTION_SPACE_CREATE_INFO};
            space.poseInActionSpace.orientation.w = 1.0f;
            space.subactionPath = m_hand_path[hand];
            space.action = m_grip;
            if (!check(xrCreateActionSpace(m_session, &space, &m_grip_space[hand]), "xrCreateActionSpace(grip)")) {
                why = m_last_error;
                return false;
            }
            space.action = m_aim;
            if (!check(xrCreateActionSpace(m_session, &space, &m_aim_space[hand]), "xrCreateActionSpace(aim)")) {
                why = m_last_error;
                return false;
            }
        }
        return true;
    }

#if defined(_WIN32)
    using Binding = std::pair<XrAction, const char*>;

    // One controller family's bindings; when the runtime will not take its touch sensors' paths, again
    // without them. Not every runtime knows every family: those it refuses are only noted.
    bool suggest(const char* profile, const char* extension, const std::vector<Binding>& bindings,
                 const std::vector<Binding>& touches) {
        if (extension != nullptr
            && std::find_if(m_enabled.begin(), m_enabled.end(), [&](const std::string& e) { return e == extension; })
                   == m_enabled.end()) {
            return false;
        }
        const auto send = [&](bool with_touches) {
            std::vector<XrActionSuggestedBinding> list;
            for (const Binding& b : bindings) {
                list.push_back({b.first, path(b.second)});
            }
            if (with_touches) {
                for (const Binding& b : touches) {
                    list.push_back({b.first, path(b.second)});
                }
            }
            XrInteractionProfileSuggestedBinding suggested{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
            suggested.interactionProfile = path(profile);
            suggested.suggestedBindings = list.data();
            suggested.countSuggestedBindings = static_cast<uint32_t>(list.size());
            return xrSuggestInteractionProfileBindings(m_instance, &suggested);
        };
        XrResult result = send(true);
        if (XR_FAILED(result) && !touches.empty()) {
            result = send(false);
        }
        if (XR_FAILED(result)) {
            warn(std::string("[openxr] bindings not taken for ") + profile + " (" + std::to_string(static_cast<int>(result))
                 + ")");
            return false;
        }
        return true;
    }

    // Every controller family OpenXR names (Rerac-vr suggests Touch, Index, Microsoft's and the simple one).
    // Where one lacks a button the game wants, the nearest it has: a Vive wand's or Windows Mixed Reality
    // controller's touchpad press is A and X, its menu button B and Y. SteamVR maps controllers it knows
    // otherwise (PSVR2 Sense, Steam Frame, DPVR, Pico over streaming) onto these, and its own binding page
    // can change any of it.
    bool suggest_every_controller() {
        const std::vector<Binding> poses = {
            {m_grip, "/user/hand/left/input/grip/pose"},   {m_grip, "/user/hand/right/input/grip/pose"},
            {m_aim, "/user/hand/left/input/aim/pose"},     {m_aim, "/user/hand/right/input/aim/pose"},
            {m_haptic, "/user/hand/left/output/haptic"},   {m_haptic, "/user/hand/right/output/haptic"},
        };
        const auto join = [](std::vector<Binding> a, const std::vector<Binding>& b) {
            a.insert(a.end(), b.begin(), b.end());
            return a;
        };
        const std::vector<Binding> sticks = join(poses, {
            {m_move, "/user/hand/left/input/thumbstick"},
            {m_look, "/user/hand/right/input/thumbstick"},
            {m_click_left, "/user/hand/left/input/thumbstick/click"},
            {m_click_right, "/user/hand/right/input/thumbstick/click"},
            {m_trigger_left, "/user/hand/left/input/trigger/value"},
            {m_trigger_right, "/user/hand/right/input/trigger/value"},
        });
        const std::vector<Binding> abxy = {
            {m_a, "/user/hand/right/input/a/click"}, {m_b, "/user/hand/right/input/b/click"},
            {m_x, "/user/hand/left/input/x/click"},  {m_y, "/user/hand/left/input/y/click"},
        };
        const std::vector<Binding> squeeze_value = {
            {m_squeeze_left, "/user/hand/left/input/squeeze/value"},
            {m_squeeze_right, "/user/hand/right/input/squeeze/value"},
        };
        const std::vector<Binding> squeeze_click = {
            {m_squeeze_left, "/user/hand/left/input/squeeze/click"},
            {m_squeeze_right, "/user/hand/right/input/squeeze/click"},
        };
        const std::vector<Binding> menu = {{m_menu, "/user/hand/left/input/menu/click"}};
        const std::vector<Binding> touch_like = join(join(join(sticks, abxy), squeeze_value), menu);
        const std::vector<Binding> touch_sensors = {
            {m_first_left, "/user/hand/left/input/trigger/touch"},
            {m_first_right, "/user/hand/right/input/trigger/touch"},
            {m_thumb_left, "/user/hand/left/input/x/touch"},
            {m_thumb_left, "/user/hand/left/input/y/touch"},
            {m_thumb_left, "/user/hand/left/input/thumbstick/touch"},
            {m_thumb_left, "/user/hand/left/input/thumbrest/touch"},
            {m_thumb_right, "/user/hand/right/input/a/touch"},
            {m_thumb_right, "/user/hand/right/input/b/touch"},
            {m_thumb_right, "/user/hand/right/input/thumbstick/touch"},
            {m_thumb_right, "/user/hand/right/input/thumbrest/touch"},
            {m_rest_left, "/user/hand/left/input/thumbrest/touch"},
        };
        // The Index: its a and b are A and B on the right, X and Y on the left; a firm squeeze of the left
        // touchpad is the menu button (recentre).
        const std::vector<Binding> index = join(join(sticks, squeeze_value), {
            {m_a, "/user/hand/right/input/a/click"},
            {m_b, "/user/hand/right/input/b/click"},
            {m_x, "/user/hand/left/input/a/click"},
            {m_y, "/user/hand/left/input/b/click"},
            {m_menu, "/user/hand/left/input/trackpad/force"},
        });
        const std::vector<Binding> index_sensors = {
            {m_first_left, "/user/hand/left/input/trigger/touch"},
            {m_first_right, "/user/hand/right/input/trigger/touch"},
            {m_thumb_left, "/user/hand/left/input/a/touch"},
            {m_thumb_left, "/user/hand/left/input/b/touch"},
            {m_thumb_left, "/user/hand/left/input/thumbstick/touch"},
            {m_thumb_left, "/user/hand/left/input/trackpad/touch"},
            {m_thumb_right, "/user/hand/right/input/a/touch"},
            {m_thumb_right, "/user/hand/right/input/b/touch"},
            {m_thumb_right, "/user/hand/right/input/thumbstick/touch"},
            {m_thumb_right, "/user/hand/right/input/trackpad/touch"},
        };
        const std::vector<Binding> pico_sensors = {
            {m_first_left, "/user/hand/left/input/trigger/touch"},
            {m_first_right, "/user/hand/right/input/trigger/touch"},
            {m_thumb_left, "/user/hand/left/input/x/touch"},
            {m_thumb_left, "/user/hand/left/input/y/touch"},
            {m_thumb_left, "/user/hand/left/input/thumbstick/touch"},
            {m_thumb_right, "/user/hand/right/input/a/touch"},
            {m_thumb_right, "/user/hand/right/input/b/touch"},
            {m_thumb_right, "/user/hand/right/input/thumbstick/touch"},
        };
        const std::vector<Binding> stick_sensors = {
            {m_thumb_left, "/user/hand/left/input/thumbstick/touch"},
            {m_thumb_right, "/user/hand/right/input/thumbstick/touch"},
        };
        const std::vector<Binding> focus3_sensors = join(stick_sensors, {
            {m_first_left, "/user/hand/left/input/trigger/touch"},
            {m_first_right, "/user/hand/right/input/trigger/touch"},
            {m_thumb_left, "/user/hand/left/input/thumbrest/touch"},
            {m_thumb_right, "/user/hand/right/input/thumbrest/touch"},
            {m_rest_left, "/user/hand/left/input/thumbrest/touch"},
        });
        // Windows Mixed Reality and the Odyssey: sticks, and a touchpad for the buttons they lack.
        const std::vector<Binding> mixed = join(join(sticks, squeeze_click), {
            {m_a, "/user/hand/right/input/trackpad/click"},
            {m_b, "/user/hand/right/input/menu/click"},
            {m_x, "/user/hand/left/input/trackpad/click"},
            {m_y, "/user/hand/left/input/menu/click"},
        });
        const std::vector<Binding> pad_sensors = {
            {m_thumb_left, "/user/hand/left/input/trackpad/touch"},
            {m_thumb_right, "/user/hand/right/input/trackpad/touch"},
        };
        // The Vive wand: its touchpads are the sticks, pressed for A and X.
        const std::vector<Binding> wand = join(join(poses, squeeze_click), {
            {m_move, "/user/hand/left/input/trackpad"},
            {m_look, "/user/hand/right/input/trackpad"},
            {m_trigger_left, "/user/hand/left/input/trigger/value"},
            {m_trigger_right, "/user/hand/right/input/trigger/value"},
            {m_a, "/user/hand/right/input/trackpad/click"},
            {m_b, "/user/hand/right/input/menu/click"},
            {m_x, "/user/hand/left/input/trackpad/click"},
            {m_y, "/user/hand/left/input/menu/click"},
        });
        const std::vector<Binding> simple = join(poses, {
            {m_trigger_left, "/user/hand/left/input/select/click"},
            {m_trigger_right, "/user/hand/right/input/select/click"},
            {m_y, "/user/hand/left/input/menu/click"},
            {m_a, "/user/hand/right/input/menu/click"},
        });
        int taken = 0;
        taken += suggest("/interaction_profiles/oculus/touch_controller", nullptr, touch_like, touch_sensors);
        taken += suggest("/interaction_profiles/meta/touch_controller_plus", "XR_META_touch_controller_plus", touch_like,
                         touch_sensors);
        taken += suggest("/interaction_profiles/facebook/touch_controller_pro", "XR_FB_touch_controller_pro", touch_like,
                         touch_sensors);
        taken += suggest("/interaction_profiles/valve/index_controller", nullptr, index, index_sensors);
        taken += suggest("/interaction_profiles/bytedance/pico4_controller", "XR_BD_controller_interaction", touch_like,
                         pico_sensors);
        taken += suggest("/interaction_profiles/bytedance/pico_neo3_controller", "XR_BD_controller_interaction",
                         touch_like, pico_sensors);
        taken += suggest("/interaction_profiles/htc/vive_cosmos_controller", "XR_HTC_vive_cosmos_controller_interaction",
                         join(join(join(sticks, abxy), squeeze_click), menu), stick_sensors);
        taken += suggest("/interaction_profiles/htc/vive_focus3_controller", "XR_HTC_vive_focus3_controller_interaction",
                         join(join(join(sticks, abxy), squeeze_click), menu), focus3_sensors);
        taken += suggest("/interaction_profiles/hp/mixed_reality_controller", "XR_EXT_hp_mixed_reality_controller",
                         touch_like, {});
        taken += suggest("/interaction_profiles/microsoft/motion_controller", nullptr, mixed, pad_sensors);
        taken += suggest("/interaction_profiles/samsung/odyssey_controller", "XR_EXT_samsung_odyssey_controller", mixed,
                         pad_sensors);
        taken += suggest("/interaction_profiles/htc/vive_controller", nullptr, wand, pad_sensors);
        taken += suggest("/interaction_profiles/khr/simple_controller", nullptr, simple, {});
        // Which of the controller families that need an extension this runtime offered: the others' bindings
        // were not suggested.
        std::string families;
        for (const std::string& name : m_enabled) {
            for (const char* controller : kControllerExtensions) {
                if (name == controller) {
                    families += " " + name;
                }
            }
        }
        info("[openxr] controller bindings suggested for " + std::to_string(taken)
             + " families (controller extensions the runtime has:" + (families.empty() ? std::string(" none") : families)
             + ")");
        return taken > 0;
    }

    // Whether the bound controller senses a finger on its trigger and a thumb on it, per hand: a controller
    // without (a Vive wand, Windows Mixed Reality's) has the finger on the trigger when it is pressed and the
    // thumb resting on the controller.
    bool bound(XrAction action) {
        XrBoundSourcesForActionEnumerateInfo get{XR_TYPE_BOUND_SOURCES_FOR_ACTION_ENUMERATE_INFO};
        get.action = action;
        uint32_t count = 0;
        return XR_SUCCEEDED(xrEnumerateBoundSourcesForAction(m_session, &get, 0, &count, nullptr)) && count > 0;
    }
#endif

    // Which controller each hand is bound as, when that changes: the log's first word on input.
    void log_profiles() {
        for (int hand = 0; hand < 2; ++hand) {
            XrInteractionProfileState state{XR_TYPE_INTERACTION_PROFILE_STATE};
            std::string name = "(none)";
            m_bound[hand] = false;
            if (XR_SUCCEEDED(xrGetCurrentInteractionProfile(m_session, m_hand_path[hand], &state))
                && state.interactionProfile != XR_NULL_PATH) {
                char text[XR_MAX_PATH_LENGTH] = "";
                uint32_t length = 0;
                if (XR_SUCCEEDED(xrPathToString(m_instance, state.interactionProfile, sizeof text, &length, text))) {
                    name = text;
                }
                m_bound[hand] = true;
            }
#if defined(_WIN32)
            m_first_sensed[hand] = m_bound[hand] && bound(hand == kLeft ? m_first_left : m_first_right);
            m_thumb_sensed[hand] = m_bound[hand] && bound(hand == kLeft ? m_thumb_left : m_thumb_right);
            name += std::string(", touch sensors: finger ") + (m_first_sensed[hand] ? "yes" : "no") + ", thumb "
                    + (m_thumb_sensed[hand] ? "yes" : "no");
#endif
            info(std::string("[openxr] ") + (hand == kLeft ? "left" : "right") + " hand: " + name);
        }
    }

    void session_began() {
        // Sustained high: the game thread does the world, the renderer's calls and the sound's
        // feeding, and at a lower level the sound starves first (Halo's Quest finding).
        if (m_performance_extension) {
            auto set_level =
                proc<PFN_xrPerfSettingsSetPerformanceLevelEXT>("xrPerfSettingsSetPerformanceLevelEXT");
            if (set_level != nullptr) {
                // CPU and GPU boost (launcher): the highest level an app may ask for. The runtime may lower it when the
                // headset grows warm, and says so (the performance notices below).
                const XrPerfSettingsLevelEXT level =
                    m_settings.cpu_gpu_boost ? XR_PERF_SETTINGS_LEVEL_BOOST_EXT : XR_PERF_SETTINGS_LEVEL_SUSTAINED_HIGH_EXT;
                const XrResult cpu = set_level(m_session, XR_PERF_SETTINGS_DOMAIN_CPU_EXT, level);
                const XrResult gpu = set_level(m_session, XR_PERF_SETTINGS_DOMAIN_GPU_EXT, level);
                info(std::string("[openxr] performance levels: CPU and GPU ") + (m_settings.cpu_gpu_boost ? "boost" : "sustained high")
                     + " (CPU " + (XR_SUCCEEDED(cpu) ? "taken" : "refused " + std::to_string(cpu)) + ", GPU "
                     + (XR_SUCCEEDED(gpu) ? "taken" : "refused " + std::to_string(gpu)) + ")");
            }
        }
#if defined(__ANDROID__)
        if (m_thread_extension) {
            auto set_thread = proc<PFN_xrSetAndroidApplicationThreadKHR>("xrSetAndroidApplicationThreadKHR");
            if (set_thread != nullptr) {
                set_thread(m_session, XR_ANDROID_THREAD_TYPE_RENDERER_MAIN_KHR,
                           static_cast<uint32_t>(syscall(SYS_gettid)));
            }
        }
#endif
        m_threads_given.clear();
        register_threads();
        set_refresh(m_settings.refresh_hz);
    }

    // The port's threads by name (port/common/parallel.cpp's workers, "openrac-pool"; the sound engine's, "openrac-sound"),
    // given to the runtime as the app's workers (XR_KHR_android_thread_settings), as the thread that draws is given as
    // the renderer when the session begins. The Quest then runs them on its fast cores, not wherever the scheduler puts
    // them: the frame's posing waits for the pool, and a pool on a slow core made some frames late (0.1.19, a tester's
    // "microstutters"). Looked for again with each [vr-perf] line, as the pool starts with the first frame that poses.
    void register_threads() {
#if defined(__ANDROID__)
        if (!m_thread_extension || !m_running) {
            return;
        }
        auto set_thread = proc<PFN_xrSetAndroidApplicationThreadKHR>("xrSetAndroidApplicationThreadKHR");
        DIR* tasks = set_thread != nullptr ? opendir("/proc/self/task") : nullptr;
        if (tasks == nullptr) {
            return;
        }
        while (const dirent* entry = readdir(tasks)) {
            const int tid = std::atoi(entry->d_name);
            if (tid <= 0 || std::find(m_threads_given.begin(), m_threads_given.end(), tid) != m_threads_given.end()) {
                continue;
            }
            char path[64];
            std::snprintf(path, sizeof path, "/proc/self/task/%d/comm", tid);
            FILE* file = std::fopen(path, "r");
            if (file == nullptr) {
                continue;
            }
            char name[32] = {};
            const bool read = std::fgets(name, sizeof name, file) != nullptr;
            std::fclose(file);
            if (!read || (std::strncmp(name, "openrac-pool", 12) != 0 && std::strncmp(name, "openrac-sound", 13) != 0)) {
                continue;
            }
            m_threads_given.push_back(tid);
            name[std::strcspn(name, "\n")] = '\0';
            const XrResult result =
                set_thread(m_session, XR_ANDROID_THREAD_TYPE_APPLICATION_WORKER_KHR, static_cast<uint32_t>(tid));
            info("[openxr] thread " + std::to_string(tid) + " (" + name + ") given to the headset as a worker"
                 + (XR_SUCCEEDED(result) ? "" : ": refused " + std::to_string(result)));
        }
        closedir(tasks);
#endif
    }

#if defined(__ANDROID__)
    // Foveated rendering (XrSettings::foveation), on the texture the port draws the eye into (GL_QCOM_texture_foveated,
    // Qualcomm's: the tiles towards the picture's edges drawn at a lower resolution and scaled up as they are written
    // out). The Quest's own foveation (XR_FB_foveation) is set on a swapchain's images, which the port does not draw
    // into; this is the same thing on its own texture. Pixel density at a point d from the focal point (NDC) is
    // 1 / max(gain^2 |d|^2 - area, 1), no less than the level's least.
    void foveate(int eye, unsigned texture) override {
        constexpr unsigned kFeatureBits = 0x8BFB, kMinDensity = 0x8BFC, kFeatureQuery = 0x8BFD;
        constexpr int kEnable = 0x1, kScaledBins = 0x2;
        struct Level {
            float gain, area, least;
            const char* name;
        };
        static constexpr Level kLevels[] = {{0, 0, 1, "off"},
                                            {1.25f, 1.0f, 0.5f, "low"},
                                            {1.75f, 0.5f, 0.4f, "medium"},
                                            {2.5f, 0.25f, 0.25f, "high"}};
        const int level = std::clamp(m_settings.foveation, 0, 3);
        if (level == 0 || texture == 0 || eye < 0 || eye > 1) {
            return;
        }
        if (!m_foveation_checked) {
            m_foveation_checked = true;
            if (has_gl_extension("GL_QCOM_texture_foveated")) {
                m_foveate = reinterpret_cast<FoveateCall>(eglGetProcAddress("glTextureFoveationParametersQCOM"));
            }
            info(m_foveate != nullptr ? std::string("[openxr] foveated rendering: ") + kLevels[level].name
                                      : "[openxr] foveated rendering: the graphics chip does not offer it (GL_QCOM_texture_foveated)");
        }
        if (m_foveate == nullptr) {
            return;
        }
        // Straight ahead in the eye's picture: its fields of view are not even about it.
        const XrFovf& fov = m_views[eye].fov;
        const float left = std::tan(fov.angleLeft), right = std::tan(fov.angleRight);
        const float up = std::tan(fov.angleUp), down = std::tan(fov.angleDown);
        const float focal_x = right - left > 0.0f ? -(right + left) / (right - left) : 0.0f;
        const float focal_y = up - down > 0.0f ? -(up + down) / (up - down) : 0.0f;
        // The texture's own bits, each frame: the port makes its target anew at another size, and GL may give the new
        // texture the old one's number.
        GLint bound = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound);
        glBindTexture(GL_TEXTURE_2D, texture);
        GLint bits = 0;
        glGetTexParameteriv(GL_TEXTURE_2D, kFeatureBits, &bits);
        const bool fresh = (bits & kEnable) == 0 || m_foveated[eye] != texture;
        if (!fresh && std::abs(focal_x - m_focal[eye][0]) < 0.01f && std::abs(focal_y - m_focal[eye][1]) < 0.01f) {
            glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(bound));
            return;
        }
        if ((bits & kEnable) == 0) {
            GLint offered = 0;
            glGetTexParameteriv(GL_TEXTURE_2D, kFeatureQuery, &offered);
            if ((offered & (kEnable | kScaledBins)) != (kEnable | kScaledBins)) {
                glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(bound));
                warn("[openxr] foveated rendering: the eye's texture cannot be foveated (offered " + std::to_string(offered) + ")");
                m_foveate = nullptr;
                return;
            }
            glTexParameteri(GL_TEXTURE_2D, kFeatureBits, kEnable | kScaledBins);
            glTexParameterf(GL_TEXTURE_2D, kMinDensity, kLevels[level].least);
        }
        m_foveate(texture, 0, 0, focal_x, focal_y, kLevels[level].gain, kLevels[level].gain, kLevels[level].area);
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(bound));
        if (fresh) {
            char line[160];
            std::snprintf(line, sizeof line, "[openxr] foveated rendering: eye %d's picture (texture %u), straight ahead at (%.2f %.2f)",
                          eye, texture, focal_x, focal_y);
            info(line);
        }
        m_foveated[eye] = texture;
        m_focal[eye][0] = focal_x;
        m_focal[eye][1] = focal_y;
    }
#endif

    void set_refresh(float hertz) {
        if (!m_refresh_extension) {
            return;
        }
        auto enumerate =
            proc<PFN_xrEnumerateDisplayRefreshRatesFB>("xrEnumerateDisplayRefreshRatesFB");
        auto request = proc<PFN_xrRequestDisplayRefreshRateFB>("xrRequestDisplayRefreshRateFB");
        if (enumerate == nullptr || request == nullptr) {
            return;
        }
        uint32_t count = 0;
        enumerate(m_session, 0, &count, nullptr);
        std::vector<float> rates(count);
        enumerate(m_session, count, &count, rates.data());
        std::string listed;
        for (float rate : rates) {
            listed += std::to_string(static_cast<int>(rate)) + " ";
        }
        info("[openxr] refresh rates offered: " + listed);

        float chosen = 0;
        float lowest = 0;
        for (float rate : rates) {
            if (rate <= hertz + 0.5f && rate > chosen) {
                chosen = rate;
            }
            if (lowest <= 0.0f || rate < lowest) {
                lowest = rate;
            }
        }
        if (hertz <= 0.0f || lowest <= 0.0f) {
            return;
        }
        if (chosen <= 0.0f) {
            // Below all the headset has (a PC headset without 72 Hz, say): the nearest it has.
            chosen = lowest;
            info("[openxr] " + std::to_string(static_cast<int>(hertz)) + " Hz is not offered: the nearest it has");
        }
        if (check(request(m_session, chosen), "xrRequestDisplayRefreshRateFB")) {
            m_refresh_rate = chosen;
            info("[openxr] refresh rate " + std::to_string(static_cast<int>(chosen)) + " Hz");
        }
    }

    void state_changed(XrSessionState state) {
        m_state = state;
        m_focused = state == XR_SESSION_STATE_FOCUSED;
        // The runtime need not send a profile event for controllers already bound: asked here too.
        if (m_focused) {
            log_profiles();
        }
        switch (state) {
            case XR_SESSION_STATE_READY: {
                XrSessionBeginInfo begin{XR_TYPE_SESSION_BEGIN_INFO};
                begin.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                if (check(xrBeginSession(m_session, &begin), "xrBeginSession")) {
                    m_running = true;
                    info("[openxr] session begun");
                    session_began();
                }
                break;
            }
            case XR_SESSION_STATE_STOPPING:
                check(xrEndSession(m_session), "xrEndSession");
                m_running = false;
                info("[openxr] session stopped");
                break;
            case XR_SESSION_STATE_EXITING:
            case XR_SESSION_STATE_LOSS_PENDING:
                m_running = false;
                m_exiting = true;
                info("[openxr] the session ends: the game leaves");
                break;
            default:
                break;
        }
    }

    void poll_events() {
        XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
        while (xrPollEvent(m_instance, &event) == XR_SUCCESS) {
            switch (event.type) {
                case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
                    state_changed(reinterpret_cast<const XrEventDataSessionStateChanged*>(&event)->state);
                    break;
                case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
                    warn("[openxr] the instance is to be lost");
                    m_exiting = true;
                    break;
                case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED:
                    log_profiles();
                    break;
                case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
                    m_recentred = true;
                    break;
                case XR_TYPE_EVENT_DATA_PERF_SETTINGS_EXT: {
                    const auto* perf = reinterpret_cast<const XrEventDataPerfSettingsEXT*>(&event);
                    warn("[openxr] performance notice: domain " + std::to_string(perf->domain)
                         + " level " + std::to_string(perf->fromLevel) + " to "
                         + std::to_string(perf->toLevel));
                    break;
                }
                default:
                    break;
            }
            event = XrEventDataBuffer{XR_TYPE_EVENT_DATA_BUFFER};
        }
    }

    void locate_views(FrameInfo& out) {
        out.recentred = m_recentred;
        m_recentred = false;
        XrViewLocateInfo locate{XR_TYPE_VIEW_LOCATE_INFO};
        locate.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        locate.displayTime = m_frame_time;
        locate.space = m_local;
        XrViewState state{XR_TYPE_VIEW_STATE};
        m_views[0] = XrView{XR_TYPE_VIEW};
        m_views[1] = XrView{XR_TYPE_VIEW};
        uint32_t count = 0;
        m_views_valid = XR_SUCCEEDED(xrLocateViews(m_session, &locate, &state, 2, &count, m_views))
                        && count == 2
                        && (state.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) != 0
                        && (state.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) != 0;
        out.views_valid = m_views_valid;
        if (!m_views_valid) {
            return;
        }
        for (int eye = 0; eye < 2; ++eye) {
            out.eye[eye] = pose_of(m_views[eye].pose);
            out.fov[eye][0] = m_views[eye].fov.angleLeft;
            out.fov[eye][1] = m_views[eye].fov.angleRight;
            out.fov[eye][2] = m_views[eye].fov.angleUp;
            out.fov[eye][3] = m_views[eye].fov.angleDown;
        }
        XrSpaceLocation head{XR_TYPE_SPACE_LOCATION};
        if (XR_SUCCEEDED(xrLocateSpace(m_view, m_local, m_frame_time, &head))
            && (head.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) != 0) {
            out.head = pose_of(head.pose);
        } else {
            out.head = out.eye[0];
        }
        XrSpaceLocation above{XR_TYPE_SPACE_LOCATION};
        if (m_stage != XR_NULL_HANDLE && XR_SUCCEEDED(xrLocateSpace(m_view, m_stage, m_frame_time, &above))
            && (above.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0) {
            out.head_above_floor = above.pose.position.y;
        }
    }

    float read_float(XrAction action) {
        XrActionStateGetInfo get{XR_TYPE_ACTION_STATE_GET_INFO};
        get.action = action;
        XrActionStateFloat state{XR_TYPE_ACTION_STATE_FLOAT};
        return XR_SUCCEEDED(xrGetActionStateFloat(m_session, &get, &state)) && state.isActive
                   ? state.currentState
                   : 0.0f;
    }

    bool read_stick(XrAction action, float& x, float& y) {
        XrActionStateGetInfo get{XR_TYPE_ACTION_STATE_GET_INFO};
        get.action = action;
        XrActionStateVector2f state{XR_TYPE_ACTION_STATE_VECTOR2F};
        if (!XR_SUCCEEDED(xrGetActionStateVector2f(m_session, &get, &state)) || !state.isActive) {
            return false;
        }
        x = state.currentState.x;
        y = state.currentState.y;
        return true;
    }

    bool read_bool(XrAction action) {
        XrActionStateGetInfo get{XR_TYPE_ACTION_STATE_GET_INFO};
        get.action = action;
        XrActionStateBoolean state{XR_TYPE_ACTION_STATE_BOOLEAN};
        return XR_SUCCEEDED(xrGetActionStateBoolean(m_session, &get, &state)) && state.isActive
               && state.currentState == XR_TRUE;
    }

    void sync_input(FrameInfo& out) {
        for (int hand = 0; hand < 2; ++hand) {
            XrSpaceLocation grip{XR_TYPE_SPACE_LOCATION};
            XrSpaceLocation aim{XR_TYPE_SPACE_LOCATION};
            const XrSpaceLocationFlags valid =
                XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
            out.hand_bound[hand] = m_bound[hand];
            if (XR_SUCCEEDED(xrLocateSpace(m_grip_space[hand], m_local, m_frame_time, &grip))
                && (grip.locationFlags & valid) == valid) {
                out.hand_valid[hand] = true;
                out.hand_grip[hand] = pose_of(grip.pose);
                out.hand_aim[hand] = out.hand_grip[hand];
                if (XR_SUCCEEDED(xrLocateSpace(m_aim_space[hand], m_local, m_frame_time, &aim))
                    && (aim.locationFlags & valid) == valid) {
                    out.hand_aim[hand] = pose_of(aim.pose);
                }
            }
        }

        // Without focus the system has the controllers.
        if (!m_focused) {
            return;
        }
        XrActiveActionSet active{m_set, XR_NULL_PATH};
        XrActionsSyncInfo sync{XR_TYPE_ACTIONS_SYNC_INFO};
        sync.countActiveActionSets = 1;
        sync.activeActionSets = &active;
        if (!XR_SUCCEEDED(xrSyncActions(m_session, &sync))) {
            return;
        }
        Controls& c = out.controls;
        out.stick_active[kLeft] = read_stick(m_move, c.left_x, c.left_y);
        out.stick_active[kRight] = read_stick(m_look, c.right_x, c.right_y);
        c.left_trigger = read_float(m_trigger_left);
        c.right_trigger = read_float(m_trigger_right);
        c.left_grip = read_float(m_squeeze_left);
        c.right_grip = read_float(m_squeeze_right);
        c.left_click = read_bool(m_click_left);
        c.right_click = read_bool(m_click_right);
        c.a = read_bool(m_a);
        c.b = read_bool(m_b);
        c.x = read_bool(m_x);
        c.y = read_bool(m_y);
        c.menu = read_bool(m_menu);
        c.left_first_touch = read_bool(m_first_left);
        c.right_first_touch = read_bool(m_first_right);
        c.left_thumb_touch = read_bool(m_thumb_left);
        c.right_thumb_touch = read_bool(m_thumb_right);
        c.left_rest_touch = read_bool(m_rest_left);
#if defined(_WIN32)
        if (!m_first_sensed[kLeft]) {
            c.left_first_touch = c.left_trigger > 0.05f;
        }
        if (!m_first_sensed[kRight]) {
            c.right_first_touch = c.right_trigger > 0.05f;
        }
        c.left_thumb_touch = c.left_thumb_touch || !m_thumb_sensed[kLeft];
        c.right_thumb_touch = c.right_thumb_touch || !m_thumb_sensed[kRight];
#endif
    }

    // The pointer's picture, a soft white spot both quads use: filled once, the first time a menu
    // has it, and never touched again. The GL state it touches is put back.
    bool draw_pointer() {
        if (m_pointer_ready) {
            return true;
        }
        std::string why;
        if (m_pointer_chain.handle == XR_NULL_HANDLE
            && !create_target(m_pointer_chain, kPointerSize, kPointerSize, why)) {
            m_pointer = {};
            return false;
        }
        std::vector<std::uint8_t> pixels(static_cast<std::size_t>(kPointerSize) * kPointerSize * 4);
        for (int y = 0; y < kPointerSize; ++y) {
            for (int x = 0; x < kPointerSize; ++x) {
                const float dx = (static_cast<float>(x) + 0.5f) / kPointerSize * 2.0f - 1.0f;
                const float dy = (static_cast<float>(y) + 0.5f) / kPointerSize * 2.0f - 1.0f;
                const float edge = std::clamp((1.0f - std::sqrt(dx * dx + dy * dy)) * 4.0f, 0.0f, 1.0f);
                std::uint8_t* p = pixels.data() + (static_cast<std::size_t>(y) * kPointerSize + x) * 4;
                p[0] = p[1] = p[2] = static_cast<std::uint8_t>(255.0f * edge);  // premultiplied
                p[3] = static_cast<std::uint8_t>(255.0f * edge);
            }
        }
        // Every image of the chain, as the quads take whichever comes next.
        for (std::size_t i = 0; i < m_pointer_chain.images.size(); ++i) {
            XrSwapchainImageAcquireInfo acquire{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
            if (!check(xrAcquireSwapchainImage(m_pointer_chain.handle, &acquire, &m_pointer_chain.index),
                       "xrAcquireSwapchainImage(pointer)")) {
                return false;
            }
            XrSwapchainImageWaitInfo wait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
            wait.timeout = XR_INFINITE_DURATION;
            check(xrWaitSwapchainImage(m_pointer_chain.handle, &wait), "xrWaitSwapchainImage(pointer)");
            GLint texture = 0, unpack_buffer = 0, alignment = 0;
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
            glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpack_buffer);
            glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
            glBindTexture(GL_TEXTURE_2D, m_pointer_chain.images[m_pointer_chain.index].image);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kPointerSize, kPointerSize, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture));
            glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, static_cast<GLuint>(unpack_buffer));
            XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
            check(xrReleaseSwapchainImage(m_pointer_chain.handle, &release), "xrReleaseSwapchainImage(pointer)");
        }
        m_pointer_ready = true;
        return true;
    }

    // The warning's own small picture, made the first time one is shown: nothing of this runs in a
    // session with both controllers in hand. The GL state it touches is put back.
    bool draw_notice() {
        if (m_notice_chain.handle == XR_NULL_HANDLE) {
            std::string why;
            if (!create_target(m_notice_chain, kNoticeWidth, kNoticeHeight, why)) {
                m_notice.clear();
                return false;
            }
        }
        if (m_notice_pixels.empty()) {
            m_notice_pixels = render_notice(m_notice.c_str());
        }
        XrSwapchainImageAcquireInfo acquire{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        if (!check(xrAcquireSwapchainImage(m_notice_chain.handle, &acquire, &m_notice_chain.index),
                   "xrAcquireSwapchainImage(notice)")) {
            return false;
        }
        XrSwapchainImageWaitInfo wait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
        wait.timeout = XR_INFINITE_DURATION;
        check(xrWaitSwapchainImage(m_notice_chain.handle, &wait), "xrWaitSwapchainImage(notice)");

        GLint texture = 0, unpack_buffer = 0, alignment = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
        glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpack_buffer);
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glBindTexture(GL_TEXTURE_2D, m_notice_chain.images[m_notice_chain.index].image);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kNoticeWidth, kNoticeHeight, GL_RGBA, GL_UNSIGNED_BYTE,
                        m_notice_pixels.data());
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture));
        glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, static_cast<GLuint>(unpack_buffer));

        XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        check(xrReleaseSwapchainImage(m_notice_chain.handle, &release), "xrReleaseSwapchainImage(notice)");
        return true;
    }

#if defined(_WIN32)
    // A test's proof of what goes to the runtime (QUEST_PC_SHOTS=<folder>, vr/tests/pcvr_null.ps1 -Play, where the
    // headset is not there to look through): the left eye and the panel as they are in the swapchain, every 15
    // seconds or so, as .ppm files. Only with that set.
    void test_shots() {
        static const char* const folder = std::getenv("QUEST_PC_SHOTS");
        static int frames = 0;
        if (folder == nullptr || ++frames % 900 != 0 || sim_gl_proc == nullptr) {
            return;
        }
        const auto get_image = reinterpret_cast<void(__stdcall*)(GLenum, GLint, GLenum, GLenum, void*)>(
            sim_gl_proc("glGetTexImage"));
        if (get_image == nullptr) {
            return;
        }
        GLint texture = 0, pack_buffer = 0, alignment = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
        glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &pack_buffer);
        glGetIntegerv(GL_PACK_ALIGNMENT, &alignment);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        for (const auto& [target, name] : {std::pair{kTargetLeftEye, "eye"}, std::pair{kTargetPanel, "panel"}}) {
            const Swapchain& chain = m_targets[target];
            if (!chain.acquired) {
                continue;
            }
            std::vector<unsigned char> rgba(static_cast<size_t>(chain.width) * chain.height * 4);
            glBindTexture(GL_TEXTURE_2D, chain.images[chain.index].image);
            get_image(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
            const std::string path = std::string(folder) + "\\" + name + "-" + std::to_string(frames) + ".ppm";
            if (std::FILE* file = std::fopen(path.c_str(), "wb")) {
                std::fprintf(file, "P6\n%d %d\n255\n", chain.width, chain.height);
                std::vector<unsigned char> row(static_cast<size_t>(chain.width) * 3);
                for (int y = chain.height - 1; y >= 0; --y) {
                    const unsigned char* from = &rgba[static_cast<size_t>(y) * chain.width * 4];
                    for (int x = 0; x < chain.width; ++x) {
                        std::memcpy(&row[static_cast<size_t>(x) * 3], from + static_cast<size_t>(x) * 4, 3);
                    }
                    std::fwrite(row.data(), 1, row.size(), file);
                }
                std::fclose(file);
            }
        }
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture));
        glPixelStorei(GL_PACK_ALIGNMENT, alignment);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, static_cast<GLuint>(pack_buffer));
    }
#endif

    void release_targets() {
        const auto release_chain = [&](Swapchain& chain) {
            if (chain.acquired) {
                XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
                check(xrReleaseSwapchainImage(chain.handle, &release), "xrReleaseSwapchainImage");
                chain.acquired = false;
            }
        };
        for (Swapchain& chain : m_targets) {
            release_chain(chain);
        }
        for (int eye = 0; eye < 2; ++eye) {
            release_chain(m_motion[eye]);
            release_chain(m_motion_depth[eye]);
        }
    }

    // Each ten seconds a line for the log, and a rate above 72 that is not held given up: after
    // two windows short of 90% of it, back to 72 (Halo's rule).
    void statistics(const FrameInfo& out) {
        const double now = now_seconds();
        if (m_stats_start == 0.0) {
            m_stats_start = now;
            return;
        }
        const double seconds = now - m_stats_start;
        if (seconds < 10.0) {
            return;
        }
        const double rate = m_frames / seconds;
        // Smoother frames: the share of frames given with motion, over which the headset paces the game at half its rate.
        const double warped = m_frames > 0 ? static_cast<double>(m_warp_frames) / static_cast<double>(m_frames) : 0.0;
        // The frame's own work, and the displays missed: a frame longer than the period is shown a display late, which
        // the headset's own counter (OVR Metrics) shows as a dip, the "small wobbles" a tester saw.
        char line[320];
        std::snprintf(line, sizeof line,
                      "[vr-perf] %.1f frames/s over %.0f s, period %.2f ms, waited %.2f ms a frame, worked %.2f ms (most "
                      "%.2f, %ld over the period), %ld displays missed%s",
                      rate, seconds, out.frame_seconds * 1000.0,
                      m_frames > 0 ? m_wait_seconds / m_frames * 1000.0 : 0.0,
                      m_frames > 0 ? m_work_seconds / m_frames * 1000.0 : 0.0, m_work_most * 1000.0, m_work_over,
                      m_displays_missed,
                      m_space_warp_ready ? (", smoother frames on " + std::to_string(static_cast<int>(warped * 100.0 + 0.5))
                                            + "% of them").c_str()
                                         : "");
        info(line);
        m_work_seconds = 0;
        m_work_most = 0;
        m_work_over = 0;
        m_displays_missed = 0;
        register_threads();
        // Keep the refresh rate (on by default from 0.1.14, the owner's wish: smooth wherever the game keeps up) leaves
        // the chosen rate alone; off, a rate above 72 not held for two spells in a row goes back to 72. With smoother
        // frames the game is meant to draw half the rate.
        if (m_refresh_rate > 72.5f && m_focused) {
            const double wanted = m_refresh_rate * (warped > 0.5 ? 0.5 : 1.0);
            m_refresh_misses = rate < wanted * 0.9 ? m_refresh_misses + 1 : 0;
            if (m_refresh_misses >= 2 && m_settings.keep_refresh) {
                if (!m_refresh_kept_told) {
                    m_refresh_kept_told = true;
                    info("[openxr] " + std::to_string(static_cast<int>(m_refresh_rate))
                         + " Hz not held here: kept, as Keep the refresh rate asks");
                }
                m_refresh_misses = 0;
            } else if (m_refresh_misses >= 2) {
                const float held_not = m_refresh_rate;
                m_refresh_misses = 0;
                set_refresh(72.0f);
                if (m_refresh_rate < held_not) {
                    warn("[openxr] " + std::to_string(static_cast<int>(held_not)) + " Hz not held: back to "
                         + std::to_string(static_cast<int>(m_refresh_rate)) + " Hz");
                } else {
                    // A headset with nothing lower: asked once, and left at it.
                    warn("[openxr] " + std::to_string(static_cast<int>(held_not))
                         + " Hz not held, and the headset has nothing lower");
                    m_refresh_rate = 0;
                }
            }
        }
        m_frames = 0;
        m_warp_frames = 0;
        m_wait_seconds = 0;
        m_stats_start = now;
    }

    void (*m_log)(int, const char*);
    XrSettings m_settings;
    std::string m_last_error;
#if defined(__ANDROID__)
    JavaVM* m_vm = nullptr;
    jobject m_activity = nullptr;
#endif

    XrInstance m_instance = XR_NULL_HANDLE;
    XrSystemId m_system = XR_NULL_SYSTEM_ID;
    XrSession m_session = XR_NULL_HANDLE;
    XrSpace m_local = XR_NULL_HANDLE;
    XrSpace m_view = XR_NULL_HANDLE;
    XrSpace m_stage = XR_NULL_HANDLE;  // the floor, where the runtime knows one
    XrSessionState m_state = XR_SESSION_STATE_UNKNOWN;
    bool m_running = false;
    bool m_focused = false;
    bool m_exiting = false;
    bool m_frame_begun = false;
    bool m_views_valid = false;
    XrTime m_frame_time = 0;
    XrView m_views[2] = {{XR_TYPE_VIEW}, {XR_TYPE_VIEW}};
    bool m_recentred = false;
    XrPosef m_drawn_pose[2]{};  // where the eyes' pictures were taken, kept with them when they are shown again
    XrFovf m_drawn_fov[2]{};

    int64_t m_format = 0;
    bool m_srgb_control = false;
    Swapchain m_targets[kTargetCount];
    Swapchain m_notice_chain;
    Swapchain m_pointer_chain;
    PointerLayers m_pointer;
    bool m_pointer_ready = false;
    std::string m_notice;
    std::vector<std::uint8_t> m_notice_pixels;

    bool m_refresh_extension = false;
    bool m_performance_extension = false;
    bool m_thread_extension = false;
    // When frames are shown (next_display_in): the runtime's clock, and the last display times' steps.
    bool m_time_extension = false;
#if defined(_WIN32)
    PFN_xrConvertWin32PerformanceCounterToTimeKHR m_clock = nullptr;
#else
    PFN_xrConvertTimespecTimeToTimeKHR m_clock = nullptr;
#endif
    static constexpr int kDisplaySteps = 8;
    XrTime m_last_display = 0;
    XrDuration m_display_steps[kDisplaySteps]{};
    int m_display_step_at = 0;
    XrDuration m_display_ahead[kDisplaySteps]{};
    int m_display_ahead_at = 0;
#if defined(_WIN32)
    HMODULE m_loader = nullptr;
    std::vector<std::string> m_enabled;  // the controller families' extensions the runtime has
    bool m_first_sensed[2]{}, m_thumb_sensed[2]{};
#endif
    float m_refresh_rate = 0;
    int m_refresh_misses = 0;
    bool m_refresh_kept_told = false;

    // Smoother frames (create_space_warp_targets).
    std::vector<int64_t> m_formats;  // the swapchain formats the runtime offers
    bool m_space_warp_extension = false;
    bool m_space_warp_ready = false;
    int m_motion_width = 0, m_motion_height = 0;
    Swapchain m_motion[2];
    Swapchain m_motion_depth[2];
    SpaceWarpFrame m_warp;
    bool m_warp_set = false;
    long m_warp_frames = 0;  // frames given with motion since the last [vr-perf] line

    XrActionSet m_set = XR_NULL_HANDLE;
    XrPath m_hand_path[2] = {XR_NULL_PATH, XR_NULL_PATH};
    XrAction m_grip = XR_NULL_HANDLE, m_aim = XR_NULL_HANDLE;
    XrAction m_move = XR_NULL_HANDLE, m_look = XR_NULL_HANDLE;
    XrAction m_trigger_left = XR_NULL_HANDLE, m_trigger_right = XR_NULL_HANDLE;
    XrAction m_squeeze_left = XR_NULL_HANDLE, m_squeeze_right = XR_NULL_HANDLE;
    XrAction m_first_left = XR_NULL_HANDLE, m_first_right = XR_NULL_HANDLE;
    XrAction m_thumb_left = XR_NULL_HANDLE, m_thumb_right = XR_NULL_HANDLE;
    XrAction m_rest_left = XR_NULL_HANDLE;  // the left thumb rest's touch alone (the D-pad's "Left thumb rest")
    XrAction m_click_left = XR_NULL_HANDLE, m_click_right = XR_NULL_HANDLE;
    XrAction m_a = XR_NULL_HANDLE, m_b = XR_NULL_HANDLE, m_x = XR_NULL_HANDLE, m_y = XR_NULL_HANDLE;
    XrAction m_menu = XR_NULL_HANDLE;
    bool m_bound[2] = {false, false};
    XrAction m_haptic = XR_NULL_HANDLE;
    XrSpace m_grip_space[2] = {XR_NULL_HANDLE, XR_NULL_HANDLE};
    XrSpace m_aim_space[2] = {XR_NULL_HANDLE, XR_NULL_HANDLE};

    double m_stats_start = 0;
    double m_wait_seconds = 0;
    long m_frames = 0;
    // The frame's own work (from xrBeginFrame's return to xrEndFrame) since the last [vr-perf] line: in all, the most,
    // the frames longer than the display's period, and the displays the frames came too late for (display times
    // skipped between one frame and the next).
    double m_work_start = 0;
    double m_period_seconds = 1.0 / 72.0;
    double m_work_seconds = 0;
    double m_work_most = 0;
    long m_work_over = 0;
    long m_displays_missed = 0;
    int m_frame_periods = 1;  // the last frame's display periods: 2 with smoother frames (the headset's half rate)

    // Foveated rendering (foveate): GL_QCOM_texture_foveated's call, once looked for; each eye's texture set up last
    // and the focal point it was given.
    using FoveateCall = void (*)(unsigned, unsigned, unsigned, float, float, float, float, float);
    bool m_foveation_checked = false;
    FoveateCall m_foveate = nullptr;
    unsigned m_foveated[2]{};
    float m_focal[2][2]{};

    // The port's threads named for the runtime (register_threads): the thread ids already given to it.
    std::vector<int> m_threads_given;
};

}  // namespace

#if defined(__ANDROID__)
Headset* make_headset(void (*log)(int, const char*)) { return new OpenXr(log); }
#else
// The PC: xr_sim.cpp's make_headset gives this when no test script is set.
Headset* make_openxr_headset(void (*log)(int, const char*)) { return new OpenXr(log); }
#endif

}  // namespace quest

#endif  // __ANDROID__ || _WIN32
