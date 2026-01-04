#include "TightLooper.h"

namespace App::Loop
{
    void TightLooper::Start(const HandlerPtr handler)
    {
        UpdateCtx updateCtx{*this};
        updateCtx.Initialize();

        auto proceed = true;
        while (proceed) {
            updateCtx.Tick();
            proceed = handler->Update(*this, updateCtx);
        }
    }

    void TightLooper::Finish(const FinishData& finishData)
    {
        // No-op for TightLooper because it exits from synchronous Start().
    }
}
