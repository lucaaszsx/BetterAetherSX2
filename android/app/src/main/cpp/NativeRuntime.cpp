// SPDX-License-Identifier: GPL-3.0-or-later
// JNI entry points for io.github.lucaaszsx.betteraethersx2.runtime.NativeRuntime.

#include "bax/version.h"
#include "pcsx2/BuildVersion.h"

#include <jni.h>
#include <string>

extern "C" JNIEXPORT jstring JNICALL
Java_io_github_lucaaszsx_betteraethersx2_runtime_NativeRuntime_nativeCoreVersion(JNIEnv* env, jclass) {
    const std::string version = std::string(bax::coreVersion()) + " (PCSX2 " + BuildVersion::GitTag + ")";
    return env->NewStringUTF(version.c_str());
}
