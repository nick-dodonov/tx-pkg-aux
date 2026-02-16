#pragma once
#include "Handler.h"
#include <memory>

namespace App::Loop
{
    /// Factory for simple console runner based on platform
    std::shared_ptr<IRunner> CreateDefaultRunner(std::shared_ptr<Handler> handler);

    /// Factory for test runner (read test/app/WASM_TESTING.md for details)
    std::shared_ptr<IRunner> CreateTestRunner(std::shared_ptr<Handler> handler);
}
