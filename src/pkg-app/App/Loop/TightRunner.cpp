#include "TightRunner.h"
#include "Log/Log.h"

namespace App::Loop
{
    int TightRunner::Run()
    {
        UpdateCtx updateCtx{*this};
        updateCtx.Initialize();

        _running = true;
        if (!InvokeStarted()) {
            Log::Error("Started handler failed");
            _running = false;
            return NotStartedExitCode;
        }

        while (_running) {
            updateCtx.Tick();
            _running = InvokeUpdate(updateCtx);
        }

        InvokeStopping();
        _running = false;
        return GetExitCode().value_or(SuccessExitCode);
    }

    void TightRunner::Exit(int exitCode)
    {
        _running = false;
        SetExitCode(exitCode);
    }
}
