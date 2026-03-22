#pragma once
#include "Exec/Delay/ITimerBackend.h"
#include "PureLoopContext.h"

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <stdexec/execution.hpp>

namespace Exec
{
    /// Shared control block for a single timed operation.
    ///
    /// Inherits OperationBase so it can be enqueued directly into the
    /// PureLoopContext's lock-free queue. Lifetime is shared via shared_ptr
    /// between the TimedOperation (in the coroutine frame) and the timer
    /// callback lambda — so it remains valid even if the TimedOperation is
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
            if (self.stopWon || stopToken.stop_requested()) {
                stdexec::set_stopped(static_cast<Receiver&&>(self.receiver));
            } else {
                stdexec::set_value(static_cast<Receiver&&>(self.receiver));
            }
        }
    };

    /// Operation state for a timed delay. Holds the shared control block by shared_ptr.
    ///
    /// Lifetime: sits inside the coroutine frame (awaitable storage). Destroyed
    /// when the coroutine frame is destroyed — which may happen while the timer
    /// callback lambda still holds a shared_ptr to the shared state. That is safe:
    /// the lambda only touches the shared state, never the TimedOperation itself.
    ///
    /// On destruction, the stop callback is deregistered and the backend timer is
    /// cancelled (advisory — correctness is guaranteed by the CAS in DelaySharedState).
    template <class Receiver>
    struct TimedOperation
    {
        using operation_state_concept = stdexec::operation_state_t;

        using SharedState = DelaySharedState<Receiver>;
        using StopToken   = stdexec::stop_token_of_t<stdexec::env_of_t<Receiver>>;
        using StopCb      = stdexec::stop_callback_for_t<StopToken, std::function<void()>>;

        std::shared_ptr<SharedState> state;
        ITimerBackend*               backend;
        ITimerBackend::TimePoint     deadline;
        ITimerBackend::TimerId       timerId{};
        std::optional<StopCb>        stopCb{};

        TimedOperation(PureLoopContext* scheduler, ITimerBackend* be, ITimerBackend::TimePoint dl, Receiver rcvr)
            : state(std::make_shared<SharedState>(scheduler, std::forward<Receiver>(rcvr)))
            , backend(be)
            , deadline(dl)
        {}

        ~TimedOperation()
        {
            // Deregister stop callback before cancelling the timer to avoid a
            // spurious CAS win racing with the Cancel call.
            stopCb.reset();
            if (timerId) {
                backend->Cancel(timerId);
            }
        }

        TimedOperation(const TimedOperation&) = delete;
        TimedOperation& operator=(const TimedOperation&) = delete;

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

            // Register stop callback — fires synchronously if the token is
            // already in the stop-requested state when the callback is constructed.
            // Captures state by shared_ptr so the lambda is independent of
            // this TimedOperation's lifetime.
            stopCb.emplace(stopToken,
                [s = state]() mutable noexcept {
                    bool expected = false;
                    if (s->claimed.compare_exchange_strong(expected, true,
                            std::memory_order_acq_rel, std::memory_order_relaxed)) {
                        s->stopWon = true;
                        s->scheduler->Enqueue(s.get());
                    }
                    // If we lost the CAS, the timer already won — nothing to do.
                });

            // Schedule the timer — capture state by shared_ptr only,
            // never capture `this` (the TimedOperation may be destroyed first).
            timerId = backend->ScheduleAt(deadline,
                [s = state]() mutable {
                    bool expected = false;
                    if (s->claimed.compare_exchange_strong(expected, true,
                            std::memory_order_acq_rel, std::memory_order_relaxed)) {
                        s->scheduler->Enqueue(s.get());
                    }
                    // If we lost, the stop side already won — nothing to do.
                    // shared_ptr releases here; if TimedOperation is gone, SharedState GC'd.
                });
        }
    };
}
