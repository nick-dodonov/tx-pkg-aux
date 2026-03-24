#include "Asio/AsioDomain.h"
#include "Log/Log.h"
#include "TestRunner.h"
#include "pkg/asio/Asio/AsioDomain.h"

#include <boost/asio.hpp>
#include <chrono>
#include <atomic>
#include <gtest/gtest.h>

namespace asio = boost::asio;
using namespace Asio;

// ---------------------------------------------------------------------------
// ManualDrive_SimpleCompletion
//
// The coroutine completes immediately. A single DriveUpdate() drains the
// io_context queue and fires the co_spawn completion handler which calls
// Runner::Exit(42). No event loop is needed.
// ---------------------------------------------------------------------------
TEST(AsioDomainUnit, ManualDrive_SimpleCompletion)
{
    auto domain = std::make_shared<AsioDomain>(
        []() -> asio::awaitable<int> { co_return 42; }());

    TestRunner runner{domain};
    runner.DriveStart();
    runner.DriveUpdate();   // drains: co_spawn posts, co_return 42 fires Exit(42)

    EXPECT_EQ(runner.exitCode, 42);
}

// ---------------------------------------------------------------------------
// ManualDrive_StopCancelsPendingOp
//
// The coroutine is waiting on a 1-hour timer — it will never complete on its
// own. Stop() emits the cancellation signal so that co_await timer.async_wait
// gets operation_aborted, the coroutine unwinds, and the completion handler
// sets CancelledExitCode (because no exit code was established yet).
// ---------------------------------------------------------------------------
TEST(AsioDomainUnit, ManualDrive_StopCancelsPendingOp)
{
    auto domain = std::make_shared<AsioDomain>(
        []() -> asio::awaitable<int> {
            auto timer = asio::steady_timer(co_await asio::this_coro::executor);
            timer.expires_after(std::chrono::hours(1));
            // Use as_tuple so cancellation returns an error code instead of throwing
            // (required for -fno-exceptions builds; AsioDomain's _cancelled flag handles
            // the exit code — we just need the coroutine to return cleanly).
            co_await timer.async_wait(asio::as_tuple(asio::use_awaitable));
            co_return 0;
        }());

    TestRunner runner{domain};
    runner.DriveStart();
    runner.DriveUpdate();   // launches the coroutine; it suspends on the timer

    runner.DriveStop();     // emits cancellation_signal → operation_aborted
    runner.DriveUpdate();   // drains: coroutine unwinds, completion handler fires

    ASSERT_TRUE(runner.exitCode.has_value());
    EXPECT_EQ(*runner.exitCode, RunLoop::Runner::CancelledExitCode);
}

// ---------------------------------------------------------------------------
// ManualDrive_StopDoesNotOverrideExitCode
//
// The coroutine co_returns 7 before Stop() is called. The completion handler
// sets exitCode = 7. A subsequent Stop() must not overwrite it — TestRunner::Exit
// only stores the first code (first caller wins).
// ---------------------------------------------------------------------------
TEST(AsioDomainUnit, ManualDrive_StopDoesNotOverrideExitCode)
{
    auto domain = std::make_shared<AsioDomain>(
        []() -> asio::awaitable<int> { co_return 7; }());

    TestRunner runner{domain};
    runner.DriveStart();
    runner.DriveUpdate();   // coroutine completes → Exit(7) called

    ASSERT_EQ(runner.exitCode, 7);

    runner.DriveStop();     // cancellation signal emitted, but Exit(7) already set
    runner.DriveUpdate();   // drain any residual tasks

    EXPECT_EQ(runner.exitCode, 7); // must remain 7
}

// ---------------------------------------------------------------------------
// CancellationSlotReachesCoroutine
//
// Verifies that the cancellation signal wired in Start() actually propagates
// into the coroutine body: co_await asio::this_coro::cancellation_state gives
// a connected slot, and after Stop() the coroutine catches operation_aborted.
// ---------------------------------------------------------------------------
TEST(AsioDomainUnit, CancellationSlotReachesCoroutine)
{
    auto slotConnected = std::make_shared<std::atomic<bool>>(false);
    auto cancelReceived = std::make_shared<std::atomic<bool>>(false);

    auto domain = std::make_shared<AsioDomain>(
        [slotConnected, cancelReceived]() -> asio::awaitable<int> {
            // Check that our cancellation slot is connected (wired via bind_cancellation_slot).
            auto cs = co_await asio::this_coro::cancellation_state;
            slotConnected->store(cs.slot().is_connected());

            // Await a long timer — will be interrupted by cancellation.
            auto timer = asio::steady_timer(co_await asio::this_coro::executor);
            timer.expires_after(std::chrono::hours(1));
            auto [ec] = co_await timer.async_wait(asio::as_tuple(asio::use_awaitable));

            cancelReceived->store(ec == asio::error::operation_aborted);
            co_return 0;
        }());

    TestRunner runner{domain};
    runner.DriveStart();
    runner.DriveUpdate();   // coroutine starts, checks slot, suspends on timer

    EXPECT_TRUE(slotConnected->load()) << "cancellation slot must be connected inside coroMain";

    runner.DriveStop();
    runner.DriveUpdate();   // coroutine unwinds

    EXPECT_TRUE(cancelReceived->load()) << "coroutine must observe operation_aborted on cancellation";
}

// ---------------------------------------------------------------------------
// ManualDrive_CancelDirectly
//
// Same scenario as ManualDrive_StopCancelsPendingOp but without a timer:
// the coroutine suspends via AwaitCancellation(), which parks a handler
// directly in the cancellation slot. No timer queue slot is allocated.
// ---------------------------------------------------------------------------
TEST(AsioDomainUnit, ManualDrive_CancelDirectly)
{
    auto cancelReached = std::make_shared<std::atomic<bool>>(false);

    auto domain = std::make_shared<AsioDomain>(
        [cancelReached]() -> asio::awaitable<int> {
            auto executor = co_await asio::this_coro::executor;
            
            Log::Info("cancellation await...");
            co_await AsyncCancelled();
            Log::Info("cancellation received");

            // Reaching this line confirms cancellation was delivered and
            // the coroutine resumed cleanly. _cancelled flag in AsioDomain
            // ensures CancelledExitCode is used regardless of co_return value.
            cancelReached->store(true);
            co_return 0;
        }());

    TestRunner runner{domain};
    runner.DriveStart();
    runner.DriveUpdate();   // coroutine starts, registers OnCancel in slot, suspends

    EXPECT_FALSE(runner.exitCode.has_value()) << "coroutine should be suspended at AwaitCancellation";

    runner.DriveStop();     // emits cancellation_signal → OnCancel fires → coroutine resumes
    runner.DriveUpdate();   // drain any residual work

    EXPECT_TRUE(cancelReached->load());
    ASSERT_TRUE(runner.exitCode.has_value());
    EXPECT_EQ(*runner.exitCode, RunLoop::Runner::CancelledExitCode);
}
