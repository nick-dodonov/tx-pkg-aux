// Cross-platform integration tests for WsSigClient factory.
//
// Requires an external signaling server running at 127.0.0.1:8000.
// Start it with:
//   node demo/try/datachannel/ws/signaling-node/signaling-server.js
//
// On wasm: compiled with -sASYNCIFY so emscripten_sleep() spins the JS event
// loop while waiting for async WebSocket callbacks.
// On host/droid: std::this_thread::sleep_for() is used instead.

#include "Rtt/Rtc/WsSigClient.h"

#include <gtest/gtest.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#include <chrono>
#include <thread>
#endif

using namespace Rtt;
using namespace Rtt::Rtc;

#ifdef __ANDROID__
// Special alias to your host loopback interface (127.0.0.1 on your development machine) for Android Emulator
//  https://developer.android.com/studio/run/emulator-networking
static constexpr auto Host = "10.0.2.2";
#else
static constexpr auto Host = "localhost";
#endif

// ---------------------------------------------------------------------------
// Platform-portable sleep that yields the event loop on wasm.
// ---------------------------------------------------------------------------

static void platformSleep(int ms)
{
#ifdef __EMSCRIPTEN__
    emscripten_sleep(ms);
#else
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
#endif
}

static bool waitFor(const volatile bool& flag, int timeoutMs = 5000)
{
    for (int elapsed = 0; elapsed < timeoutMs; elapsed += 50) {
        if (flag) {
            return true;
        }
        platformSleep(50);
    }
    return flag;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static WsSigClient::Options serverOptions()
{
    return {
        .host = Host,
        .port = 8000
    };
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST(WsSigClient_Integration, Join_ConnectsToServer)
{
    auto client = WsSigClient::MakeDefault(serverOptions());

    volatile bool done = false;
    std::shared_ptr<ISigUser> joinedUser;

    client->Join(
        PeerId{"integ-join"},
        [](const SigMessage&) {},
        [&](SigJoinResult r) {
            if (r.has_value()) {
                joinedUser = *r;
            }
            done = true;
        });

    ASSERT_TRUE(waitFor(done));
    ASSERT_NE(joinedUser, nullptr);
    EXPECT_EQ(joinedUser->LocalId().value, "integ-join");
}

TEST(WsSigClient_Integration, Join_ServerUnavailable_ReportsError)
{
    auto client = WsSigClient::MakeDefault({.host = "127.0.0.1", .port = 1});

    volatile bool done = false;
    bool errorReported = false;

    client->Join(
        PeerId{"nobody"},
        [](const SigMessage&) {},
        [&](SigJoinResult r) {
            errorReported = !r.has_value();
            done = true;
        });

    ASSERT_TRUE(waitFor(done, 8000));
    EXPECT_TRUE(errorReported);
}

TEST(WsSigClient_Integration, Send_RelayedToPeer)
{
    auto sender   = WsSigClient::MakeDefault(serverOptions());
    auto receiver = WsSigClient::MakeDefault(serverOptions());

    volatile bool receiverReady = false;
    volatile bool done = false;
    std::string receivedPayload;

    std::shared_ptr<ISigUser> receiverUser;
    receiver->Join(
        PeerId{"integ-recv"},
        [&](const SigMessage& msg) {
            receivedPayload = msg.payload;
            done = true;
        },
        [&](SigJoinResult r) {
            if (r.has_value()) {
                receiverUser = *r;
            }
            receiverReady = true;
        });

    ASSERT_TRUE(waitFor(receiverReady));
    ASSERT_NE(receiverUser, nullptr);

    std::shared_ptr<ISigUser> senderUser;
    volatile bool senderReady = false;
    sender->Join(
        PeerId{"integ-send"},
        [](const SigMessage&) {},
        [&](SigJoinResult r) {
            if (r.has_value()) {
                senderUser = *r;
            }
            senderReady = true;
        });

    ASSERT_TRUE(waitFor(senderReady));
    ASSERT_NE(senderUser, nullptr);

    senderUser->Send(PeerId{"integ-recv"}, "integration-relay-test");

    ASSERT_TRUE(waitFor(done));
    EXPECT_EQ(receivedPayload, "integration-relay-test");
}

TEST(WsSigClient_Integration, Send_Bidirectional)
{
    auto ca = WsSigClient::MakeDefault(serverOptions());
    auto cb = WsSigClient::MakeDefault(serverOptions());

    std::string fromB, fromA;
    volatile bool gotA = false, gotB = false;
    std::shared_ptr<ISigUser> ua, ub;
    volatile bool aReady = false, bReady = false;

    ca->Join(
        PeerId{"integ-peer-a"},
        [&](const SigMessage& msg) { fromB = msg.payload; gotB = true; },
        [&](SigJoinResult r) {
            if (r.has_value()) { ua = *r; }
            aReady = true;
        });

    cb->Join(
        PeerId{"integ-peer-b"},
        [&](const SigMessage& msg) { fromA = msg.payload; gotA = true; },
        [&](SigJoinResult r) {
            if (r.has_value()) { ub = *r; }
            bReady = true;
        });

    ASSERT_TRUE(waitFor(aReady));
    ASSERT_TRUE(waitFor(bReady));
    ASSERT_NE(ua, nullptr);
    ASSERT_NE(ub, nullptr);

    ua->Send(PeerId{"integ-peer-b"}, "from-a");
    ub->Send(PeerId{"integ-peer-a"}, "from-b");

    ASSERT_TRUE(waitFor(gotA));
    ASSERT_TRUE(waitFor(gotB));
    EXPECT_EQ(fromA, "from-a");
    EXPECT_EQ(fromB, "from-b");
}
