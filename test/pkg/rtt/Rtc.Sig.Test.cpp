#include "Rtt/Rtc/LocalSigClient.h"
#include "Rtt/Rtc/SigHub.h"

#include <gtest/gtest.h>

#include <atomic>
#include <functional>
#include <latch>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace Rtt;
using namespace Rtt::Rtc;
using namespace std::chrono_literals;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Block until `latch` reaches zero or a 3-second timeout.
static bool await(std::latch& latch)
{
    return latch.try_wait() || [&] {
        for (int i = 0; i < 300; ++i) {
            std::this_thread::sleep_for(10ms);
            if (latch.try_wait()) {
                return true;
            }
        }
        return false;
    }();
}

// Minimal ISigHandler backed by lambdas.
struct FuncHandler : ISigHandler
{
    explicit FuncHandler(std::function<void(SigMessage&&)> onMsg,
                         std::function<void(std::error_code)> onLeft = {})
        : _onMessage(std::move(onMsg)), _onLeft(std::move(onLeft)) {}
    void OnMessage(SigMessage&& msg) override { if (_onMessage) { _onMessage(std::move(msg)); } }
    void OnLeft(std::error_code ec) override   { if (_onLeft)    { _onLeft(ec); } }
private:
    std::function<void(SigMessage&&)> _onMessage;
    std::function<void(std::error_code)> _onLeft;
};

// Build a SigJoinHandler from separate callbacks.
// handlerOut keeps the ISigHandler alive for the connection lifetime.
static SigJoinHandler makeJoinHandler(
    std::shared_ptr<ISigHandler>& handlerOut,
    std::shared_ptr<ISigUser>* userOut,
    std::function<void(SigMessage&&)> onMessage,
    std::function<void(bool)> onJoined = {})
{
    auto h = std::make_shared<FuncHandler>(std::move(onMessage));
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

struct SignalFixture : testing::Test
{
    std::shared_ptr<SigHub> hub = std::make_shared<SigHub>();
    std::shared_ptr<LocalSigClient> alice = std::make_shared<LocalSigClient>(hub);
    std::shared_ptr<LocalSigClient> bob = std::make_shared<LocalSigClient>(hub);
};

// ---------------------------------------------------------------------------
// Static interface checks
// ---------------------------------------------------------------------------

static_assert(SigClientLike<LocalSigClient>);

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_F(SignalFixture, Join_CallsHandlerWithUser)
{
    std::shared_ptr<ISigUser> joinedUser;
    std::shared_ptr<ISigHandler> handler;

    alice->Join(
        PeerId{"alice"},
        makeJoinHandler(handler, &joinedUser, [](SigMessage&&) {}));

    ASSERT_NE(joinedUser, nullptr);
    EXPECT_EQ(joinedUser->LocalId().value, "alice");
}

TEST_F(SignalFixture, Send_DeliveredToTarget)
{
    std::string received;
    std::latch  done{1};

    std::shared_ptr<ISigUser> aliceUser, bobUser;
    std::shared_ptr<ISigHandler> aliceHandler, bobHandler;

    bob->Join(
        PeerId{"bob"},
        makeJoinHandler(bobHandler, &bobUser, [&](SigMessage&& msg) {
            received = msg.payload;
            done.count_down();
        }));

    alice->Join(
        PeerId{"alice"},
        makeJoinHandler(aliceHandler, &aliceUser, [](SigMessage&&) {}));

    aliceUser->Send(PeerId{"bob"}, "hello");

    ASSERT_TRUE(await(done));
    EXPECT_EQ(received, "hello");
}

TEST_F(SignalFixture, Send_Bidirectional)
{
    std::string aliceGot, bobGot;
    std::latch  done{2};

    std::shared_ptr<ISigUser> aliceUser, bobUser;
    std::shared_ptr<ISigHandler> aliceHandler, bobHandler;

    alice->Join(
        PeerId{"alice"},
        makeJoinHandler(aliceHandler, &aliceUser, [&](SigMessage&& msg) {
            aliceGot = msg.payload;
            done.count_down();
        }));

    bob->Join(
        PeerId{"bob"},
        makeJoinHandler(bobHandler, &bobUser, [&](SigMessage&& msg) {
            bobGot = msg.payload;
            done.count_down();
        }));

    aliceUser->Send(PeerId{"bob"},   "ping");
    bobUser->Send(PeerId{"alice"}, "pong");

    ASSERT_TRUE(await(done));
    EXPECT_EQ(bobGot,   "ping");
    EXPECT_EQ(aliceGot, "pong");
}

TEST_F(SignalFixture, UserLeave_SendToLeaverIsIgnored)
{
    std::latch arrived{1};

    std::shared_ptr<ISigUser> aliceUser, bobUser;
    std::shared_ptr<ISigHandler> aliceHandler, bobHandler;

    alice->Join(
        PeerId{"alice"},
        makeJoinHandler(aliceHandler, &aliceUser, [&](SigMessage&&) {
            arrived.count_down(); // should not be called
        }));

    bob->Join(
        PeerId{"bob"},
        makeJoinHandler(bobHandler, &bobUser, [](SigMessage&&) {}));

    // Alice leaves — drop the handle so the hub sees no live handler.
    aliceUser.reset();
    aliceHandler.reset();

    // Bob tries to reach alice — should silently drop.
    bobUser->Send(PeerId{"alice"}, "anyone home?");

    // Give a moment for any spurious delivery.
    std::this_thread::sleep_for(50ms);
    EXPECT_FALSE(arrived.try_wait());
}

TEST_F(SignalFixture, ConcurrentSend_ThreadSafe)
{
    constexpr int kMessages = 100;
    std::atomic<int> count{0};
    std::latch done{kMessages};

    std::shared_ptr<ISigUser> aliceUser, bobUser;
    std::shared_ptr<ISigHandler> aliceHandler, bobHandler;

    alice->Join(
        PeerId{"alice"},
        makeJoinHandler(aliceHandler, &aliceUser, [&](SigMessage&&) {
            count.fetch_add(1);
            done.count_down();
        }));

    bob->Join(
        PeerId{"bob"},
        makeJoinHandler(bobHandler, &bobUser, [](SigMessage&&) {}));

    std::vector<std::jthread> senders;
    senders.reserve(kMessages);
    for (int i = 0; i < kMessages; ++i) {
        senders.emplace_back([&, i] {
            bobUser->Send(PeerId{"alice"}, "msg-" + std::to_string(i));
        });
    }
    senders.clear(); // join all

    ASSERT_TRUE(await(done));
    EXPECT_EQ(count.load(), kMessages);
}


// ---------------------------------------------------------------------------
// Helpers
