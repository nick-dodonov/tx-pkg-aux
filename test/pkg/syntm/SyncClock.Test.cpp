#include "SynTm/Clock.h"
#include "SynTm/Consensus.h"
#include "SynTm/Integrate.h"
#include "SynTm/Probe.h"
#include "SynTm/SessionConfig.h"
#include "SynTm/SyncClock.h"
#include "SynTm/TruncTime.h"
#include "SynTm/Types.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <gtest/gtest.h>
#include <vector>

using namespace SynTm;

// ===========================================================================
// Helper
// ===========================================================================

namespace
{
    void SimulateRound(
        Consensus& nodeA, FakeClock& clockA, const std::string& peerIdOnA,
        Consensus& nodeB, FakeClock& clockB, const std::string& peerIdOnB,
        Ticks delay)
    {
        auto req = nodeA.MakeProbeRequest(peerIdOnA);
        ASSERT_TRUE(req.has_value());
        clockA.Advance(delay);
        clockB.Advance(delay);
        auto resp = nodeB.HandleProbeRequest(peerIdOnB, *req);
        ASSERT_TRUE(resp.has_value());
        clockA.Advance(delay);
        clockB.Advance(delay);
        nodeA.HandleProbeResponse(peerIdOnA, *resp, nodeB.OurEpochInfo());
    }
}

// ===========================================================================
// SyncClock — basic API
// ===========================================================================

TEST(SyncClock, ReturnsZeroBeforeUpdate)
{
    FakeClock clock;
    Consensus consensus(clock);
    SyncClock syncClock(consensus);

    EXPECT_EQ(syncClock.NowNanos(), 0);
}

