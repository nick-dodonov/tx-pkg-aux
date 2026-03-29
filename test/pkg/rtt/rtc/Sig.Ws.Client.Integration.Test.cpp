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
#include <cerrno>
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
// Tests
// ---------------------------------------------------------------------------

TEST(WsSigClient_Integration, Join_ConnectsToServer)
{
    auto client = WsSigClient::MakeDefault(serverOptions());

    volatile bool done = false;
    std::shared_ptr<ISigUser> joinedUser;
    std::shared_ptr<ISigHandler> handler;

    client->Join(
        PeerId{"integ-join"},
        makeJoinHandler(handler, &joinedUser, 
            [](const SigMessage&) {}, 
            [&](bool) { done = true; }
        ));

    ASSERT_TRUE(waitFor(done));
    ASSERT_NE(joinedUser, nullptr);
    EXPECT_EQ(joinedUser->LocalId().value, "integ-join");
}

TEST(WsSigClient_Integration, Join_ServerUnavailable_ReportsError)
{
    auto client = WsSigClient::MakeDefault({.host = "127.0.0.1", .port = 1});

    volatile bool done = false;
    bool errorReported = false;
    std::shared_ptr<ISigHandler> handler;

    client->Join(
        PeerId{"nobody"},
        makeJoinHandler(handler, nullptr,
            [](const SigMessage&) {},
            [&](bool ok) {
                errorReported = !ok;
                done = true;
            }
        ));

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
    std::shared_ptr<ISigHandler> receiverHandler;
    receiver->Join(
        PeerId{"integ-recv"},
        makeJoinHandler(receiverHandler, &receiverUser, 
            [&](const SigMessage& msg) {
                receivedPayload = msg.payload;
                done = true;
            }, 
            [&](bool) { receiverReady = true; }
        ));

    ASSERT_TRUE(waitFor(receiverReady));
    ASSERT_NE(receiverUser, nullptr);

    std::shared_ptr<ISigUser> senderUser;
    volatile bool senderReady = false;
    std::shared_ptr<ISigHandler> senderHandler;
    sender->Join(
        PeerId{"integ-send"},
        makeJoinHandler(senderHandler, &senderUser, 
            [](const SigMessage&) {}, 
            [&](bool) { senderReady = true; }
        ));

    ASSERT_TRUE(waitFor(senderReady));
    ASSERT_NE(senderUser, nullptr);

    senderUser->Send(PeerId{"integ-recv"}, "integration-relay-test");

    ASSERT_TRUE(waitFor(done));
    EXPECT_EQ(receivedPayload, "integration-relay-test");
}

TEST(WsSigClient_Integration, Leave_CallsOnLeft_Clean)
{
    auto client = WsSigClient::MakeDefault(serverOptions());

    volatile bool connected = false;
    volatile bool leftNotified = false;
    std::error_code leftEc{std::make_error_code(std::errc::interrupted)}; // non-empty sentinel

    std::shared_ptr<ISigUser> user;
    std::shared_ptr<ISigHandler> handler;

    client->Join(
        PeerId{"leave-integ"},
        makeJoinHandler(
            handler, &user,
            [](SigMessage&&) {},
            [&](bool) { connected = true; },
            [&](std::error_code ec) { leftEc = ec; leftNotified = true; }));

    ASSERT_TRUE(waitFor(connected));
    ASSERT_NE(user, nullptr);

    user->Leave();

    ASSERT_TRUE(waitFor(leftNotified));
    EXPECT_FALSE(leftEc) << "expected empty error code for clean Leave(), got: " << leftEc.message();
}

TEST(WsSigClient_Integration, Send_Bidirectional)
{
    auto ca = WsSigClient::MakeDefault(serverOptions());
    auto cb = WsSigClient::MakeDefault(serverOptions());

    std::string fromB, fromA;
    volatile bool gotA = false, gotB = false;
    std::shared_ptr<ISigUser> ua, ub;
    volatile bool aReady = false, bReady = false;

    std::shared_ptr<ISigHandler> ha, hb;
    ca->Join(
        PeerId{"integ-peer-a"},
        makeJoinHandler(ha, &ua, 
            [&](const SigMessage& msg) { 
                fromB = msg.payload;
                gotB = true;
            },
            [&](bool) { aReady = true; }
        ));

    cb->Join(
        PeerId{"integ-peer-b"},
        makeJoinHandler(hb, &ub, 
            [&](const SigMessage& msg) { 
                fromA = msg.payload;
                gotA = true;
            },
            [&](bool) { bReady = true; }
        ));


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
