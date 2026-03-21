#include "App/Loop/Factory.h"
#include "Coro/Domain.h"
#include "Log/Log.h"

#include <gtest/gtest.h>

// Minimal coroutine that returns immediately.
// If this test hangs, the issue is in Domain/TightRunner plumbing,
// not in any I/O or async operation.
TEST(CoroDomainTest, ImmediateReturn)
{
    auto task = []() -> Coro::Task<int> {
        Log::Trace("co_return 42");
        co_return 42;
    };

    auto domain = std::make_shared<Coro::Domain>(task());
    auto runner = App::Loop::CreateTestRunner(domain);

    Log::Trace("runner->Run() ...");
    int exitCode = runner->Run();
    Log::Trace("runner->Run() => {}", exitCode);

    EXPECT_EQ(exitCode, 42);
}

// Coroutine that does a trivial co_await (suspend + resume).
TEST(CoroDomainTest, TrivialCoAwait)
{
    auto task = []() -> Coro::Task<int> {
        // Force at least one real suspend/resume via co_await
        auto inner = []() -> Coro::Task<int> {
            co_return 7;
        };
        int v = co_await inner();
        Log::Trace("inner returned {}", v);
        co_return v;
    };

    auto domain = std::make_shared<Coro::Domain>(task());
    auto runner = App::Loop::CreateTestRunner(domain);
    int exitCode = runner->Run();
    EXPECT_EQ(exitCode, 7);
}
