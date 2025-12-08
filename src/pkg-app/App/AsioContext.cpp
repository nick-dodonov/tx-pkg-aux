#include "AsioContext.h"
#include "Log/Log.h"
#include "Boot/Boot.h"

#if __EMSCRIPTEN__
    #include "Loop/WasmLooper.h"
    #include <emscripten.h>
#else
    #include "Loop/TightLooper.h"
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
        // _io_context.run();
        // asio::detail::global<asio::system_context>().join();

#if __EMSCRIPTEN__
        auto looper = Loop::WasmLooper{{.Fps = 120}};
#else
        auto looper = Loop::TightLooper{};
#endif
        looper.Start([&](const Loop::UpdateCtx& ctx) -> bool {
            //Log::Trace("frame={} delta={} µs", ctx.FrameIndex, ctx.FrameDelta.count());
            if (_io_context.stopped()) {
                Log::Debug("context stopped on frame={}", ctx.FrameIndex);
                return false;
            }
            if (const auto count = _io_context.poll(); count > 0) {
                Log::Trace("context polled {} tasks on frame={}", count, ctx.FrameIndex);
            }
            return true;
        });
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
                    // In wasm we need to explicitly exit the process because emscripten_set_main_loop_arg() never returns
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
