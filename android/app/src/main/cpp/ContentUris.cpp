// SPDX-License-Identifier: GPL-3.0-or-later
// Opens content:// URIs for the core (patch 0006), so it can read games from folders chosen through Android's Storage
// Access Framework. The Kotlin side owns the ContentResolver; any core thread may ask, so each call attaches its
// thread to the JVM first.

#include "ContentUris.h"

#include "Session.h"
#include "common/FileSystem.h"
#include "pcsx2/GameList.h"

#include <android/log.h>
#include <jni.h>

namespace {

constexpr const char* LOG_TAG = "BetterAetherSX2";

// Cached on a Java thread, because threads the core creates cannot find app classes through FindClass.
jclass s_content_access = nullptr;
jmethodID s_open_fd = nullptr;
jmethodID s_stat = nullptr;

jstring newString(JNIEnv* env, const char* text) {
    return env->NewStringUTF(text);
}

// A pending exception would abort the next JNI call; log it to logcat and report failure instead.
bool clearException(JNIEnv* env) {
    if (!env->ExceptionCheck()) return false;
    env->ExceptionDescribe();
    env->ExceptionClear();
    return true;
}

} // namespace

void initContentUris(JNIEnv* env) {
    jclass local = env->FindClass("io/github/lucaaszsx/betteraethersx2/runtime/ContentAccess");
    s_content_access = static_cast<jclass>(env->NewGlobalRef(local));
    env->DeleteLocalRef(local);
    s_open_fd = env->GetStaticMethodID(s_content_access, "openFd", "(Ljava/lang/String;)I");
    s_stat = env->GetStaticMethodID(s_content_access, "stat", "(Ljava/lang/String;)[J");
}

int FileSystem::Android::OpenContentUri(const char* uri) {
    JNIEnv* env = attachCurrentThread();
    jstring juri = newString(env, uri);
    const jint fd = env->CallStaticIntMethod(s_content_access, s_open_fd, juri);
    env->DeleteLocalRef(juri);
    return clearException(env) ? -1 : fd;
}

bool FileSystem::Android::StatContentUri(const char* uri, s64* size, s64* modified) {
    JNIEnv* env = attachCurrentThread();
    jstring juri = newString(env, uri);
    auto result = static_cast<jlongArray>(env->CallStaticObjectMethod(s_content_access, s_stat, juri));
    env->DeleteLocalRef(juri);
    if (clearException(env) || !result) return false;
    jlong values[2];
    env->GetLongArrayRegion(result, 0, 2, values);
    env->DeleteLocalRef(result);
    *size = values[0];
    *modified = values[1];
    return true;
}

// Returns {serial, title, region} for a PS2 disc image, or null. Reading the disc goes through the core's global CDVD
// interface, which a running VM also uses, so this refuses while one exists.
extern "C" JNIEXPORT jobjectArray JNICALL
Java_io_github_lucaaszsx_betteraethersx2_runtime_NativeRuntime_nativeIdentifyGame(JNIEnv* env, jclass, jstring uri) {
    if (isVmActive()) {
        __android_log_print(ANDROID_LOG_WARN, LOG_TAG, "Not identifying a game while a VM runs");
        return nullptr;
    }
    const char* uri_chars = env->GetStringUTFChars(uri, nullptr);
    GameList::Entry entry;
    const bool found = GameList::PopulateEntryFromPath(uri_chars, &entry);
    env->ReleaseStringUTFChars(uri, uri_chars);
    if (!found || entry.type != GameList::EntryType::PS2Disc) return nullptr;

    jobjectArray result = env->NewObjectArray(3, env->FindClass("java/lang/String"), nullptr);
    env->SetObjectArrayElement(result, 0, newString(env, entry.serial.c_str()));
    env->SetObjectArrayElement(result, 1, newString(env, entry.title.c_str()));
    env->SetObjectArrayElement(result, 2, newString(env, GameList::RegionToString(entry.region, false)));
    return result;
}
