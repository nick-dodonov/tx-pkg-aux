// In-process integration tests for DcWsSigClient + DcWsSigServer.
//
// DcWsSigServer is started in-process to avoid an external Node.js dependency.
// The server is started once per test suite on an ephemeral OS-assigned port
// (Options::port = 0) and torn down afterwards.

#include "Rtt/Rtc/SigHub.h"
#include "Rtt/Rtc/Dc/DcWsSigClient.h"
#include "Rtt/Rtc/Dc/DcWsSigServer.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cerrno>
#include <latch>
#include <memory>
#include <string>
#include <thread>

using namespace Rtt;
using namespace Rtt::Rtc;
using namespace std::chrono_literals;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static bool await(std::latch& latch, std::chrono::milliseconds timeout = 3s)
{
    auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (latch.try_wait()) {
            return true;
        }
        std::this_thread::sleep_for(10ms);
    }
    return false;
}

// Minimal ISigHandler that routes messages and disconnects to std::function callbacks.
struct FuncHandler : ISigHandler
{
    explicit FuncHandler(
        std::function<void(SigMessage&&)> onMessage,
        std::function<void(std::error_code)> onLeft = {})
        : _onMessage(std::move(onMessage))
        , _onLeft(std::move(onLeft))
    {}
    void OnMessage(SigMessage&& msg) override { if (_onMessage) { _onMessage(std::move(msg)); } }
    void OnLeft(std::error_code ec) override { if (_onLeft) { _onLeft(ec); } }
private:
    std::function<void(SigMessage&&)> _onMessage;
    std::function<void(std::error_code)> _onLeft;
};

// Builds a SigJoinHandler (factory) from separate callbacks.
// handlerOut keeps the handler alive for the duration of the connection.
static SigJoinHandler makeJoinHandler(
    std::shared_ptr<ISigHandler>& handlerOut,
    std::shared_ptr<ISigUser>* userOut,
    std::function<void(SigMessage&&)> onMessage,
    std::function<void(bool)> onJoined = {},
    std::function<void(std::error_code)> onLeft = {})
{
    auto h = std::make_shared<FuncHandler>(std::move(onMessage), std::move(onLeft));
    handlerOut = h;
    std::weak_ptr<ISigHandler> wh{h};
    return [wh, userOut, onJoined = std::move(onJoined)](SigJoinResult r) mutable -> std::weak_ptr<ISigHandler> {
        bool ok = r.has_value();
        if (ok && userOut) {
            *userOut = *r;
        }
        if (onJoined) {
            onJoined(ok);
        }
        return ok ? wh : std::weak_ptr<ISigHandler>{};
    };
}

// ---------------------------------------------------------------------------
// Test fixture — one DcWsSigServer shared by all tests in the suite
// ---------------------------------------------------------------------------

class DcWsSigInProcessTest : public testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        sHub = std::make_shared<SigHub>();
        server = std::make_unique<DcWsSigServer>(sHub, DcWsSigServer::Options{.port = 0});
        server->Start();
        // Give the server a moment to bind its port.
        std::this_thread::sleep_for(50ms);
    }

    static void TearDownTestSuite()
    {
        server.reset();
        sHub.reset();
    }

    // Make a DcWsSigClient pointed at the shared server.
    static DcWsSigClient makeClient()
    {
        return DcWsSigClient{DcWsSigClient::Options{
            .host = "127.0.0.1",
            .port = server->Port()
        }};
    }

    static std::shared_ptr<SigHub> sHub;
    static std::unique_ptr<DcWsSigServer> server;
};

std::shared_ptr<SigHub> DcWsSigInProcessTest::sHub;
std::unique_ptr<DcWsSigServer> DcWsSigInProcessTest::server;

// ---------------------------------------------------------------------------
// Static interface checks
// ---------------------------------------------------------------------------

static_assert(SigClientLike<DcWsSigClient>);
static_assert(SigServerLike<DcWsSigServer>);

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_F(DcWsSigInProcessTest, Join_ConnectsToServer)
{
    auto client = makeClient();

    std::shared_ptr<ISigUser> joinedUser;
    std::latch done{1};

    std::shared_ptr<ISigHandler> handler;
    client.Join(
        PeerId{"test-join"},
        makeJoinHandler(handler, &joinedUser, [](SigMessage&&) {}, [&](bool) { done.count_down(); }));

    ASSERT_TRUE(await(done));
    ASSERT_NE(joinedUser, nullptr);
    EXPECT_EQ(joinedUser->LocalId().value, "test-join");
}

TEST_F(DcWsSigInProcessTest, Join_ServerUnavailable_ReportsError)
{
    // Connect to a port where nobody is listening.
    DcWsSigClient badClient{DcWsSigClient::Options{.host = "127.0.0.1", .port = 1}};

    bool errorReported = false;
    std::latch done{1};

    std::shared_ptr<ISigHandler> handler;
    badClient.Join(
        PeerId{"nobody"},
        makeJoinHandler(handler, nullptr, [](SigMessage&&) {}, [&](bool ok) {
            errorReported = !ok;
            done.count_down();
        }));

    ASSERT_TRUE(await(done, 5s));
    EXPECT_TRUE(errorReported);
}

