#include "App/Exec/Domain.h"
#include "App/Factory.h"
#include "Boot/Boot.h"
#include "Log/Log.h"

#include <chrono>

/// Demonstrates timed delay via exec::schedule_after inside a RunTask coroutine.
///
/// The scheduler is obtained from the receiver environment via
/// stdexec::read_env(stdexec::get_scheduler). RunTask<T> stores the concrete
/// Exec::RunContext::Scheduler without type-erasure, so exec::schedule_after
/// works without a scheduler parameter.
static Exec::RunTask<int> MainTask()
{
    const auto sched = co_await stdexec::read_env(stdexec::get_scheduler);
    Log::Info("[delay-demo] starting");
    Log::Info("[delay-demo] waiting 10 ms...");
    auto delay = std::chrono::milliseconds(10);
    co_await exec::schedule_after(sched, delay);
    Log::Info("[delay-demo] delay elapsed, returning 42");
    co_return 42;
}

int main(const int argc, const char* argv[])
{
    Boot::DefaultInit(argc, argv);

    // Direct sender form: RunTask<int> is sent as a sender to Domain.
    // The scheduler is available inside the coroutine body via read_env,
    // without exposing it as a parameter.
    auto domain = std::make_shared<App::Exec::Domain>(MainTask());

    auto runner = App::CreateDefaultRunner(domain);
    return runner->Run();
}
