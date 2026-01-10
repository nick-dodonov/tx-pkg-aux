#pragma once
#include "Handler.h"
#include <memory>

namespace App::Loop
{
    /// Context passed to the runner when updating is finished.
    /// Used to signal exit code or other finalization options
    /// in several runner implementations that cannot exit from the synchronous Start().
    struct FinishData
    {
        explicit FinishData(const int exitCode)
            : ExitCode(exitCode)
        {}

        int ExitCode{};
    };

    /// Base runner that runs the update action while the handler returns true
    class IRunner: public std::enable_shared_from_this<IRunner>
    {
    public:
        virtual ~IRunner() = default;

        virtual void Start() = 0;
        virtual void Finish(const FinishData& finishData) = 0;
    };

    /// Typed runner for specific runner type
    template <typename THandler>
    class Runner: public IRunner
    {
    public:
        using HandlerPtr = std::shared_ptr<THandler>;
        Runner(HandlerPtr handler)
            : _handler(std::move(handler))
        {}

    protected:
        HandlerPtr _handler;
    };

    /// Factory for simple console runner based on platform
    std::shared_ptr<IRunner> CreateDefaultRunner(std::shared_ptr<IHandler> handler);
}
