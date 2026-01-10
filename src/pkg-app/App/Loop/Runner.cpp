#include "Runner.h"
#include <memory>
#if __EMSCRIPTEN__
    #include "WasmRunner.h"
#else
    #include "TightRunner.h"
#endif

namespace App::Loop
{
    std::shared_ptr<IRunner> CreateDefaultRunner(std::shared_ptr<IHandler> handler)
    {
#if __EMSCRIPTEN__
        return std::make_shared<WasmRunner>(std::move(handler), WasmRunner::Options{.Fps = 120});
#else
        return std::make_shared<TightRunner>(std::move(handler));
#endif
    }
}
