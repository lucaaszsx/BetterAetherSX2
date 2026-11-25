// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "common/Pcsx2Types.h"

#include <jni.h>
#include <string_view>

// Keeps the latest error the core reported, so a session that ends because of it can tell the app why.
void recordCoreError(std::string_view message);

// Sets one input of the first controller, from any thread; the CPU thread applies it at the next vsync.
// bind is a PadDualshock2::Inputs value; value runs from 0 to 1.
void setPadInput(u32 bind, float value);

// The JNIEnv of the calling thread, attaching it to the JVM if needed. A thread attached here detaches when it exits.
JNIEnv* attachCurrentThread();

// True from the start of a session until its VM has shut down.
bool isVmActive();
