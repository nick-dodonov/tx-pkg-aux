#include "Domain.h"
#include "Log/Log.h"
#include "Boot/Boot.h"
#include <memory>

#if __EMSCRIPTEN__
    #include "Loop/WasmLooper.h"
#else
    #include "Loop/TightLooper.h"
#endif

namespace App
{
    namespace
    {
        std::shared_ptr<Loop::ILooper> CreateDefaultLooper()
        {
#if __EMSCRIPTEN__
            return std::make_shared<Loop::WasmLooper>(Loop::WasmLooper{{.Fps = 120}});
#else
            return std::make_shared<Loop::TightLooper>();
#endif
        }
    }

    Domain::Domain(const int argc, const char** argv)
        : Domain{Boot::CliArgs(argc, argv)}
    {}

    Domain::Domain(const int argc, const char** argv, std::shared_ptr<Loop::ILooper> looper)
        : Domain{Boot::CliArgs(argc, argv), std::move(looper)}
    {}

    Domain::Domain(Boot::CliArgs cliArgs)
        : Domain{std::move(cliArgs), CreateDefaultLooper()}
    {}

    Domain::Domain(Boot::CliArgs cliArgs, std::shared_ptr<Loop::ILooper> looper)
        : _cliArgs{std::move(cliArgs)}
        , _looper{std::move(looper)}
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
                    _looper->Finish(Loop::FinishData{exitCode});
                } else {
                    Log::Fatal("finished w/ unhandled exception");
                    std::rethrow_exception(ex);
                }
            });

        _looper->Start(shared_from_this());
        return _exitCode;
    }

    bool Domain::Started(Loop::ILooper& looper)
    {
        Log::Debug("looper started");
        return true;
    }

    bool Domain::Update(Loop::ILooper& looper, const Loop::UpdateCtx& ctx)
    {
        //Log::Trace("frame={} delta={} µs", ctx.frame.index, ctx.frame.delta.count());
        if (_io_context.stopped()) {
            Log::Debug("stopped on frame={}", ctx.frame.index);
            return false;
        }
        if (const auto count = _io_context.poll(); count > 0) {
            Log::Trace("polled {} tasks on frame={}", count, ctx.frame.index);
        }
        return true;
    }

    void Domain::Stopping(Loop::ILooper& looper)
    {
        Log::Debug("looper stopping");
    }
}