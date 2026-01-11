#include "Domain.h"
#include "Log/Log.h"
#include "Boot/Boot.h"
#include "Loop/Runner.h"

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

    int Domain::RunCoroMain(const std::shared_ptr<Loop::IRunner>& runner, boost::asio::awaitable<int> coroMain)
    {
        Log::Trace("starting");
        boost::asio::co_spawn(
            _io_context,
            std::move(coroMain),
            [runner](const std::exception_ptr& ex, const int exitCode) {
                if (!ex) {
                    Log::Trace("finished: {}", exitCode);
                    runner->Exit(exitCode);
                } else {
                    Log::Fatal("finished w/ unhandled exception");
                    std::rethrow_exception(ex);
                }
            });

        return runner->Run();
    }

    bool Domain::Start()
    {
        Log::Debug("runner started");
        return true;
    }

    void Domain::Stop()
    {
        Log::Debug("runner stopping");
    }

    void Domain::Update(const Loop::UpdateCtx& ctx)
    {
        //Log::Trace("frame={} delta={} µs", ctx.frame.index, ctx.frame.delta.count());
        if (_io_context.stopped()) {
            Log::Debug("stopped on frame={}", ctx.frame.index);
            return;
        }
        if (const auto count = _io_context.poll(); count > 0) {
            Log::Trace("polled {} tasks on frame={}", count, ctx.frame.index);
        }
    }
}
