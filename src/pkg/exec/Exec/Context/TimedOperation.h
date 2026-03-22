#pragma once
#include "Exec/Delay/ITimerBackend.h"
#include "PureLoopContext.h"

#include <atomic>
#include <memory>
#include <stdexec/execution.hpp>

namespace Exec
{
    /// Shared control block for a single delay operation.
    ///
    /// Inherits OperationBase so it can be enqueued directly into the
    /// PureLoopContext's lock-free queue. Lifetime is shared via shared_ptr
    /// between the DelayOperation (in the coroutine frame) and the timer
    /// callback lambda — so it remains valid even if the DelayOperation is
    /// destroyed (e.g., when the stop side wins the CAS and the coroutine frame
    /// is torn down) before the timer callback fires.
    ///
    /// The `claimed` flag is the single synchronisation point between the timer
    /// and the stop side: whichever raises it first wins and enqueues this node.
    template <class Receiver>
    struct DelaySharedState : OperationBase
    {
        PureLoopContext* scheduler;
        Receiver receiver;
        std::atomic<bool> claimed{false};
        bool stopWon{false};

        DelaySharedState(PureLoopContext* sched, Receiver&& rcvr)
            : OperationBase{&Execute}
            , scheduler(sched)
            , receiver(std::move(rcvr))
        {}

        static void Execute(OperationBase* base) noexcept
        {
            auto& self = *static_cast<DelaySharedState*>(base);
            const auto stopToken = stdexec::get_stop_token(stdexec::get_env(self.receiver));
            const auto stopped = self.stopWon || stopToken.stop_requested();
            if (stopped) {
                stdexec::set_stopped(static_cast<Receiver&&>(self.receiver));
            } else {
                stdexec::set_value(static_cast<Receiver&&>(self.receiver));
            }
        }
    };

    /// TimedOperation state for a delay. Holds the shared control block by shared_ptr.
    ///
    /// Lifetime: sits inside the coroutine frame (awaitable storage). Destroyed
    /// when the coroutine frame is destroyed — which may happen while the timer
    /// callback lambda still holds a shared_ptr to the shared state. That is safe:
    /// the lambda only touches the shared state, never the TimedOperation itself.
    template <class Receiver>
    struct TimedOperation
    {
        using operation_state_concept = stdexec::operation_state_t;

        using SharedState = DelaySharedState<Receiver>;
        using StopToken   = stdexec::stop_token_of_t<stdexec::env_of_t<Receiver>>;
        using StopCb      = stdexec::inplace_stop_callback<std::function<void()>>;

        std::shared_ptr<SharedState>    state;
        ITimerBackend*                  backend;
        ITimerBackend::TimePoint        deadline;
        ITimerBackend::TimerId          timerId{};
        alignas(StopCb) std::byte       stopCbStorage[sizeof(StopCb)]{};
        bool                            stopCbActive{false};

        TimedOperation(PureLoopContext* scheduler, ITimerBackend* be, ITimerBackend::TimePoint dl, Receiver rcvr)
            : state(std::make_shared<SharedState>(scheduler, std::forward<Receiver>(rcvr)))
            , backend(be)
            , deadline(dl)
        {}

        ~TimedOperation()
        {
            if (stopCbActive) {
                std::destroy_at(reinterpret_cast<StopCb*>(stopCbStorage)); //NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
            }
        }

        TimedOperation(const TimedOperation&) = delete;
        TimedOperation& operator=(const TimedOperation&) = delete;
        TimedOperation(TimedOperation&&) = default;
        TimedOperation& operator=(TimedOperation&&) = default;

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
}
