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
        frame.startTime = Clock::now();
        frame.index = 0;
        frame.delta = {};
        frame.microseconds = {};
        frame.seconds = 0.0f;
    }

    void UpdateCtx::Tick()
    {
        auto startTime = Clock::now();
        frame.delta = startTime - frame.startTime;
        frame.startTime = startTime;
        ++frame.index;

        // Cache converted values
        frame.microseconds = std::chrono::duration_cast<std::chrono::microseconds>(frame.delta);
        frame.seconds = std::chrono::duration<float>(frame.delta).count();
    }
}
