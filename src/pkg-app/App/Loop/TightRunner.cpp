#include "TightRunner.h"

namespace App::Loop
{
    void TightRunner::Start(const HandlerPtr handler)
    {
        UpdateCtx updateCtx{*this};
        updateCtx.Initialize();

        _running = true;
        handler->Started(*this);

        while (_running) {
            updateCtx.Tick();
            _running = handler->Update(*this, updateCtx);
        }

        handler->Stopping(*this);
        _running = false;
    }

    void TightRunner::Finish(const FinishData& finishData)
    {
        _running = false;
    }
}
