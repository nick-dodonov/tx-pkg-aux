#include "Factory.h"
#include "TightRunner.h"
#if __EMSCRIPTEN__
#include "WasmRunner.h"
#endif

namespace App::Loop
{
    std::shared_ptr<IRunner> CreateDefaultRunner(std::shared_ptr<Handler> handler)
    {
#if __EMSCRIPTEN__
        return std::make_shared<WasmRunner>(std::move(handler), WasmRunner::Options{.Fps = 120});
#else
        return std::make_shared<TightRunner>(std::move(handler));
#endif
    }

    std::shared_ptr<IRunner> CreateTestRunner(std::shared_ptr<Handler> handler)
    {
        return std::make_shared<TightRunner>(std::move(handler));
    }
}
