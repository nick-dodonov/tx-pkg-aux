#pragma once
#include "RunLoop/Handler.h"
#include "RunLoop/Runner.h"

namespace App
{
    /// Factory for simple console runner based on platform
    std::shared_ptr<RunLoop::IRunner> CreateDefaultRunner(std::shared_ptr<RunLoop::Handler> handler);

    /// Factory for test runner (read test/app/WASM_TESTING.md for details)
    std::shared_ptr<RunLoop::IRunner> CreateTestRunner(std::shared_ptr<RunLoop::Handler> handler);
}
