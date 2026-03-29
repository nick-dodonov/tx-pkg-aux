#include "Rtt/Rtc/LocalSigClient.h"
#include "Rtt/Rtc/SigHub.h"

#include <gtest/gtest.h>

#include <atomic>
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

    alice->Join(
        PeerId{"alice"},
        [](const SigMessage&) {},
        [&](SigJoinResult r) {
            ASSERT_TRUE(r.has_value());
            joinedUser = *r;
        });

    ASSERT_NE(joinedUser, nullptr);
    EXPECT_EQ(joinedUser->LocalId().value, "alice");
}

TEST_F(SignalFixture, Send_DeliveredToTarget)
{
    std::string received;
    std::latch  done{1};

    std::shared_ptr<ISigUser> bobUser;
    bob->Join(
        PeerId{"bob"},
        [&](const SigMessage& msg) {
            received = msg.payload;
            done.count_down();
        },
        [&](SigJoinResult r) { bobUser = *r; });

    std::shared_ptr<ISigUser> aliceUser;
    alice->Join(
        PeerId{"alice"},
        [](const SigMessage&) {},
        [&](SigJoinResult r) { aliceUser = *r; });

    aliceUser->Send(PeerId{"bob"}, "hello");

    ASSERT_TRUE(await(done));
    EXPECT_EQ(received, "hello");
}

TEST_F(SignalFixture, Send_Bidirectional)
{
    std::string aliceGot, bobGot;
    std::latch  done{2};

    std::shared_ptr<ISigUser> aliceUser, bobUser;

    alice->Join(
        PeerId{"alice"},
        [&](const SigMessage& msg) { aliceGot = msg.payload; done.count_down(); },
        [&](SigJoinResult r) { aliceUser = *r; });

    bob->Join(
        PeerId{"bob"},
        [&](const SigMessage& msg) { bobGot = msg.payload; done.count_down(); },
        [&](SigJoinResult r) { bobUser = *r; });

    aliceUser->Send(PeerId{"bob"},   "ping");
    bobUser->Send(PeerId{"alice"}, "pong");

    ASSERT_TRUE(await(done));
    EXPECT_EQ(bobGot,   "ping");
    EXPECT_EQ(aliceGot, "pong");
}

TEST_F(SignalFixture, UserLeave_SendToLeaverIsIgnored)
{
    std::latch  arrived{1};

    std::shared_ptr<ISigUser> aliceUser, bobUser;

    alice->Join(
        PeerId{"alice"},
        [&](const SigMessage&) { arrived.count_down(); }, // should not be called
        [&](SigJoinResult r) { aliceUser = *r; });

    bob->Join(
        PeerId{"bob"},
        [](const SigMessage&) {},
        [&](SigJoinResult r) { bobUser = *r; });

    // Alice leaves.
    aliceUser.reset();

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

    alice->Join(
        PeerId{"alice"},
        [&](const SigMessage&) { count.fetch_add(1); done.count_down(); },
        [&](SigJoinResult r) { aliceUser = *r; });

    bob->Join(
        PeerId{"bob"},
        [](const SigMessage&) {},
        [&](SigJoinResult r) { bobUser = *r; });

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
