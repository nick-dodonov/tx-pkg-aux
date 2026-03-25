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
        Log::Trace("create");

        // Register this instance as the Service for our io_context.
        boost::asio::make_service<Service>( // static_cast otherwise need to use obsolete execution_context::id for service registration
            // ReSharper disable once CppRedundantCastExpression - cast must be specified in the current asio version (1.90)
            static_cast<boost::asio::execution_context&>(_io_context), this);
    }

    AsioDomain::~AsioDomain()
    {
        Log::Trace("destroy");
    }

    bool AsioDomain::Start()
    {
        Log::Debug(".");
        boost::asio::co_spawn(
            _io_context,
            std::move(_coroMain),
            boost::asio::bind_cancellation_slot(
                _cancelSignal.slot(),
                [this](const std::exception_ptr& ex, const int exitCode) {
                    Completed(ex, exitCode);
                }));
        return true;
    }

    void AsioDomain::Completed(const std::exception_ptr& ex, const int exitCode)
    {
        if (ex) {
            // Unhandled coroutine exception — not expected in -fno-exceptions builds.
            Log::Fatal("coroMain terminated with unhandled exception");
            std::terminate();
        }

        // If Stop() was called before the coroutine finished, use Cancelled exit code
        // so the runner can distinguish a normal exit from a forced stop.
        const auto code = _cancelled ? RunLoop::ExitCode::Cancelled : exitCode;
        Log::Trace("finished: exitCode={} cancelled={}", code, _cancelled);

        if (auto* runner = GetRunner(); !runner->Exiting()) {
            runner->Exit(code);
        }
    }

    void AsioDomain::Stop()
    {
        const auto hasHandlers = _cancelSignal.slot().has_handler();
        Log::Trace("{}", hasHandlers ? "w/ cancel handlers" : "no cancel handler");

        // Record that this shutdown is a forced stop so the completion handler
        // can use Cancelled exit code when the coroutine eventually finishes.
        _cancelled = true;
        // Signal cancellation into the running coroutine.
        // This causes any co_await point inside coroMain (timer, channel, etc.)
        // to receive operation_aborted, which unwinds the coroutine and fires the
        // co_spawn completion handler — where Cancelled exit code is set if no exit
        // code has been established yet.
        _cancelSignal.emit(boost::asio::cancellation_type::all);

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

    [[nodiscard]] boost::asio::awaitable<boost::system::error_code> AsyncCancelled()
    {
        const auto executor = co_await boost::asio::this_coro::executor;
        auto timer = boost::asio::steady_timer(executor, boost::asio::steady_timer::time_point::max());
        auto [ec] = co_await timer.async_wait(boost::asio::as_tuple(boost::asio::use_awaitable));
        co_return ec;
    }
}
