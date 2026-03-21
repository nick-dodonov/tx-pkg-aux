#include "App/Exec/Domain.h"
#include "App/Loop/Runner.h"
#include "Log/Log.h"

namespace App::Exec
{
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
        // Drain any tasks already enqueued so they observe stop_requested() and call
        // set_stopped() before we destroy the operation state. Without this, queued
        // Operation<R> nodes would become dangling pointers inside the lock-free queue.
        _scheduler.DrainQueue();
        _opState.reset();
    }

    void Domain::Update(const App::Loop::UpdateCtx& ctx)
    {
        if (const auto count = _scheduler.DrainQueue(); count > 0) {
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

} // namespace App::Exec
