// SPDX-License-Identifier: GPL-3.0-or-later
// The emulation session: settings and folders, one CPU thread that owns the VM, and the window the app hands over
// from its Surface. The CPU thread follows upstream's pcsx2-gsrunner (PCSX2 Dev Team, GPL-3.0+).

#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/WindowInfo.h"
#include "pcsx2/CDVD/CDVDcommon.h"
#include "pcsx2/Config.h"
#include "pcsx2/GS/GS.h"
#include "pcsx2/Host.h"
#include "pcsx2/Host/AudioStream.h"
#include "pcsx2/INISettingsInterface.h"
#include "pcsx2/ImGui/ImGuiManager.h"
#include "pcsx2/Input/InputManager.h"
#include "pcsx2/MTGS.h"
#include "pcsx2/VMManager.h"

#include <android/log.h>
#include <android/native_window_jni.h>
#include <atomic>
#include <deque>
#include <functional>
#include <future>
#include <jni.h>
#include <memory>
#include <mutex>
#include <thread>

namespace {

constexpr const char* LOG_TAG = "BetterAetherSX2";

std::unique_ptr<INISettingsInterface> s_settings;
std::vector<u8> s_font_data;

std::mutex s_window_mutex;
WindowInfo s_window;

std::thread s_cpu_thread;
std::atomic<std::thread::id> s_cpu_thread_id;

// Work other threads ask the CPU thread to run, drained once per frame in PumpMessagesOnCPUThread.
std::mutex s_cpu_queue_mutex;
std::deque<std::function<void()>> s_cpu_queue;

// Settings the Android build always needs, whatever the user file says.
void applyAndroidOverrides(SettingsInterface& si) {
    si.SetIntValue("EmuCore/GS", "Renderer", static_cast<int>(GSRendererType::VK));
    // Audio output comes with its own backend; until then the core runs silent.
    si.SetStringValue("SPU2/Output", "Backend", AudioStream::GetBackendName(AudioBackend::Null));
    // Input arrives from the app, not from SDL.
    si.SetBoolValue("InputSources", "SDL", false);
}

bool loadFonts() {
    const std::string path =
        EmuFolders::GetOverridableResourcePath("fonts" FS_OSPATH_SEPARATOR_STR "Roboto-Regular.ttf");
    std::optional<std::vector<u8>> data = FileSystem::ReadBinaryFile(path.c_str());
    if (!data) return false;
    s_font_data = std::move(*data);

    ImGuiManager::FontInfo font{};
    font.data = s_font_data;
    ImGuiManager::SetFonts({font});
    return true;
}

bool initialize(const std::string& data_dir, const std::string& resources_dir) {
    EmuFolders::AppRoot = data_dir;
    EmuFolders::DataRoot = data_dir;
    EmuFolders::Resources = resources_dir;
    // Upstream sets this inside SetDataDirectory, which picks desktop paths; the other folders are relative to it.
    EmuFolders::Settings = Path::Combine(data_dir, "inis");

    const char* hardware_error = nullptr;
    if (!VMManager::PerformEarlyHardwareChecks(&hardware_error)) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "Hardware check failed: %s", hardware_error);
        return false;
    }
    const std::string settings_path = Path::Combine(data_dir, "settings.ini");
    const bool first_run = !FileSystem::FileExists(settings_path.c_str());
    s_settings = std::make_unique<INISettingsInterface>(settings_path);
    if (!first_run) s_settings->Load();
    Host::Internal::SetBaseSettingsLayer(s_settings.get());
    if (first_run) VMManager::SetDefaultSettings(*s_settings, true, true, true, true, true);
    applyAndroidOverrides(*s_settings);
    s_settings->Save();

    EmuFolders::LoadConfig(*s_settings);
    if (!EmuFolders::EnsureFoldersExist()) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "Could not create the core folders in %s", data_dir.c_str());
        return false;
    }
    if (!loadFonts()) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "Fonts missing from %s", resources_dir.c_str());
        return false;
    }
    VMManager::Internal::LoadStartupSettings();
    return true;
}

// Boots with no disc, which runs the BIOS to its menu, and runs the VM until something stops it.
void cpuThreadMain() {
    s_cpu_thread_id = std::this_thread::get_id();
    if (VMManager::Internal::CPUThreadInitialize()) {
        VMManager::ApplySettings();
        VMBootParameters params;
        params.source_type = CDVD_SourceType::NoDisc;
        if (VMManager::Initialize(params) == VMBootResult::StartupSuccess) {
            VMManager::SetState(VMState::Running);
            while (VMManager::GetState() == VMState::Running) VMManager::Execute();
            VMManager::Shutdown(false);
        } else {
            __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "The VM failed to start");
        }
    }
    VMManager::Internal::CPUThreadShutdown();
    // Run what arrived during shutdown, so no blocking caller waits forever.
    Host::PumpMessagesOnCPUThread();
    s_cpu_thread_id = std::thread::id();
}

