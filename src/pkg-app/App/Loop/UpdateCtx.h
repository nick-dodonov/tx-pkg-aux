#pragma once
#include <chrono>

namespace App::Loop
{
    class ILooper;

    /// Context passed to the looper update action.
    /// Filled with frame timing information depending on the looper implementation.
    struct UpdateCtx
    {
        using Clock = std::chrono::high_resolution_clock;

        explicit UpdateCtx(ILooper& looper);

        ILooper& Looper;
        uint64_t FrameIndex{};

        std::chrono::time_point<Clock> FrameStartTime;
        std::chrono::microseconds FrameDelta{};
    };
}
