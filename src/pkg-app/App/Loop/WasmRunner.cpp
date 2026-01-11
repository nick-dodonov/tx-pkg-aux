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
        _updateCtx.Tick();
        InvokeUpdate(_updateCtx);
    }

    void WasmRunner::Exit(int exitCode)
    {
        SetExitCode(exitCode);
        InvokeStop();

        Log::Debug("emscripten_cancel_main_loop");
        emscripten_cancel_main_loop();

        // TODO: find what stops the exit with error message: "program exited (with status: 1), but keepRuntimeAlive() is set (counter=1) due to an async
        // operation, so halting execution but not exiting the runtime or preventing further async execution (you can use emscripten_force_exit, if you want to
        // force a true shutdown)"
        //  Log::Trace("wasm: exit: {}", exitCode);
        //  exit(exitCode);

        // WASM explicit exit because emscripten_set_main_loop_arg() never returns
        // When calling from inside emscripten_set_main_loop callback, 
        //  calling emscripten_force_exit(exitCode) directly can cause issues.
        emscripten_async_call(
            [](void* arg) {
                auto exitCode = static_cast<WasmRunner*>(arg)->GetExitCode().value_or(0);
                Log::Trace("emscripten_force_exit({})", exitCode);
                emscripten_force_exit(exitCode);
            },
            this,
            0
        );
    }
}
#endif
