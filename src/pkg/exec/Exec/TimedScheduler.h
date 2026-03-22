#pragma once
#include "Delay/DelaySender.h"
#include "Delay/IDelayBackend.h"
#include "UpdateScheduler.h"

#include <chrono>
#include <exec/timed_scheduler.hpp>
#include <stdexec/execution.hpp>

namespace Exec
{
    /// A timed scheduler that integrates with a frame-based update loop.
    ///
    /// Wraps UpdateScheduler (for zero-delay frame scheduling) and an IDelayBackend
    /// (for timed delays). Satisfies both stdexec::scheduler and exec::timed_scheduler,
    /// so code that uses exec::schedule_after or co_await exec::schedule_after works
    /// without any project-specific helpers.
    ///
    /// Ownership: TimedScheduler owns the UpdateScheduler; it holds a non-owning
    /// pointer to the IDelayBackend — lifetime of the backend is managed externally
    /// (typically by App::Exec::Domain which provides a unique_ptr<IDelayBackend>).
    class TimedScheduler
    {
    public:
        struct Scheduler; // forward-declared for FrameSender::Env

    private:
        /// Frame-aligned sender returned by Scheduler::schedule().
        ///
        /// Delegates the actual enqueueing to UpdateScheduler::Sender::connect(),
        /// but overrides the Env so the completion scheduler is reported as
        /// TimedScheduler::Scheduler (required by stdexec::scheduler concept:
        /// get_completion_scheduler(get_env(schedule(sched))) == sched).
        struct FrameSender
        {
            using sender_concept        = stdexec::sender_t;
            using completion_signatures = stdexec::completion_signatures<
                stdexec::set_value_t(), stdexec::set_stopped_t()>;

            TimedScheduler* ctx;

            /// Connect by delegating to the underlying UpdateScheduler::Sender.
            /// The return type is UpdateScheduler::Operation<Receiver> (deduced via
            /// auto — naming the private type is not required here).
            template <class Receiver>
            auto connect(Receiver rcvr) const
            {
                return ctx->_frameScheduler.GetScheduler().schedule()
                           .connect(static_cast<Receiver&&>(rcvr));
            }

            struct Env
            {
                TimedScheduler* ctx;

                template <class CPO>
                [[nodiscard]] auto query(stdexec::get_completion_scheduler_t<CPO>) const noexcept
                    -> Scheduler; // defined after Scheduler is complete
            };

            [[nodiscard]] auto get_env() const noexcept -> Env { return {ctx}; }
        };

    public:
        /// Lightweight scheduler handle satisfying both stdexec::scheduler and
        /// exec::timed_scheduler.
        ///
        ///   schedule()        — enqueue work at the next frame boundary (no delay)
        ///   schedule_after()  — enqueue work after a given duration
        ///   schedule_at()     — enqueue work at a specific steady_clock time point
        ///   now()             — returns steady_clock::now()
        struct Scheduler
        {
            using scheduler_concept = stdexec::scheduler_t;

            TimedScheduler* ctx;

            /// Returns the UpdateScheduler pointer — used by DelaySender::Operation
            /// to enqueue the shared state back to the frame queue when a delay completes.
            [[nodiscard]] auto GetUpdateScheduler() const noexcept -> UpdateScheduler*
            {
                return &ctx->_frameScheduler;
            }

            [[nodiscard]] auto schedule() const noexcept -> FrameSender { return {ctx}; }

            [[nodiscard]] auto now() const noexcept -> std::chrono::steady_clock::time_point
            {
                return std::chrono::steady_clock::now();
            }

            [[nodiscard]] auto schedule_at(std::chrono::steady_clock::time_point tp) const noexcept
                -> DelaySender<Scheduler>
            {
                return {*this, ctx->_backend, tp};
            }

            [[nodiscard]] auto schedule_after(std::chrono::steady_clock::duration dur) const noexcept
                -> DelaySender<Scheduler>
            {
                return schedule_at(now() + dur);
            }

            auto operator==(const Scheduler&) const noexcept -> bool = default;
        };

        explicit TimedScheduler(IDelayBackend* backend) noexcept
            : _backend(backend)
        {}

        [[nodiscard]] auto GetScheduler() noexcept -> Scheduler { return {this}; }

        std::size_t DrainQueue() { return _frameScheduler.DrainQueue(); }

        /// Call once per frame (before DrainQueue) to fire expired timers.
        /// Thread-based backends implement this as a no-op.
        void TickBackend() noexcept { _backend->Tick(); }

    private:
        UpdateScheduler _frameScheduler;
        IDelayBackend*  _backend; // non-owning; lifetime managed by caller (Domain)
    };

    // Out-of-line definition: now that Scheduler is complete, the query() body compiles.
    template <class CPO>
    inline auto TimedScheduler::FrameSender::Env::query(
        stdexec::get_completion_scheduler_t<CPO>) const noexcept -> TimedScheduler::Scheduler
    {
        return {ctx};
    }

    static_assert(stdexec::scheduler<TimedScheduler::Scheduler>);
    static_assert(exec::timed_scheduler<TimedScheduler::Scheduler>);

} // namespace Exec
