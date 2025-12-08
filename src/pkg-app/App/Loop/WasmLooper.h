#pragma once
 #include "ILooper.h"

namespace App::Loop
{
#if __EMSCRIPTEN__
    /// Looper that integrates with emscripten main loop
    class WasmLooper final : public ILooper
    {
    public:
        WasmLooper()
            : _updateCtx{*this}
        {}

        void Start(UpdateAction updateAction) override;

    private:
        UpdateAction _updateAction;
        UpdateCtx _updateCtx;

        void Update();
    };
#endif
}
