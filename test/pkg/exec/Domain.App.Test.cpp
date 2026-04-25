#include "Exec/Domain.h"
#include "Exec/Delay/LoopTimerBackend.h"
#include "App/Factory.h"
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

/// Sender that immediately completes with set_error(exception_ptr).
struct ErrorSender
{
    using sender_concept = stdexec::sender_t;
    using completion_signatures = stdexec::completion_signatures<
        stdexec::set_value_t(int),
        stdexec::set_error_t(std::exception_ptr),
        stdexec::set_stopped_t()>;

    std::exception_ptr ex;

    template <class Receiver>
    struct OpState
    {
        Receiver receiver;
        std::exception_ptr ex;

        friend void tag_invoke(stdexec::start_t, OpState& self) noexcept
        {
            stdexec::set_error(std::move(self.receiver), std::move(self.ex));
        }
    };

    template <class Receiver>
    auto connect(Receiver receiver) const
    {
        return OpState<Receiver>{std::move(receiver), ex};
    }
};

} // namespace

// Domain(sender) wraps with starts_on internally and completes in one drain cycle.
TEST(DomainTest, PlainSenderCompletion)
{
    auto domain = std::make_shared<Exec::Domain>(stdexec::just(42));
    const auto runner = App::CreateTestRunner(domain);
    EXPECT_EQ(runner->Run(), 42);
}

// Domain(factory) stores the factory's sender directly — no extra starts_on wrapping.
TEST(DomainTest, FactoryConstructor)
{
    auto domain = std::make_shared<Exec::Domain>([](auto sched) {
        return stdexec::schedule(sched) | stdexec::then([] { return 7; });
    });
    const auto runner = App::CreateTestRunner(domain);
    EXPECT_EQ(runner->Run(), 7);
}

// Stop() called before Update(): the pending task must observe stop_requested()
// and call set_stopped() via DrainQueue(), not be destroyed while still queued
// (which would leave a dangling Operation<R>* inside the lock-free queue).
TEST(DomainTest, StopBeforeDrainCancelsTask)
{
    auto domain = std::make_shared<Exec::Domain>(stdexec::just(99));
    TestRunner runner{domain};

    domain->Start();  // enqueues the starts_on bridge task
    domain->Stop();   // request_stop → DrainQueue → set_stopped → Exit(0)

    EXPECT_EQ(runner.exitCode, RunLoop::ExitCode::Cancelled);
}

// An exec::task coroutine that reads get_scheduler from the environment and
// re-schedules via it. Verifies that DomainReceiver::get_env() correctly
// propagates get_scheduler — without it, read_env(get_scheduler) returns nothing
// and co_await schedule(sched) would fail at compile time or produce wrong results.
TEST(DomainTest, CoroutineSenderWithSchedulerAccess)
{
    auto domain = std::make_shared<Exec::Domain>(TwoHopTask());
    const auto runner = App::CreateTestRunner(domain);
    // DrainQueue() is greedy — it exhausts newly-enqueued tasks within the same
    // call, so all hops (starts_on bridge → exec::task start-on-scheduler →
    // co_await schedule resume) complete in a single Update() frame.
    EXPECT_EQ(runner->Run(), 55);
}

// A sender that completes with a null exception_ptr should still exit
// gracefully with ExitCode::Failure (covers the no-exceptions / empty-ptr branch).
TEST(DomainTest, NullExceptionPtrExitsWithFailure)
{
    auto domain = std::make_shared<Exec::Domain>(
        ErrorSender{std::exception_ptr{}},
        Exec::DomainOptions{.backend = std::make_unique<Exec::LoopTimerBackend>()});
    TestRunner runner{domain};

    runner.DriveStart();
    runner.DriveUpdate();

    ASSERT_TRUE(runner.exitCode.has_value());
    EXPECT_EQ(runner.exitCode.value(), RunLoop::ExitCode::Failure);
}
