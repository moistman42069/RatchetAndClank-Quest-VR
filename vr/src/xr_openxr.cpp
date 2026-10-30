// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 the Ratchet & Clank Quest VR contributors
//
// OpenXR on the Quest, with the game's OpenGL ES context. The sequence is the one proven on the
// Quest 3 by the Halo CE Quest VR port's host_xr.c: the loader through the Java VM and activity,
// the session on the game's EGL context, sustained-high performance levels at session begin, an
// sRGB swapchain written without encoding (the game's picture is gamma-encoded already), and a
// refresh rate above 72 given up when it is not held.

#if defined(__ANDROID__)

#include <EGL/egl.h>
#include <GLES3/gl32.h>
#include <dlfcn.h>
#include <jni.h>
#include <sched.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#define XR_USE_PLATFORM_ANDROID
#define XR_USE_GRAPHICS_API_OPENGL_ES
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "xr.h"

#ifndef GL_FRAMEBUFFER_SRGB_EXT
#define GL_FRAMEBUFFER_SRGB_EXT 0x8DB9
#endif

namespace quest {
namespace {

constexpr const char* kRefreshExtension = "XR_FB_display_refresh_rate";
constexpr int kInfo = 1;
constexpr int kWarn = 2;
constexpr int kError = 3;

double now_seconds() {
    timespec t{};
    clock_gettime(CLOCK_MONOTONIC, &t);
    return static_cast<double>(t.tv_sec) + static_cast<double>(t.tv_nsec) * 1e-9;
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
    std::vector<XrSwapchainImageOpenGLESKHR> images;
    bool acquired = false;
    uint32_t index = 0;
};

class OpenXr final : public Headset {
public:
    explicit OpenXr(void (*log)(int, const char*)) : m_log(log) {}

