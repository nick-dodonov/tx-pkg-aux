#include "Runner.h"

namespace App::Loop
{
    Runner::Runner(HandlerPtr handler)
        : _handler(std::move(handler))
    {
        _handler->SetRunner(this);
    }

    Runner::~Runner()
    {
        _handler->SetRunner(nullptr);
    }
}
