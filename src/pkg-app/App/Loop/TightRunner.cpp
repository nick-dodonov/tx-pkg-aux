#include "TightRunner.h"
#include "Log/Log.h"

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
