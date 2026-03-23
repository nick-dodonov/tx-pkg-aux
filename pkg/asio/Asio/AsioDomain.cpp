#include "AsioDomain.h"
#include "Log/Log.h"
#include "RunLoop/Runner.h"

namespace Asio
{
    AsioDomain::AsioDomain()
    {
        Log::Trace("initialize");
        // TODO: selector for executors strategy, i.e. support system_executor (thread pool)
        // auto executor = asio::system_executor();
        // auto& io_context = get_io_context();
    }

    AsioDomain::~AsioDomain()
    {
        Log::Trace("destroy");
    }

    int AsioDomain::RunCoroMain(const std::shared_ptr<RunLoop::IRunner>& runner, boost::asio::awaitable<int> coroMain)
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

    boost::asio::awaitable<boost::system::error_code> AsioDomain::AsyncStopped()
    {
        Log::Trace("waiting...");
        auto executor = co_await boost::asio::this_coro::executor;

        std::shared_ptr<StopChannel> channel;
        {
            Async::LockGuard lock(_mutex);
            if (!_stopChannel) {
                _stopChannel = std::make_shared<StopChannel>(executor, 1);
            }
            channel = _stopChannel;
        }

        auto [ec] = co_await channel->async_receive(boost::asio::as_tuple(boost::asio::use_awaitable));
        Log::Trace("complete: {}", ec ? ec.what(): "<success>");
        co_return ec;
    }

    bool AsioDomain::Start()
    {
        Log::Debug(".");
        return true;
    }

    void AsioDomain::Stop()
    {
        // Notify waiters if any
        std::shared_ptr<StopChannel> channel;
        {
            Async::LockGuard lock(_mutex);
            _stopChannel.swap(channel);
        }
        if (channel) {
            Log::Trace("notifying waiters");
            channel->try_send(boost::system::error_code{});
        } else {
            Log::Trace("without waiters");
        }

        // Allow io_context to handle remaining tasks
        if (!_io_context.stopped()) {
            //auto count = _io_context.run_for(std::chrono::milliseconds(500));
            auto count = _io_context.poll();
            if (count > 0) {
                Log::Trace("polled {} tasks on stop", count);
            }
        }
    }

    void AsioDomain::Update(const RunLoop::UpdateCtx& ctx)
    {
        //Log::Trace("frame={} delta={} µs", ctx.frame.index, ctx.frame.delta.count());
        if (_io_context.stopped()) {
            Log::Debug("stopped on frame={}", ctx.frame.index);
            return;
        }
        if (const auto count = _io_context.poll(); count > 0) {
            //TODO: enable verbose logging later and make it configurable (too noisy for now), make summary instead
            //Log::Trace("polled {} tasks on frame={}", count, ctx.frame.index);
        }
    }
}
