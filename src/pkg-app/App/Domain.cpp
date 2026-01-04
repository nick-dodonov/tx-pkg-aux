#include "Domain.h"
#include "Log/Log.h"
#include "Boot/Boot.h"
#include <memory>

#if __EMSCRIPTEN__
    #include "Loop/WasmRunner.h"
#else
    #include "Loop/TightRunner.h"
#endif

namespace App
{
    namespace
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

    Domain::Domain(const int argc, const char** argv)
        : Domain{Boot::CliArgs(argc, argv)}
    {}

    Domain::Domain(const int argc, const char** argv, std::shared_ptr<Loop::IRunner> runner)
        : Domain{Boot::CliArgs(argc, argv), std::move(runner)}
    {}

    Domain::Domain(Boot::CliArgs cliArgs)
        : Domain{std::move(cliArgs), CreateDefaultRunner()}
    {}

    Domain::Domain(Boot::CliArgs cliArgs, std::shared_ptr<Loop::IRunner> runner)
        : _cliArgs{std::move(cliArgs)}
        , _runner{std::move(runner)}
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
                    _runner->Finish(Loop::FinishData{exitCode});
                } else {
                    Log::Fatal("finished w/ unhandled exception");
                    std::rethrow_exception(ex);
                }
            });

        _runner->Start(shared_from_this());
        return _exitCode;
    }

    bool Domain::Started(Loop::IRunner& runner)
    {
        Log::Debug("runner started");
        return true;
    }

    void Domain::Stopping(Loop::IRunner& runner)
    {
        Log::Debug("runner stopping");
    }

    bool Domain::Update(Loop::IRunner& runner, const Loop::UpdateCtx& ctx)
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
}