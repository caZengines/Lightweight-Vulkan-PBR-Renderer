// clock_nanosleep() is POSIX.1-2001; glibc hides it under a strict -std=c++NN
// unless a feature-test macro asks for it. (g++ defines _GNU_SOURCE and makes
// this redundant; clang with libc++ can need it.) Must precede every include.
#if defined(__linux__) && !defined(_POSIX_C_SOURCE)
    #define _POSIX_C_SOURCE 200809L
#endif

#include "platform/cpu.h"

#if defined(_WIN32)

    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    // CreateWaitableTimerExW + CREATE_WAITABLE_TIMER_HIGH_RESOLUTION (Win10 1803+).
    #ifndef _WIN32_WINNT
        #define _WIN32_WINNT 0x0A00
    #endif
    #include <windows.h>

    // Older SDK / MinGW headers do not carry the flag yet; the value is stable
    // and unrecognised flags make CreateWaitableTimerExW fail, which we handle.
    #ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
        #define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
    #endif

#elif defined(__linux__)

    #include <cerrno>
    #include <ctime>

#endif

namespace platform {

namespace {

#if defined(_WIN32)

// Per-thread high-resolution timer. Per-thread because SetWaitableTimer()
// retargets its handle process-wide, so two threads pacing at once would steal
// each other's wake-up. CreateWaitableTimerExW() needs Windows 10 1803+.
class HighResolutionTimer final {
public:
    HighResolutionTimer() noexcept
        : handle_(::CreateWaitableTimerExW(nullptr, nullptr,
                                          CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                          TIMER_ALL_ACCESS)) {}
    ~HighResolutionTimer() {
        if (handle_ != nullptr) ::CloseHandle(handle_);
    }

    HighResolutionTimer(const HighResolutionTimer&)            = delete;
    HighResolutionTimer& operator=(const HighResolutionTimer&) = delete;

    [[nodiscard]] HANDLE get() const noexcept { return handle_; }

private:
    HANDLE handle_ = nullptr;
};

HighResolutionTimer& waitTimer() noexcept {
    static thread_local HighResolutionTimer timer;
    return timer;
}

#endif

}  // namespace

void sleepUntil(SteadyClock::time_point deadline, std::chrono::microseconds margin) noexcept {
    const auto wake = deadline - margin;

#if defined(_WIN32)

    const auto remain = wake - SteadyClock::now();
    if (remain <= SteadyClock::duration::zero()) return;
    // Short waits are cheaper to spin than to hand to the kernel, whose own
    // overhead (~0.2 ms) would exceed the wait itself.
    if (remain < std::chrono::milliseconds(1)) return;

    const HANDLE timer = waitTimer().get();
    if (timer == nullptr) {
        // Pre-1803 Windows, or the flag was rejected: degrade to the coarse
        // sleep rather than spinning.
        std::this_thread::sleep_until(wake);
        return;
    }

    LARGE_INTEGER due{};
    due.QuadPart = -static_cast<LONGLONG>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(remain).count() / 100);
    if (due.QuadPart >= 0) return;  // sub-100 ns left: not worth a syscall

    ::SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE);
    ::WaitForSingleObject(timer, INFINITE);

#elif defined(__linux__)
    // clock_nanosleep() is relative here on purpose: that keeps us off any
    // assumption about steady_clock's epoch matching CLOCK_MONOTONIC. Remaining
    // time is recomputed each round, so EINTR never accumulates error.
    for (;;) {
        const auto remain = wake - SteadyClock::now();
        if (remain <= SteadyClock::duration::zero()) return;

        const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(remain);
        timespec request{};
        request.tv_sec  = static_cast<time_t>(ns.count() / 1'000'000'000);
        request.tv_nsec = static_cast<long>(ns.count() % 1'000'000'000);

        // clock_nanosleep returns the error number directly instead of setting errno.
        if (::clock_nanosleep(CLOCK_MONOTONIC, 0, &request, nullptr) != EINTR) return;
    }

#else
    std::this_thread::sleep_until(wake);
#endif
}

}  // namespace platform
