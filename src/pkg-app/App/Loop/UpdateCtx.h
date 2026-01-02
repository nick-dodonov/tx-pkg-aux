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

        /// Initialize frame timing information at the start of the loop
        void Initialize();

        /// Update frame timing information for each frame
        void Tick();

        ILooper& Looper;
        uint64_t FrameIndex{};

        std::chrono::time_point<Clock> FrameStartTime;
        std::chrono::microseconds FrameDelta{};
    };
}
