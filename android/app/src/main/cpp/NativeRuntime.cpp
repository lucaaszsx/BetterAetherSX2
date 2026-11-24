// SPDX-License-Identifier: GPL-3.0-or-later
// JNI entry points for io.github.lucaaszsx.betteraethersx2.runtime.NativeRuntime.

#include "bax/version.h"
#include "common/Pcsx2Types.h"
#include "fmt/format.h"
#include "pcsx2/BuildVersion.h"
#include "pcsx2/ps2/BiosTools.h"

#include <jni.h>
#include <string>

extern "C" JNIEXPORT jstring JNICALL
Java_io_github_lucaaszsx_betteraethersx2_runtime_NativeRuntime_nativeCoreVersion(JNIEnv* env, jclass) {
    // A shallow checkout of the submodule has no tags, so the commit hash stands in for the release name.
    const std::string upstream = *BuildVersion::GitTag ? BuildVersion::GitTag : BuildVersion::GitHash;
    const std::string version = std::string(bax::coreVersion()) + " (PCSX2 " + upstream + ")";
    return env->NewStringUTF(version.c_str());
}

// Returns {region, version} when the file is a PS2 BIOS the core can boot, or null otherwise. Uses the same check
// the core runs at boot, so a file accepted here is a file the core will load.
extern "C" JNIEXPORT jobjectArray JNICALL
Java_io_github_lucaaszsx_betteraethersx2_runtime_NativeRuntime_nativeIdentifyBios(JNIEnv* env, jclass, jstring path) {
    const char* path_chars = env->GetStringUTFChars(path, nullptr);
    u32 version = 0;
    u32 region = 0;
    std::string description;
    std::string zone;
    const bool is_bios = IsBIOS(path_chars, version, description, region, zone);
    env->ReleaseStringUTFChars(path, path_chars);
    if (!is_bios) return nullptr;

    constexpr u32 MAJOR_SHIFT = 8;
    constexpr u32 MINOR_MASK = 0xFF;
    const std::string version_text = fmt::format("{}.{:02}", version >> MAJOR_SHIFT, version & MINOR_MASK);
    jobjectArray result = env->NewObjectArray(2, env->FindClass("java/lang/String"), nullptr);
    env->SetObjectArrayElement(result, 0, env->NewStringUTF(zone.c_str()));
    env->SetObjectArrayElement(result, 1, env->NewStringUTF(version_text.c_str()));
    return result;
}
