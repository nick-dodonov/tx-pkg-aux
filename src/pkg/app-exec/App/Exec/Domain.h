#pragma once

#include "App/Loop/Handler.h"
#include "Exec/UpdateScheduler.h"

#include <memory>

namespace App::Exec
{
    /// Loop handler that drives a sender<int> on an UpdateScheduler.
    ///
    /// Call Launch() before handing the domain to a runner.
    /// When the sender completes the runner is exited with the returned int.
    class Domain: public App::Loop::Handler
    {
        using Scheduler = ::Exec::UpdateScheduler::Scheduler;

    public:
        /// Returns the scheduler used to enqueue work into the update loop.
        Scheduler GetScheduler() noexcept { return _scheduler.GetScheduler(); }

        /// Launch a sender directly.
        /// The sender is automatically wrapped with starts_on(GetScheduler(), sender)
        /// so its work runs on the update loop.
        ///
        /// Example:
        ///   domain->Launch(MainTask());
        template <stdexec::sender S>
        requires (!std::invocable<S, Scheduler>)
        void Launch(S sender)
        {
            Store(stdexec::starts_on(GetScheduler(), std::move(sender)));
        }

        /// Launch via a factory callable: (Scheduler) -> sender<int>.
        /// Use this when the pipeline itself depends on the scheduler
        /// (e.g. stdexec::continues_on between steps).
        ///
        /// Example:
        ///   domain->Launch([](auto sched) {
        ///       return stdexec::schedule(sched) | stdexec::then([] { return 42; });
        ///   });
        template <class F>
        requires std::invocable<F, Scheduler>
        void Launch(F factory)
        {
            Store(std::move(factory)(GetScheduler()));
        }

        ~Domain();

        // App::Loop::Handler
        bool Start() override;
        void Stop() override;
        void Update(const App::Loop::UpdateCtx& ctx) override;

    private:
        void OnComplete(int exitCode);
        void OnStopped();

        friend struct DomainReceiver;

        // ---------------------------------------------------------------
        // Type erasure for the operation state
        //
        // stdexec::connect() returns a different, non-movable type for every
        // sender type S. Because Domain is a concrete (non-template) class it
        // must hold the operation state behind a pointer. The virtual pair
        // IOpState / OpStateBox<S> is the minimal mechanism for this.
        // Alternatives (unique_ptr<void> + std::function) are equally complex
        // but harder to read.
        // ---------------------------------------------------------------

        struct IOpState
        {
            virtual ~IOpState() = default;
            virtual void start() = 0;
        };

        // OpStateBox<S> is defined after DomainReceiver below (forward-declared here).
        template <class Sender>
        struct OpStateBox;

        // Store() is defined after DomainReceiver below.
        template <class Sender>
        void Store(Sender sender);

        ::Exec::UpdateScheduler _scheduler;
        std::unique_ptr<IOpState> _opState;
    };

    // ---------------------------------------------------------------
    // DomainReceiver
    //
    // stdexec receivers must be named, concrete types — lambdas cannot
    // satisfy the receiver concept. This minimal struct just forwards
    // the three possible completion signals to Domain's private handlers.
    // ---------------------------------------------------------------
    struct DomainReceiver
    {
        using receiver_concept = stdexec::receiver_t;

        Domain* domain;

        void set_value(int exitCode) const noexcept { domain->OnComplete(exitCode); }
        void set_stopped() const noexcept { domain->OnStopped(); }
        [[noreturn]] void set_error(auto&&) noexcept { std::terminate(); }
    };

    // OpStateBox<S> holds the concrete, non-movable operation state returned by
    // stdexec::connect(). It is defined here so DomainReceiver is already complete.
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
