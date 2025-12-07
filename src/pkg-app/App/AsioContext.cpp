#include "AsioContext.h"
#include "Log/Log.h"
#include "Boot/Boot.h"

#if __EMSCRIPTEN__
    #include <emscripten.h>
#endif

namespace App
{
    AsioContext::AsioContext()
    {
        Log::Trace("initializing");

        // TODO: selector for executors strategy, i.e. support system_executor (thread pool)
        // auto executor = asio::system_executor();
        // auto& io_context = get_io_context();
    }

    AsioContext::~AsioContext()
    {
        Log::Trace("destroyed");
    }

    void AsioContext::Run()
    {
#if __EMSCRIPTEN__
        emscripten_set_main_loop_arg(
            [](void* arg) {
                auto* io_context = static_cast<boost::asio::io_context*>(arg);
                if (!io_context->stopped()) {
                    auto count = io_context->poll();
                    if (count > 0) {
                        Log::Trace("wasm: polled {} tasks", count);
                    }
                } else {
                    Log::Debug("wasm: stopped, cancelling main loop");
                    emscripten_cancel_main_loop();
                    // emscripten_async_call(
                    //     [](void*) {
                    //         Log::Trace("wasm: emscripten_force_exit(0)");
                    //         emscripten_force_exit(0);
                    //     },
                    //     nullptr,
                    //     0
                    // );
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
    }

    int AsioContext::RunCoroMain(const int argc, const char** argv, boost::asio::awaitable<int> coroMain)
    {
        Boot::LogHeader(argc, argv);

        Log::Trace("starting");
        boost::asio::co_spawn(
            _io_context,
            std::move(coroMain),
            [this](const std::exception_ptr& ex, const int exitCode) {
                if (!ex) {
                    Log::Trace("finished: {}", exitCode);
                    _exitCode = exitCode;
#if __EMSCRIPTEN__
                    Log::Trace("wasm: emscripten_force_exit: {}", exitCode);
                    emscripten_force_exit(_exitCode);
#endif
                } else {
                    Log::Fatal("finished w/ unhandled exception");
                    std::rethrow_exception(ex);
                }
            });
        Run();
        return _exitCode;
    }
}
