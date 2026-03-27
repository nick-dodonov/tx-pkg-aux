#pragma once
#include "RunLoop/Handler.h"
#include "RunLoop/Runner.h"
#include <chrono>

namespace App
{
    /// Synchronous runner that executes the update loop as fast as possible
    /// Polls io_context directly until Exit() is called
    /// Suitable for: tests, benchmarks, and applications without frame rate limits
    class TightRunner final: public RunLoop::Runner
    {
    public:
        struct Options
        {
            /// Enable workaround for emscripten exit code handling (see TightRunner.cpp)
            bool wasmExitWorkaround{};
            /// Sleep duration per iteration outside emscripten (0 = no sleep)
            std::chrono::milliseconds sleepDuration{};
        };

        TightRunner(HandlerPtr handler, Options options);
        ~TightRunner();

        int Run() override;
        void Exit(int exitCode) override;

    private:
        Options _options;
        bool _running{};
    };
}