TEST_F(DcWsSigInProcessTest, Send_RelayedToPeer)
{
    auto sender   = makeClient();
    auto receiver = makeClient();

    std::string receivedPayload;
    std::latch  done{1};

    std::shared_ptr<ISigUser> receiverUser;
    std::shared_ptr<ISigHandler> receiverHandler;
    receiver.Join(
        PeerId{"recv"},
        makeJoinHandler(receiverHandler, &receiverUser, [&](SigMessage&& msg) {
            receivedPayload = msg.payload;
            done.count_down();
        }));

    // Wait for receiver to connect before sending.
    std::this_thread::sleep_for(100ms);

    std::shared_ptr<ISigUser> senderUser;
    std::shared_ptr<ISigHandler> senderHandler;
    sender.Join(
        PeerId{"send"},
        makeJoinHandler(senderHandler, &senderUser, [](SigMessage&&) {}));

    std::this_thread::sleep_for(100ms);
    ASSERT_NE(senderUser, nullptr);

    senderUser->Send(PeerId{"recv"}, "relay-test");

    ASSERT_TRUE(await(done));
    EXPECT_EQ(receivedPayload, "relay-test");
}

TEST_F(DcWsSigInProcessTest, Send_Bidirectional)
{
    auto ca = makeClient();
    auto cb = makeClient();

    std::string fromB, fromA;
    std::latch  done{2};

    std::shared_ptr<ISigUser> ua, ub;
    std::shared_ptr<ISigHandler> ha, hb;

    ca.Join(
        PeerId{"peer-a"},
        makeJoinHandler(ha, &ua, [&](SigMessage&& msg) { fromB = msg.payload; done.count_down(); }));

    cb.Join(
        PeerId{"peer-b"},
        makeJoinHandler(hb, &ub, [&](SigMessage&& msg) { fromA = msg.payload; done.count_down(); }));

    // Wait for both sides to connect.
    std::this_thread::sleep_for(150ms);

    ASSERT_NE(ua, nullptr);
    ASSERT_NE(ub, nullptr);

    ua->Send(PeerId{"peer-b"}, "from-a");
    ub->Send(PeerId{"peer-a"}, "from-b");

    ASSERT_TRUE(await(done));
    EXPECT_EQ(fromA, "from-a");
    EXPECT_EQ(fromB, "from-b");
}

TEST_F(DcWsSigInProcessTest, UserLeave_ClosesWebSocket)
{
    auto client = makeClient();

    std::latch connected{1};
    std::shared_ptr<ISigUser> user;
    std::shared_ptr<ISigHandler> handler;

    client.Join(
        PeerId{"leaver"},
        makeJoinHandler(handler, &user, [](SigMessage&&) {}, [&](bool) { connected.count_down(); }));

    ASSERT_TRUE(await(connected));
    ASSERT_NE(user, nullptr);

    // Dropping the user should close the socket without crashing.
    user.reset();

    // Give time for close event to propagate.
    std::this_thread::sleep_for(100ms);
    // If we reach here without a crash, the test passes.
    SUCCEED();
}

TEST_F(DcWsSigInProcessTest, Leave_CallsOnLeft_Clean)
{
    auto client = makeClient();

    std::latch connected{1};
    std::latch leftNotified{1};
    std::error_code leftEc{std::make_error_code(std::errc::interrupted)}; // non-empty sentinel

    std::shared_ptr<ISigUser> user;
    std::shared_ptr<ISigHandler> handler;

    client.Join(
        PeerId{"leave-clean"},
        makeJoinHandler(
            handler, &user,
            [](SigMessage&&) {},
            [&](bool) { connected.count_down(); },
            [&](std::error_code ec) { leftEc = ec; leftNotified.count_down(); }));

    ASSERT_TRUE(await(connected));
    ASSERT_NE(user, nullptr);

    user->Leave();

    ASSERT_TRUE(await(leftNotified));
    EXPECT_FALSE(leftEc) << "expected empty error code for clean Leave(), got: " << leftEc.message();
}

TEST_F(DcWsSigInProcessTest, ServerDropped_CallsOnLeft_WithError)
{
    auto client = makeClient();

    std::latch connected{1};
    auto leftNotified = std::make_shared<std::latch>(1);
    auto leftEc = std::make_shared<std::error_code>();

    std::shared_ptr<ISigUser> user;
    std::shared_ptr<ISigHandler> handler;

    client.Join(
        PeerId{"drop-test"},
        makeJoinHandler(
            handler, &user,
            [](SigMessage&&) {},
            [&](bool) { connected.count_down(); },
            [leftEc, leftNotified](std::error_code ec) { *leftEc = ec; leftNotified->count_down(); }));

    ASSERT_TRUE(await(connected));
    ASSERT_NE(user, nullptr);

    // Close the server-side socket — the client will receive onClosed without
    // having called Leave(), so it should report ConnectionLost.
    server->DisconnectPeer(PeerId{"drop-test"});

    ASSERT_TRUE(await(*leftNotified));
    EXPECT_EQ(*leftEc, SigError::ConnectionLost);
}
