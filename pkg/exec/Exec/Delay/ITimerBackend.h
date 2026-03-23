#pragma once
#include <chrono>
#include <cstdint>
#include <functional>

namespace Exec
{
    /// Abstract timer backend for the Delay sender.
    ///
    /// Implementations must fire the registered callback at or after the given
    /// deadline, then allow the P2300 operation state to decide whether to
    /// complete with set_value() or set_stopped() (the CAS in DelaySharedState
    /// resolves any race with concurrent cancellation).
    ///
    /// Concurrency contract:
    ///   - ScheduleAt() may be called from any thread.
    ///   - Cancel() is advisory: it returns false if the callback is already firing
    ///     or has fired; callers must not rely on it for correctness (the CAS in
    ///     DelaySharedState handles the race).
    ///   - Tick() is called from the main/update thread only (via Domain::Update).
    ///     Thread-based backends (ThreadTimerBackend) implement it as a no-op.
    class ITimerBackend
    {
    public:
        using TimerId   = std::uint64_t;
        using TimePoint = std::chrono::steady_clock::time_point;
        using Duration  = std::chrono::steady_clock::duration;
        using Callback  = std::function<void()>;

        static constexpr TimerId InvalidTimerId = 0;

        virtual ~ITimerBackend() = default;

        /// Schedule a callback to be invoked at or after `deadline`.
        ///
        /// Returns a TimerId that can be passed to Cancel(). The callback must be
        /// invoked exactly once — either when the deadline expires or when Cancel()
        /// is not called in time to suppress invocation.
        virtual TimerId ScheduleAt(TimePoint deadline, Callback callback) = 0;

        /// Attempt to cancel a previously scheduled callback.
        ///
        /// Returns true if the callback was removed before firing.
        /// Returns false if the callback has already fired or is currently executing.
        ///
        /// If the callback is currently executing, Cancel() MUST block until it finishes
        /// before returning. This guarantee allows callers to safely destroy the data
        /// that the callback may access, immediately after Cancel() returns.
        virtual bool Cancel(TimerId id) noexcept = 0;

        /// Called from the main/update thread each frame (before DrainQueue).
        ///
        /// Loop-integrated backends (LoopTimerBackend) override this to fire
        /// expired timers. Thread-based backends may leave this as a no-op.
        virtual void Tick() noexcept {}
    };
}
