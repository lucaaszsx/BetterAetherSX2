// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <jni.h>

// Looks up the Kotlin side of content:// access. Called once, from a Java thread, before the core opens any file.
void initContentUris(JNIEnv* env);
