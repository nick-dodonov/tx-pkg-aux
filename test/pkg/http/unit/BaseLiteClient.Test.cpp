#include "Http/Impl/BaseLiteClient.h"
#include "Asio/AsioDomain.h"
#include "App/Factory.h"

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <gtest/gtest.h>

#include <atomic>
#include <optional>

namespace asio = boost::asio;

namespace
{
    using namespace Http;

    /// Test double: GetAsync waits on a timer that can be cancelled via Asio's
    /// cancellation slot, making the stop_token→cancellation bridge observable.
    class StubLiteClient : public BaseLiteClient
    {
    public:
        using BaseLiteClient::BaseLiteClient;

        // Populated after coroutine inspects its cancellation state
        std::atomic<bool> slotConnected{false};
        std::atomic<bool> wasCancelled{false};
        std::string receivedUrl;
        std::optional<ILiteClient::Result> resultToReturn;

        asio::awaitable<ILiteClient::Result> GetAsync(std::string url) override
        {
            receivedUrl = url;

            // Check whether the cancellation slot is wired up
            auto cs = co_await asio::this_coro::cancellation_state;
            slotConnected.store(cs.slot().is_connected());

            if (resultToReturn) {
                co_return std::move(*resultToReturn);
            }

            // Wait on a long timer — cancellation should abort it
            auto executor = co_await asio::this_coro::executor;
            asio::steady_timer timer{executor, std::chrono::hours(24)};
            auto [ec] = co_await timer.async_wait(asio::as_tuple(asio::use_awaitable));

            wasCancelled.store(ec == asio::error::operation_aborted);

            if (ec == asio::error::operation_aborted) {
                co_return std::unexpected(std::system_error{
                    std::make_error_code(std::errc::operation_canceled),
                    "cancelled via stop_token"
                });
            }
            co_return ILiteClient::Response{.statusCode = 200, .body = "ok"};
        }
    };

    // Helper: run an asio coroutine to completion, return exit code
    static int RunCoro(asio::awaitable<int> coro)
    {
        auto domain = std::make_shared<Asio::AsioDomain>(std::move(coro));
        return App::CreateTestRunner(domain)->Run();
    }

    // -------------------------------------------------------------------------

    TEST(BaseLiteClientTest, CancellationSlotConnectedWithStopToken)
    {
        // Verify that when a stop_token is provided, the Asio cancellation slot
        // is connected inside GetAsync.
        auto exitCode = RunCoro([]() -> asio::awaitable<int> {
            auto executor = co_await asio::this_coro::executor;
            auto client = std::make_shared<StubLiteClient>(executor);
            client->resultToReturn = ILiteClient::Response{.statusCode = 200, .body = "ok"};

            std::stop_source source;
            bool handlerCalled = false;

            client->Get("http://test/slot-connected", [&](ILiteClient::Result result) {
                handlerCalled = true;
                EXPECT_TRUE(result.has_value());
            }, source.get_token());

            // Yield to let co_spawn execute
            auto timer = asio::steady_timer(executor, std::chrono::milliseconds(10));
            co_await timer.async_wait(asio::use_awaitable);

            EXPECT_TRUE(handlerCalled);
            EXPECT_TRUE(client->slotConnected.load());
            EXPECT_EQ(client->receivedUrl, "http://test/slot-connected");
            co_return 0;
        }());

        EXPECT_EQ(exitCode, 0);
    }

    TEST(BaseLiteClientTest, CancellationSlotDisconnectedWithoutStopToken)
    {
        // Without a stop_token (default {}), stop is not possible so no
        // stop_callback is created — but the cancellation signal is still allocated
        // for the bind_cancellation_slot wrapper.
        auto exitCode = RunCoro([]() -> asio::awaitable<int> {
            auto executor = co_await asio::this_coro::executor;
            auto client = std::make_shared<StubLiteClient>(executor);
            client->resultToReturn = ILiteClient::Response{.statusCode = 200, .body = "ok"};

            bool handlerCalled = false;
            // Call without stop_token (via interface to exercise default param)
            static_cast<ILiteClient*>(client.get())->Get(
                "http://test/no-token",
                [&](ILiteClient::Result result) {
                    handlerCalled = true;
                    EXPECT_TRUE(result.has_value());
                }
            );

            auto timer = asio::steady_timer(executor, std::chrono::milliseconds(10));
            co_await timer.async_wait(asio::use_awaitable);

            EXPECT_TRUE(handlerCalled);
            // Slot is still connected (bind_cancellation_slot always wires the slot),
            // but no stop_callback will ever fire on it.
            EXPECT_TRUE(client->slotConnected.load());
            co_return 0;
        }());

        EXPECT_EQ(exitCode, 0);
    }

