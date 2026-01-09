#include "Runner.h"
#if __EMSCRIPTEN__
    #include "WasmRunner.h"
#else
    #include "TightRunner.h"
#endif

namespace App::Loop
{
    std::shared_ptr<IRunner> CreateDefaultRunner()
    {
#if __EMSCRIPTEN__
        return std::make_shared<WasmRunner>(WasmRunner{{.Fps = 120}});
#else
        return std::make_shared<TightRunner>();
#endif
    }
}
