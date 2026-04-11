#include "SynTm/Clock.h"
#include "SynTm/Consensus.h"
#include "SynTm/Epoch.h"
#include "SynTm/SessionConfig.h"
#include "SynTm/Types.h"

#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <vector>

using namespace SynTm;

// ===========================================================================
// Helper: simulate a probe round between two Consensus instances
// ===========================================================================

namespace
{
    void SimulateConsensusProbeRound(
        Consensus& nodeA, FakeClock& clockA, const std::string& peerIdOnA,
        Consensus& nodeB, FakeClock& clockB, const std::string& peerIdOnB,
        Ticks oneWayDelay)
    {
        // A probes B.
        auto reqOpt = nodeA.MakeProbeRequest(peerIdOnA);
        ASSERT_TRUE(reqOpt.has_value());

        clockA.Advance(oneWayDelay);
        clockB.Advance(oneWayDelay);

        auto respOpt = nodeB.HandleProbeRequest(peerIdOnB, *reqOpt);
        ASSERT_TRUE(respOpt.has_value());

        clockA.Advance(oneWayDelay);
        clockB.Advance(oneWayDelay);

        nodeA.HandleProbeResponse(peerIdOnA, *respOpt, nodeB.OurEpochInfo());
    }
}

// ===========================================================================
// Epoch
// ===========================================================================

TEST(Epoch, DefaultIsInvalid)
{
    SyncEpoch epoch;
    EXPECT_FALSE(epoch.IsValid());
}

TEST(Epoch, ValidAfterInit)
{
    SyncEpoch epoch{.id = 42, .baseTime = 1000, .createdAt = 1000};
    EXPECT_TRUE(epoch.IsValid());
}

TEST(Epoch, OlderIsStronger)
{
    SyncEpoch older{.id = 1, .createdAt = 100};
    SyncEpoch newer{.id = 2, .createdAt = 200};
    EXPECT_TRUE(older.IsStrongerThan(newer));
    EXPECT_FALSE(newer.IsStrongerThan(older));
}

TEST(Epoch, LargerGroupWinsOnTie)
{
    SyncEpoch small{.id = 1, .memberCount = 2, .createdAt = 100};
    SyncEpoch large{.id = 2, .memberCount = 5, .createdAt = 100};
    EXPECT_TRUE(large.IsStrongerThan(small));
    EXPECT_FALSE(small.IsStrongerThan(large));
}

TEST(Epoch, LowerIdWinsOnDoubleTie)
{
    SyncEpoch a{.id = 1, .memberCount = 3, .createdAt = 100};
    SyncEpoch b{.id = 2, .memberCount = 3, .createdAt = 100};
    EXPECT_TRUE(a.IsStrongerThan(b));
    EXPECT_FALSE(b.IsStrongerThan(a));
}

// ===========================================================================
// Consensus — peer management
// ===========================================================================

TEST(Consensus, AddRemovePeer)
{
    FakeClock clock;
    Consensus consensus(clock);

    EXPECT_TRUE(consensus.AddPeer("node-1"));
    EXPECT_FALSE(consensus.AddPeer("node-1")); // Duplicate.
    EXPECT_EQ(consensus.PeerCount(), 1u);

    EXPECT_TRUE(consensus.RemovePeer("node-1"));
    EXPECT_FALSE(consensus.RemovePeer("node-1")); // Already removed.
    EXPECT_EQ(consensus.PeerCount(), 0u);
}

