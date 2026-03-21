#pragma once

#include "App/Loop/Handler.h"
#include "Exec/UpdateScheduler.h"

#include <functional>
#include <memory>

namespace App::Exec
{
    /// Loop handler that runs a sender<int> on an UpdateScheduler.
    /// When the sender completes, exits the runner with the resulting int.
    /// Analogous to Coro::Domain from app-capy, but using stdexec senders.
    class Domain: public App::Loop::Handler
    {
    public:
        /// Type-erased factory: call to connect + store the operation, return a starter.
        using TaskFactory = std::function<void(Domain&)>;

        /// Create a Domain from a callable: (Exec::UpdateScheduler::Scheduler) -> sender<int>
        template <class SenderFactory>
        requires std::invocable<SenderFactory, ::Exec::UpdateScheduler::Scheduler>
        explicit Domain(SenderFactory factory)
            : _taskFactory(MakeFactory(std::move(factory)))
        {}

        ~Domain();

        ::Exec::UpdateScheduler& GetUpdateScheduler() noexcept { return _scheduler; }

        // App::Loop::Handler
        bool Start() override;
        void Stop() override;
        void Update(const App::Loop::UpdateCtx& ctx) override;

    private:
        /// Called by the receiver when the sender completes with a value
        void OnComplete(int exitCode);

        /// Called by the receiver when the sender is stopped
        void OnStopped();

        friend struct DomainReceiver;

        template <class SenderFactory>
        static TaskFactory MakeFactory(SenderFactory factory);

        ::Exec::UpdateScheduler _scheduler;
        TaskFactory _taskFactory;

        /// Type-erased operation state storage
        struct IOpState
        {
            virtual ~IOpState() = default;
            virtual void Start() = 0;
        };
        std::unique_ptr<IOpState> _opState;

        template <class Sender>
        struct OpStateBox;
    };

    /// Receiver that forwards completion to the Domain
    struct DomainReceiver
    {
        using receiver_concept = stdexec::receiver_t;

        Domain* domain;

        void set_value(int exitCode) const noexcept { domain->OnComplete(exitCode); }

        void set_stopped() const noexcept { domain->OnStopped(); }

        [[noreturn]] void set_error(auto&& /*unused*/) noexcept { std::terminate(); }
    };

    template <class Sender>
    struct Domain::OpStateBox: Domain::IOpState
    {
        using OpState = stdexec::connect_result_t<Sender, DomainReceiver>;

        // Construct the operation state in-place via connect
        OpStateBox(Sender&& sender, Domain& domain)
            : op(stdexec::connect(static_cast<Sender&&>(sender), DomainReceiver{&domain}))
        {}

        void Start() override { stdexec::start(op); }

        OpState op;
    };

    template <class SenderFactory>
    Domain::TaskFactory Domain::MakeFactory(SenderFactory factory)
    {
        return [f = std::move(factory)](Domain& self) mutable {
            auto sender = std::move(f)(self._scheduler.GetScheduler());
            using S = decltype(sender);
            self._opState = std::make_unique<OpStateBox<S>>(std::move(sender), self);
            self._opState->Start();
        };
    }
} // namespace App::Exec
