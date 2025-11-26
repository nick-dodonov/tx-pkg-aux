#include "AsioContext.h"
#include "Log/Log.h"
#if __EMSCRIPTEN__
    #include <emscripten.h>
#endif

namespace App
{
    AsioContext::AsioContext()
    {
        Log::Trace("AsioContext: initializing");

        // TODO: selector for executors strategy, i.e. support system_executor (thread pool)
        // auto executor = asio::system_executor();
        // auto& io_context = get_io_context();
    }

    AsioContext::~AsioContext()
    {
        Log::Trace("AsioContext: destroyed");
    }

    [[nodiscard]] int AsioContext::Run()
    {
        Log::Debug("AsioContext: Run");
#if __EMSCRIPTEN__
        emscripten_set_main_loop_arg(
            [](void* arg) {
                auto* io_context = static_cast<boost::asio::io_context*>(arg);
                if (!io_context->stopped()) {
                    auto count = io_context->poll();
                    if (count > 0) {
                        Log::Trace("AsioContext: wasm: polled {} tasks", count);
                    }
                } else {
                    Log::Debug("AsioContext: wasm: stopped, cancelling main loop");
                    emscripten_cancel_main_loop();
                    emscripten_async_call(
                        [](void*) {
                            Log::Trace("AsioContext: wasm: emscripten_force_exit(0)");
                            emscripten_force_exit(0);
                        },
                        nullptr,
                        0
                    );
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
