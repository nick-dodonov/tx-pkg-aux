#pragma once
#include "Handler.h"
#include "Runner.h"

namespace App::Loop
{
    /// Synchronous runner that executes the update loop as fast as possible
    /// Polls io_context directly until Exit() is called
    /// Suitable for: tests, benchmarks, and applications without frame rate limits
    class TightRunner final: public Runner
    {
    public:
        using Runner::Runner;

        int Run() override;
        void Exit(int exitCode) override;

    private:
        bool _running{};
    };
}
