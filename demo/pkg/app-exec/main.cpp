#include "App/Exec/Domain.h"
#include "App/Loop/Factory.h"
#include "Boot/Boot.h"
#include "Log/Log.h"
#include <exec/task.hpp>

/// Coroutine sender — returned directly to Launch(), scheduler is injected automatically.
static auto MainTask() -> exec::task<int>
{
    Log::Info("[main] Hello from MainTask coroutine!");
    co_return 42;
}

int main(const int argc, const char* argv[])
{
    Boot::DefaultInit(argc, argv);

    stdexec::run_loop loop;
    auto sender = loop.get_scheduler().schedule()
        | stdexec::then([] { Log::Info("[run_loop] Hello from the run_loop!"); })
        | stdexec::then([] { return 42; })
        | stdexec::then([&loop](int exitCode) {
            Log::Info("[run_loop] completed with result: {}", exitCode);
            loop.finish();
        });
    stdexec::start_detached(sender);
    loop.run();

    auto domain = std::make_shared<App::Exec::Domain>();

    // Option A: coroutine (or any ready sender) — scheduler injected via starts_on
    domain->Launch(MainTask());

    // Option B: factory form — use when the pipeline depends on the scheduler internally
    // domain->Launch([](auto sched) {
    //     return stdexec::schedule(sched)
    //         | stdexec::then([] { Log::Info("[main] step 1"); })
    //         | stdexec::continues_on(sched)
    //         | stdexec::then([] { return 42; });
    // });

    auto runner = App::Loop::CreateDefaultRunner(domain);
    return runner->Run();
}
