#include "TightRunner.h"

namespace App::Loop
{
    void TightRunner::Start()
    {
        UpdateCtx updateCtx{*this};
        updateCtx.Initialize();

        _running = true;
        _handler->Started(*this);

        while (_running) {
            updateCtx.Tick();
            _running = _handler->Update(*this, updateCtx);
        }

        _handler->Stopping(*this);
        _running = false;
    }

    void TightRunner::Finish(const FinishData& finishData)
    {
        _running = false;
    }
}
