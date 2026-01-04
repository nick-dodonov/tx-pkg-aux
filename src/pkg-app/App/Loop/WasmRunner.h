#pragma once
 #include "IRunner.h"

namespace App::Loop
{
#if __EMSCRIPTEN__
    /// Runner that integrates with emscripten main loop
    class WasmRunner final : public IRunner
    {
    public:
        struct Options
        {
            int Fps{};
        };

        WasmRunner(Options options);
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
