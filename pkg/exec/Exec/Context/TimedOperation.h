#pragma once
#include "Exec/Delay/ITimerBackend.h"
#include "PureLoopContext.h"

#include <atomic>
#include <stdexec/execution.hpp>

namespace Exec
{
    /// Operation state for a timed delay.
    ///
    /// Inherits OperationBase so it can be enqueued directly into PureLoopContext's
    /// lock-free queue. Contains the receiver, synchronisation flag, and ownership
    /// of both the stop callback and timer registration — no heap allocation needed.
    ///
    /// Lifetime is guaranteed by P2300: the op state must remain alive until a
    /// completion signal (set_value / set_stopped) is delivered. Combined with
    /// ITimerBackend::Cancel() blocking until any in-flight timer callback finishes,
    /// this means it is safe to destroy TimedOperation immediately after
    /// the completion signal returns.
    ///
    /// Once start() is called, the stop callback and timer lambda both capture `this`.
    /// TimedOperation must NOT be moved after start() — it is immovable by design.
    ///
    /// The `state` atomic is the single synchronisation point between the timer
    /// and the stop side: whichever side raises it first prevents the other from
    /// enqueuing this node a second time.
    template <class Receiver>
    struct TimedOperation : OperationBase
    {
        using operation_state_concept = stdexec::operation_state_t;

        using StopToken = stdexec::stop_token_of_t<stdexec::env_of_t<Receiver>>;

        /// Lightweight stop callback — captures `this` by raw pointer.
        /// Safe because the stop callback is deregistered (stopRegistration.reset()) in
        /// the destructor before TimedOperation's storage is released.
        struct StopCallback
        {
            TimedOperation* op;
            void operator()() noexcept
            {
                op->TryEnqueueStop();
            }
        };
        using StopRegistration = stdexec::stop_callback_for_t<StopToken, StopCallback>;

        enum class State : std::uint8_t { Pending, TimerWon, StopWon };

        PureLoopContext* scheduler;
        ITimerBackend* backend;
        ITimerBackend::TimePoint deadline;
        Receiver receiver;
        std::atomic<State> state{State::Pending};
        ITimerBackend::TimerId timerId{};
        std::optional<StopRegistration> stopRegistration{};

        TimedOperation(PureLoopContext* sched, ITimerBackend* be, ITimerBackend::TimePoint dl, Receiver rcvr)
            : OperationBase{&Execute}
            , scheduler(sched)
            , backend(be)
            , deadline(dl)
            , receiver(std::move(rcvr))
        {}

        ~TimedOperation()
        {
            // Deregister stop callback first — prevents a spurious CAS win from
            // racing with the Cancel call below.
            stopRegistration.reset();
            // Cancel blocks until any in-flight timer callback has returned,
            // so `this` is safe to destroy the moment Cancel() returns.
            if (timerId) {
                backend->Cancel(timerId);
            }
        }

        // Immovable: lambdas registered in start() capture `this` by raw pointer.
        // Guaranteed copy elision (C++17) ensures connect() constructs the op state
        // directly in the caller's storage — no move is ever needed.
        TimedOperation(const TimedOperation&) = delete;
        TimedOperation& operator=(const TimedOperation&) = delete;
        TimedOperation(TimedOperation&&) = delete;
        TimedOperation& operator=(TimedOperation&&) = delete;

        static void Execute(OperationBase* base) noexcept
        {
            auto& self = *static_cast<TimedOperation*>(base);
            auto& receiver = self.receiver;
            // Outcome is determined by stop_requested() at drain time, not by which
            // side won the CAS. This gives frame-accurate cancellation: a stop
            // request raised after the timer fires but before DrainQueue runs is
            // still honoured. StopWon always implies stop_requested() == true;
            // TimerWon may or may not, depending on what happened in the interim.
            const auto stopToken = stdexec::get_stop_token(stdexec::get_env(receiver));
            if (stopToken.stop_requested()) {
                stdexec::set_stopped(static_cast<Receiver&&>(receiver));
            } else {
                stdexec::set_value(static_cast<Receiver&&>(receiver));
            }
        }

        void start() & noexcept
        {
            //TODO: `if constexpr (unstoppable_token<stop_token_of_t<env_of_t<Receiver>>>)` to avoid registering a stop callback at all in that case
            const auto stopToken = stdexec::get_stop_token(stdexec::get_env(receiver));

            // Register stop callback. If stop is already requested at this point,
            // stop_callback_for_t fires StopCallback synchronously during construction,
            // winning the CAS and setting stopWon — no need to schedule the timer.
            stopRegistration.emplace(stopToken, StopCallback{this});
            if (state.load(std::memory_order_relaxed) != State::Pending) {
                return;
            }

            // Schedule the timer. The lambda captures `this` by raw pointer — safe
            // because ITimerBackend::Cancel() blocks until the callback completes,
            // guaranteeing `this` is alive for the callback's entire execution.
            timerId = backend->ScheduleAt(deadline, [this]() {
                timerId = ITimerBackend::InvalidTimerId; // prevent unnecessary cancel in destructor as already fired
                TryEnqueue(State::TimerWon);
            });
        }

        void TryEnqueueStop()
        {
            // Cancel the timer eagerly when stop wins — 
            //  avoids leaving a now-pointless entry in the backend
            if (timerId) {
                backend->Cancel(std::exchange(timerId, ITimerBackend::InvalidTimerId));
            }
            TryEnqueue(State::StopWon);
        }

        void TryEnqueue(State desired)
        {
            State expected = State::Pending;
            if (state.compare_exchange_strong(expected, desired,
                    std::memory_order_acq_rel, std::memory_order_relaxed)) {
                scheduler->Enqueue(this);
            }
        }
    };
}

