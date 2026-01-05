#if __EMSCRIPTEN__
    #include "WasmRunner.h"
#else
    #include "TightRunner.h"
#endif

namespace App::Loop
{
    std::shared_ptr<Loop::IRunner> CreateDefaultRunner()
    {
#if __EMSCRIPTEN__
        return std::make_shared<Loop::WasmRunner>(Loop::WasmRunner{{.Fps = 120}});
#else
        return std::make_shared<Loop::TightRunner>();
#endif
    }
}