TEST(SyncClock, UpdateRefreshesCache)
{
    FakeClock clock;
    clock.SetNow(42'000'000'000LL);
    Consensus consensus(clock);
    SyncClock syncClock(consensus);

    syncClock.Update();
    // Before sync, consensus returns local time.
    EXPECT_EQ(syncClock.NowNanos(), 42'000'000'000LL);
}

TEST(SyncClock, NowReturnsChronoTimePoint)
{
    FakeClock clock;
    clock.SetNow(1'000'000'000LL);
    Consensus consensus(clock);
    SyncClock syncClock(consensus);

    syncClock.Update();
    auto tp = syncClock.Now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                  tp.time_since_epoch())
                  .count();
    EXPECT_EQ(ns, 1'000'000'000LL);
}

TEST(SyncClock, IsSyncedDelegatesToConsensus)
{
    FakeClock clock;
    Consensus consensus(clock);
    SyncClock syncClock(consensus);
    EXPECT_FALSE(syncClock.IsSynced());
}

TEST(SyncClock, QualityDelegatesToConsensus)
{
    FakeClock clock;
    Consensus consensus(clock);
    SyncClock syncClock(consensus);
    EXPECT_EQ(syncClock.Quality(), SyncQuality::None);
}

// ===========================================================================
// SyncClock — TruncTime round-trip
// ===========================================================================

TEST(SyncClock, TruncTimeRoundTrip)
{
    FakeClock clockA, clockB;
    clockA.SetNow(1'000'000'000LL);
    clockB.SetNow(1'000'000'000LL);

    SessionConfig config;
    config.minSamplesForSync = 2;
    config.filterWindowSize  = 4;

    Consensus nodeA(clockA, ConsensusMode::Voter, config);
    Consensus nodeB(clockB, ConsensusMode::Voter, config);
    nodeA.AddPeer("B");
    nodeB.AddPeer("A");

    constexpr Ticks delay = 2'000'000;
    for (int i = 0; i < 4; ++i) {
        SimulateRound(nodeA, clockA, "B", nodeB, clockB, "A", delay);
        clockA.Advance(100'000'000);
        clockB.Advance(100'000'000);
    }

    SyncClock clockSyncA(nodeA);
    SyncClock clockSyncB(nodeB);
    clockSyncA.Update();
    clockSyncB.Update();

    // Truncate on A's side.
    auto trunc = clockSyncA.Truncate<TruncTime16_1ms>();

    // Expand on B's side — should give approximately the same time.
    Ticks expandedB = clockSyncB.Expand(trunc);
    Ticks originalA = clockSyncA.NowNanos();

    Ticks diff = expandedB - originalA;
    if (diff < 0) {
        diff = -diff;
    }
    // Within 1 quantum (1ms) + sync tolerance.
    EXPECT_LE(diff, 5'000'000); // 5ms tolerance.
}

// ===========================================================================
// SyncClock — events
// ===========================================================================

TEST(SyncClock, EventCallback)
{
    FakeClock clock;
    clock.SetNow(1'000'000'000LL); // Set after foreign epoch's creation time.
    Consensus consensus(clock);
    SyncClock syncClock(consensus);

    std::vector<SyncEvent> events;
    syncClock.OnEvent([&](SyncEvent e) { events.push_back(e); });

    consensus.AddPeer("remote");
    // Inject a foreign epoch that is stronger (older creation time).
    EpochInfo foreign{
        .epochId     = 999,
        .baseTime    = 50'000'000,
        .createdAt   = 50'000'000, // Created earlier → stronger.
        .memberCount = 10,
    };
    consensus.HandleRemoteEpoch(foreign);

    ASSERT_FALSE(events.empty());
    EXPECT_EQ(events.back(), SyncEvent::EpochChanged);
}

// ===========================================================================
// Integrate — serialization round-trip
// ===========================================================================

TEST(Integrate, ProbeRequestRoundTrip)
{
    EpochInfo epoch{
        .epochId     = 0xDEADBEEF,
        .baseTime    = 1'000'000'000LL,
        .createdAt   = 500'000'000LL,
        .memberCount = 5,
    };
    ProbeRequest req{.t1 = 42'000'000'000LL};

    std::array<std::byte, 128> buf{};
    auto written = WriteSyncProbeRequest(buf, epoch, req);
    EXPECT_GT(written, 0u);

    auto parsed = ParseSyncMessage(std::span<const std::byte>(buf.data(), written));
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->type, SyncMessageType::ProbeRequest);
    EXPECT_EQ(parsed->epoch.epochId, epoch.epochId);
    EXPECT_EQ(parsed->epoch.baseTime, epoch.baseTime);
    EXPECT_EQ(parsed->epoch.createdAt, epoch.createdAt);
    EXPECT_EQ(parsed->epoch.memberCount, epoch.memberCount);
    ASSERT_TRUE(parsed->request.has_value());
    EXPECT_EQ(parsed->request->t1, req.t1);
}

TEST(Integrate, ProbeResponseRoundTrip)
{
    EpochInfo epoch{
        .epochId     = 0x12345678,
        .baseTime    = 2'000'000'000LL,
        .createdAt   = 1'000'000'000LL,
        .memberCount = 3,
    };
    ProbeResponse resp{
        .t1 = 11'000'000'000LL,
        .t2 = 22'000'000'000LL,
        .t3 = 33'000'000'000LL,
    };

    std::array<std::byte, 128> buf{};
    auto written = WriteSyncProbeResponse(buf, epoch, resp);
    EXPECT_GT(written, 0u);

    auto parsed = ParseSyncMessage(std::span<const std::byte>(buf.data(), written));
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->type, SyncMessageType::ProbeResponse);
    EXPECT_EQ(parsed->epoch.epochId, epoch.epochId);
    ASSERT_TRUE(parsed->response.has_value());
    EXPECT_EQ(parsed->response->t1, resp.t1);
    EXPECT_EQ(parsed->response->t2, resp.t2);
    EXPECT_EQ(parsed->response->t3, resp.t3);
}

TEST(Integrate, BufferTooSmall)
{
    EpochInfo epoch{};
    ProbeRequest req{};
    std::array<std::byte, 4> buf{};
    EXPECT_EQ(WriteSyncProbeRequest(buf, epoch, req), 0u);
}

TEST(Integrate, InvalidVersion)
{
    std::array<std::byte, 64> buf{};
    buf[0] = std::byte{255}; // Invalid version.
    auto parsed = ParseSyncMessage(buf);
    EXPECT_FALSE(parsed.has_value());
}

TEST(Integrate, TruncatedBuffer)
{
    std::array<std::byte, 10> buf{};
    buf[0] = std::byte{1}; // Valid version.
    auto parsed = ParseSyncMessage(buf);
    EXPECT_FALSE(parsed.has_value()); // Too short.
}
