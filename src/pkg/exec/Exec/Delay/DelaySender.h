#pragma once
#include "IDelayBackend.h"
#include "Exec/RunLoopContext.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <stdexec/execution.hpp>

namespace Exec
{
    /// Shared control block for a single delay operation.
    ///
    /// Inherits OperationBase so it can be enqueued directly into the
    /// RunLoopContext's lock-free queue. Lifetime is shared via shared_ptr
    /// between the DelayOperation (in the coroutine frame) and the timer
    /// callback lambda — so it remains valid even if the DelayOperation is
    /// destroyed (e.g., when the stop side wins the CAS and the coroutine frame
    /// is torn down) before the timer callback fires.
    ///
    /// The `claimed` flag is the single synchronisation point between the timer
    /// and the stop side: whichever raises it first wins and enqueues this node.
    template <class Receiver>
    struct DelaySharedState : RunLoopContext::OperationBase
    {
        RunLoopContext* scheduler;
        Receiver receiver;
        std::atomic<bool> claimed{false};
        bool stopWon{false};

        DelaySharedState(RunLoopContext* sched, Receiver&& rcvr)
            : scheduler(sched)
            , receiver(static_cast<Receiver&&>(rcvr))
        {
            this->execute = [](RunLoopContext::OperationBase* base) noexcept {
                auto& self = *static_cast<DelaySharedState*>(base);
                const auto stopToken = stdexec::get_stop_token(stdexec::get_env(self.receiver));
                const auto stopped = self.stopWon || stopToken.stop_requested();
                if (stopped) {
                    stdexec::set_stopped(static_cast<Receiver&&>(self.receiver));
                } else {
                    stdexec::set_value(static_cast<Receiver&&>(self.receiver));
                }
            };
        }
    };

    // Forward-declare for the scheduler type alias used in DelaySender::Env.
    // TimedLoopContext.h is NOT included here to avoid a circular dependency.
    // Instead, DelaySender is parametrised on the scheduler type.

    /// Sender produced by TimedLoopContext::Scheduler::schedule_at().
    ///
    /// Completion signals: set_value_t() (timer fired first) or set_stopped_t()
    /// (stop token was raised first). Never produces set_error.
    ///
    /// The Scheduler template parameter is the concrete TimedLoopContext::Scheduler
    /// type; passing it here lets the Env report the correct completion scheduler
    /// so that down-stream continues_on / exec::task chaining works correctly.
    template <class TimedSchedulerHandle>
    struct DelaySender
    {
        using sender_concept = stdexec::sender_t;
        using completion_signatures = stdexec::completion_signatures<
            stdexec::set_value_t(), stdexec::set_stopped_t()>;

        TimedSchedulerHandle timedSched; // for the Env query AND for RunLoopContext access
        IDelayBackend*       backend;
        IDelayBackend::TimePoint deadline;

        /// Operation state for a delay. Holds the shared control block by shared_ptr.
        ///
        /// Lifetime: sits inside the coroutine frame (awaitable storage). Destroyed
        /// when the coroutine frame is destroyed — which may happen while the timer
        /// callback lambda still holds a shared_ptr to the shared state. That is safe:
        /// the lambda only touches the shared state, never the DelayOperation itself.
        template <class Receiver>
        struct Operation
        {
            using operation_state_concept = stdexec::operation_state_t;

            using SharedState = DelaySharedState<Receiver>;
            using StopToken   = stdexec::stop_token_of_t<stdexec::env_of_t<Receiver>>;
            using StopCb      = stdexec::inplace_stop_callback<std::function<void()>>;

            std::shared_ptr<SharedState>    state;
            IDelayBackend*                  backend;
            IDelayBackend::TimePoint        deadline;
            IDelayBackend::TimerId          timerId{};
            alignas(StopCb) std::byte       stopCbStorage[sizeof(StopCb)]{};
            bool                            stopCbActive{false};

            Operation(TimedSchedulerHandle sched, IDelayBackend* be,
                      IDelayBackend::TimePoint dl, Receiver rcvr)
                : state(std::make_shared<SharedState>(
                      sched.GetRunLoop(), static_cast<Receiver&&>(rcvr)))
                , backend(be)
                , deadline(dl)
            {}

            ~Operation()
            {
                if (stopCbActive) {
                    std::destroy_at(reinterpret_cast<StopCb*>(stopCbStorage));
                }
            }

            Operation(const Operation&) = delete;
            Operation& operator=(const Operation&) = delete;

            void start() & noexcept
            {
                const auto stopToken = stdexec::get_stop_token(stdexec::get_env(state->receiver));

                // Fast path: already cancelled before we even start.
                if (stopToken.stop_requested()) {
                    bool expected = false;
                    if (state->claimed.compare_exchange_strong(expected, true,
                            std::memory_order_acq_rel, std::memory_order_relaxed)) {
                        state->stopWon = true;
                        state->scheduler->Enqueue(state.get());
                    }
                    return;
                }

                // Capture state by shared_ptr so the lambda is independent of
                // this Operation's lifetime.
                auto capturedState = state;
                auto* capturedScheduler = state->scheduler;

                // Register stop callback — fires synchronously if the token is
                // already in the stop-requested state.
                new (stopCbStorage) StopCb(stopToken,
                    [s = std::move(capturedState)]() mutable noexcept {
                        bool expected = false;
                        if (s->claimed.compare_exchange_strong(expected, true,
                                std::memory_order_acq_rel, std::memory_order_relaxed)) {
                            s->stopWon = true;
                            s->scheduler->Enqueue(s.get());
                        }
                        // If we lost the CAS, the timer already won — nothing to do.
                    });
                stopCbActive = true;

                // Schedule the timer — capture state by shared_ptr only,
                // never capture `this` (the Operation may be destroyed first).
                timerId = backend->ScheduleAt(deadline,
                    [s = state, sched = capturedScheduler]() mutable {
                        bool expected = false;
                        if (s->claimed.compare_exchange_strong(expected, true,
                                std::memory_order_acq_rel, std::memory_order_relaxed)) {
                            sched->Enqueue(s.get());
                        }
                        // If we lost, the stop side already won — nothing to do.
                        // shared_ptr releases here; if Operation is gone, SharedState GC'd.
                    });
            }
        };

        template <class Receiver>
        auto connect(Receiver rcvr) const -> Operation<Receiver>
        {
            return {timedSched, backend, deadline, static_cast<Receiver&&>(rcvr)};
        }

        struct Env
        {
            TimedSchedulerHandle timedSched;

            template <class CPO>
            [[nodiscard]] auto query(stdexec::get_completion_scheduler_t<CPO>) const noexcept
                -> TimedSchedulerHandle
            {
                return timedSched;
            }
        };

        [[nodiscard]] auto get_env() const noexcept -> Env { return {timedSched}; }
    };

} // namespace Exec