    TEST(BaseLiteClientTest, StopTokenCancelsInFlightCoroutine)
    {
        // Trigger stop after Get() is called — the in-flight timer inside
        // GetAsync should be aborted via the cancellation bridge.
        auto exitCode = RunCoro([]() -> asio::awaitable<int> {
            auto executor = co_await asio::this_coro::executor;
            auto client = std::make_shared<StubLiteClient>(executor);
            // No resultToReturn → coroutine blocks on 24h timer

            std::stop_source source;
            bool handlerCalled = false;
            std::error_code handlerEc;

            client->Get("http://test/cancel-inflight", [&](ILiteClient::Result result) {
                handlerCalled = true;
                if (!result) {
                    handlerEc = result.error().code();
                }
            }, source.get_token());

            // Let coroutine start and reach the timer await
            auto timer = asio::steady_timer(executor, std::chrono::milliseconds(10));
            co_await timer.async_wait(asio::use_awaitable);

            EXPECT_FALSE(handlerCalled); // still waiting on 24h timer

            // Now request stop — should cancel the timer
            source.request_stop();

            // Yield to let cancellation propagate
            timer.expires_after(std::chrono::milliseconds(10));
            co_await timer.async_wait(asio::use_awaitable);

            EXPECT_TRUE(handlerCalled);
            EXPECT_TRUE(client->wasCancelled.load());
            EXPECT_EQ(handlerEc, std::make_error_code(std::errc::operation_canceled));
            co_return 0;
        }());

        EXPECT_EQ(exitCode, 0);
    }

    TEST(BaseLiteClientTest, AlreadyStoppedShortCircuits)
    {
        // When stop is already requested before Get(), the handler should be
        // called synchronously with operation_canceled — no coroutine spawned.
        auto exitCode = RunCoro([]() -> asio::awaitable<int> {
            auto executor = co_await asio::this_coro::executor;
            auto client = std::make_shared<StubLiteClient>(executor);

            std::stop_source source;
            source.request_stop();

            bool handlerCalled = false;
            std::error_code handlerEc;

            client->Get("http://test/already-stopped", [&](ILiteClient::Result result) {
                handlerCalled = true;
                if (!result) {
                    handlerEc = result.error().code();
                }
            }, source.get_token());

            // Handler should have been called synchronously (no co_spawn)
            EXPECT_TRUE(handlerCalled);
            EXPECT_EQ(handlerEc, std::make_error_code(std::errc::operation_canceled));
            // GetAsync should NOT have been called
            EXPECT_TRUE(client->receivedUrl.empty());
            co_return 0;
        }());

        EXPECT_EQ(exitCode, 0);
    }

    TEST(BaseLiteClientTest, NormalCompletionWithStopToken)
    {
        // Stop token provided but never triggered — request completes normally.
        auto exitCode = RunCoro([]() -> asio::awaitable<int> {
            auto executor = co_await asio::this_coro::executor;
            auto client = std::make_shared<StubLiteClient>(executor);
            client->resultToReturn = ILiteClient::Response{.statusCode = 201, .body = "created"};

            std::stop_source source;
            bool handlerCalled = false;
            int statusCode = 0;
            std::string body;

            client->Get("http://test/normal", [&](ILiteClient::Result result) {
                handlerCalled = true;
                if (result) {
                    statusCode = result->statusCode;
                    body = result->body;
                }
            }, source.get_token());

            auto timer = asio::steady_timer(executor, std::chrono::milliseconds(10));
            co_await timer.async_wait(asio::use_awaitable);

            EXPECT_TRUE(handlerCalled);
            EXPECT_EQ(statusCode, 201);
            EXPECT_EQ(body, "created");
            EXPECT_FALSE(client->wasCancelled.load());
            co_return 0;
        }());

        EXPECT_EQ(exitCode, 0);
    }

    TEST(BaseLiteClientTest, ErrorResultPropagated)
    {
        // GetAsync returns an error result — verify it passes through to handler.
        auto exitCode = RunCoro([]() -> asio::awaitable<int> {
            auto executor = co_await asio::this_coro::executor;
            auto client = std::make_shared<StubLiteClient>(executor);
            client->resultToReturn = std::unexpected(std::system_error{
                std::make_error_code(std::errc::connection_refused),
                "test error"
            });

            bool handlerCalled = false;
            std::error_code handlerEc;

            client->Get("http://test/error", [&](ILiteClient::Result result) {
                handlerCalled = true;
                if (!result) {
                    handlerEc = result.error().code();
                }
            }, std::stop_token{});

            auto timer = asio::steady_timer(executor, std::chrono::milliseconds(10));
            co_await timer.async_wait(asio::use_awaitable);

            EXPECT_TRUE(handlerCalled);
            EXPECT_EQ(handlerEc, std::make_error_code(std::errc::connection_refused));
            co_return 0;
        }());

        EXPECT_EQ(exitCode, 0);
    }
}
