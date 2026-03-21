#pragma once
#include <stdexec/execution.hpp>

#include "concurrentqueue.h"

namespace Exec
{
    /// Custom stdexec scheduler that enqueues work to be drained
    /// manually during an update loop (e.g., in a frame update callback).
    class UpdateScheduler
    {
        struct TaskBase
        {
            void (*execute)(TaskBase*) noexcept {};
        };

        template <class Receiver>
        struct Operation: TaskBase
        {
            UpdateScheduler* scheduler;
            Receiver receiver;

            Operation(UpdateScheduler* sched, Receiver rcvr)
                : scheduler(sched)
                , receiver(static_cast<Receiver&&>(rcvr))
            {
                this->execute = [](TaskBase* base) noexcept {
                    auto& self = *static_cast<Operation*>(base);
                    if (stdexec::get_stop_token(stdexec::get_env(self.receiver)).stop_requested()) {
                        stdexec::set_stopped(static_cast<Receiver&&>(self.receiver));
                    } else {
                        stdexec::set_value(static_cast<Receiver&&>(self.receiver));
                    }
                };
            }

            void start() & noexcept { scheduler->Push(this); }
        };

        struct Sender;

    public:
        struct Scheduler
        {
            using scheduler_concept = stdexec::scheduler_t;

            UpdateScheduler* ctx;

            [[nodiscard]] auto schedule() const noexcept -> Sender;

            auto operator==(const Scheduler&) const noexcept -> bool = default;
        };

    private:
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
                auto query(stdexec::get_completion_scheduler_t<CPO>) const noexcept -> Scheduler
                {
                    return {ctx};
                }
            };

            [[nodiscard]] auto get_env() const noexcept -> Env { return {ctx}; }
        };

    public:
        auto GetScheduler() noexcept -> Scheduler { return {this}; }

        /// Drain and execute all pending tasks. Call from the update loop.
        /// Returns the number of tasks executed.
        std::size_t DrainQueue()
        {
            std::size_t count = 0;
            TaskBase* task{};
            while (_queue.try_dequeue(task)) {
                task->execute(task);
                ++count;
            }
            return count;
        }

    private:
        void Push(TaskBase* task) noexcept { _queue.enqueue(task); }

        moodycamel::ConcurrentQueue<TaskBase*> _queue;
    };

    inline auto UpdateScheduler::Scheduler::schedule() const noexcept -> UpdateScheduler::Sender
    {
        return {ctx};
    }
} // namespace Exec
