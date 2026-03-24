#pragma once
#include "Handler.h"
#include <memory>
#include <optional>

namespace RunLoop
{
    /// Base runner that runs the update action
    class IRunner: public std::enable_shared_from_this<IRunner>
    {
    public:
        virtual ~IRunner() = default;

        virtual int Run() = 0;
        virtual void Exit(int exitCode) = 0;

        /// Returns the pending exit code if Exit() has already been called, or
        /// std::nullopt if the runner has not yet been asked to exit.
        /// Use this to avoid overwriting an already-requested exit code.
        [[nodiscard]] virtual std::optional<int> Exiting() const = 0;
    };

    class Runner: public IRunner
    {
    public:
        using HandlerPtr = std::shared_ptr<Handler>;
        Runner(HandlerPtr handler);
        ~Runner();

        Runner(const Runner&) = delete;
        Runner& operator=(const Runner&) = delete;
        Runner(Runner&&) = default;
        Runner& operator=(Runner&&) = default;

        /// Exit code used when the runner is stopped via cancellation before
        /// the coroutine/task had a chance to produce its own exit code.
        static constexpr int CancelledExitCode = 130;

        // IRunner
        [[nodiscard]] std::optional<int> Exiting() const override { return GetExitCode(); }

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
}
