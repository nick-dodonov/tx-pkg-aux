#include "Runner.h"

namespace RunLoop
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
