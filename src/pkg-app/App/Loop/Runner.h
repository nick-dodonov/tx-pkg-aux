#include "IRunner.h"

namespace App::Loop
{
    /// Factory for simple console runner based on platform
    std::shared_ptr<IRunner> CreateDefaultRunner(std::shared_ptr<IHandler> handler);
}
