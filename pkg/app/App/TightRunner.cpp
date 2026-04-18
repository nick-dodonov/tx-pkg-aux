#include "TightRunner.h"
#include "Log/Log.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#include <thread>
#endif

namespace App
{
#ifdef __EMSCRIPTEN__
    // Workaround for emscripten exit handling with error code.
    // Emscripten has strange behavior when emscripten_sleep(0) is used:
    //  if main function returns with error, node/emrun exit code is anyway 0 (it breaks tests).
    // So we register atexit handler to catch normal exit and set non-zero code in that case.
    //
    // TODO: possibly rewrite TightRunner for emscripten in another way to avoid this hack.
    static bool _asyncifyEnabled;
    static bool _handlerRegistered;
    static std::optional<int> _lastExitCode;
    static void OnExitHandler()
    {
        if (_lastExitCode) {
            auto exitCode = *_lastExitCode;
            Log::Trace("{}", exitCode);
            exit(exitCode);
        }
    }
#endif

    TightRunner::TightRunner(HandlerPtr handler, Options options)
        : Runner(std::move(handler))
        , _options(options)
    {
#ifdef __EMSCRIPTEN__
        _asyncifyEnabled = (emscripten_has_asyncify() != 0);
        Log::Trace("asyncify={} exitWorkaround={}",
            _asyncifyEnabled ? "ON" : "OFF",
            _options.wasmExitWorkaround ? "ON" : "OFF");
        if (!_handlerRegistered && _asyncifyEnabled && _options.wasmExitWorkaround) {
            _handlerRegistered = true;
            if (std::atexit(OnExitHandler) != 0) {
                Log::Error("Failed to register exit handler");
            }
        }
#else
        Log::Trace("timeout={}ms", _options.sleepDuration.count());
#endif
    }

    TightRunner::~TightRunner()
    {
        Log::Trace("destroy");
    }

    int TightRunner::Run()
    {
        RunLoop::UpdateCtx updateCtx{};
        updateCtx.Initialize();

        if (!InvokeStart()) {
            Log::Error("Started handler failed");
            return RunLoop::ExitCode::NotStarted;
        }

        _running = true;
        while (_running) {
            updateCtx.Tick();
            InvokeUpdate(updateCtx);

#ifdef __EMSCRIPTEN__
            if (_asyncifyEnabled) {
                // Yield control to the event loop
                //  https://emscripten.org/docs/api_reference/emscripten.h.html#sleeping
                emscripten_sleep(0);
            }
#else
            if (_options.sleepDuration.count() > 0) {
                std::this_thread::sleep_for(_options.sleepDuration);
            }
#endif
        }

        InvokeStop();

        auto exitCode = GetExitCode().value_or(RunLoop::ExitCode::Success);
#ifdef __EMSCRIPTEN__
        _lastExitCode = exitCode;
#endif
        return exitCode;
    }

    void TightRunner::Exit(int exitCode)
    {
        SetExitCode(exitCode);
        _running = false;
    }
}
