#pragma once
#include "Handler.h"
#include <memory>

namespace App::Loop
{
    /// Factory for simple console runner based on platform
    std::shared_ptr<IRunner> CreateDefaultRunner(std::shared_ptr<Handler> handler);
}
