#include "TightLooper.h"

namespace App::Loop
{
    void TightLooper::Start(const HandlerPtr handler)
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

    void TightLooper::Finish(const FinishData& finishData)
    {
        _running = false;
    }
}
