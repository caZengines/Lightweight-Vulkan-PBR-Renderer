#pragma once

#include <chrono>
#include <thread>

#if defined(_MSC_VER)
    #include <intrin.h>
#elif defined(__GNUC__) || defined(__clang__)
    #if defined(__x86_64__) || defined(__i386__)
        #include <immintrin.h>
    #elif defined(__aarch64__) || defined(__arm__)
        #include <arm_acle.h>
    #endif
#endif

namespace platform {

// One-instruction spin hint: no scheduler involvement, no memory barrier.
inline void relax() noexcept {
#if defined(_MSC_VER)
    #if defined(_M_X64) || defined(_M_IX86)
        _mm_pause();
    #elif defined(_M_ARM64) || defined(_M_ARM)
        __yield();
    #else
        std::this_thread::yield();
    #endif
#elif defined(__GNUC__) || defined(__clang__)
    #if defined(__x86_64__) || defined(__i386__)
        _mm_pause();
    #elif defined(__aarch64__) || defined(__arm__)
        __yield();
    #else
        std::this_thread::yield();
    #endif
#else
    std::this_thread::yield();
#endif
}

using SteadyClock = std::chrono::steady_clock;

// Absolute-time wait on the steady clock, for frame pacing.
//
// Sleeps until `deadline - margin`, never later than that by intent: the margin
// is left for the caller to burn with relax(), because a wait primitive's own
// wake latency is added *on top of* the requested delay.
//
// do not std::this_thread::sleep_until: on Windows the sleep granularity is one
// 15.625 ms tick, so a 6.94 ms request (144 FPS) actually sleeps ~15.1 ms -- any
// frame limiter above 64 FPS is silently pinned there, and timeBeginPeriod(1)
// does not change it. This wrapper uses a
// CREATE_WAITABLE_TIMER_HIGH_RESOLUTION timer instead (~0.2 ms late). On Linux
// clock_nanosleep() on CLOCK_MONOTONIC is already high resolution.
//
// Pass margin = 0us to sleep up to `deadline` exactly (the wake may then be a
// little late, which is fine when the caller does not need to spin).
void sleepUntil(SteadyClock::time_point deadline,
                std::chrono::microseconds margin = std::chrono::microseconds(200)) noexcept;

}  // namespace platform
