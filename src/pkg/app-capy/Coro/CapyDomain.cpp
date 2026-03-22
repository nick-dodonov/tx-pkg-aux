#include "CapyDomain.h"
#include "RunLoop/Runner.h"
#include "Log/Log.h"

#include <boost/capy/ex/run_async.hpp>

namespace Coro
{
    CapyDomain::CapyDomain(Coro::Task<int> mainTask)
        : _mainTask(std::move(mainTask))
    {
        Log::Trace("created");
    }

    CapyDomain::~CapyDomain()
    {
        Log::Trace("destroy");
    }

    bool CapyDomain::Start()
    {
        Log::Trace("running");
        // auto executor = _pool.get_executor();
        auto executor = _context.get_executor();
        boost::capy::run_async(executor,
            [this](auto&& exitCode) {
                Log::Trace("completed: {}", exitCode);
                GetRunner()->Exit(exitCode);
            },
            [](std::exception_ptr ep) {
                Log::Fatal("completed w/ unhandled exception");
                std::rethrow_exception(ep);
            }
        )(std::move(_mainTask));
        return true;
    }

    void CapyDomain::Stop()
    {
        Log::Trace("stopping");
        // _pool.stop();
        _context.stop();
    }

    void CapyDomain::Update(const RunLoop::UpdateCtx& ctx)
    {
        // Log::Trace("frame={} delta={} µs", ctx.frame.index, ctx.frame.delta.count());
        if (const auto count = _context.poll(); count > 0) {
            // TODO: enable verbose logging later and make it configurable (too noisy for now), make summary instead
            Log::Trace("polled {} tasks on frame={}", count, ctx.frame.index);
        }
    }
}
