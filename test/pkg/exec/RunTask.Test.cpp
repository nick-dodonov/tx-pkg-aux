#include "Exec/Domain.h"
#include "App/Factory.h"
#include "Exec/Delay/LoopTimerBackend.h"
#include "Exec/RunContext.h"
#include "Exec/RunTask.h"
#include "TestRunner.h"

#include <gtest/gtest.h>
#include <stdexec/execution.hpp>
#include <exec/timed_scheduler.hpp>
#include <chrono>

namespace {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// Build a Domain with LoopTimerBackend (deterministic, no background thread).
auto MakeDomain(Exec::RunTask<int> task)
{
    return std::make_shared<Exec::Domain>(
        std::move(task), Exec::DomainOptions{.backend = std::make_unique<Exec::LoopTimerBackend>()});
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

// RunTask<int> that simply co_returns a value. No scheduling involved.
// Verifies the basic send-and-complete path in Domain.
TEST(RunTaskTest, ReturnsValue)
{
    auto domain = MakeDomain([]() -> Exec::RunTask<int> { co_return 42; }());
    EXPECT_EQ(App::CreateTestRunner(domain)->Run(), 42);
}

// co_await stdexec::read_env(stdexec::get_scheduler) inside a RunTask must
// return the concrete Exec::RunContext::Scheduler — verified via static_assert
// inside the coroutine body (compile-time check) and the runtime return value.
TEST(RunTaskTest, SchedulerAvailableViaEnv)
{
    auto domain = MakeDomain([]() -> Exec::RunTask<int> {
        const auto sched = co_await stdexec::read_env(stdexec::get_scheduler);
        static_assert(std::same_as<std::remove_cvref_t<decltype(sched)>,
                                   Exec::RunContext::Scheduler>,
            "RunTask must expose the concrete RunContext::Scheduler via get_scheduler");
        co_return 7;
    }());
    EXPECT_EQ(App::CreateTestRunner(domain)->Run(), 7);
}

// Zero-duration schedule_after obtained via read_env completes in one frame.
// Confirms exec::schedule_after works with the env-sourced scheduler.
TEST(RunTaskTest, ScheduleAfterViaEnv)
{
    auto domain = MakeDomain([]() -> Exec::RunTask<int> {
        const auto sched = co_await stdexec::read_env(stdexec::get_scheduler);
        co_await exec::schedule_after(sched, std::chrono::nanoseconds(0));
        co_return 10;
    }());
    EXPECT_EQ(App::CreateTestRunner(domain)->Run(), 10);
}

// schedule_at with a past time_point fires immediately. Uses exec::now(sched)
// obtained from the environment — same clock source as LoopTimerBackend::Tick().
TEST(RunTaskTest, ScheduleAtPastTimePointViaEnv)
{
    auto domain = MakeDomain([]() -> Exec::RunTask<int> {
        const auto sched = co_await stdexec::read_env(stdexec::get_scheduler);
        const auto past  = exec::now(sched) - std::chrono::seconds(1);
        co_await exec::schedule_at(sched, past);
        co_return 77;
    }());
    EXPECT_EQ(App::CreateTestRunner(domain)->Run(), 77);
}

// Stop() before the delay fires: set_stopped propagates via exec::task → OnStopped.
TEST(RunTaskTest, StopCancellationBeforeDelay)
{
    auto domain = MakeDomain([]() -> Exec::RunTask<int> {
        const auto sched = co_await stdexec::read_env(stdexec::get_scheduler);
        co_await exec::schedule_after(sched, std::chrono::hours(24));
        co_return 99; // never reached
    }());
    TestRunner runner{domain};
    domain->Start();
    domain->Stop();
    EXPECT_EQ(runner.exitCode, RunLoop::ExitCode::Cancelled);
}

// Nested RunTask: outer RunTask co_awaits inner RunTask. Both retrieve the
// scheduler via read_env — inner inherits it from outer's RunTaskCtx.
namespace {

Exec::RunTask<int> InnerTask()
{
    const auto sched = co_await stdexec::read_env(stdexec::get_scheduler);
    // Verify concrete type in nested context.
    static_assert(std::same_as<std::remove_cvref_t<decltype(sched)>,
                               Exec::RunContext::Scheduler>);
    co_await exec::schedule_after(sched, std::chrono::nanoseconds(0));
    co_return 55;
}

Exec::RunTask<int> OuterTask()
{
    // The outer task's RunTaskCtx provides the scheduler.
    // InnerTask receives it through the parent promise env when co_awaited.
    co_return co_await InnerTask();
}

} // namespace

TEST(RunTaskTest, NestedRunTasksShareScheduler)
{
    auto domain = MakeDomain(OuterTask());
    EXPECT_EQ(App::CreateTestRunner(domain)->Run(), 55);
}

} // namespace