void setWindow(ANativeWindow* window) {
    std::lock_guard lock(s_window_mutex);
    if (s_window.window_handle) ANativeWindow_release(static_cast<ANativeWindow*>(s_window.window_handle));
    s_window = {};
    if (!window) return;
    s_window.type = WindowInfo::Type::Android;
    s_window.window_handle = window;
    s_window.surface_width = static_cast<u32>(ANativeWindow_getWidth(window));
    s_window.surface_height = static_cast<u32>(ANativeWindow_getHeight(window));
}

void stopSession() {
    if (!s_cpu_thread.joinable()) return;
    VMManager::SetState(VMState::Stopping);
    s_cpu_thread.join();
}

} // namespace

std::optional<WindowInfo> Host::AcquireRenderWindow(bool) {
    std::lock_guard lock(s_window_mutex);
    if (!s_window.window_handle) return std::nullopt;
    return s_window;
}

void Host::ReleaseRenderWindow() {}

std::optional<WindowInfo> Host::GetTopLevelWindowInfo() {
    std::lock_guard lock(s_window_mutex);
    return s_window;
}

void Host::RunOnCPUThread(std::function<void()> function, bool block) {
    // Waiting on itself would deadlock, so the CPU thread runs its own requests immediately.
    if (std::this_thread::get_id() == s_cpu_thread_id.load()) {
        function();
        return;
    }
    if (!block) {
        std::lock_guard lock(s_cpu_queue_mutex);
        s_cpu_queue.push_back(std::move(function));
        return;
    }
    std::promise<void> done;
    std::future<void> finished = done.get_future();
    {
        std::lock_guard lock(s_cpu_queue_mutex);
        s_cpu_queue.push_back([&function, &done] {
            function();
            done.set_value();
        });
    }
    finished.wait();
}

void Host::PumpMessagesOnCPUThread() {
    std::deque<std::function<void()>> pending;
    {
        std::lock_guard lock(s_cpu_queue_mutex);
        pending.swap(s_cpu_queue);
    }
    for (std::function<void()>& function : pending) function();
}

void Host::RequestVMShutdown(bool, bool, bool) {
    VMManager::SetState(VMState::Stopping);
}

extern "C" JNIEXPORT jboolean JNICALL Java_io_github_lucaaszsx_betteraethersx2_runtime_NativeRuntime_nativeInitialize(
    JNIEnv* env, jclass, jstring data_dir, jstring resources_dir) {
    const char* data_chars = env->GetStringUTFChars(data_dir, nullptr);
    const char* resources_chars = env->GetStringUTFChars(resources_dir, nullptr);
    const bool initialized = initialize(data_chars, resources_chars);
    env->ReleaseStringUTFChars(data_dir, data_chars);
    env->ReleaseStringUTFChars(resources_dir, resources_chars);
    return initialized;
}

extern "C" JNIEXPORT void JNICALL Java_io_github_lucaaszsx_betteraethersx2_runtime_NativeRuntime_nativeStartSession(
    JNIEnv* env, jclass, jobject surface) {
    if (s_cpu_thread.joinable()) return;
    setWindow(ANativeWindow_fromSurface(env, surface));
    s_cpu_thread = std::thread(cpuThreadMain);
}

// The session does not survive losing its surface yet: the app stops it and starts a new one.
extern "C" JNIEXPORT void JNICALL
Java_io_github_lucaaszsx_betteraethersx2_runtime_NativeRuntime_nativeStopSession(JNIEnv*, jclass) {
    stopSession();
    setWindow(nullptr);
}

extern "C" JNIEXPORT void JNICALL Java_io_github_lucaaszsx_betteraethersx2_runtime_NativeRuntime_nativeResizeSurface(
    JNIEnv*, jclass, jint width, jint height) {
    {
        std::lock_guard lock(s_window_mutex);
        if (!s_window.window_handle) return;
        s_window.surface_width = static_cast<u32>(width);
        s_window.surface_height = static_cast<u32>(height);
    }
    if (s_cpu_thread.joinable()) MTGS::ResizeDisplayWindow(static_cast<u32>(width), static_cast<u32>(height), 1.0f);
}
