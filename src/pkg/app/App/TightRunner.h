#pragma once
#include "RunLoop/Handler.h"
#include "RunLoop/Runner.h"

namespace App
{
    /// Synchronous runner that executes the update loop as fast as possible
    /// Polls io_context directly until Exit() is called
    /// Suitable for: tests, benchmarks, and applications without frame rate limits
    class TightRunner final: public RunLoop::Runner
    {
    public:
        TightRunner(HandlerPtr handler, bool wasmExitWorkaround = true);
        ~TightRunner();

        int Run() override;
        void Exit(int exitCode) override;

    private:
        bool _running{};
    };
}
