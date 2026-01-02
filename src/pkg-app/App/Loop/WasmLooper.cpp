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
        _updateCtx.Initialize();

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
        _updateCtx.Tick();

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

    void WasmLooper::Finish(const FinishData& finishData)
    {
            // WASM explicit exit because emscripten_set_main_loop_arg() never returns
            auto exitCode = finishData.ExitCode;

            //TODO: find what stops the exit with error message: "program exited (with status: 1), but keepRuntimeAlive() is set (counter=1) due to an async operation, so halting execution but not exiting the runtime or preventing further async execution (you can use emscripten_force_exit, if you want to force a true shutdown)"
            // Log::Trace("wasm: exit: {}", exitCode);
            // exit(exitCode);

            Log::Trace("wasm: emscripten_force_exit: {}", exitCode);
            emscripten_force_exit(exitCode);
    }
}
#endif
