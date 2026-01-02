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
        using TimePoint = Clock::time_point;
        using Duration = Clock::duration;

        struct Frame
        {
            uint64_t index{};
            TimePoint startTime;

            Duration delta{};  // maximum precision
            std::chrono::microseconds microseconds{};
            float seconds{};
        };

        explicit UpdateCtx(ILooper& looper);

        /// Initialize frame timing information at the start of the loop
        void Initialize();

        /// Update frame timing information for each frame
        void Tick();

        ILooper& Looper;
        Frame frame;
    };
}
