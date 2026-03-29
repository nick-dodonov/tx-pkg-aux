// Integration tests for WsSigClient + WsSigServer.
//
// WsSigServer is used directly to avoid an external Node.js dependency.
// The server is started once per test suite on an ephemeral OS-assigned port
// (Options::port = 0) and torn down afterwards.

#include "Rtt/Rtc/SigHub.h"
#include "Rtt/Rtc/Dc/DcWsSigClient.h"
#include "Rtt/Rtc/Dc/DcWsSigServer.h"

#include <gtest/gtest.h>

#include <chrono>
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

// ---------------------------------------------------------------------------
// Test fixture — one WsSigServer shared by all tests in the suite
// ---------------------------------------------------------------------------

class WsSigClientTest : public testing::Test
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

    // Make a WsSigClient pointed at the shared server.
    static DcWsSigClient makeClient()
    {
        return DcWsSigClient{DcWsSigClient::Options{.host = "127.0.0.1", .port = server->Port()}};
    }

    static std::shared_ptr<SigHub> sHub;
    static std::unique_ptr<DcWsSigServer> server;
};

std::shared_ptr<SigHub> WsSigClientTest::sHub;
std::unique_ptr<DcWsSigServer> WsSigClientTest::server;

// ---------------------------------------------------------------------------
// Static interface checks
// ---------------------------------------------------------------------------

static_assert(SigClientLike<DcWsSigClient>);
static_assert(SigServerLike<DcWsSigServer>);

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_F(WsSigClientTest, Join_ConnectsToServer)
{
    auto client = makeClient();

    std::shared_ptr<ISigUser> joinedUser;
    std::latch done{1};

    client.Join(
        PeerId{"test-join"},
        [](const SigMessage&) {},
        [&](SigJoinResult r) {
            if (r.has_value()) {
                joinedUser = *r;
            }
            done.count_down();
        });

    ASSERT_TRUE(await(done));
    ASSERT_NE(joinedUser, nullptr);
    EXPECT_EQ(joinedUser->LocalId().value, "test-join");
}

TEST_F(WsSigClientTest, Join_ServerUnavailable_ReportsError)
{
    // Connect to a port where nobody is listening.
    DcWsSigClient badClient{DcWsSigClient::Options{.host = "127.0.0.1", .port = 1}};

    bool errorReported = false;
    std::latch done{1};

    badClient.Join(
        PeerId{"nobody"},
        [](const SigMessage&) {},
        [&](SigJoinResult r) {
            errorReported = !r.has_value();
            done.count_down();
        });

    ASSERT_TRUE(await(done, 5s));
    EXPECT_TRUE(errorReported);
}

TEST_F(WsSigClientTest, Send_RelayedToPeer)
{
    auto sender   = makeClient();
    auto receiver = makeClient();

    std::string receivedPayload;
    std::latch  done{1};

    std::shared_ptr<ISigUser> receiverUser;
    receiver.Join(
        PeerId{"recv"},
        [&](const SigMessage& msg) {
            receivedPayload = msg.payload;
            done.count_down();
        },
        [&](SigJoinResult r) {
            if (r.has_value()) {
                receiverUser = *r;
            }
        });

    // Wait for receiver to connect before sending.
    std::this_thread::sleep_for(100ms);

    std::shared_ptr<ISigUser> senderUser;
    sender.Join(
        PeerId{"send"},
        [](const SigMessage&) {},
        [&](SigJoinResult r) {
            if (r.has_value()) {
                senderUser = *r;
            }
        });

    std::this_thread::sleep_for(100ms);
    ASSERT_NE(senderUser, nullptr);

    senderUser->Send(PeerId{"recv"}, "relay-test");

    ASSERT_TRUE(await(done));
    EXPECT_EQ(receivedPayload, "relay-test");
}

TEST_F(WsSigClientTest, Send_Bidirectional)
{
    auto ca = makeClient();
    auto cb = makeClient();

    std::string fromB, fromA;
    std::latch  done{2};

    std::shared_ptr<ISigUser> ua, ub;

    ca.Join(
        PeerId{"peer-a"},
        [&](const SigMessage& msg) { fromB = msg.payload; done.count_down(); },
        [&](SigJoinResult r) { 
            if (r.has_value()) { 
                ua = *r; 
            }
        });

    cb.Join(
        PeerId{"peer-b"},
        [&](const SigMessage& msg) { fromA = msg.payload; done.count_down(); },
        [&](SigJoinResult r) { 
            if (r.has_value()) { 
                ub = *r; 
            }
        });

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

TEST_F(WsSigClientTest, UserLeave_ClosesWebSocket)
{
    auto client = makeClient();

    std::latch  connected{1};
    std::shared_ptr<ISigUser> user;

    client.Join(
        PeerId{"leaver"},
        [](const SigMessage&) {},
        [&](SigJoinResult r) {
            if (r.has_value()) user = *r;
            connected.count_down();
        });

    ASSERT_TRUE(await(connected));
    ASSERT_NE(user, nullptr);

    // Dropping the user should close the socket without crashing.
    user.reset();

    // Give time for close event to propagate.
    std::this_thread::sleep_for(100ms);
    // If we reach here without a crash, the test passes.
    SUCCEED();
}
