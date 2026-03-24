#include "AsioDomain.h"
#include "Log/Log.h"
#include "RunLoop/Runner.h"

namespace Asio
{
    // TODO: selector for executors strategy, i.e. support system_executor (thread pool)
    //  auto executor = asio::system_executor();
    //  auto& io_context = get_io_context();
    AsioDomain::AsioDomain(boost::asio::awaitable<int> coroMain)
        : _coroMain(std::move(coroMain))
    {
        Log::Trace("initialize");

        // Register this instance as the Service for our io_context.
        boost::asio::make_service<Service>( // static_cast otherwise need to use obsolete execution_context::id for service registration
            static_cast<boost::asio::execution_context&>(_io_context), this);
    }

    AsioDomain::~AsioDomain()
    {
        Log::Trace("destroy");
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
        boost::asio::co_spawn(
            _io_context,
            std::move(_coroMain),
            [this](const std::exception_ptr& ex, const int exitCode) {
                if (!ex) {
                    Log::Trace("finished: {}", exitCode);
                    GetRunner()->Exit(exitCode);
                } else {
                    Log::Fatal("finished w/ unhandled exception");
                    std::rethrow_exception(ex);
                }
            });
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
