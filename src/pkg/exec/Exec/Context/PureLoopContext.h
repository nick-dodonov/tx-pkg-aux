#pragma once
#include "LoopContext.h"
#include "OperationBase.h"
#include "concurrentqueue.h"

#include <stdexec/execution.hpp>

namespace Exec
{
    /// Execution context that integrates a P2300-compatible scheduler with a
    /// frame-based update loop.
    ///
    /// Instead of a dedicated thread, PureLoopContext uses a manual, non-blocking
    /// drain model: work scheduled via the Scheduler handle is enqueued in a
    /// lock-free queue and executed synchronously when DrainQueue() is called —
    /// typically once per frame in a game/render loop.
    ///
    /// Relationship to standard alternatives:
    ///   stdexec::run_loop      — blocking, CV-driven; meant for dedicated threads
    ///   exec::inline_scheduler — executes immediately on start(), never defers
    ///   PureLoopContext        — non-blocking, frame-boundary deferred, main-thread loops
    class PureLoopContext
    {
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
            PureLoopContext* scheduler;
            Receiver receiver;

            Operation(PureLoopContext* sched, Receiver rcvr)
                : OperationBase{&Execute}
                , scheduler(sched)
                , receiver(static_cast<Receiver&&>(rcvr))
            {}

            static void Execute(OperationBase* base) noexcept
            {
                auto& self = *static_cast<Operation*>(base);
                const auto stopToken = stdexec::get_stop_token(stdexec::get_env(self.receiver));
                if (stopToken.stop_requested()) {
                    stdexec::set_stopped(static_cast<Receiver&&>(self.receiver));
                } else {
                    stdexec::set_value(static_cast<Receiver&&>(self.receiver));
                }
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
        /// Lightweight scheduler handle — holds a pointer to the owning PureLoopContext.
        ///
        /// Satisfies stdexec::scheduler: schedule() returns a Sender whose get_env()
        /// advertises this Scheduler as the completion scheduler for all signals,
        /// allowing stdexec::continues_on and stdexec::on to chain correctly.
        struct Scheduler
        {
            using scheduler_concept = stdexec::scheduler_t;

            PureLoopContext* ctx;

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
            using completion_signatures = stdexec::completion_signatures<
                stdexec::set_value_t(),
                stdexec::set_stopped_t()
            >;

            PureLoopContext* ctx;

            template <class Receiver>
            auto connect(Receiver rcvr) const -> Operation<Receiver>
            {
                return {ctx, static_cast<Receiver&&>(rcvr)};
            }

            struct Env
            {
                PureLoopContext* ctx;

                template <class CPO>
                [[nodiscard]] auto query(stdexec::get_completion_scheduler_t<CPO> _) const noexcept -> Scheduler
                {
                    return {ctx};
                }
            };

            [[nodiscard]] auto get_env() const noexcept -> Env { return {ctx}; }
        };

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
        /// pointer to the OperationBase (such as TimerSharedState). Push() remains
        /// private so only Operation<Receiver>::start() can enqueue via that path.
        void Enqueue(OperationBase* task) noexcept { _queue.enqueue(task); }

    private:
        void Push(OperationBase* task) noexcept { _queue.enqueue(task); }

        moodycamel::ConcurrentQueue<OperationBase*> _queue;
    };

    // Out-of-line definition: now Sender is complete
    inline auto PureLoopContext::Scheduler::schedule() const noexcept -> Sender
    {
        return {ctx};
    }

    static_assert(LoopContext<PureLoopContext>);
    // Verify that PureLoopContext::Scheduler fully satisfies the P2300 scheduler concept,
    // including the get_completion_scheduler<CPO> round-trip mandated by stdexec::scheduler.
    static_assert(stdexec::scheduler<PureLoopContext::Scheduler>);
}
