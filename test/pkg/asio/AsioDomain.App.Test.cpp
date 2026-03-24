#include "App/Factory.h"
#include "Asio/AsioDomain.h"
#include "Boot/Boot.h"
#include "CoroAsserts.h"
#include "pkg/asio/Asio/AsioDomain.h"

#include <boost/asio.hpp>
#include <chrono>
#include <gtest/gtest.h>

namespace asio = boost::asio;
using namespace App;
using namespace Asio;

// Test basic Domain creation and destruction
TEST(DomainTest, CreateAndDestroy)
{
    auto coroMain = []() -> asio::awaitable<int> { co_return 0;};
    auto domain = std::make_shared<AsioDomain>(coroMain());
    EXPECT_NE(domain, nullptr);
}

// Test simple coroutine execution that returns immediately
TEST(DomainTest, RunSimpleCoroMain)
{
    auto coroMain = []() -> asio::awaitable<int> {
        co_return 42;
    };
    
    auto domain = std::make_shared<AsioDomain>(coroMain());
    auto runner = App::CreateTestRunner(domain);
    
    int exitCode = runner->Run();
    EXPECT_EQ(exitCode, 42);
}

// Test coroutine with async timer
TEST(DomainTest, RunCoroWithAsyncTimer)
{
    auto coroMain = []() -> asio::awaitable<int> {
        auto timer = asio::steady_timer(co_await asio::this_coro::executor);
        timer.expires_after(std::chrono::milliseconds(10));
        co_await timer.async_wait(asio::use_awaitable);
        co_return 0;
    };
    
    auto domain = std::make_shared<AsioDomain>(coroMain());
    auto runner = App::CreateTestRunner(domain);
    
    int exitCode = runner->Run();
    EXPECT_EQ(exitCode, 0);
}

// Test coroutine with multiple async operations
TEST(DomainTest, RunCoroWithMultipleAsyncOps)
{
    auto coroMain = []() -> asio::awaitable<int> {
        auto executor = co_await asio::this_coro::executor;
        
        // First timer
        auto timer1 = asio::steady_timer(executor);
        timer1.expires_after(std::chrono::milliseconds(5));
        co_await timer1.async_wait(asio::use_awaitable);
        
        // Second timer
        auto timer2 = asio::steady_timer(executor);
        timer2.expires_after(std::chrono::milliseconds(5));
        co_await timer2.async_wait(asio::use_awaitable);
        
        co_return 123;
    };
    
    auto domain = std::make_shared<AsioDomain>(coroMain());
    auto runner = App::CreateTestRunner(domain);
    
    int exitCode = runner->Run();
    EXPECT_EQ(exitCode, 123);
}

// Test Domain with nested coroutines
TEST(DomainTest, RunCoroWithNestedCoro)
{
    auto nestedCoro = []() -> asio::awaitable<int> {
        auto timer = asio::steady_timer(co_await asio::this_coro::executor);
        timer.expires_after(std::chrono::milliseconds(5));
        co_await timer.async_wait(asio::use_awaitable);
        co_return 99;
    };
    
    auto mainCoro = [&nestedCoro]() -> asio::awaitable<int> {
        int result = co_await nestedCoro();
        co_return result;
    };
    
    auto domain = std::make_shared<AsioDomain>(mainCoro());
    auto runner = App::CreateTestRunner(domain);
    
    int exitCode = runner->Run();
    EXPECT_EQ(exitCode, 99);
}

// Test Domain can be retrieved from executor in nested coroutine
TEST(DomainTest, FromExecutorInNestedCoro)
{
    auto coroMain = []() -> asio::awaitable<int> {
        auto executor = co_await asio::this_coro::executor;

        // Retrieve domain from executor — no closure capture needed
        auto subTask = []() -> asio::awaitable<void> {
            auto ex = co_await asio::this_coro::executor;
            auto* domain = AsioDomain::FromExecutor(ex);
            CO_ASSERT_NE(domain, nullptr);
        };

        asio::co_spawn(executor, subTask(), asio::detached);

        // Give some time for the wait to be set up
        auto timer = asio::steady_timer(executor);
        timer.expires_after(std::chrono::milliseconds(10));
        co_await timer.async_wait(asio::use_awaitable);

        co_return 0;
    };

    auto domain = std::make_shared<AsioDomain>(coroMain());
    auto runner = App::CreateTestRunner(domain);
    int exitCode = runner->Run();
    EXPECT_EQ(exitCode, 0);
}

// Test Domain AsyncCancelled functionality
TEST(DomainTest, AsyncCancelledSignaling)
{
    auto coroMain = []() -> asio::awaitable<int> {
        auto executor = co_await asio::this_coro::executor;

        // Retrieve domain from executor — no closure capture needed
        auto stopTask = []() -> asio::awaitable<void> {
            auto ec = co_await AsyncCancelled();
            EXPECT_FALSE(ec); // Should complete successfully
        };

        asio::co_spawn(executor, stopTask(), asio::detached);

        // Give some time for the wait to be set up
        auto timer = asio::steady_timer(executor);
        timer.expires_after(std::chrono::milliseconds(10));
        co_await timer.async_wait(asio::use_awaitable);

        co_return 0;
    };

    auto domain = std::make_shared<AsioDomain>(coroMain());
    auto runner = App::CreateTestRunner(domain);

    int exitCode = runner->Run();
    EXPECT_EQ(exitCode, 0);
}

int main(int argc, char** argv)
{
    Boot::DefaultInit(argc, const_cast<const char**>(argv));
    ::testing::InitGoogleTest(&argc, argv);
    exit(RUN_ALL_TESTS()); // exit to workaround emscripten issue (read comment in TightRunner)
}
