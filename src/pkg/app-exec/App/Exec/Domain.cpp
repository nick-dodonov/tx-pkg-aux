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

        //TODO: RESEARCH/TEST: do we need to drain the queue here to ensure that any pending tasks that observe the stop signal are given a chance to run before we destroy the op state?

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
