// SPDX-License-Identifier: GPL-3.0-or-later
// The emulation session: settings and folders, one CPU thread that owns the VM, and the window the app hands over
// from its Surface. The CPU thread follows upstream's pcsx2-gsrunner (PCSX2 Dev Team, GPL-3.0+).

#include "Session.h"

#include "common/Console.h"
#include "common/Error.h"
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
#include "pcsx2/SIO/Pad/Pad.h"
#include "pcsx2/SIO/Pad/PadDualshock2.h"
#include "pcsx2/VMManager.h"

#include <android/log.h>
#include <android/native_window_jni.h>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <jni.h>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>

namespace {

constexpr const char* LOG_TAG = "BetterAetherSX2";

std::unique_ptr<INISettingsInterface> s_settings;
std::vector<u8> s_font_data;

std::mutex s_window_mutex;
WindowInfo s_window;

// The core expects one CPU thread for the life of the process, as on desktop: it initializes once and runs one VM
// after another. Everything below is guarded by s_cpu_mutex.
std::thread s_cpu_thread;
std::atomic<std::thread::id> s_cpu_thread_id;
std::mutex s_cpu_mutex;
std::condition_variable s_cpu_wakeup;
// Work other threads ask the CPU thread to run, drained once per frame and whenever the thread is idle.
std::deque<std::function<void()>> s_cpu_queue;
bool s_boot_requested = false;
bool s_stop_requested = false;
bool s_vm_active = false;
std::condition_variable s_vm_finished;

JavaVM* s_jvm = nullptr;
// The CPU thread's JNIEnv, valid for the life of the process because that thread never exits.
JNIEnv* s_cpu_env = nullptr;
// The app object told when the session ends. Guarded by s_cpu_mutex.
jobject s_session_listener = nullptr;

std::mutex s_error_mutex;
std::string s_last_core_error;

int androidLogPriority(LOGLEVEL level) {
    switch (level) {
    case LOGLEVEL_ERROR:
        return ANDROID_LOG_ERROR;
    case LOGLEVEL_WARNING:
        return ANDROID_LOG_WARN;
    case LOGLEVEL_INFO:
        return ANDROID_LOG_INFO;
    default:
        return ANDROID_LOG_DEBUG;
    }
}

// The core writes its console to stdout, which Android discards; this sends it to logcat instead.
void logToLogcat(LOGLEVEL level, ConsoleColors, std::string_view message) {
    __android_log_print(androidLogPriority(level), "PCSX2", "%.*s", static_cast<int>(message.size()), message.data());
}

// Settings the Android build always needs, whatever the user file says.
void applyAndroidOverrides(SettingsInterface& si) {
    si.SetIntValue("EmuCore/GS", "Renderer", static_cast<int>(GSRendererType::VK));
    // SDL plays through AAudio on Android. PCSX2's copy of cubeb, the desktop default, has no Android backend.
    si.SetStringValue("SPU2/Output", "Backend", AudioStream::GetBackendName(AudioBackend::SDL));
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
    Log::SetHostOutputLevel(LOGLEVEL_DEV, logToLogcat);
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

std::string takeLastCoreError() {
    std::lock_guard lock(s_error_mutex);
    return std::exchange(s_last_core_error, {});
}

// A paused VM does not reach the per-frame pump, so the queue is drained here until a request resumes or stops it.
void waitWhilePaused() {
    {
        std::unique_lock lock(s_cpu_mutex);
        s_cpu_wakeup.wait(lock, [] { return !s_cpu_queue.empty(); });
    }
    Host::PumpMessagesOnCPUThread();
}

// Boots with no disc, which runs the BIOS to its menu, and runs the VM until something stops it. Returns why the VM
// could not start (possibly empty, when the core gave no reason), or nothing when it started.
std::optional<std::string> runVm() {
    VMManager::ApplySettings();
    VMBootParameters params;
    params.source_type = CDVD_SourceType::NoDisc;
    Error error;
    if (VMManager::Initialize(params, &error) != VMBootResult::StartupSuccess) {
        std::string reason = error.IsValid() ? error.GetDescription() : takeLastCoreError();
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "The VM failed to start: %s", reason.c_str());
        return reason;
    }
    {
        std::lock_guard lock(s_cpu_mutex);
        if (!s_stop_requested) VMManager::SetState(VMState::Running);
    }
    for (;;) {
        const VMState state = VMManager::GetState();
        if (state == VMState::Running)
            VMManager::Execute();
        else if (state == VMState::Paused)
            waitWhilePaused();
        else
            break;
    }
    VMManager::Shutdown(false);
    return std::nullopt;
}

// Tells the app the session is over. A VM that stops without the app asking has failed, and the reason is the last
// error the core reported. Called with s_cpu_mutex held, so the listener must only hand the news to another thread.
void notifySessionEnded(const std::optional<std::string>& boot_failure) {
    if (!s_session_listener) return;
    const bool failed = boot_failure.has_value() || !s_stop_requested;
    const std::string failure = boot_failure.value_or(failed ? takeLastCoreError() : std::string());
    jclass listener_class = s_cpu_env->GetObjectClass(s_session_listener);
    jmethodID on_ended = s_cpu_env->GetMethodID(listener_class, "onSessionEnded", "(ZLjava/lang/String;)V");
    jstring reason = s_cpu_env->NewStringUTF(failure.c_str());
    s_cpu_env->CallVoidMethod(s_session_listener, on_ended, failed, reason);
    // A pending exception would abort the next JNI call; log it to logcat and go on.
    if (s_cpu_env->ExceptionCheck()) {
        s_cpu_env->ExceptionDescribe();
        s_cpu_env->ExceptionClear();
    }
    s_cpu_env->DeleteLocalRef(reason);
    s_cpu_env->DeleteLocalRef(listener_class);
    s_cpu_env->DeleteGlobalRef(s_session_listener);
    s_session_listener = nullptr;
}

void cpuThreadMain() {
    s_cpu_thread_id = std::this_thread::get_id();
    s_jvm->AttachCurrentThread(&s_cpu_env, nullptr);
    if (!VMManager::Internal::CPUThreadInitialize()) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "The CPU thread failed to initialize");
        return;
    }
    for (;;) {
        bool boot = false;
        {
            std::unique_lock lock(s_cpu_mutex);
            s_cpu_wakeup.wait(lock, [] { return s_boot_requested || !s_cpu_queue.empty(); });
            boot = std::exchange(s_boot_requested, false);
        }
        Host::PumpMessagesOnCPUThread();
        if (!boot) continue;
        takeLastCoreError();
        const std::optional<std::string> boot_failure = runVm();
        // Run what arrived during shutdown, so no blocking caller waits forever.
        Host::PumpMessagesOnCPUThread();
        std::lock_guard lock(s_cpu_mutex);
        notifySessionEnded(boot_failure);
        s_vm_active = false;
        s_vm_finished.notify_all();
    }
}

