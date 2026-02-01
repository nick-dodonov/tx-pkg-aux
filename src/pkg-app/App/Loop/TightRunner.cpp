#include "TightRunner.h"
#include "Log/Log.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace App::Loop
{
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
            // Yield control to browser to process events
            emscripten_sleep(0);
#endif
        }

        InvokeStop();
        return GetExitCode().value_or(SuccessExitCode);
    }

    void TightRunner::Exit(int exitCode)
    {
        SetExitCode(exitCode);
        _running = false;
    }
}
