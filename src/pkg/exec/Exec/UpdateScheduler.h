#pragma once
#include <stdexec/execution.hpp>

#include "concurrentqueue.h"

namespace Exec
{
    /// A P2300-compatible scheduler that integrates with a frame-based update loop.
    ///
    /// Instead of a dedicated thread, UpdateScheduler uses a manual, non-blocking
    /// drain model: work scheduled via the Scheduler handle is enqueued in a
    /// lock-free queue and executed synchronously when DrainQueue() is called —
    /// typically once per frame in a game/render loop.
    ///
    /// Relationship to standard alternatives:
    ///   stdexec::run_loop      — blocking, CV-driven; meant for dedicated threads
    ///   exec::inline_scheduler — executes immediately on start(), never defers
    ///   UpdateScheduler        — non-blocking, frame-boundary deferred, main-thread loops
    class UpdateScheduler
    {
    public:
        /// Minimal task node stored in the concurrent queue.
        ///
        /// Public so external operation states (e.g., DelaySharedState) can inherit
        /// from it and be enqueued via Enqueue(). Uses a raw function pointer instead
        /// of virtual dispatch — a pattern also used in stdexec::run_loop's intrusive
        /// queue. The execute pointer is set by the inheriting type's constructor to
        /// capture its concrete type without requiring a virtual base class.
        ///
        /// Lifetime: the inheriting node must not be destroyed while it is still in
        /// the queue.
        struct OperationBase
        {
            void (*execute)(OperationBase*) noexcept {};
        };

    private:

        /// Concrete operation state produced by Sender::connect().
        ///
        /// Satisfies stdexec::operation_state: start() & noexcept enqueues this
        /// into the scheduler's lock-free queue rather than executing inline —
        /// actual execution is deferred to the next DrainQueue() call.
        ///
        /// Stop-token aware: at drain time the receiver's stop token is checked;
        /// if cancellation was requested, set_stopped() is called instead of set_value().
        template <class Receiver>
        struct Operation: OperationBase
        {
            UpdateScheduler* scheduler;
            Receiver receiver;

            Operation(UpdateScheduler* sched, Receiver rcvr)
                : scheduler(sched)
                , receiver(static_cast<Receiver&&>(rcvr))
            {
                this->execute = [](OperationBase* base) noexcept {
                    auto& self = *static_cast<Operation*>(base);
                    const auto stopToken = stdexec::get_stop_token(stdexec::get_env(self.receiver));
                    if (stopToken.stop_requested()) {
                        stdexec::set_stopped(static_cast<Receiver&&>(self.receiver));
                    } else {
                        stdexec::set_value(static_cast<Receiver&&>(self.receiver));
                    }
                };
            }

            void start() & noexcept { scheduler->Push(this); }
        };

        // Minimal receiver used only to verify Operation<R> satisfies operation_state.
        // stdexec::operation_state<Op> requires: destructible, is_object, stdexec::start(op).
        // Unlike sender/receiver/scheduler, no operation_state_concept tag is required.
        struct MinReceiver
        {
            using receiver_concept = stdexec::receiver_t;
            [[nodiscard]] auto get_env() const noexcept { return stdexec::env<>{}; }
            void set_value() noexcept {}
            void set_stopped() noexcept {}
            void set_error(auto&& _) noexcept {}
        };
        static_assert(stdexec::receiver<MinReceiver>);
        static_assert(stdexec::operation_state<Operation<MinReceiver>>);

        struct Sender;

    public:
        /// Lightweight scheduler handle — holds a pointer to the owning UpdateScheduler.
        ///
        /// Satisfies stdexec::scheduler: schedule() returns a Sender whose get_env()
        /// advertises this Scheduler as the completion scheduler for all signals,
        /// allowing stdexec::continues_on and stdexec::on to chain correctly.
        struct Scheduler
        {
            using scheduler_concept = stdexec::scheduler_t;

            UpdateScheduler* ctx;

            [[nodiscard]] auto schedule() const noexcept -> Sender;

            auto operator==(const Scheduler&) const noexcept -> bool = default;
        };

    private:
        /// Sender returned by Scheduler::schedule().
        ///
        /// Completion signatures: set_value_t() and set_stopped_t() only —
        /// set_error is never produced because Push() is noexcept.
        ///
        /// get_env() returns an Env that reports this Scheduler as the completion
        /// scheduler for all CPOs, enabling downstream algorithms (continues_on, on)
        /// to discover where work will complete.
        struct Sender
        {
            using sender_concept = stdexec::sender_t;
            using completion_signatures = stdexec::completion_signatures<stdexec::set_value_t(), stdexec::set_stopped_t()>;

            UpdateScheduler* ctx;

            template <class Receiver>
            auto connect(Receiver rcvr) const -> Operation<Receiver>
            {
                return {ctx, static_cast<Receiver&&>(rcvr)};
            }

            struct Env
            {
                UpdateScheduler* ctx;

                template <class CPO>
                [[nodiscard]] auto query(stdexec::get_completion_scheduler_t<CPO> _) const noexcept -> Scheduler
                {
                    return {ctx};
                }
            };

            [[nodiscard]] auto get_env() const noexcept -> Env { return {ctx}; }
        };

        // Verify that UpdateScheduler::Scheduler fully satisfies the P2300 scheduler concept,
        // including the get_completion_scheduler<CPO> round-trip mandated by stdexec::scheduler.
        static_assert(stdexec::scheduler<Scheduler>);

    public:
        auto GetScheduler() noexcept -> Scheduler { return {this}; }

        /// Drain and execute all pending tasks. Call once per frame from the update loop.
        ///
        /// Uses try_dequeue (lock-free, non-blocking), so it returns immediately if
        /// the queue is empty — unlike stdexec::run_loop::run() which blocks on a CV.
        ///
        /// Returns the number of tasks executed.
        std::size_t DrainQueue()
        {
            std::size_t count = 0;
            OperationBase* task{};
            while (_queue.try_dequeue(task)) {
                task->execute(task);
                ++count;
            }
            return count;
        }

        /// Enqueue a task node from outside the scheduler (e.g., from a timer callback).
        ///
        /// Prefer this over Push() for code in other translation units that hold a
        /// pointer to the OperationBase (such as DelaySharedState). Push() remains
        /// private so only Operation<Receiver>::start() can enqueue via that path.
        void Enqueue(OperationBase* task) noexcept { _queue.enqueue(task); }

    private:
        void Push(OperationBase* task) noexcept { _queue.enqueue(task); }

        moodycamel::ConcurrentQueue<OperationBase*> _queue;
    };

    inline auto UpdateScheduler::Scheduler::schedule() const noexcept -> UpdateScheduler::Sender
    {
        return {ctx};
    }

} // namespace Exec
