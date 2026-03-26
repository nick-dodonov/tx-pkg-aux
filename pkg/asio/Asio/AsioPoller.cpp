#include "AsioPoller.h"
#include "Log/Log.h"

namespace Asio
{
    AsioPoller::AsioPoller()
        : _workGuard(boost::asio::make_work_guard(_io))
    {
        Log::Trace("create");
    }

    AsioPoller::~AsioPoller()
    {
        Log::Trace("destroy");
    }

    void AsioPoller::Update(const RunLoop::UpdateCtx& ctx)
    {
        // Impossible: if the io_context is stopped, it means the work guard was destroyed
        // if (_io.stopped()) {
        //     Log::Debug("stopped on frame={}", ctx.frame.index);
        //     return;
        // }
        if (const auto count = _io.poll(); count > 0) {
            //TODO: enable verbose logging later and make it configurable (too noisy for now), make summary instead
            Log::Trace("polled {} handler(s) on frame={}", count, ctx.frame.index);
        }
    }
}
