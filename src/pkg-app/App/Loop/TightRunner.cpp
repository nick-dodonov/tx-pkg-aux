#include "TightRunner.h"
#include "Log/Log.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace App::Loop
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

    TightRunner::TightRunner(HandlerPtr handler)
        : Runner(std::move(handler))
    {
#ifdef __EMSCRIPTEN__
        _asyncifyEnabled = (emscripten_has_asyncify() != 0);
        Log::Trace("created (asyncify={})", _asyncifyEnabled ? "ON" : "OFF");
        if (!_handlerRegistered && _asyncifyEnabled) {
            _handlerRegistered = true;
            if (std::atexit(OnExitHandler) != 0) {
                Log::Error("Failed to register exit handler");
            }
        }
#else
        Log::Trace("created");
#endif
    }

    TightRunner::~TightRunner()
    {
        Log::Trace("destroy");
    }

    int TightRunner::Run()
    {
        UpdateCtx updateCtx{*this};
        updateCtx.Initialize();

        if (!InvokeStart()) {
            Log::Error("Started handler failed");
            return NotStartedExitCode;
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
#endif
        }

        InvokeStop();

        auto exitCode = GetExitCode().value_or(SuccessExitCode);
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
