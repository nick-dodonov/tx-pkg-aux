#include "Exec/Delay/ThreadTimerBackend.h"
#include "Exec/Domain.h"
#include "Log/Exception.h"
#include "Log/Log.h"
#include "RunLoop/Runner.h"

namespace Exec
{
    std::unique_ptr<ITimerBackend> Domain::MakeDefaultBackend()
    {
        return std::make_unique<ThreadTimerBackend>();
    }

    Domain::~Domain()
    {
        _logger.Trace("destroy");
    }

    bool Domain::Start()
    {
        _logger.Trace("starting operation");
        _opState->start();
        return true;
    }

    void Domain::Stop()
    {
        _logger.Trace("stopping operation");
        _stopSource.request_stop();

        // Fire any timers that were pending at shutdown so their shared states
        // observe stop_requested() correctly, then drain the frame queue so that
        // queued operations call set_stopped() before the op state is destroyed.
        if (const auto count = _scheduler.DrainQueue(); count > 0) {
            //TODO: enable verbose logging later and make it configurable (too noisy for now), make summary instead
            _logger.Trace("drained {} task(s)", count);
        }
        _opState.reset();
    }

    void Domain::Update(const RunLoop::UpdateCtx& ctx)
    {
        if (const auto count = _scheduler.DrainQueue(); count > 0) {
            //TODO: enable verbose logging later and make it configurable (too noisy for now), make summary instead
            //Log::Trace("drained {} task(s) on frame={}", count, ctx.frame.index);
        }
    }

    void Domain::Completed(int exitCode)
    {
        _logger.Trace("{}", exitCode);
        GetRunner()->Exit(exitCode);
    }

    void Domain::Failed(std::exception_ptr ex)
    {
        const auto msg = Log::ExceptionMessage(ex);
        if (!msg.empty()) {
            _logger.Error("unhandled exception: {}", msg);
        } else {
            _logger.Error("sender completed with error");
        }
        GetRunner()->Exit(RunLoop::ExitCode::Failure);
    }

    void DomainReceiver::set_error(std::exception_ptr&& ex) noexcept
    {
        domain->Failed(std::move(ex));
    }

    void Domain::Stopped()
    {
        _logger.Trace("");

        // distinguish between cooperative stop (exit code from coroutine) and forced cancellation
        if (auto* runner = GetRunner(); !runner->Exiting()) {
            runner->Exit(RunLoop::ExitCode::Cancelled);
        }
    }
}
