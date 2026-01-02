#include "TightLooper.h"

namespace App::Loop
{
    void TightLooper::Start(const UpdateAction updateAction)
    {
        UpdateCtx updateCtx{*this};
        updateCtx.Initialize();

        auto proceed = true;
        while (proceed) {
            updateCtx.Tick();
            proceed = updateAction(updateCtx);
        }
    }

    void TightLooper::Finish(const FinishData& finishData)
    {
        // No-op for TightLooper because it exits from synchronous Start().
    }
}
