#include "Log/Log.h"

#include <stdexec/execution.hpp>
#include <exec/start_detached.hpp>

auto main() -> int
{
    stdexec::run_loop loop;
    auto sender = loop.get_scheduler().schedule()
        | stdexec::then([] { Log::Info("[run_loop] Hello from the run_loop!"); })
        | stdexec::then([] { return 42; })
        | stdexec::then([&loop](int exitCode) {
            Log::Info("[run_loop] completed with result: {}", exitCode);
            loop.finish();
        });
    exec::start_detached(sender);

    // drain the queue, taking care to execute any tasks that get added while
    // executing the remaining tasks (also wait for other tasks that might still be in flight):
    // while (loop.__execute_all() || loop.__task_count_.load(std::memory_order_acquire) > 0) { ; }
    loop.run();
    return 1;
}
