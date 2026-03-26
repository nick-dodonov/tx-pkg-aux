#include "Log/Log.h"

#include <stdexec/execution.hpp>
#include <sstream>
#include <exec/task.hpp>
#include <exec/start_detached.hpp>
#include <exec/async_scope.hpp>
#include <exec/start_now.hpp>

static exec::task<void> Sub(int input)
{
    Log::Info("input = {}", input);
    co_return;
}

static exec::task<int> Foo(int input)
{
    Log::Info("input = {}", input);
    co_await Sub(input);
    co_return input * 2;
}

static auto Read(std::string path)
{
    Log::Info("Reading from {}", path);
    std::ostringstream ss;
    ss << "Hello from " << path << std::ends;
    return stdexec::just(ss.str());
}

auto main() -> int
{
    Log::Info("starting run_loop");

    stdexec::run_loop loop;
    auto sender = loop.get_scheduler().schedule()
        | stdexec::then([] { Log::Info("[then] Hello from the run_loop!"); })
        | stdexec::then([] { return 7; })
        | stdexec::let_value([](int value) { return Foo(value); })
        | stdexec::then([&loop](int result) {
            Log::Info("[then] completed with result: {}", result);
            loop.finish();
        });
    exec::start_detached(sender);
    // drain the queue, taking care to execute any tasks that get added while
    // executing the remaining tasks (also wait for other tasks that might still be in flight):
    // while (loop.__execute_all() || loop.__task_count_.load(std::memory_order_acquire) > 0) { ; }
    loop.run();

    Log::Info("starting simple sync_wait task");
    stdexec::sync_wait(Foo(31) | stdexec::then([](int result) { Log::Info("[sync_wait] completed with result: {}", result); }));

    Log::Info("starting simple sender from function");
    // Analog of ContinueWith: pipe then/upon_error onto any sender
    stdexec::sync_wait(
        Read("file.txt")
        | stdexec::then([](std::string content) { Log::Info("[then] read content: {}", content); })
        | stdexec::upon_error([](auto&&) { Log::Info("[upon_error] read failed"); })
    );

    // Non-blocking with async_scope: fire-and-track, collect result later via spawn_future
    Log::Info("starting async_scope demo");
    exec::async_scope scope;
    auto future = scope.spawn_future(Read("data.bin"));
    // ... other work here (non-blocking) ...
    Log::Info("[main] doing other work while Read runs...");
    // collect result when ready:
    stdexec::sync_wait(
        std::move(future)
        | stdexec::then([](std::string content) { Log::Info("[future] got: {}", content); })
        | stdexec::upon_error([](auto&&) { Log::Info("[future] error"); })
    );
    stdexec::sync_wait(scope.on_empty());

    return 1;
}