// Replaces the window the GS draws into and returns the previous one, which the caller releases once the GS no longer
// uses it. Without a window the GS runs surfaceless: it keeps its state and presents nothing.
ANativeWindow* exchangeWindow(ANativeWindow* window) {
    std::lock_guard lock(s_window_mutex);
    ANativeWindow* previous = static_cast<ANativeWindow*>(s_window.window_handle);
    s_window = {};
    if (!window) return previous;
    s_window.type = WindowInfo::Type::Android;
    s_window.window_handle = window;
    s_window.surface_width = static_cast<u32>(ANativeWindow_getWidth(window));
    s_window.surface_height = static_cast<u32>(ANativeWindow_getHeight(window));
    return previous;
}

void releaseWindow(ANativeWindow* window) {
    if (window) ANativeWindow_release(window);
}

// Moves the GS to the current window. With block, returns once the GS has dropped the old one.
void updateGsWindow(bool block) {
    Host::RunOnCPUThread(
        [block] {
            if (!MTGS::IsOpen()) return;
            MTGS::UpdateDisplayWindow();
            if (block) MTGS::WaitGS(false);
        },
        block);
}

// Android forbids touching a surface after surfaceDestroyed returns, so this waits for the GS to let go of it.
void detachSurface() {
    ANativeWindow* previous = exchangeWindow(nullptr);
    if (!previous) return;
    updateGsWindow(true);
    releaseWindow(previous);
}

void attachSurface(ANativeWindow* window) {
    releaseWindow(exchangeWindow(window));
    updateGsWindow(false);
}

// The app pauses the VM while it is in the background; requests made before the VM exists do nothing.
void setPaused(bool paused) {
    Host::RunOnCPUThread([paused] {
        if (VMManager::HasValidVM()) VMManager::SetPaused(paused);
    });
}

// Takes ownership of the window and of the listener's global ref. A second start while a VM runs is ignored, so the
// VM never loses its window.
void startSession(ANativeWindow* window, JNIEnv* env, jobject listener) {
    std::lock_guard lock(s_cpu_mutex);
    if (s_vm_active) {
        ANativeWindow_release(window);
        env->DeleteGlobalRef(listener);
        return;
    }
    releaseWindow(exchangeWindow(window));
    s_session_listener = listener;
    s_vm_active = true;
    s_boot_requested = true;
    s_stop_requested = false;
    if (!s_cpu_thread.joinable()) s_cpu_thread = std::thread(cpuThreadMain);
    s_cpu_wakeup.notify_one();
}

// Returns once the VM has shut down, so the caller may release the window afterwards.
void stopSession() {
    std::unique_lock lock(s_cpu_mutex);
    if (!s_vm_active) return;
    s_stop_requested = true;
    // The state change must happen on the CPU thread: leaving the interpreter jumps back into that thread's
    // execution loop, which corrupts the stack when done from any other thread.
    s_cpu_queue.push_back([] {
        if (VMManager::GetState() == VMState::Running || VMManager::GetState() == VMState::Paused)
            VMManager::SetState(VMState::Stopping);
    });
    s_cpu_wakeup.notify_one();
    s_vm_finished.wait(lock, [] { return !s_vm_active; });
}

