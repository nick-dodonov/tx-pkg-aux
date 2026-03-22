#pragma once

#include "RunLoop/Handler.h"
#include "Exec/Delay/ITimerBackend.h"
#include "Exec/RunContext.h"

#include <memory>

namespace Exec
{
    /// Loop handler that drives a sender<int> on a RunLoopContext.
    ///
    /// Bridges the P2300 sender/receiver model with the RunLoop update cycle.
    /// Construct with a sender or factory, then pass to a runner. Each frame,
    /// Update() drains the scheduler queue. When the sender completes with an int,
    /// the runner exits with that code.
    ///
    /// Stop-token propagation: the Domain's stop source is exposed to the running
    /// sender via DomainReceiver::get_env(). When Stop() is called, request_stop()
    /// is signalled and pending queue entries are drained before the op state is
    /// destroyed, giving cancellation-aware senders (e.g. exec::task) a chance to
    /// unwind cleanly instead of being destroyed mid-flight.
    class Domain: public RunLoop::Handler
    {
        using Scheduler = ::Exec::RunContext::Scheduler;

    public:
        /// Returns the timed scheduler handle for this domain.
        ///
        /// The returned handle satisfies both stdexec::scheduler (for zero-delay
        /// frame scheduling) and exec::timed_scheduler (for exec::schedule_after /
        /// exec::schedule_at).
        Scheduler GetScheduler() noexcept { return _scheduler.GetScheduler(); }

        /// Construct with a sender directly.
        ///
        /// Wraps the sender with starts_on(GetScheduler(), sender) so that its
        /// work begins on the update loop scheduler. The !invocable<S, Scheduler>
        /// constraint prevents this overload from matching factory lambdas.
        ///
        /// Example:
        ///   auto domain = std::make_shared<Domain>(MainTask());
        ///   auto domain = std::make_shared<Domain>(stdexec::just(42));
        /// `backend` defaults to nullptr which triggers MakeDefaultBackend() in the
        /// member initializer — ThreadTimerBackend on desktop, LoopTimerBackend on WASM.
        /// Pass a custom backend (e.g. LoopTimerBackend for tests) to override.
        template <stdexec::sender S>
        requires (!std::invocable<S, Scheduler>)
        explicit Domain(S sender, std::unique_ptr<::Exec::ITimerBackend> backend = nullptr)
            : _timerBackend(backend ? std::move(backend) : MakeDefaultBackend())
            , _scheduler(_timerBackend.get())
        {
            Store(stdexec::starts_on(GetScheduler(), std::move(sender)));
        }

        /// Construct via a factory callable: (Scheduler) → sender<int>.
        ///
        /// Use this form when the pipeline itself needs the scheduler handle
        /// internally — e.g. to insert stdexec::continues_on between pipeline
        /// steps, or to capture the scheduler for nested scheduling decisions.
        ///
        /// Example:
        ///   auto domain = std::make_shared<Domain>([](auto sched) {
        ///       return stdexec::schedule(sched)
        ///           | stdexec::then([] { return 42; });
        ///   });
        template <class F>
        requires std::invocable<F, Scheduler>
        explicit Domain(F factory, std::unique_ptr<::Exec::ITimerBackend> backend = nullptr)
            : _timerBackend(backend ? std::move(backend) : MakeDefaultBackend())
            , _scheduler(_timerBackend.get())
        {
            Store(std::move(factory)(GetScheduler()));
        }

        ~Domain();

        // RunLoop::Handler
        bool Start() override;
        void Stop() override;
        void Update(const RunLoop::UpdateCtx& ctx) override;

    private:
        void OnComplete(int exitCode);
        void OnStopped();

        friend struct DomainReceiver;

