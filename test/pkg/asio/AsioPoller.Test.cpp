#include "Asio/AsioPoller.h"
#include "TestRunner.h"

#include <boost/asio.hpp>
#include <gtest/gtest.h>

namespace asio = boost::asio;
using namespace Asio;

// ---------------------------------------------------------------------------
// Standalone_DrivesSingleTimer
//
// Post a zero-delay handler via asio::post(), call Update() once, and verify
// the handler was invoked. Proves that AsioPoller drives the io_context.
// ---------------------------------------------------------------------------
TEST(AsioPollerUnit, Standalone_DrivesSingleTimer)
{
    auto poller = std::make_shared<AsioPoller>();

    bool fired = false;
    asio::post(poller->GetIoContext(), [&] { fired = true; });

    TestRunner runner{poller};
    runner.DriveStart();
    runner.DriveUpdate();

    EXPECT_TRUE(fired);
}

// ---------------------------------------------------------------------------
// Standalone_ExposesWorkingExecutor
//
// Schedule work through GetExecutor() (not directly on io_context) and verify
// it is dispatched by Update(). Tests the executor accessor path.
// ---------------------------------------------------------------------------
TEST(AsioPollerUnit, Standalone_ExposesWorkingExecutor)
{
    auto poller = std::make_shared<AsioPoller>();

    int count = 0;
    asio::post(poller->GetExecutor(), [&] { ++count; });
    asio::post(poller->GetExecutor(), [&] { ++count; });

    TestRunner runner{poller};
    runner.DriveStart();
    runner.DriveUpdate();

    EXPECT_EQ(count, 2);
}

// ---------------------------------------------------------------------------
// Standalone_WorkGuardPreventsEarlyStop
//
// Without a work guard, io_context::poll() on an empty queue would mark the
// context as stopped; subsequent poll() calls would return 0 immediately even
// for newly posted work. With the work guard present, the io_context stays
// "live" across multiple Update() frames.
// ---------------------------------------------------------------------------
TEST(AsioPollerUnit, Standalone_WorkGuardPreventsEarlyStop)
{
    auto poller = std::make_shared<AsioPoller>();

    TestRunner runner{poller};
    runner.DriveStart();
    runner.DriveUpdate(0); // empty queue — io_context must NOT stop

    EXPECT_FALSE(poller->GetIoContext().stopped());

    // Work posted after the first (empty) frame must still be dispatched.
    bool fired = false;
    asio::post(poller->GetIoContext(), [&] { fired = true; });
    runner.DriveUpdate(1);

    EXPECT_TRUE(fired);
}
