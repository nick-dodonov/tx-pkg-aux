#pragma once

#include "Exec/TimedLoopContext.h"

#include <exec/task.hpp>
#include <stdexec/execution.hpp>

namespace Exec
{
    // Forward-declared so RunTaskCtx can reference it in awaiter_context_t.
    template <class ParentPromise>
    struct RunAwaiterCtx;

    /// Context policy for exec::basic_task that preserves the concrete
    /// TimedLoopContext::Scheduler without type-erasure.
    ///
    /// Unlike the default exec::task context which erases the scheduler to
    /// any_scheduler<>, RunTaskCtx stores TimedLoopContext::Scheduler directly.
    /// This allows coroutine bodies to retrieve the full timed scheduler via
    /// stdexec::read_env(stdexec::get_scheduler) and call exec::schedule_after /
    /// exec::schedule_at without a scheduler parameter.
    ///
    /// Not constructed directly — use RunTask<T>.
    struct RunTaskCtx
    {
        TimedLoopContext::Scheduler _scheduler;   // concrete; set in ctor from parent env
        stdexec::inplace_stop_token _stopToken{}; // populated by RunAwaiterCtx

        // --- exec::basic_task context policy interface ---

        template <class>
        using promise_context_t = RunTaskCtx;

        /// awaiter_context_t is intentionally constrained: the parent promise must
        /// expose a scheduler convertible to TimedLoopContext::Scheduler.
        ///
        /// This constraint produces a compile error when co_await RunTask<T> is
        /// attempted from exec::task<T>, because exec::task erases the scheduler
        /// to any_scheduler<> which is not convertible to the concrete type.
        template <class, class PP>
            requires requires(PP& pp) {
                { stdexec::get_scheduler(stdexec::get_env(pp)) }
                    -> std::convertible_to<TimedLoopContext::Scheduler>;
            }
        using awaiter_context_t = RunAwaiterCtx<PP>;

        /// Constructed from the parent coroutine's promise when this task is
        /// co_await-ed (called by basic_task's await_suspend). Extracts the
        /// concrete scheduler from the parent's environment.
        template <class PP>
            requires requires(PP& pp) {
                { stdexec::get_scheduler(stdexec::get_env(pp)) }
                    -> std::convertible_to<TimedLoopContext::Scheduler>;
            }
        explicit RunTaskCtx(PP& parent) noexcept
            : _scheduler(stdexec::get_scheduler(stdexec::get_env(parent)))
        {}

        [[nodiscard]] auto query(stdexec::get_scheduler_t) const noexcept
            -> TimedLoopContext::Scheduler const&
        {
            return _scheduler;
        }

        [[nodiscard]] auto query(stdexec::get_stop_token_t) const noexcept
            -> stdexec::inplace_stop_token
        {
            return _stopToken;
        }

        [[nodiscard]] auto stop_requested() const noexcept -> bool
        {
            return _stopToken.stop_requested();
        }

        /// Called by basic_task's sticky-scheduler mechanism when the coroutine
        /// reschedules itself via reschedule_coroutine_on. Since RunTask is always
        /// pinned to TimedLoopContext::Scheduler, only the same scheduler type is valid.
        void set_scheduler(TimedLoopContext::Scheduler sched) noexcept
        {
            _scheduler = sched;
        }
    };

    /// Awaiter context for RunTask: propagates the parent's stop token into the
    /// child coroutine's stop state.
    ///
    /// Mirrors exec::task's __default_awaiter_context for the concrete-scheduler
    /// case. Handles two relevant cases via if constexpr:
    ///   1. Parent provides inplace_stop_token — direct copy (common path in Domain).
    ///   2. Parent has unstoppable token — no-op.
    ///   3. Parent has any other stoppable token — static_assert (not supported).
    template <class ParentPromise>
    struct RunAwaiterCtx
    {
        explicit RunAwaiterCtx(RunTaskCtx& self, ParentPromise& parent) noexcept
        {
            if constexpr (requires(ParentPromise const& pp) {
                              stdexec::get_stop_token(stdexec::get_env(pp)); })
            {
                using StopTokenT =
                    stdexec::stop_token_of_t<stdexec::env_of_t<ParentPromise>>;

                if constexpr (std::same_as<StopTokenT, stdexec::inplace_stop_token>)
                {
                    // Direct copy: share the parent's inplace_stop_token without
                    // allocating a bridge stop source.
                    self._stopToken = stdexec::get_stop_token(stdexec::get_env(parent));
                }
                else if constexpr (!stdexec::unstoppable_token<StopTokenT>)
                {
                    // Bridge case: not required for current use (all callers in
                    // this project use inplace_stop_token). Assert to catch surprises.
                    static_assert(std::same_as<StopTokenT, stdexec::inplace_stop_token>,
                        "RunTask requires the parent execution context to use "
                        "inplace_stop_token. Non-inplace stop token types are "
                        "not supported; chain RunTask from Domain or another RunTask.");
                }
                // else: unstoppable token — leave _stopToken default-initialised (never fires).
            }
        }
    };

    /// RunTask<T>: a coroutine task with concrete TimedLoopContext::Scheduler access.
    ///
    /// Unlike exec::task<T>, the ambient scheduler is not type-erased. Inside the
    /// body the fully concrete timed scheduler is available via:
    ///
    ///   auto sched = co_await stdexec::read_env(stdexec::get_scheduler);
    ///   // sched has type Exec::TimedLoopContext::Scheduler
    ///   // satisfies exec::timed_scheduler — exec::schedule_after / schedule_at work
    ///
    /// RunTask<T> satisfies stdexec::sender and can be:
    ///   - Sent directly to App::Exec::Domain (no scheduler parameter required).
    ///   - co_await-ed from another RunTask<T>.
    ///
    /// Attempting to co_await RunTask<T> from exec::task<T> is a compile error:
    /// exec::task erases the scheduler to any_scheduler<> which is not convertible
    /// to TimedLoopContext::Scheduler, making the awaiter_context_t constraint fail.
    template <class T>
    using RunTask = exec::basic_task<T, RunTaskCtx>;

} // namespace Exec
