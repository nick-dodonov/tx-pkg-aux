#pragma once
#include "Handler.h"
#include "Runner.h"

namespace App::Loop
{
    /// Runner that runs as fast as possible
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
