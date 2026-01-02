#include "UpdateCtx.h"
#include "ILooper.h"

namespace App::Loop
{
    UpdateCtx::UpdateCtx(ILooper& looper)
        : Looper(looper)
    {
    }

    void UpdateCtx::Initialize()
    {
        FrameStartTime = Clock::now();
        FrameIndex = 0;
        FrameDelta = {};
    }

    void UpdateCtx::Tick()
    {
        auto startTime = Clock::now();
        FrameDelta = std::chrono::duration_cast<std::chrono::microseconds>(startTime - FrameStartTime);
        FrameStartTime = startTime;
        ++FrameIndex;
    }
}
