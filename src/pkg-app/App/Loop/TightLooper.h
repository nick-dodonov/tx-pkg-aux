#pragma once
#include "ILooper.h"

namespace App::Loop
{
    /// Runner that runs as fast as possible
    class TightRunner final : public IRunner
    {
    public:
        void Start(HandlerPtr handler) override;
        void Finish(const FinishData& finishData) override;

    private:
        bool _running{};
    };
}
