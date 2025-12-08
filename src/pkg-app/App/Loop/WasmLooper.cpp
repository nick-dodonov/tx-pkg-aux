#if __EMSCRIPTEN__
#include "WasmLooper.h"
#include "Log/Log.h"

#include <emscripten.h>

namespace App::Loop
{
    WasmLooper::WasmLooper(Options options)
        : _options{options}
        , _updateCtx{*this}
    {}

    void WasmLooper::Start(UpdateAction updateAction)
    {
        _updateAction = std::move(updateAction);
        _updateCtx.FrameStartTime = UpdateCtx::Clock::now();

        emscripten_set_main_loop_arg(
            [](void* arg) {
                auto& self = *static_cast<WasmLooper*>(arg);
                self.Update();
            },
            this,
            _options.Fps,
            true
        );
    }

    void WasmLooper::Update()
    {
        auto startTime = UpdateCtx::Clock::now();
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
