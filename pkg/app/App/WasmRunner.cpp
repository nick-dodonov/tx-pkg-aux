#if __EMSCRIPTEN__
#include "WasmRunner.h"
#include "Log/Log.h"

#include <emscripten.h>

namespace App
{
    WasmRunner::WasmRunner(HandlerPtr handler, Options options)
        : Runner{std::move(handler)}
        , _options{options}
        , _updateCtx{*this}
    {}

    int WasmRunner::Run()
    {
        _updateCtx.Initialize();

        if (!InvokeStart()) {
            Log::Error("Started handler failed");
            return NotStartedExitCode;
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

        // emscripten_set_main_loop_arg never returns control
        __builtin_unreachable();
        return NotStartedExitCode;
    }

    void WasmRunner::Update()
    {
        if (auto exitCode = GetExitCode()) {
            StopAndExit(exitCode.value());
            return;
        }

        _updateCtx.Tick();
        InvokeUpdate(_updateCtx);
    }

    void WasmRunner::Exit(int exitCode)
    {
        SetExitCode(exitCode);

        // cannot StopAndExit() here because in case of concurrent Exit() 
        //  stop handler invoke on wrong thread and exit/emscripten_force_exit fails to stop main thread (hanging)
        // TODO: find the way to invoke callback on main thread without `if` in Update()
    }

    void WasmRunner::StopAndExit(int exitCode)
    {
        InvokeStop();

        Log::Debug("emscripten_cancel_main_loop");
        emscripten_cancel_main_loop();

        // WASM explicit exit because emscripten_set_main_loop_arg() never returns
        // When calling from inside emscripten_set_main_loop callback, 
        //  calling emscripten_force_exit(exitCode) directly can cause issues.
        emscripten_async_call(
            [](void* arg) {
                auto exitCode = static_cast<WasmRunner*>(arg)->GetExitCode().value_or(0);

                Log::Trace("emscripten_force_exit({})", exitCode);
                emscripten_force_exit(exitCode);

                Log::Trace("exit({})", exitCode);
                exit(exitCode);
            },
            this,
            0
        );
    }
}
#endif
