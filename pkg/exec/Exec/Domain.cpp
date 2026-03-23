#include "Domain.h"
#include "RunLoop/Runner.h"
#include "Exec/Delay/ThreadTimerBackend.h"
#include "Log/Log.h"

namespace Exec
{
    std::unique_ptr<::Exec::ITimerBackend> Domain::MakeDefaultBackend()
    {
        return std::make_unique<::Exec::ThreadTimerBackend>();
    }

    Domain::~Domain()
    {
        Log::Trace("destroy");
    }

    bool Domain::Start()
    {
        Log::Trace("running");
        _opState->start();
        return true;
    }

    void Domain::Stop()
    {
        Log::Trace("stopping");
        _stopSource.request_stop();
        // Fire any timers that were pending at shutdown so their shared states
        // observe stop_requested() correctly, then drain the frame queue so that
        // queued operations call set_stopped() before the op state is destroyed.
        _scheduler.DrainQueue();
        _opState.reset();
    }

    void Domain::Update(const RunLoop::UpdateCtx& ctx)
    {
        const auto count = _scheduler.DrainQueue();
        if (count > 0) {
            Log::Trace("drained {} tasks on frame={}", count, ctx.frame.index);
        }
    }

    void Domain::OnComplete(int exitCode)
    {
        Log::Trace("completed: {}", exitCode);
        GetRunner()->Exit(exitCode);
    }

    void Domain::OnStopped()
    {
        Log::Trace("stopped");
        GetRunner()->Exit(0);
    }
}
