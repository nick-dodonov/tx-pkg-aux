#pragma once
#include "Handler.h"
#include "Runner.h"

namespace App::Loop
{
    /// Runner that runs as fast as possible
    class TightRunner final: public Runner<IHandler>
    {
    public:
        using Runner<IHandler>::Runner;

        void Start() override;
        void Finish(const FinishData& finishData) override;

    private:
        bool _running{};
    };
}
