#include "Coro/Domain.h"
#include "App/Loop/Runner.h"
#include "Log/Log.h"

#include <boost/capy/ex/run_async.hpp>

//XXXXXXXXXXXXXXXXXXXXX TMP
#include <boost/capy/ex/thread_pool.hpp>

namespace Coro
{
    //XXXXXXXXXXXXXXXXXXXXX TMP //TODO: put to Domain and replace w/ loop impl
    static boost::capy::thread_pool _pool(1);

    Domain::Domain(Coro::Task<int> mainTask)
        : _mainTask(std::move(mainTask))
    {
        Log::Trace("created");
    }

    Domain::~Domain()
    {
        Log::Trace("destroy");
    }

    bool Domain::Start()
    {
        Log::Trace("running");
        boost::capy::run_async(_pool.get_executor(), 
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

    void Domain::Stop()
    {
        Log::Trace("stopping");
        _pool.stop();
        _pool.join();
    }

    void Domain::Update(const App::Loop::UpdateCtx& ctx)
    {
    }
}
