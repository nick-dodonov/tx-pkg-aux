#include "App/Exec/Domain.h"
#include "App/Loop/Factory.h"
#include "TestRunner.h"

#include <gtest/gtest.h>
#include <stdexec/execution.hpp>
#include <exec/task.hpp>

namespace {

/// Coroutine that performs one extra queue hop via co_await schedule().
/// Frame 1: coroutine starts, suspends at co_await schedule(sched).
/// Frame 2: resumes, co_return 55.
exec::task<int> TwoHopTask()
{
    // read_env is synchronous; schedule(sched) enqueues one drain-cycle hop.
    const auto sched = co_await stdexec::read_env(stdexec::get_scheduler);
    co_await stdexec::schedule(sched);
    co_return 55;
}

} // namespace

// Domain(sender) wraps with starts_on internally and completes in one drain cycle.
TEST(DomainTest, PlainSenderCompletion)
{
    auto domain = std::make_shared<App::Exec::Domain>(stdexec::just(42));
    const auto runner = App::Loop::CreateTestRunner(domain);
    EXPECT_EQ(runner->Run(), 42);
}

// Domain(factory) stores the factory's sender directly — no extra starts_on wrapping.
TEST(DomainTest, FactoryConstructor)
{
    auto domain = std::make_shared<App::Exec::Domain>([](auto sched) {
        return stdexec::schedule(sched) | stdexec::then([] { return 7; });
    });
    const auto runner = App::Loop::CreateTestRunner(domain);
    EXPECT_EQ(runner->Run(), 7);
}

// Stop() called before Update(): the pending task must observe stop_requested()
// and call set_stopped() via DrainQueue(), not be destroyed while still queued
// (which would leave a dangling Operation<R>* inside the lock-free queue).
TEST(DomainTest, StopBeforeDrainCancelsTask)
{
    auto domain = std::make_shared<App::Exec::Domain>(stdexec::just(99));
    TestRunner runner{domain};

    domain->Start();  // enqueues the starts_on bridge task
    domain->Stop();   // request_stop → DrainQueue → set_stopped → Exit(0)

    EXPECT_EQ(runner.exitCode, 0);
}

// An exec::task coroutine that reads get_scheduler from the environment and
// re-schedules via it. Verifies that DomainReceiver::get_env() correctly
// propagates get_scheduler — without it, read_env(get_scheduler) returns nothing
// and co_await schedule(sched) would fail at compile time or produce wrong results.
TEST(DomainTest, CoroutineSenderWithSchedulerAccess)
{
    auto domain = std::make_shared<App::Exec::Domain>(TwoHopTask());
    const auto runner = App::Loop::CreateTestRunner(domain);
    // DrainQueue() is greedy — it exhausts newly-enqueued tasks within the same
    // call, so all hops (starts_on bridge → exec::task start-on-scheduler →
    // co_await schedule resume) complete in a single Update() frame.
    EXPECT_EQ(runner->Run(), 55);
}
