#include "Exec/Delay/ThreadTimerBackend.h"
#include "Exec/Domain.h"
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
        Log::Trace("destroy");
    }

    bool Domain::Start()
    {
        Log::Trace("starting operation");
        _opState->start();
        return true;
    }

    void Domain::Stop()
    {
        Log::Trace("stopping operation");
        _stopSource.request_stop();

        // Fire any timers that were pending at shutdown so their shared states
        // observe stop_requested() correctly, then drain the frame queue so that
        // queued operations call set_stopped() before the op state is destroyed.
        if (const auto count = _scheduler.DrainQueue(); count > 0) {
            Log::Trace("drained {} tasks", count);
        }
        _opState.reset();
    }

    void Domain::Update(const RunLoop::UpdateCtx& ctx)
    {
        if (const auto count = _scheduler.DrainQueue(); count > 0) {
            Log::Trace("drained {} tasks on frame={}", count, ctx.frame.index);
        }
    }

    void Domain::Completed(int exitCode)
    {
        Log::Trace("{}", exitCode);
        GetRunner()->Exit(exitCode);
    }

    void Domain::Stopped()
    {
        Log::Trace("");

        // distinguish between cooperative stop (exit code from coroutine) and forced cancellation
        if (auto* runner = GetRunner(); !runner->Exiting()) {
            runner->Exit(RunLoop::ExitCode::Cancelled);
        }
    }
}
