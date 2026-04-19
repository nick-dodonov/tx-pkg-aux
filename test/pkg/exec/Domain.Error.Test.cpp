#include "Exec/Domain.h"
#include "Exec/Delay/LoopTimerBackend.h"
#include "TestRunner.h"

#include <gtest/gtest.h>
#include <stdexec/execution.hpp>

#include <stdexcept>

namespace {

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

// A sender that completes with a std::runtime_error should cause the Domain
// to exit with ExitCode::Failure and log the exception message — not crash.
TEST(DomainErrorTest, ExceptionPtrExitsWithFailure)
{
    auto ex = std::make_exception_ptr(std::runtime_error("test error from sender"));
    auto domain = std::make_shared<Exec::Domain>(
        ErrorSender{ex},
        std::make_unique<Exec::LoopTimerBackend>());
    TestRunner runner{domain};

    runner.DriveStart();
    runner.DriveUpdate();

    ASSERT_TRUE(runner.exitCode.has_value());
    EXPECT_EQ(runner.exitCode.value(), RunLoop::ExitCode::Failure);
}

// A sender that completes with a non-std::exception type should still exit
// gracefully with ExitCode::Failure (the "unknown type" branch).
TEST(DomainErrorTest, UnknownExceptionTypeExitsWithFailure)
{
    auto ex = std::make_exception_ptr(42); // not derived from std::exception
    auto domain = std::make_shared<Exec::Domain>(
        ErrorSender{ex},
        std::make_unique<Exec::LoopTimerBackend>());
    TestRunner runner{domain};

    runner.DriveStart();
    runner.DriveUpdate();

    ASSERT_TRUE(runner.exitCode.has_value());
    EXPECT_EQ(runner.exitCode.value(), RunLoop::ExitCode::Failure);
}
