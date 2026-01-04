#pragma once
#include "ILooper.h"

namespace App::Loop
{
    /// Looper that runs as fast as possible
    class TightLooper final : public ILooper
    {
    public:
        void Start(HandlerPtr handler) override;
        void Finish(const FinishData& finishData) override;

    private:
        bool _running{};
    };
}
