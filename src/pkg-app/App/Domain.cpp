#include "Domain.h"
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
    Domain::Domain(const int argc, const char** argv)
        : Domain{Boot::CliArgs(argc, argv)}
    {}

    Domain::Domain(Boot::CliArgs cliArgs)
        : _cliArgs{std::move(cliArgs)}
    {
        Boot::LogHeader(_cliArgs);
        Boot::SetupAbortHandlers();

        Log::Trace("initialize");
        // TODO: selector for executors strategy, i.e. support system_executor (thread pool)
        // auto executor = asio::system_executor();
        // auto& io_context = get_io_context();
    }

    Domain::~Domain()
    {
        Log::Trace("destroy");
    }

    void Domain::RunContext()
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
                Log::Debug("stopped on frame={}", ctx.FrameIndex);
                return false;
            }
            if (const auto count = _io_context.poll(); count > 0) {
                Log::Trace("polled {} tasks on frame={}", count, ctx.FrameIndex);
            }
            return true;
        });
    }

    int Domain::RunCoroMain(boost::asio::awaitable<int> coroMain)
    {
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

        RunContext();
        return _exitCode;
    }
}
