// SPDX-License-Identifier: GPL-3.0-or-later
// The callbacks the PCSX2 core expects from its frontend. Adapted from upstream's pcsx2-gsrunner/Main.cpp
// (PCSX2 Dev Team, GPL-3.0+). Most are no-ops until the session loop, input and UI exist; each returns the value
// that tells the core the feature is unavailable. The callbacks tied to the session (window, CPU thread) live in
// Session.cpp.

#include "common/Assertions.h"
#include "common/HTTPDownloader.h"
#include "common/ProgressCallback.h"
#include "common/SmallString.h"
#include "pcsx2/Achievements.h"
#include "pcsx2/GS/GS.h"
#include "pcsx2/Host.h"
#include "pcsx2/ImGui/FullscreenUI.h"
#include "pcsx2/ImGui/ImGuiFullscreen.h"
#include "pcsx2/ImGui/ImGuiManager.h"
#include "pcsx2/Input/InputManager.h"
#include "pcsx2/VMManager.h"

#include <android/log.h>
#include <cstring>

namespace {

constexpr const char* LOG_TAG = "BetterAetherSX2";

void logMessage(int priority, std::string_view title, std::string_view message) {
    __android_log_print(priority, LOG_TAG, "%.*s: %.*s", static_cast<int>(title.size()), title.data(),
                        static_cast<int>(message.size()), message.data());
}

} // namespace

// The app has no network access, so there is never an HTTP downloader; callers treat null as unavailable.
std::unique_ptr<HTTPDownloader> HTTPDownloader::Create(std::string) {
    return nullptr;
}

void Host::CommitBaseSettingChanges() {}

void Host::LoadSettings(SettingsInterface&, std::unique_lock<std::mutex>&) {}

void Host::CheckForSettingsChanges(const Pcsx2Config&) {}

bool Host::RequestResetSettings(bool, bool, bool, bool, bool) {
    return false;
}

void Host::SetDefaultUISettings(SettingsInterface&) {}

bool Host::LocaleCircleConfirm() {
    return false;
}

std::unique_ptr<ProgressCallback> Host::CreateHostProgressCallback() {
    return ProgressCallback::CreateNullProgressCallback();
}

void Host::ReportInfoAsync(const std::string_view title, const std::string_view message) {
    logMessage(ANDROID_LOG_INFO, title, message);
}

void Host::ReportErrorAsync(const std::string_view title, const std::string_view message) {
    logMessage(ANDROID_LOG_ERROR, title, message);
}

void Host::OpenURL(const std::string_view) {}

bool Host::CopyTextToClipboard(const std::string_view) {
    return false;
}

std::string Host::GetTextFromClipboard() {
    return {};
}

void Host::BeginTextInput() {}

void Host::EndTextInput() {}

void Host::OnInputDeviceConnected(const std::string_view, const std::string_view) {}

void Host::OnInputDeviceDisconnected(const InputBindingKey, const std::string_view) {}

void Host::SetMouseMode(bool, bool) {}

void Host::SetMouseLock(bool) {}

void Host::BeginPresentFrame() {}

void Host::RequestResizeHostDisplay(s32, s32) {}

void Host::OnVMStarting() {}

void Host::OnVMStarted() {}

void Host::OnVMDestroyed() {}

void Host::OnVMPaused() {}

void Host::OnVMResumed() {}

void Host::OnGameChanged(const std::string&, const std::string&, const std::string&, const std::string&, u32, u32) {}

void Host::OnPerformanceMetricsUpdated() {}

void Host::OnSaveStateLoading(const std::string_view) {}

void Host::OnSaveStateLoaded(const std::string_view, bool) {}

void Host::OnSaveStateSaved(const std::string_view) {}

void Host::RefreshGameListAsync(bool) {}

void Host::CancelGameListRefresh() {}

bool Host::IsFullscreen() {
    return true;
}

void Host::SetFullscreen(bool) {}

void Host::OnCaptureStarted(const std::string&) {}

void Host::OnCaptureStopped() {}

void Host::RequestExitApplication(bool) {}

void Host::RequestExitBigPicture() {}

void Host::OnAchievementsLoginSuccess(const char*, u32, u32, u32) {}

void Host::OnAchievementsLoginRequested(Achievements::LoginRequestReason) {}

void Host::OnAchievementsHardcoreModeChanged(bool) {}

void Host::OnAchievementsRefreshed() {}

bool Host::InBatchMode() {
    return false;
}

bool Host::InNoGUIMode() {
    return false;
}

bool Host::ShouldPreferHostFileSelector() {
    return false;
}

void Host::OpenHostFileSelectorAsync(std::string_view, bool, FileSelectorCallback callback, FileSelectorFilters,
                                     std::string_view) {
    callback(std::string());
}

int Host::LocaleSensitiveCompare(std::string_view lhs, std::string_view rhs) {
    return lhs.compare(rhs);
}

// Core strings stay untranslated: the app shows its own UI strings, not the core's.
s32 Host::Internal::GetTranslatedStringImpl(const std::string_view, const std::string_view msg, char* tbuf,
                                            size_t tbuf_space) {
    if (msg.size() > tbuf_space) return -1;
    std::memcpy(tbuf, msg.data(), msg.size());
    return static_cast<s32>(msg.size());
}

std::string Host::TranslatePluralToString(const char*, const char* msg, const char*, int count) {
    const TinyString count_text = TinyString::from_format("{}", count);
    std::string text(msg);
    for (auto pos = text.find("%n"); pos != std::string::npos; pos = text.find("%n", pos + count_text.length()))
        text.replace(pos, 2, count_text.view());
    return text;
}

std::optional<u32> InputManager::ConvertHostKeyboardStringToCode(const std::string_view) {
    return std::nullopt;
}

std::optional<std::string> InputManager::ConvertHostKeyboardCodeToString(u32) {
    return std::nullopt;
}

const char* InputManager::ConvertHostKeyboardCodeToIcon(u32) {
    return nullptr;
}

BEGIN_HOTKEY_LIST(g_host_hotkeys)
END_HOTKEY_LIST()