        // ---------------------------------------------------------------
        // Type erasure for the operation state
        //
        // stdexec::connect() returns a distinct, non-movable concrete type for
        // every sender S. Because Domain is a non-template class it cannot store
        // the op state inline. The virtual pair IOpState / OpStateBox<S> is the
        // minimal mechanism: one heap allocation per construction, zero overhead at
        // Start() and Stop().
        //
        // Alternative considered: unique_ptr<void> + std::function — equally one
        // allocation, but separates lifetime from dispatch, harder to read.
        // ---------------------------------------------------------------
        struct IOpState
        {
            virtual ~IOpState() = default;
            virtual void start() = 0;
        };

        // OpStateBox<S> is defined after DomainReceiver (which must be complete to form connect_result_t). Forward-declared here.
        template <class Sender>
        struct OpStateBox;

        // Store() connects the sender to a DomainReceiver and boxes the resulting
        // non-movable operation state. Called from constructors.
        template <class Sender>
        void Store(Sender sender);

        /// Returns the platform-default timer backend.
        /// Defined in Domain.cpp to avoid including ThreadTimerBackend.h here.
        static std::unique_ptr<::Exec::ITimerBackend> MakeDefaultBackend();

        // _timerBackend must be declared before _scheduler — it is initialized first
        // (member init order follows declaration order), and the TimedLoopContext
        // constructor takes the raw backend pointer which must already be valid.
        std::unique_ptr<::Exec::ITimerBackend> _timerBackend;
        ::Exec::TimedLoopContext _scheduler;
        std::unique_ptr<IOpState> _opState;

        // Stop-token source propagated to the running sender via DomainReceiver::get_env().
        // Stop() calls request_stop() before destroying _opState so that
        // stop-token-aware senders can observe the signal and unwind cleanly.
        stdexec::inplace_stop_source _stopSource;
    };

    // ---------------------------------------------------------------
    // DomainReceiver
    //
    // P2300 requires receivers to be named, concrete types: lambdas and
    // generic callables cannot satisfy stdexec::receiver because the concept
    // checks for a receiver_concept tag and a queryable get_env().
    //
    // get_env() returns a composed environment that exposes:
    //   get_scheduler  → the Domain's RunLoopContext handle, so that nested
    //                    senders (e.g. stdexec::read_env(get_scheduler) inside
    //                    a coroutine) can discover and reschedule onto it.
    //   get_stop_token → the Domain's inplace_stop_source token, so that
    //                    cancellation-aware senders observe stop requests.
    // ---------------------------------------------------------------
    struct DomainReceiver
    {
        using receiver_concept = stdexec::receiver_t;

        Domain* domain;

        [[nodiscard]] auto get_env() const noexcept
        {
            return stdexec::env{
                stdexec::prop{stdexec::get_scheduler, domain->GetScheduler()},
                stdexec::prop{stdexec::get_stop_token, domain->_stopSource.get_token()}
            };
        }

        void set_value(int exitCode) const noexcept { domain->OnComplete(exitCode); }
        void set_stopped() const noexcept { domain->OnStopped(); }
        [[noreturn]] void set_error(auto&& _) noexcept { std::terminate(); }
    };

    // Verify DomainReceiver satisfies the P2300 receiver concept:
    // receiver_concept tag, queryable get_env(), move-constructible.
    static_assert(stdexec::receiver<DomainReceiver>);

    // OpStateBox<S> holds the concrete, non-movable operation state returned by
    // stdexec::connect(). Defined here so DomainReceiver is already complete.
    template <class Sender>
    struct Domain::OpStateBox: Domain::IOpState
    {
        stdexec::connect_result_t<Sender, DomainReceiver> op;

        OpStateBox(Sender sender, Domain& domain)
            : op(stdexec::connect(std::move(sender), DomainReceiver{&domain}))
        {}

        void start() override { stdexec::start(op); }
    };

    template <class Sender>
    void Domain::Store(Sender sender)
    {
        _opState = std::make_unique<OpStateBox<Sender>>(std::move(sender), *this);
    }

} // namespace App::Exec
