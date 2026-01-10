#pragma once
#include "Handler.h"
#include "Runner.h"

namespace App::Loop
{
#if __EMSCRIPTEN__
    /// Runner that integrates with emscripten main loop
    class WasmRunner final: public Runner<IHandler>
    {
    public:
        struct Options
        {
            int Fps{};
        };

        WasmRunner(HandlerPtr handler, Options options);
        void Start() override;
        void Finish(const FinishData& finishData) override;

    private:
        Options _options;
        UpdateCtx _updateCtx;

        void Update();
    };
#endif
}
