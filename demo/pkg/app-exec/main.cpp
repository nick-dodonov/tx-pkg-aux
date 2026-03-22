#include "App/Exec/Domain.h"
#include "App/Loop/Factory.h"
#include "Boot/Boot.h"
#include "Log/Log.h"

#include <exec/task.hpp>
#include <exec/timed_scheduler.hpp>
#include <chrono>

namespace {

/// Demonstrates timed delay via exec::schedule_after inside an exec::task coroutine.
///
/// The scheduler is received as a parameter (concrete type from the Domain) rather
/// than via read_env(get_scheduler) — exec::task type-erases the ambient scheduler
/// to any_scheduler<> which does not satisfy exec::timed_scheduler.
exec::task<int> MainTask(auto sched)
{
    Log::Info("[delay-demo] starting");
    Log::Info("[delay-demo] waiting 10 ms...");
    co_await exec::schedule_after(sched, std::chrono::milliseconds(10));
    Log::Info("[delay-demo] delay elapsed, returning 42");
    co_return 42;
}

} // namespace

int main(const int argc, const char* argv[])
{
    Boot::DefaultInit(argc, argv);

    // Factory form: receives the concrete TimedScheduler::Scheduler and passes it
    // to the coroutine — avoids the any_scheduler<> type-erasure inside exec::task.
    auto domain = std::make_shared<App::Exec::Domain>([](auto sched) {
        return MainTask(sched);
    });

    auto runner = App::Loop::CreateDefaultRunner(domain);
    return runner->Run();
}
