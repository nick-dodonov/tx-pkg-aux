#include "IRunner.h"

namespace App::Loop
{
    /// Factory for simple console runner based on platform
    std::shared_ptr<Loop::IRunner> CreateDefaultRunner();
}
