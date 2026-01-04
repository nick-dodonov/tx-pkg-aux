#pragma once
 #include "ILooper.h"

namespace App::Loop
{
#if __EMSCRIPTEN__
    /// Looper that integrates with emscripten main loop
    class WasmLooper final : public ILooper
    {
    public:
        struct Options
        {
            int Fps{};
        };

        WasmLooper(Options options);
        void Start(HandlerPtr handler) override;
        void Finish(const FinishData& finishData) override;

    private:
        Options _options;
        HandlerPtr _handler;
        UpdateCtx _updateCtx;

        void Update();
    };
#endif
}
