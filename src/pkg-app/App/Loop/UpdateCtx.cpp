#include "UpdateCtx.h"

namespace App::Loop
{
    UpdateCtx::UpdateCtx(IRunner& runner)
        : Runner(runner)
    {}

    void UpdateCtx::Initialize()
    {
        session.startTime = Clock::now();
        frame.startTime = session.startTime;
        frame.index = 0;
        frame.delta = {};
        frame.deltaUs = {};
        frame.deltaSeconds = {};
        session.passed = {};
        session.passedUs = {};
        session.passedSeconds = {};
    }

    void UpdateCtx::Tick()
    {
        auto startTime = Clock::now();

        // Update session elapsed time
        session.passed = startTime - session.startTime;
        session.passedSeconds = std::chrono::duration<float>(session.passed).count();

        // Update frame delta time (calculate BEFORE updating frame.startTime)
        frame.delta = startTime - frame.startTime;
        frame.deltaUs = std::chrono::duration_cast<std::chrono::microseconds>(frame.delta);
        frame.deltaSeconds = std::chrono::duration<float>(frame.delta).count();

        // Now update frame timing for next frame
        ++frame.index;
        frame.startTime = startTime;
    }
}
