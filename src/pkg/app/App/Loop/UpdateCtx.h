#pragma once
#include <chrono>

namespace App::Loop
{
    class IRunner;

    /// Context passed to the runner update action.
    /// Filled with frame timing information depending on the runner implementation.
    struct UpdateCtx
    {
        using Clock = std::chrono::high_resolution_clock;
        using TimePoint = Clock::time_point;
        using Duration = Clock::duration;

        struct Frame
        {
            /// Frame index from initialization
            uint64_t index{};

            /// Frame start time point
            TimePoint startTime;

            /// Time delta since last frame
            Duration delta{};

            /// Delta time in microseconds
            std::chrono::microseconds deltaUs{};

            /// Delta time in seconds as float
            float deltaSeconds{};
        };

        struct Session
        {
            /// Session start time point
            TimePoint startTime;

            /// Total time passed since session initialization
            Duration passed{};

            /// Passed time in microseconds
            std::chrono::microseconds passedUs{};

            /// Passed time in seconds as float
            float passedSeconds{};
        };

        explicit UpdateCtx(IRunner& runner);

        /// Initialize frame timing information at the start of the loop
        void Initialize();

        /// Update frame timing information for each frame
        void Tick();

        IRunner& Runner;
        Frame frame;
        Session session;
    };
}
