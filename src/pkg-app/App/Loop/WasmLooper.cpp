#if __EMSCRIPTEN__
#include "WasmLooper.h"
#include "Log/Log.h"

#include <emscripten.h>

namespace App::Loop
{
    void WasmLooper::Start(UpdateAction updateAction)
    {
        _updateAction = std::move(updateAction);
        _updateCtx.FrameStartTime = std::chrono::steady_clock::now();

        emscripten_set_main_loop_arg(
            [](void* arg) {
                auto& self = *static_cast<WasmLooper*>(arg);
                self.Update();
            },
            this,
            0,
            true
        );
    }

    void WasmLooper::Update()
    {
        auto startTime = std::chrono::steady_clock::now();
        _updateCtx.FrameDelta = std::chrono::duration_cast<std::chrono::microseconds>(startTime - _updateCtx.FrameStartTime);
        _updateCtx.FrameStartTime = startTime;
        ++_updateCtx.FrameIndex;

        auto proceed = _updateAction(_updateCtx);
        if (!proceed) {
            Log::Debug("emscripten_cancel_main_loop");
            emscripten_cancel_main_loop();
            // emscripten_async_call(
            //     [](void*) {
            //         Log::Trace("emscripten_force_exit(0)");
            //         emscripten_force_exit(0);
            //     },
            //     nullptr,
            //     0
            // );
        }
    }
}
#endif
