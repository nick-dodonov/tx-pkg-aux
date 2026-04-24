#include "Exec/Domain.h"
#include "App/Factory.h"
#include "Exec/Delay/LoopTimerBackend.h"
#include "TestRunner.h"

#include <gtest/gtest.h>
#include <stdexec/execution.hpp>
#include <exec/task.hpp>
#include <exec/timed_scheduler.hpp>

namespace {

/// Returns a domain using LoopTimerBackend (no background thread, fully deterministic).
/// The factory lambda receives the concrete TimedLoopContext::Scheduler — necessary
/// because exec::task type-erases the ambient scheduler to any_scheduler<>, which
/// does not satisfy exec::timed_scheduler and therefore cannot be passed to
/// exec::schedule_after / schedule_at.
template <class F>
auto MakeDomainWithLoop(F factory)
{
    return std::make_shared<Exec::Domain>(
        std::move(factory), Exec::Domain::Options{.backend = std::make_unique<Exec::LoopTimerBackend>()});
}

} // namespace

// Zero-duration schedule_after completes in one Update() frame.
// LoopTimerBackend::Tick() checks steady_clock::now(); nanoseconds(0) deadline
// is already in the past by the time the first Update() runs.
TEST(DelayTest, ZeroDurationDelayCompletesInOneFrame)
{
    auto domain = MakeDomainWithLoop([](auto sched) -> exec::task<int> {
        co_await exec::schedule_after(sched, std::chrono::nanoseconds(0));
        co_return 10;
    });
    EXPECT_EQ(App::CreateTestRunner(domain)->Run(), 10);
}

// schedule_at() with a time_point one second in the past fires immediately on Tick().
// Use exec::now(sched) — not std::chrono::steady_clock::now() — so the time point
// is relative to the same clock source that LoopTimerBackend::Tick() uses.
// On some Android emulator configurations the two can differ (e.g. CLOCK_BOOTTIME
// vs CLOCK_MONOTONIC split across translation units / linked libraries).
TEST(DelayTest, ScheduleAtPastTimePointFiresImmediately)
{
    auto domain = MakeDomainWithLoop([](auto sched) -> exec::task<int> {
        const auto past = exec::now(sched) - std::chrono::seconds(1);
        co_await exec::schedule_at(sched, past);
        co_return 77;
    });
    EXPECT_EQ(App::CreateTestRunner(domain)->Run(), 77);
}

// Stop() before the delay fires: the stop callback wins the claimed CAS →
// set_stopped propagates through exec::task → OnStopped() → Exit(0).
TEST(DelayTest, StopBeforeDelayFiresCancelsCorrectly)
{
    auto domain = MakeDomainWithLoop([](auto sched) -> exec::task<int> {
        co_await exec::schedule_after(sched, std::chrono::hours(24));
        co_return 99; // unreachable if stop fires
    });
    TestRunner runner{domain};

    // Start(): exec::task begins, runs to co_await schedule_after, registers timer
    // (24h deadline with LoopTimerBackend) and stop callback, then suspends.
    domain->Start();

    // Stop(): request_stop() → stop callback fires synchronously (CAS wins) →
    // enqueues shared state. TickBackend() is a no-op (24h not expired).
    // DrainQueue() → shared state executes → set_stopped → OnStopped → Exit(0).
    domain->Stop();

    EXPECT_EQ(runner.exitCode, RunLoop::ExitCode::Cancelled);
}

// exec::now(sched) returns a steady_clock time_point within [before, after].
// This is a correctness + compile-time check that TimedLoopContext::Scheduler
// satisfies exec::__timed_scheduler (requires a .now() member returning a time_point).
TEST(DelayTest, TimedLoopContextNowReturnsSteadyClockTime)
{
    auto domain = std::make_shared<Exec::Domain>(
        stdexec::just(0), Exec::Domain::Options{.backend = std::make_unique<Exec::LoopTimerBackend>()});

    const auto before = std::chrono::steady_clock::now();
    const auto t      = exec::now(domain->GetScheduler());
    const auto after  = std::chrono::steady_clock::now();

    EXPECT_GE(t, before);
    EXPECT_LE(t, after);
}
