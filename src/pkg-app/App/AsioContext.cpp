#include "AsioContext.h"
#include "Log/Log.h"
#if __EMSCRIPTEN__
    #include <emscripten.h>
#endif

namespace App
{
    AsioContext::AsioContext()
    {
        Log::Debug("AsioContext: initializing");

        // TODO: runtime selector for executors strategy
        // auto executor = asio::system_executor();
        // auto& io_context = get_io_context();
    }

    AsioContext::~AsioContext()
    {
        Log::Debug("AsioContext: destroyed");
    }

    [[nodiscard]] int AsioContext::Run()
    {
        Log::Debug("AsioContext: Run");
#if __EMSCRIPTEN__
        emscripten_set_main_loop_arg(
            [](void* arg) {
                constexpr auto kRunFor = std::chrono::milliseconds(16);

                auto* io_context = static_cast<boost::asio::io_context*>(arg);
                auto count = io_context->run_for(kRunFor);
                if (io_context->stopped()) {
                    Log::Debug("AsioContext: wasm: ran count: {} (stopped)", count);
                    emscripten_cancel_main_loop();
                    Log::Debug("AsioContext: wasm: exit(0)");
                    exit(0);
                    Log::Debug("AsioContext: wasm: emscripten_force_exit(0)");
                    emscripten_force_exit(0);
                } else if (count > 0) {
                    Log::Debug("AsioContext: wasm: ran count: {} (continue)", count);
                }
            },
            &_io_context,
            0,
            true
        );
#else
        // asio::detail::global<asio::system_context>().join();
        _io_context.run();
#endif
        return 0;
    }
}
