#include "TightLooper.h"

namespace App::Loop
{
    void TightLooper::Start(const UpdateAction updateAction)
    {
        UpdateCtx updateCtx{*this};
        updateCtx.FrameStartTime = UpdateCtx::Clock::now();

        auto proceed = true;
        while (proceed) {
            auto startTime = UpdateCtx::Clock::now();
            updateCtx.FrameDelta = std::chrono::duration_cast<std::chrono::microseconds>(startTime - updateCtx.FrameStartTime);
            updateCtx.FrameStartTime = startTime;
            ++updateCtx.FrameIndex;

            proceed = updateAction(updateCtx);
        }
    }
}
