#pragma once
#include "Handler.h"
#include <memory>
#include <optional>
#include <sys/types.h>

namespace RunLoop
{
    namespace ExitCode
    {
        constexpr int Success = 0;
        constexpr int Failure = 1;
        constexpr int NotStarted = 126; // Similar to shell's "Command Cannot Execute"

        /// Exit code used when the runner is stopped via cancellation before
        /// the coroutine/task had a chance to produce its own exit code.
        constexpr int Cancelled = 130; // Commonly used exit code for processes terminated via SIGINT (Ctrl+C) as 128+signal
    }

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
        explicit Runner(HandlerPtr handler);
        ~Runner() override;

        Runner(const Runner&) = delete;
        Runner& operator=(const Runner&) = delete;
        Runner(Runner&&) = default;
        Runner& operator=(Runner&&) = default;

        // IRunner
        [[nodiscard]] std::optional<int> Exiting() const override { return GetExitCode(); }

    protected:
        [[nodiscard]] std::optional<int> GetExitCode() const { return _exitCode; }
        void SetExitCode(int exitCode) { _exitCode = exitCode; }

        [[nodiscard]] bool InvokeStart() const { return _handler->Start(); }
        void InvokeStop() const { _handler->Stop(); }
        void InvokeUpdate(const UpdateCtx& ctx) const { _handler->Update(ctx); }

    private:
        HandlerPtr _handler;
        std::optional<int> _exitCode;
    };
}
