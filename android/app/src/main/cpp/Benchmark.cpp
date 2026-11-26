// SPDX-License-Identifier: GPL-3.0-or-later
// Benchmark mode for runs driven from adb (tools/bench/run.sh). The core updates its performance metrics every half
// second on the GS thread; in benchmark mode each update becomes one JSON line in logcat under the BaxBench tag.

#include "pcsx2/Host.h"
#include "pcsx2/PerformanceMetrics.h"
#include "pcsx2/VMManager.h"

#include <android/log.h>
#include <atomic>
#include <jni.h>

namespace {

std::atomic<bool> s_benchmark = false;

} // namespace

void Host::OnPerformanceMetricsUpdated() {
    if (!s_benchmark) return;
    __android_log_print(ANDROID_LOG_INFO, "BaxBench",
                        R"({"fps":%.2f,"speed":%.1f,"frameMs":%.2f,"frameMsMin":%.2f,"frameMsMax":%.2f,)"
                        R"("eeThread":%.1f,"gsThread":%.1f,"vuThread":%.1f,"gpu":%.1f,"gpuMs":%.2f})",
                        PerformanceMetrics::GetFPS(), PerformanceMetrics::GetSpeed(),
                        PerformanceMetrics::GetAverageFrameTime(), PerformanceMetrics::GetMinimumFrameTime(),
                        PerformanceMetrics::GetMaximumFrameTime(), PerformanceMetrics::GetCPUThreadUsage(),
                        PerformanceMetrics::GetGSThreadUsage(), PerformanceMetrics::GetVUThreadUsage(),
                        PerformanceMetrics::GetGPUUsage(), PerformanceMetrics::GetGPUAverageTime());
}

extern "C" JNIEXPORT void JNICALL
Java_io_github_lucaaszsx_betteraethersx2_runtime_NativeRuntime_nativeSetBenchmark(JNIEnv*, jclass, jboolean enabled) {
    s_benchmark = enabled;
}
