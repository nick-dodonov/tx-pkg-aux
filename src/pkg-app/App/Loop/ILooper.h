#pragma once
#include <chrono>
#include <functional>

namespace App::Loop
{
    class ILooper;

    /// Context passed to the looper update action.
    /// Filled with frame timing information depending on the looper implementation.
    struct UpdateCtx
    {
        using Clock = std::chrono::high_resolution_clock;

        explicit UpdateCtx(ILooper& looper)
            : Looper(looper)
        {}

        ILooper& Looper;
        uint64_t FrameIndex{};

        std::chrono::time_point<Clock> FrameStartTime;
        std::chrono::microseconds FrameDelta{};
    };

    /// Context passed to the looper when updating is finished.
    /// Used to signal exit code or other finalization options 
    /// in several looper implementations that cannot exit from the synchronous Start().
    struct FinishData
    {
        explicit FinishData(int exitCode)
            : ExitCode(exitCode)
        {}

        int ExitCode{};
    };

    /// Simple looper that runs the update action while it returns true
    class ILooper : public std::enable_shared_from_this<ILooper>
    {
    public:
        virtual ~ILooper() = default;

        using UpdateAction = std::function<bool(const UpdateCtx& ctx)>;
        virtual void Start(UpdateAction updateAction) = 0;

        virtual void Finish(const FinishData& finishData) = 0;

    private:
        UpdateAction _updateAction;
    };
}
