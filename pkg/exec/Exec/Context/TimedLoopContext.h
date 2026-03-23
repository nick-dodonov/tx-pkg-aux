#pragma once
#include "PureLoopContext.h"
#include "TimedOperation.h"
#include "Exec/Delay/ITimerBackend.h"

#include <chrono>
#include <exec/timed_scheduler.hpp>
#include <stdexec/execution.hpp>

namespace Exec
{
    /// Execution context that augments PureLoopContext with timed scheduling.
    ///
    /// Wraps PureLoopContext (for zero-delay frame scheduling) and an ITimerBackend
    /// (for timed delays). Satisfies both stdexec::scheduler and exec::timed_scheduler,
    /// so code that uses exec::schedule_after or co_await exec::schedule_after works
    /// without any project-specific helpers.
    ///
    /// Ownership: TimedLoopContext owns the PureLoopContext; it holds a non-owning
    /// pointer to the ITimerBackend — lifetime of the backend is managed externally
    /// (typically by Exec::Domain which provides a unique_ptr<ITimerBackend>).
    class TimedLoopContext
    {
        // forward-declared for Scheduler
        struct LoopSender;
        struct TimedSender;

    public:
        /// Lightweight scheduler handle satisfying both stdexec::scheduler and exec::timed_scheduler.
        ///
        ///   schedule()        — enqueue work at the next loop drain (frame boundary, no delay)
        ///   schedule_after()  — enqueue work after a given duration
        ///   schedule_at()     — enqueue work at a specific steady_clock time point
        ///   now()             — returns steady_clock::now()
        struct Scheduler
        {
            using scheduler_concept = stdexec::scheduler_t;

            TimedLoopContext* ctx;

            [[nodiscard]] auto now() const noexcept -> std::chrono::steady_clock::time_point
            {
                return std::chrono::steady_clock::now();
            }

            [[nodiscard]] auto schedule() const noexcept
                -> LoopSender;

            [[nodiscard]] auto schedule_at(std::chrono::steady_clock::time_point tp) const noexcept
                -> TimedSender;

            [[nodiscard]] auto schedule_after(std::chrono::steady_clock::duration dur) const noexcept
                -> TimedSender
            {
                return schedule_at(now() + dur);
            }

            auto operator==(const Scheduler&) const noexcept -> bool = default;
        };

    private:
        /// Base sender type for all schedule methods.
        /// 
        /// Overrides the Env so the completion scheduler is reported as TimedLoopContext::Scheduler
        /// (required by stdexec::scheduler concept: get_completion_scheduler(get_env(schedule(sched))) == sched).
        ///
        /// Final senders must specify connect() to produce the correct operation state 
        /// (e.g. LoopSender produces PureLoopContext::Operation, TimedSender produces TimedOperation).
        struct BaseSender
        {
            using sender_concept = stdexec::sender_t;
            using completion_signatures = stdexec::completion_signatures<
                stdexec::set_value_t(),
                stdexec::set_stopped_t()
            >;

            TimedLoopContext* ctx;

            BaseSender(TimedLoopContext* ctx_) noexcept : ctx(ctx_) {}

            struct Env
            {
                TimedLoopContext* ctx;

                template <class CPO>
                [[nodiscard]] auto query(stdexec::get_completion_scheduler_t<CPO> _) const noexcept
                    -> Scheduler
                {
                    return {ctx};
                }
            };

            [[nodiscard]] auto get_env() const noexcept -> Env { return {ctx}; }
        };

        /// Loop-aligned sender returned by Scheduler::schedule().
        ///
        /// Delegates the actual enqueueing to PureLoopContext::Sender::connect(),
        /// but overrides the Env so the completion scheduler is reported as TimedLoopContext::Scheduler
        /// (required by stdexec::scheduler concept: get_completion_scheduler(get_env(schedule(sched))) == sched).
        struct LoopSender : BaseSender
        {
            using BaseSender::BaseSender;

            /// Connect by delegating to the underlying PureLoopContext::Sender.
            /// The return type is PureLoopContext::Operation<Receiver>
            /// (deduced via auto — naming the private type is not required here).
            template <class Receiver>
            auto connect(Receiver rcvr) const
            {
                return ctx->_loopContext.GetScheduler().schedule().connect(static_cast<Receiver&&>(rcvr));
            }
        };

        /// Sender produced by TimedLoopContext::Scheduler::schedule_at().
        ///
        /// Completion signals: set_value_t() (timer fired first) or set_stopped_t()
        /// (stop token was raised first). Never produces set_error.
        ///
        /// The Scheduler template parameter is the concrete TimedLoopContext::Scheduler
        /// type; passing it here lets the Env report the correct completion scheduler
        /// so that down-stream continues_on / exec::task chaining works correctly.
        struct TimedSender : BaseSender
        {
            ITimerBackend* backend;
            ITimerBackend::TimePoint deadline;

            TimedSender(TimedLoopContext* ctx_, ITimerBackend* backend_, ITimerBackend::TimePoint deadline_) noexcept
                : BaseSender(ctx_)
                , backend(backend_)
                , deadline(deadline_)
            {}

            template <class Receiver>
            auto connect(Receiver rcvr) const -> TimedOperation<Receiver>
            {
                return {&ctx->_loopContext, backend, deadline, static_cast<Receiver&&>(rcvr)};
            }
        };

    public:

        explicit TimedLoopContext(ITimerBackend* backend) noexcept
            : _backend(backend)
        {}

        [[nodiscard]] auto GetScheduler() noexcept -> Scheduler { return {this}; }

        std::size_t DrainQueue()
        {
            // Fire expired timer callbacks first so they can enqueue operations (thread-based backends implement this as a no-op)
            _backend->Tick();
            return _loopContext.DrainQueue();
        }

    private:
        PureLoopContext _loopContext;
        ITimerBackend* _backend; // non-owning; lifetime managed by caller (Domain)
    };

    // Out-of-line definition: now that sender types are complete
    inline auto TimedLoopContext::Scheduler::schedule() const noexcept 
        -> LoopSender
    {
        return {ctx};
    }

    inline auto TimedLoopContext::Scheduler::schedule_at(std::chrono::steady_clock::time_point tp) const noexcept 
        -> TimedSender
    {
        return {ctx, ctx->_backend, tp};
    }

    static_assert(LoopContext<TimedLoopContext>);
    static_assert(stdexec::scheduler<TimedLoopContext::Scheduler>);
    static_assert(exec::timed_scheduler<TimedLoopContext::Scheduler>);
}
