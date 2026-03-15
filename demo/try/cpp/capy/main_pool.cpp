#include "Boot/Boot.h"
#include "Log/Log.h"

#include <boost/capy/task.hpp>
#include <boost/capy/ex/run_async.hpp>
#include <boost/capy/ex/thread_pool.hpp>

namespace capy = boost::capy;

// A coroutine that returns a value
capy::task<int> answer()
{
    Log::Info("enter");
    co_return 42;
}

// A coroutine that awaits another coroutine
capy::task<void> greet()
{
    Log::Info("enter");
    auto n = co_await answer();
    Log::Info("exit: {}", n);
}

int main(const int argc, const char** argv)
{
    Boot::DefaultInit(argc, argv);

    Log::Info("thread_pool");
    capy::thread_pool pool(1);

    // Launch the coroutine on the pool's executor
    Log::Info("run_async");
    auto executor = pool.get_executor();
    capy::run_async(executor)(greet());

    // Pool destructor waits for all work to complete
    Log::Info("join");
    pool.join();
    Log::Info("exiting");
    return 0;
}
