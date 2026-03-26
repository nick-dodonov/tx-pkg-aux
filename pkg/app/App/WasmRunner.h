#pragma once
#if __EMSCRIPTEN__
#include "RunLoop/Handler.h"
#include "RunLoop/Runner.h"

namespace App
{
    /// Runner that integrates with emscripten main loop
    class WasmRunner final: public RunLoop::Runner
    {
    public:
        struct Options
        {
            int Fps{};
        };

        WasmRunner(HandlerPtr handler, Options options);
        int Run() override;
        void Exit(int exitCode) override;

    private:
        Options _options;
        RunLoop::UpdateCtx _updateCtx;

        void Update();
        void StopAndExit(int exitCode);
    };
}
#endif
