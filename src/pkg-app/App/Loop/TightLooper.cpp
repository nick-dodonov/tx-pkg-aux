#include "TightLooper.h"

namespace App::Loop
{
    void TightLooper::Start(const UpdateAction updateAction)
    {
        UpdateCtx updateCtx{*this};
        updateCtx.FrameStartTime = std::chrono::steady_clock::now();

        auto proceed = true;
        while (proceed) {
            auto startTime = std::chrono::steady_clock::now();
            updateCtx.FrameDelta = std::chrono::duration_cast<std::chrono::microseconds>(startTime - updateCtx.FrameStartTime);
            updateCtx.FrameStartTime = startTime;
            ++updateCtx.FrameIndex;

            proceed = updateAction(updateCtx);
        }
    }
}
