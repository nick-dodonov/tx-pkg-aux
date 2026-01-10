#include "TightRunner.h"
#include "Log/Log.h"

namespace App::Loop
{
    void TightRunner::Start()
    {
        UpdateCtx updateCtx{*this};
        updateCtx.Initialize();

        _running = true;
        if (!InvokeStarted()) {
            Log::Error("Started handler failed");
            _running = false;
            return;
        }

        while (_running) {
            updateCtx.Tick();
            _running = InvokeUpdate(updateCtx);
        }

        InvokeStopping();
        _running = false;
    }

    void TightRunner::Finish(const FinishData& finishData)
    {
        _running = false;
    }
}