TEST(Consensus, CreatesEpochOnFirstPeer)
{
    FakeClock clock;
    clock.SetNow(1'000'000'000LL);
    Consensus consensus(clock);

    EXPECT_FALSE(consensus.Epoch().IsValid());
    consensus.AddPeer("node-1");
    EXPECT_TRUE(consensus.Epoch().IsValid());
}

// ===========================================================================
// Consensus — two-node convergence
// ===========================================================================

TEST(Consensus, TwoNodeConvergence)
{
    FakeClock clockA;
    FakeClock clockB;
    clockA.SetNow(1'000'000'000LL);
    clockB.SetNow(1'000'000'000LL);

    SessionConfig config;
    config.minSamplesForSync = 3;
    config.filterWindowSize  = 4;

    Consensus nodeA(clockA, ConsensusMode::Voter, config);
    Consensus nodeB(clockB, ConsensusMode::Voter, config);

    std::vector<SyncEvent> eventsA;
    std::vector<SyncEvent> eventsB;
    nodeA.OnEvent([&](SyncEvent e) { eventsA.push_back(e); });
    nodeB.OnEvent([&](SyncEvent e) { eventsB.push_back(e); });

    nodeA.AddPeer("B");
    nodeB.AddPeer("A");

    constexpr Ticks delay = 5'000'000;

    for (int i = 0; i < 6; ++i) {
        // A probes B.
        SimulateConsensusProbeRound(nodeA, clockA, "B", nodeB, clockB, "A", delay);
        clockA.Advance(100'000'000);
        clockB.Advance(100'000'000);

        // B probes A.
        SimulateConsensusProbeRound(nodeB, clockB, "A", nodeA, clockA, "B", delay); //NOLINT(readability-suspicious-call-argument)
        clockA.Advance(100'000'000);
        clockB.Advance(100'000'000);
    }

    EXPECT_TRUE(nodeA.IsSynced());
    EXPECT_TRUE(nodeB.IsSynced());
    EXPECT_EQ(nodeA.Quality(), SyncQuality::High);
}

// ===========================================================================
// Consensus — three-node chain (A↔B↔C)
// ===========================================================================

TEST(Consensus, ThreeNodeChainPropagation)
{
    FakeClock clockA, clockB, clockC;
    clockA.SetNow(1'000'000'000LL);
    clockB.SetNow(1'000'000'000LL);
    clockC.SetNow(1'000'000'000LL);

    SessionConfig config;
    config.minSamplesForSync = 3;
    config.filterWindowSize  = 4;

    Consensus nodeA(clockA, ConsensusMode::Voter, config);
    Consensus nodeB(clockB, ConsensusMode::Voter, config);
    Consensus nodeC(clockC, ConsensusMode::Voter, config);

    // A connects to B, B connects to C. A and C are not directly connected.
    nodeA.AddPeer("B");
    nodeB.AddPeer("A");
    nodeB.AddPeer("C");
    nodeC.AddPeer("B");

    constexpr Ticks delay = 5'000'000;

    for (int i = 0; i < 8; ++i) {
        // A ↔ B probes.
        SimulateConsensusProbeRound(nodeA, clockA, "B", nodeB, clockB, "A", delay);
        clockA.Advance(50'000'000);
        clockB.Advance(50'000'000);
        clockC.Advance(50'000'000);

        // B ↔ C probes.
        SimulateConsensusProbeRound(nodeB, clockB, "C", nodeC, clockC, "B", delay); //NOLINT(readability-suspicious-call-argument)
        clockA.Advance(50'000'000);
        clockB.Advance(50'000'000);
        clockC.Advance(50'000'000);

        // B ↔ A probes (reverse).
        SimulateConsensusProbeRound(nodeB, clockB, "A", nodeA, clockA, "B", delay); //NOLINT(readability-suspicious-call-argument)
        clockA.Advance(50'000'000);
        clockB.Advance(50'000'000);
        clockC.Advance(50'000'000);

        // C ↔ B probes (reverse).
        SimulateConsensusProbeRound(nodeC, clockC, "B", nodeB, clockB, "A", delay);
        clockA.Advance(50'000'000);
        clockB.Advance(50'000'000);
        clockC.Advance(50'000'000);
    }

    // All three should converge to the same epoch.
    EXPECT_EQ(nodeA.Epoch().id, nodeB.Epoch().id);
    EXPECT_EQ(nodeB.Epoch().id, nodeC.Epoch().id);
}

// ===========================================================================
// Consensus — group merge
// ===========================================================================

TEST(Consensus, GroupMergeAdoptsStrongerEpoch)
{
    FakeClock clockA, clockB;
    clockA.SetNow(100'000'000LL);  // A created earlier → stronger.
    clockB.SetNow(200'000'000LL);

    Consensus nodeA(clockA);
    Consensus nodeB(clockB);

    // Each creates its own epoch.
    nodeA.AddPeer("X"); // Dummy to trigger epoch creation.
    nodeB.AddPeer("Y");

    auto epochAId = nodeA.Epoch().id;
    auto epochBId = nodeB.Epoch().id;
    EXPECT_NE(epochAId, epochBId);

    std::vector<SyncEvent> eventsB;
    nodeB.OnEvent([&](SyncEvent e) { eventsB.push_back(e); });

    // Now B receives A's epoch info — A's epoch is older/stronger.
    nodeB.HandleRemoteEpoch(nodeA.OurEpochInfo());

    // B should have adopted A's epoch.
    EXPECT_EQ(nodeB.Epoch().id, epochAId);

    // B should have emitted EpochChanged.
    ASSERT_FALSE(eventsB.empty());
    EXPECT_EQ(eventsB.back(), SyncEvent::EpochChanged);
}

// ===========================================================================
// Consensus — viewer mode
// ===========================================================================

TEST(Consensus, ViewerAdoptsAnyEpoch)
{
    FakeClock clockA, clockV;
    clockA.SetNow(1'000'000'000LL);
    clockV.SetNow(2'000'000'000LL); // Viewer created later — would normally be weaker.

    Consensus nodeA(clockA, ConsensusMode::Voter);
    Consensus viewer(clockV, ConsensusMode::Viewer);

    nodeA.AddPeer("V");
    viewer.AddPeer("A");

    // Even though viewer was created later, as a viewer it should adopt A's epoch.
    viewer.HandleRemoteEpoch(nodeA.OurEpochInfo());
    EXPECT_EQ(viewer.Epoch().id, nodeA.Epoch().id);
    EXPECT_EQ(viewer.Mode(), ConsensusMode::Viewer);
}

// ===========================================================================
// Consensus — SyncLost on all peers removed
// ===========================================================================

TEST(Consensus, EmitsSyncLostOnLastPeerRemoved)
{
    FakeClock clockA, clockB;
    clockA.SetNow(1'000'000'000LL);
    clockB.SetNow(1'000'000'000LL);

    SessionConfig config;
    config.minSamplesForSync = 2;
    config.filterWindowSize  = 4;

    Consensus nodeA(clockA, ConsensusMode::Voter, config);
    Consensus nodeB(clockB, ConsensusMode::Voter, config);

    std::vector<SyncEvent> events;
    nodeA.OnEvent([&](SyncEvent e) { events.push_back(e); });

    nodeA.AddPeer("B");
    nodeB.AddPeer("A");

    constexpr Ticks delay = 2'000'000;

    // Converge.
    for (int i = 0; i < 5; ++i) {
        SimulateConsensusProbeRound(nodeA, clockA, "B", nodeB, clockB, "A", delay);
        clockA.Advance(100'000'000);
        clockB.Advance(100'000'000);
    }

    EXPECT_TRUE(nodeA.IsSynced());

    // Remove the only peer.
    nodeA.RemovePeer("B");

    // Should have emitted SyncLost.
    ASSERT_FALSE(events.empty());
    EXPECT_EQ(events.back(), SyncEvent::SyncLost);
    EXPECT_FALSE(nodeA.IsSynced());
}
