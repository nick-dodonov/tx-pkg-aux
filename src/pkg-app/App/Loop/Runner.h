#pragma once
#include "Handler.h"
#include <memory>
#include <optional>

namespace App::Loop
{
    /// Base runner that runs the update action while the handler returns true
    class IRunner: public std::enable_shared_from_this<IRunner>
    {
    public:
        virtual ~IRunner() = default;

        virtual int Run() = 0;
        virtual void Exit(int exitCode) = 0;
    };

    class Runner: public IRunner
    {
    public:
        using HandlerPtr = std::shared_ptr<Handler>;
        Runner(HandlerPtr handler)
            : _handler(std::move(handler))
        {
            _handler->SetRunner(this);
        }

        ~Runner()
        {
            _handler->SetRunner(nullptr);
        }

        Runner(const Runner&) = delete;
        Runner& operator=(const Runner&) = delete;
        Runner(Runner&&) = default;
        Runner& operator=(Runner&&) = default;

    protected:
        static constexpr int SuccessExitCode = 0;
        static constexpr int NotStartedExitCode = 101;
        static constexpr int FailureExitCode = 202;
        [[nodiscard]] std::optional<int> GetExitCode() const { return _exitCode; }
        void SetExitCode(int exitCode) { _exitCode = exitCode; }

        [[nodiscard]] bool InvokeStart() { return _handler->Start(); }
        void InvokeStop() { _handler->Stop(); }
        void InvokeUpdate(const UpdateCtx& ctx) { _handler->Update(ctx); }

    private:
        HandlerPtr _handler;
        std::optional<int> _exitCode;
    };

    /// Factory for simple console runner based on platform
    std::shared_ptr<IRunner> CreateDefaultRunner(std::shared_ptr<Handler> handler);
}