// Input arrives on the UI thread; the pads belong to the CPU thread, which applies it at the next vsync.
void setPadInput(u32 bind, float value) {
    if (bind >= PadDualshock2::Inputs::LENGTH) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "Ignored input for unknown pad bind %u", bind);
        return;
    }
    Host::RunOnCPUThread([bind, value] {
        if (VMManager::HasValidVM()) Pad::SetControllerState(0, bind, value);
    });
}

} // namespace

void recordCoreError(std::string_view message) {
    std::lock_guard lock(s_error_mutex);
    s_last_core_error = message;
}

std::optional<WindowInfo> Host::AcquireRenderWindow(bool) {
    std::lock_guard lock(s_window_mutex);
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
        std::lock_guard lock(s_cpu_mutex);
        s_cpu_queue.push_back(std::move(function));
        s_cpu_wakeup.notify_one();
        return;
    }
    std::promise<void> done;
    std::future<void> finished = done.get_future();
    {
        std::lock_guard lock(s_cpu_mutex);
        s_cpu_queue.push_back([&function, &done] {
            function();
            done.set_value();
        });
        s_cpu_wakeup.notify_one();
    }
    finished.wait();
}

void Host::PumpMessagesOnCPUThread() {
    std::deque<std::function<void()>> pending;
    {
        std::lock_guard lock(s_cpu_mutex);
        pending.swap(s_cpu_queue);
    }
    for (std::function<void()>& function : pending) function();
}

void Host::RequestVMShutdown(bool, bool, bool) {
    Host::RunOnCPUThread([] { VMManager::SetState(VMState::Stopping); });
}

extern "C" JNIEXPORT jboolean JNICALL Java_io_github_lucaaszsx_betteraethersx2_runtime_NativeRuntime_nativeInitialize(
    JNIEnv* env, jclass, jstring data_dir, jstring resources_dir) {
    // Taken here rather than in a JNI_OnLoad of our own: libbax.so pulls in libSDL3.so, and ART runs the first
    // JNI_OnLoad it finds through libbax.so, which must stay SDL's so SDL learns the JavaVM.
    env->GetJavaVM(&s_jvm);
    const char* data_chars = env->GetStringUTFChars(data_dir, nullptr);
    const char* resources_chars = env->GetStringUTFChars(resources_dir, nullptr);
    const bool initialized = initialize(data_chars, resources_chars);
    env->ReleaseStringUTFChars(data_dir, data_chars);
    env->ReleaseStringUTFChars(resources_dir, resources_chars);
    return initialized;
}

extern "C" JNIEXPORT void JNICALL Java_io_github_lucaaszsx_betteraethersx2_runtime_NativeRuntime_nativeStartSession(
    JNIEnv* env, jclass, jobject surface, jobject listener) {
    startSession(ANativeWindow_fromSurface(env, surface), env, env->NewGlobalRef(listener));
}

extern "C" JNIEXPORT void JNICALL
Java_io_github_lucaaszsx_betteraethersx2_runtime_NativeRuntime_nativeStopSession(JNIEnv*, jclass) {
    stopSession();
    releaseWindow(exchangeWindow(nullptr));
}

extern "C" JNIEXPORT void JNICALL
Java_io_github_lucaaszsx_betteraethersx2_runtime_NativeRuntime_nativeSetPaused(JNIEnv*, jclass, jboolean paused) {
    setPaused(paused);
}

extern "C" JNIEXPORT void JNICALL Java_io_github_lucaaszsx_betteraethersx2_runtime_NativeRuntime_nativeAttachSurface(
    JNIEnv* env, jclass, jobject surface) {
    attachSurface(ANativeWindow_fromSurface(env, surface));
}

extern "C" JNIEXPORT void JNICALL
Java_io_github_lucaaszsx_betteraethersx2_runtime_NativeRuntime_nativeDetachSurface(JNIEnv*, jclass) {
    detachSurface();
}

extern "C" JNIEXPORT void JNICALL Java_io_github_lucaaszsx_betteraethersx2_runtime_NativeRuntime_nativeResizeSurface(
    JNIEnv*, jclass, jint width, jint height) {
    {
        std::lock_guard lock(s_window_mutex);
        if (!s_window.window_handle) return;
        s_window.surface_width = static_cast<u32>(width);
        s_window.surface_height = static_cast<u32>(height);
    }
    // The GS may have failed to open, or not opened yet; it picks up the size from the window when it does.
    if (MTGS::IsOpen()) MTGS::ResizeDisplayWindow(static_cast<u32>(width), static_cast<u32>(height), 1.0f);
}

extern "C" JNIEXPORT void JNICALL Java_io_github_lucaaszsx_betteraethersx2_runtime_NativeRuntime_nativeSetPadInput(
    JNIEnv*, jclass, jint bind, jfloat value) {
    setPadInput(static_cast<u32>(bind), value);
}