    bool start(const XrSettings& settings, std::string& why) override {
        m_settings = settings;
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
            timespec pause{0, 10 * 1000 * 1000};
            nanosleep(&pause, nullptr);
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
        const double after = now_seconds();
        m_wait_seconds += after - before;
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

    unsigned acquire(Target target) override {
        Swapchain& chain = m_targets[target];
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

    void finish_frame(bool eyes, const PanelLayer& panel) override {
        if (!m_frame_begun) {
            return;
        }
        m_frame_begun = false;

        // The window is not swapped in VR, so nothing else sends the frame's drawing on its way.
        glFlush();

        XrCompositionLayerProjectionView views[2]{};
        XrCompositionLayerProjection projection{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
        XrCompositionLayerQuad quad{XR_TYPE_COMPOSITION_LAYER_QUAD};
        const XrCompositionLayerBaseHeader* layers[2];
        uint32_t count = 0;

        const bool have_eyes = eyes && m_views_valid && m_targets[kTargetLeftEye].acquired
                               && m_targets[kTargetRightEye].acquired;
        if (have_eyes) {
            for (int eye = 0; eye < 2; ++eye) {
                const Swapchain& chain = m_targets[kTargetLeftEye + eye];
                views[eye].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
                views[eye].pose = m_views[eye].pose;
                views[eye].fov = m_views[eye].fov;
                views[eye].subImage.swapchain = chain.handle;
                views[eye].subImage.imageRect.extent = {chain.width, chain.height};
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

        release_targets();

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
            xrDestroySession(m_session);
            m_session = XR_NULL_HANDLE;
        }
        if (m_instance != XR_NULL_HANDLE) {
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
        if (m_instance != XR_NULL_HANDLE) {
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

        XrSystemGetInfo system{XR_TYPE_SYSTEM_GET_INFO};
        system.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
        if (!check(xrGetSystem(m_instance, &system, &m_system), "xrGetSystem")) {
            why = m_last_error;
            return false;
        }
        XrSystemProperties properties{XR_TYPE_SYSTEM_PROPERTIES};
        if (XR_SUCCEEDED(xrGetSystemProperties(m_instance, m_system, &properties))) {
            info(std::string("[openxr] system: ") + properties.systemName);
        }
        return true;
    }

    bool create_session(std::string& why) {
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
        m_srgb_control = has_gl_extension("GL_EXT_sRGB_write_control");
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

    bool create_target(Swapchain& chain, int width, int height, std::string& why) {
        XrSwapchainCreateInfo create{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        create.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
        create.format = m_format;
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
        chain.images.assign(count, XrSwapchainImageOpenGLESKHR{XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR});
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
        return create_target(m_targets[kTargetPanel], m_settings.panel_width, m_settings.panel_height, why);
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
        m_stick = make_action(XR_ACTION_TYPE_VECTOR2F_INPUT, "stick", "Stick", true);
        m_trigger = make_action(XR_ACTION_TYPE_FLOAT_INPUT, "trigger", "Trigger", true);
        m_squeeze = make_action(XR_ACTION_TYPE_FLOAT_INPUT, "squeeze", "Grip", true);
        m_click = make_action(XR_ACTION_TYPE_BOOLEAN_INPUT, "stick_click", "Stick click", true);
        m_a = make_action(XR_ACTION_TYPE_BOOLEAN_INPUT, "button_a", "A", false);
        m_b = make_action(XR_ACTION_TYPE_BOOLEAN_INPUT, "button_b", "B", false);
        m_x = make_action(XR_ACTION_TYPE_BOOLEAN_INPUT, "button_x", "X", false);
        m_y = make_action(XR_ACTION_TYPE_BOOLEAN_INPUT, "button_y", "Y", false);
        m_haptic = make_action(XR_ACTION_TYPE_VIBRATION_OUTPUT, "haptic", "Vibration", true);

        const std::vector<XrActionSuggestedBinding> touch = {
            {m_grip, path("/user/hand/left/input/grip/pose")},
            {m_grip, path("/user/hand/right/input/grip/pose")},
            {m_aim, path("/user/hand/left/input/aim/pose")},
            {m_aim, path("/user/hand/right/input/aim/pose")},
            {m_stick, path("/user/hand/left/input/thumbstick")},
            {m_stick, path("/user/hand/right/input/thumbstick")},
            {m_click, path("/user/hand/left/input/thumbstick/click")},
            {m_click, path("/user/hand/right/input/thumbstick/click")},
            {m_trigger, path("/user/hand/left/input/trigger/value")},
            {m_trigger, path("/user/hand/right/input/trigger/value")},
            {m_squeeze, path("/user/hand/left/input/squeeze/value")},
            {m_squeeze, path("/user/hand/right/input/squeeze/value")},
            {m_a, path("/user/hand/right/input/a/click")},
            {m_b, path("/user/hand/right/input/b/click")},
            {m_x, path("/user/hand/left/input/x/click")},
            {m_y, path("/user/hand/left/input/y/click")},
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

        for (int hand = 0; hand < 2; ++hand) {
            XrActionSpaceCreateInfo space{XR_TYPE_ACTION_SPACE_CREATE_INFO};
            space.poseInActionSpace.orientation.w = 1.0f;
            space.subactionPath = m_hand_path[hand];
            space.action = m_grip;
            xrCreateActionSpace(m_session, &space, &m_grip_space[hand]);
            space.action = m_aim;
            xrCreateActionSpace(m_session, &space, &m_aim_space[hand]);
        }

        XrSessionActionSetsAttachInfo attach{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
        attach.countActionSets = 1;
        attach.actionSets = &m_set;
        if (!check(xrAttachSessionActionSets(m_session, &attach), "xrAttachSessionActionSets")) {
            why = m_last_error;
            return false;
        }
        return true;
    }

    void session_began() {
        // Sustained high: the game thread does the world, the renderer's calls and the sound's
        // feeding, and at a lower level the sound starves first (Halo's Quest finding).
        if (m_performance_extension) {
            auto set_level =
                proc<PFN_xrPerfSettingsSetPerformanceLevelEXT>("xrPerfSettingsSetPerformanceLevelEXT");
            if (set_level != nullptr) {
                set_level(m_session, XR_PERF_SETTINGS_DOMAIN_CPU_EXT, XR_PERF_SETTINGS_LEVEL_SUSTAINED_HIGH_EXT);
                set_level(m_session, XR_PERF_SETTINGS_DOMAIN_GPU_EXT, XR_PERF_SETTINGS_LEVEL_SUSTAINED_HIGH_EXT);
                info("[openxr] performance levels: CPU and GPU sustained high");
            }
        }
        if (m_thread_extension) {
            auto set_thread = proc<PFN_xrSetAndroidApplicationThreadKHR>("xrSetAndroidApplicationThreadKHR");
            if (set_thread != nullptr) {
                set_thread(m_session, XR_ANDROID_THREAD_TYPE_RENDERER_MAIN_KHR,
                           static_cast<uint32_t>(syscall(SYS_gettid)));
            }
        }
        set_refresh(m_settings.refresh_hz);
    }

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
        for (float rate : rates) {
            if (rate <= hertz + 0.5f && rate > chosen) {
                chosen = rate;
            }
        }
        if (hertz <= 0.0f || chosen <= 0.0f) {
            return;
        }
        if (check(request(m_session, chosen), "xrRequestDisplayRefreshRateFB")) {
            m_refresh_rate = chosen;
            info("[openxr] refresh rate " + std::to_string(static_cast<int>(chosen)) + " Hz");
        }
    }

    void state_changed(XrSessionState state) {
        m_state = state;
        m_focused = state == XR_SESSION_STATE_FOCUSED;
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
    }

    float read_float(XrAction action, int hand) {
        XrActionStateGetInfo get{XR_TYPE_ACTION_STATE_GET_INFO};
        get.action = action;
        get.subactionPath = m_hand_path[hand];
        XrActionStateFloat state{XR_TYPE_ACTION_STATE_FLOAT};
        return XR_SUCCEEDED(xrGetActionStateFloat(m_session, &get, &state)) && state.isActive
                   ? state.currentState
                   : 0.0f;
    }

    bool read_bool(XrAction action, XrPath subaction) {
        XrActionStateGetInfo get{XR_TYPE_ACTION_STATE_GET_INFO};
        get.action = action;
        get.subactionPath = subaction;
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
            if (XR_SUCCEEDED(xrLocateSpace(m_grip_space[hand], m_local, m_frame_time, &grip))
                && (grip.locationFlags & valid) == valid
                && XR_SUCCEEDED(xrLocateSpace(m_aim_space[hand], m_local, m_frame_time, &aim))
                && (aim.locationFlags & valid) == valid) {
                out.hand_valid[hand] = true;
                out.hand_grip[hand] = pose_of(grip.pose);
                out.hand_aim[hand] = pose_of(aim.pose);
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
        for (int hand = 0; hand < 2; ++hand) {
            XrActionStateGetInfo get{XR_TYPE_ACTION_STATE_GET_INFO};
            get.action = m_stick;
            get.subactionPath = m_hand_path[hand];
            XrActionStateVector2f stick{XR_TYPE_ACTION_STATE_VECTOR2F};
            const bool active_stick =
                XR_SUCCEEDED(xrGetActionStateVector2f(m_session, &get, &stick)) && stick.isActive;
            const float x = active_stick ? stick.currentState.x : 0.0f;
            const float y = active_stick ? stick.currentState.y : 0.0f;
            (hand == kLeft ? c.left_x : c.right_x) = x;
            (hand == kLeft ? c.left_y : c.right_y) = y;
            (hand == kLeft ? c.left_trigger : c.right_trigger) = read_float(m_trigger, hand);
            (hand == kLeft ? c.left_grip : c.right_grip) = read_float(m_squeeze, hand);
            (hand == kLeft ? c.left_click : c.right_click) = read_bool(m_click, m_hand_path[hand]);
        }
        c.a = read_bool(m_a, XR_NULL_PATH);
        c.b = read_bool(m_b, XR_NULL_PATH);
        c.x = read_bool(m_x, XR_NULL_PATH);
        c.y = read_bool(m_y, XR_NULL_PATH);
    }

    void release_targets() {
        for (Swapchain& chain : m_targets) {
            if (chain.acquired) {
                XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
                check(xrReleaseSwapchainImage(chain.handle, &release), "xrReleaseSwapchainImage");
                chain.acquired = false;
            }
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
        char line[160];
        std::snprintf(line, sizeof line,
                      "[vr-perf] %.1f frames/s over %.0f s, period %.2f ms, waited %.2f ms a frame",
                      rate, seconds, out.frame_seconds * 1000.0,
                      m_frames > 0 ? m_wait_seconds / m_frames * 1000.0 : 0.0);
        info(line);
        if (m_refresh_rate > 72.5f && m_focused) {
            m_refresh_misses = rate < m_refresh_rate * 0.9f ? m_refresh_misses + 1 : 0;
            if (m_refresh_misses >= 2) {
                warn("[openxr] " + std::to_string(static_cast<int>(m_refresh_rate))
                     + " Hz not held: back to 72 Hz");
                m_refresh_misses = 0;
                set_refresh(72.0f);
            }
        }
        m_frames = 0;
        m_wait_seconds = 0;
        m_stats_start = now;
    }

    void (*m_log)(int, const char*);
    XrSettings m_settings;
    std::string m_last_error;
    JavaVM* m_vm = nullptr;
    jobject m_activity = nullptr;

    XrInstance m_instance = XR_NULL_HANDLE;
    XrSystemId m_system = XR_NULL_SYSTEM_ID;
    XrSession m_session = XR_NULL_HANDLE;
    XrSpace m_local = XR_NULL_HANDLE;
    XrSpace m_view = XR_NULL_HANDLE;
    XrSessionState m_state = XR_SESSION_STATE_UNKNOWN;
    bool m_running = false;
    bool m_focused = false;
    bool m_exiting = false;
    bool m_frame_begun = false;
    bool m_views_valid = false;
    XrTime m_frame_time = 0;
    XrView m_views[2] = {{XR_TYPE_VIEW}, {XR_TYPE_VIEW}};

    int64_t m_format = 0;
    bool m_srgb_control = false;
    Swapchain m_targets[kTargetCount];

    bool m_refresh_extension = false;
    bool m_performance_extension = false;
    bool m_thread_extension = false;
    float m_refresh_rate = 0;
    int m_refresh_misses = 0;

    XrActionSet m_set = XR_NULL_HANDLE;
    XrPath m_hand_path[2] = {XR_NULL_PATH, XR_NULL_PATH};
    XrAction m_grip = XR_NULL_HANDLE, m_aim = XR_NULL_HANDLE, m_stick = XR_NULL_HANDLE;
    XrAction m_trigger = XR_NULL_HANDLE, m_squeeze = XR_NULL_HANDLE, m_click = XR_NULL_HANDLE;
    XrAction m_a = XR_NULL_HANDLE, m_b = XR_NULL_HANDLE, m_x = XR_NULL_HANDLE, m_y = XR_NULL_HANDLE;
    XrAction m_haptic = XR_NULL_HANDLE;
    XrSpace m_grip_space[2] = {XR_NULL_HANDLE, XR_NULL_HANDLE};
    XrSpace m_aim_space[2] = {XR_NULL_HANDLE, XR_NULL_HANDLE};

    double m_stats_start = 0;
    double m_wait_seconds = 0;
    long m_frames = 0;
};

}  // namespace

Headset* make_headset(void (*log)(int, const char*)) { return new OpenXr(log); }

}  // namespace quest

#endif  // __ANDROID__
