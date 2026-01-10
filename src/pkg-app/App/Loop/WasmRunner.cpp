#if __EMSCRIPTEN__
#include "WasmRunner.h"
#include "Log/Log.h"

#include <emscripten.h>

namespace App::Loop
{
    WasmRunner::WasmRunner(HandlerPtr handler, Options options)
        : Runner{std::move(handler)}
        , _options{options}
        , _updateCtx{*this}
    {}

    void WasmRunner::Start()
    {
        _updateCtx.Initialize();

        if (!InvokeStarted()) {
            Log::Error("Started handler failed");
            return;
        }

        emscripten_set_main_loop_arg(
            [](void* arg) {
                auto& self = *static_cast<WasmRunner*>(arg);
                self.Update();
            },
            this,
            _options.Fps,
            true
        );
    }

    void WasmRunner::Update()
    {
        _updateCtx.Tick();

        auto proceed = InvokeUpdate(_updateCtx);
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

    void WasmRunner::Finish(const FinishData& finishData)
    {
        InvokeStopping();

        // WASM explicit exit because emscripten_set_main_loop_arg() never returns
        auto exitCode = finishData.ExitCode;

        // TODO: find what stops the exit with error message: "program exited (with status: 1), but keepRuntimeAlive() is set (counter=1) due to an async
        // operation, so halting execution but not exiting the runtime or preventing further async execution (you can use emscripten_force_exit, if you want to
        // force a true shutdown)"
        //  Log::Trace("wasm: exit: {}", exitCode);
        //  exit(exitCode);

        Log::Trace("wasm: emscripten_force_exit: {}", exitCode);
        emscripten_force_exit(exitCode);
    }
}
#endif
