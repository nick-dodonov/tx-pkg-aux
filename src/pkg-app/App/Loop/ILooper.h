#pragma once
#include <chrono>
#include <functional>

namespace App::Loop
{
    class ILooper;

    struct UpdateCtx
    {
        explicit UpdateCtx(ILooper& looper)
            : Looper(looper)
        {}

        ILooper& Looper;
        uint64_t FrameIndex{};
        std::chrono::steady_clock::time_point FrameStartTime;
        std::chrono::microseconds FrameDelta{};
    };

    /// Simple looper that runs the update action while it returns true
    class ILooper
    {
    public:
        virtual ~ILooper() = default;

        using UpdateAction = std::function<bool(const UpdateCtx&)>;
        virtual void Start(UpdateAction updateAction) = 0;

    private:
        UpdateAction _updateAction;
    };
}
