#include "Http/ILiteClient.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace
{
    using namespace Http;
    using ::testing::_;
    using ::testing::Invoke;

    // gmock doesn't handle rvalue-ref arguments well, so we relay through a
    // non-rvalue proxy that captures the callback by value.
    class MockLiteClient : public ILiteClient
    {
    public:
        MOCK_METHOD(void, DoGet, (std::string_view url, Callback handler, std::stop_token stopToken));

        void Get(std::string_view url, Callback&& handler, std::stop_token stopToken) override
        {
            DoGet(url, std::move(handler), std::move(stopToken));
        }
    };

    // Helper: return a shared_ptr<ILiteClient> so callers see the default parameter
    static std::shared_ptr<ILiteClient> AsInterface(std::shared_ptr<MockLiteClient> m)
    {
        return m;
    }

    // --- Tests ----------------------------------------------------------------

    TEST(ILiteClientTest, BasicGetCall)
    {
        auto mock = std::make_shared<MockLiteClient>();
        auto client = AsInterface(mock);
        const std::string expectedUrl = "http://example.com/test";

        EXPECT_CALL(*mock, DoGet(std::string_view{expectedUrl}, _, _)).Times(1);

        client->Get(expectedUrl, [](ILiteClient::Result) {});
    }

    TEST(ILiteClientTest, SuccessResponse)
    {
        auto mock = std::make_shared<MockLiteClient>();
        auto client = AsInterface(mock);

        EXPECT_CALL(*mock, DoGet(_, _, _))
            .WillOnce(Invoke([](std::string_view, ILiteClient::Callback handler, std::stop_token) {
                handler(ILiteClient::Response{.statusCode = 200, .body = "OK"});
            }));

        bool called = false;
        client->Get("http://example.com", [&](ILiteClient::Result result) {
            called = true;
            ASSERT_TRUE(result.has_value());
            EXPECT_EQ(result->statusCode, 200);
            EXPECT_EQ(result->body, "OK");
        });

        EXPECT_TRUE(called);
    }

    TEST(ILiteClientTest, ErrorResponse)
    {
        auto mock = std::make_shared<MockLiteClient>();
        auto client = AsInterface(mock);

        EXPECT_CALL(*mock, DoGet(_, _, _))
            .WillOnce(Invoke([](std::string_view, ILiteClient::Callback handler, std::stop_token) {
                handler(std::unexpected(std::system_error{
                    std::make_error_code(std::errc::connection_refused),
                    "connection refused"
                }));
            }));

        bool called = false;
        client->Get("http://example.com", [&](ILiteClient::Result result) {
            called = true;
            ASSERT_FALSE(result.has_value());
            EXPECT_EQ(result.error().code(), std::make_error_code(std::errc::connection_refused));
        });

        EXPECT_TRUE(called);
    }

    TEST(ILiteClientTest, DefaultStopTokenIsEmpty)
    {
        auto mock = std::make_shared<MockLiteClient>();
        auto client = AsInterface(mock);

        EXPECT_CALL(*mock, DoGet(_, _, _))
            .WillOnce(Invoke([](std::string_view, ILiteClient::Callback handler, std::stop_token token) {
                // Default-constructed stop_token: stop is never possible
                EXPECT_FALSE(token.stop_possible());
                EXPECT_FALSE(token.stop_requested());
                handler(ILiteClient::Response{.statusCode = 200, .body = {}});
            }));

        // Called without stop_token — default {} should be forwarded
        client->Get("http://example.com", [](ILiteClient::Result) {});
    }

    TEST(ILiteClientTest, StopTokenForwarded)
    {
        auto mock = std::make_shared<MockLiteClient>();
        auto client = AsInterface(mock);
        std::stop_source source;

        EXPECT_CALL(*mock, DoGet(_, _, _))
            .WillOnce(Invoke([&](std::string_view, ILiteClient::Callback handler, std::stop_token token) {
                EXPECT_TRUE(token.stop_possible());
                EXPECT_FALSE(token.stop_requested());

                // Request stop and verify propagation
                source.request_stop();
                EXPECT_TRUE(token.stop_requested());
                handler(ILiteClient::Response{.statusCode = 200, .body = {}});
            }));

        client->Get("http://example.com", [](ILiteClient::Result) {}, source.get_token());
    }

    TEST(ILiteClientTest, AlreadyStoppedToken)
    {
        auto mock = std::make_shared<MockLiteClient>();
        auto client = AsInterface(mock);
        std::stop_source source;
        source.request_stop();

        EXPECT_CALL(*mock, DoGet(_, _, _))
            .WillOnce(Invoke([](std::string_view, ILiteClient::Callback handler, std::stop_token token) {
                EXPECT_TRUE(token.stop_requested());
                // Implementation may choose to skip the request
                handler(std::unexpected(std::system_error{
                    std::make_error_code(std::errc::operation_canceled),
                    "already stopped"
                }));
            }));

        bool called = false;
        client->Get("http://example.com", [&](ILiteClient::Result result) {
            called = true;
            ASSERT_FALSE(result.has_value());
            EXPECT_EQ(result.error().code(), std::make_error_code(std::errc::operation_canceled));
        }, source.get_token());

        EXPECT_TRUE(called);
    }

    TEST(ILiteClientTest, SharedFromThis)
    {
        auto mock = std::make_shared<MockLiteClient>();

        // ILiteClient inherits enable_shared_from_this — verify it works
        auto shared = mock->shared_from_this();
        EXPECT_EQ(shared.get(), mock.get());
    }

    TEST(ILiteClientTest, MultipleGetCalls)
    {
        auto mock = std::make_shared<MockLiteClient>();
        auto client = AsInterface(mock);

        EXPECT_CALL(*mock, DoGet(std::string_view{"http://a.com"}, _, _))
            .WillOnce(Invoke([](std::string_view, ILiteClient::Callback handler, std::stop_token) {
                handler(ILiteClient::Response{.statusCode = 200, .body = "A"});
            }));
        EXPECT_CALL(*mock, DoGet(std::string_view{"http://b.com"}, _, _))
            .WillOnce(Invoke([](std::string_view, ILiteClient::Callback handler, std::stop_token) {
                handler(ILiteClient::Response{.statusCode = 404, .body = "Not Found"});
            }));

        std::vector<ILiteClient::Result> results;
        client->Get("http://a.com", [&](ILiteClient::Result r) { results.push_back(std::move(r)); });
        client->Get("http://b.com", [&](ILiteClient::Result r) { results.push_back(std::move(r)); });

        ASSERT_EQ(results.size(), 2u);
        EXPECT_EQ(results[0]->statusCode, 200);
        EXPECT_EQ(results[1]->statusCode, 404);
    }
}
