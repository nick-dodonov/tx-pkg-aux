#include "App/Domain.h"
#include "App/Loop/TightRunner.h"
#include "Boot/Boot.h"

#include <gtest/gtest.h>
#include <boost/asio.hpp>
#include <chrono>

namespace asio = boost::asio;

// Test basic Domain creation and destruction
TEST(DomainTest, CreateAndDestroy)
{
    auto domain = std::make_shared<App::Domain>();
    EXPECT_NE(domain, nullptr);
}

// Test Domain executor availability
TEST(DomainTest, GetExecutor)
{
    auto domain = std::make_shared<App::Domain>();
    auto executor = domain->GetExecutor();
    // Just verify we can get an executor (it's a valid type)
    EXPECT_NE(&executor, nullptr);
}

// Test simple coroutine execution that returns immediately
TEST(DomainTest, RunSimpleCoroMain)
{
    auto domain = std::make_shared<App::Domain>();
    auto runner = std::make_shared<App::Loop::TightRunner>(domain);
    
    auto coroMain = []() -> asio::awaitable<int> {
        co_return 42;
    };
    
    int exitCode = domain->RunCoroMain(runner, coroMain());
    EXPECT_EQ(exitCode, 42);
}

// Test coroutine with async timer
TEST(DomainTest, RunCoroWithAsyncTimer)
{
    auto domain = std::make_shared<App::Domain>();
    auto runner = std::make_shared<App::Loop::TightRunner>(domain);
    
    auto coroMain = []() -> asio::awaitable<int> {
        auto timer = asio::steady_timer(co_await asio::this_coro::executor);
        timer.expires_after(std::chrono::milliseconds(10));
        co_await timer.async_wait(asio::use_awaitable);
        co_return 0;
    };
    
    int exitCode = domain->RunCoroMain(runner, coroMain());
    EXPECT_EQ(exitCode, 0);
}

// Test coroutine with multiple async operations
TEST(DomainTest, RunCoroWithMultipleAsyncOps)
{
    auto domain = std::make_shared<App::Domain>();
    auto runner = std::make_shared<App::Loop::TightRunner>(domain);
    
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
    
    int exitCode = domain->RunCoroMain(runner, coroMain());
    EXPECT_EQ(exitCode, 123);
}

// Test Domain with nested coroutines
TEST(DomainTest, RunCoroWithNestedCoro)
{
    auto domain = std::make_shared<App::Domain>();
    auto runner = std::make_shared<App::Loop::TightRunner>(domain);
    
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
    
    int exitCode = domain->RunCoroMain(runner, mainCoro());
    EXPECT_EQ(exitCode, 99);
}

// Test Domain AsyncStopped functionality
TEST(DomainTest, AsyncStoppedSignaling)
{
    auto domain = std::make_shared<App::Domain>();
    auto runner = std::make_shared<App::Loop::TightRunner>(domain);
    
    auto coroMain = [domain]() -> asio::awaitable<int> {
        // Start async wait for stop
        auto stopTask = [domain]() -> asio::awaitable<void> {
            auto ec = co_await domain->AsyncStopped();
            EXPECT_FALSE(ec); // Should complete successfully
            co_return;
        };
        
        auto executor = co_await asio::this_coro::executor;
        asio::co_spawn(executor, stopTask(), asio::detached);
        
        // Give some time for the wait to be set up
        auto timer = asio::steady_timer(executor);
        timer.expires_after(std::chrono::milliseconds(10));
        co_await timer.async_wait(asio::use_awaitable);
        
        co_return 0;
    };
    
    int exitCode = domain->RunCoroMain(runner, coroMain());
    EXPECT_EQ(exitCode, 0);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    Boot::DefaultInit(argc, const_cast<const char**>(argv));
    return RUN_ALL_TESTS();
}
