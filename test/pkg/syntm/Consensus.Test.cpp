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
using namespace std::chrono_literals;

// ===========================================================================
// Helper: simulate a probe round between two Consensus instances
// ===========================================================================

namespace
{
    void SimulateConsensusPulseRound(
        Consensus& nodeA, FakeClock& clockA, const std::string& peerIdOnA,
        Consensus& nodeB, FakeClock& clockB, const std::string& peerIdOnB,
        Ticks oneWayDelay)
    {
        // A sends pulse to B.
        auto pulseOpt = nodeA.MakePulse(peerIdOnA);
        ASSERT_TRUE(pulseOpt.has_value());

        clockA.Advance(oneWayDelay);
        clockB.Advance(oneWayDelay);

        // B processes pulse and returns a reply (Passive role echoes back).
        auto replyOpt = nodeB.HandleSyncPulse(peerIdOnB, *pulseOpt);
        ASSERT_TRUE(replyOpt.has_value());

        clockA.Advance(oneWayDelay);
        clockB.Advance(oneWayDelay);

        // A processes B's reply; pass B's epoch for offset conversion.
        nodeA.HandleSyncPulse(peerIdOnA, *replyOpt, std::nullopt, nodeB.OurEpochInfo());
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
    SyncEpoch epoch{.id = 42, .baseTime = 1us, .createdAt = 1us};
    EXPECT_TRUE(epoch.IsValid());
}

TEST(Epoch, OlderIsStronger)
{
    SyncEpoch older{.id = 1, .createdAt = 100ns};
    SyncEpoch newer{.id = 2, .createdAt = 200ns};
    EXPECT_TRUE(older.IsStrongerThan(newer));
    EXPECT_FALSE(newer.IsStrongerThan(older));
}

TEST(Epoch, LargerGroupWinsOnTie)
{
    SyncEpoch small{.id = 1, .memberCount = 2, .createdAt = 100ns};
    SyncEpoch large{.id = 2, .memberCount = 5, .createdAt = 100ns};
    EXPECT_TRUE(large.IsStrongerThan(small));
    EXPECT_FALSE(small.IsStrongerThan(large));
}

TEST(Epoch, LowerIdWinsOnDoubleTie)
{
    SyncEpoch a{.id = 1, .memberCount = 3, .createdAt = 100ns};
    SyncEpoch b{.id = 2, .memberCount = 3, .createdAt = 100ns};
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
    clock.SetNow(1s);
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
    clockA.SetNow(1s);
    clockB.SetNow(1s);

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

    constexpr Ticks delay = 5ms;

    for (int i = 0; i < 6; ++i) {
        // A probes B.
        SimulateConsensusPulseRound(nodeA, clockA, "B", nodeB, clockB, "A", delay);
        clockA.Advance(100ms);
        clockB.Advance(100ms);

        // B probes A.
        SimulateConsensusPulseRound(nodeB, clockB, "A", nodeA, clockA, "B", delay); //NOLINT(readability-suspicious-call-argument)
        clockA.Advance(100ms);
        clockB.Advance(100ms);
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
    clockA.SetNow(1s);
    clockB.SetNow(1s);
    clockC.SetNow(1s);

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

    constexpr Ticks delay = 5ms;

    for (int i = 0; i < 8; ++i) {
        // A ↔ B probes.
        SimulateConsensusPulseRound(nodeA, clockA, "B", nodeB, clockB, "A", delay);
        clockA.Advance(50ms);
        clockB.Advance(50ms);
        clockC.Advance(50ms);

        // B ↔ C probes.
        SimulateConsensusPulseRound(nodeB, clockB, "C", nodeC, clockC, "B", delay); //NOLINT(readability-suspicious-call-argument)
        clockA.Advance(50ms);
        clockB.Advance(50ms);
        clockC.Advance(50ms);

        // B ↔ A probes (reverse).
        SimulateConsensusPulseRound(nodeB, clockB, "A", nodeA, clockA, "B", delay); //NOLINT(readability-suspicious-call-argument)
        clockA.Advance(50ms);
        clockB.Advance(50ms);
        clockC.Advance(50ms);

        // C ↔ B probes (reverse).
        SimulateConsensusPulseRound(nodeC, clockC, "B", nodeB, clockB, "C", delay);
        clockA.Advance(50ms);
        clockB.Advance(50ms);
        clockC.Advance(50ms);
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
    clockA.SetNow(100ms);  // A created earlier → stronger.
    clockB.SetNow(200ms);

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
    clockA.SetNow(1s);
    clockV.SetNow(2s); // Viewer created later — would normally be weaker.

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
    clockA.SetNow(1s);
    clockB.SetNow(1s);

    SessionConfig config;
    config.minSamplesForSync = 2;
    config.filterWindowSize  = 4;

    Consensus nodeA(clockA, ConsensusMode::Voter, config);
    Consensus nodeB(clockB, ConsensusMode::Voter, config);

    std::vector<SyncEvent> events;
    nodeA.OnEvent([&](SyncEvent e) { events.push_back(e); });

    nodeA.AddPeer("B");
    nodeB.AddPeer("A");

    constexpr Ticks delay = 2ms;

    // Converge.
    for (int i = 0; i < 5; ++i) {
        SimulateConsensusPulseRound(nodeA, clockA, "B", nodeB, clockB, "A", delay);
        clockA.Advance(100ms);
        clockB.Advance(100ms);
    }

    EXPECT_TRUE(nodeA.IsSynced());

    // Remove the only peer.
    nodeA.RemovePeer("B");

    // Should have emitted SyncLost.
    ASSERT_FALSE(events.empty());
    EXPECT_EQ(events.back(), SyncEvent::SyncLost);
    EXPECT_FALSE(nodeA.IsSynced());
}

// ===========================================================================
// H4 — GetSessionDiagnostics delegates to Session::GetDiagnostics
//
// RED until Consensus::GetSessionDiagnostics() is implemented.
// ===========================================================================

TEST(Consensus, GetSessionDiagnosticsDelegatesToSession)
{
    FakeClock clockA;
    FakeClock clockB;
    clockA.SetNow(1s);
    clockB.SetNow(1s + 15ms); // 15ms offset.

    SessionConfig config;
    config.minSamplesForSync = 2;
    config.filterWindowSize = 4;
    config.stepThreshold = 500ms;

    Consensus nodeA(clockA, ConsensusMode::Voter, config);
    Consensus nodeB(clockB, ConsensusMode::Voter, config);

    nodeA.AddPeer("B");
    nodeB.AddPeer("A");

    constexpr Ticks delay = 3ms;

    for (int i = 0; i < 4; ++i) {
        SimulateConsensusPulseRound(nodeA, clockA, "B", nodeB, clockB, "A", delay);
        clockA.Advance(100ms);
        clockB.Advance(100ms);
    }

    // GetSessionDiagnostics should return the same as direct session access.
    auto* session = nodeA.GetSession("B");
    ASSERT_NE(session, nullptr);

    auto diagDirect = session->GetDiagnostics();
    auto diagViaConsensusOpt = nodeA.GetSessionDiagnostics("B");
    ASSERT_TRUE(diagViaConsensusOpt.has_value());
    const auto& diagViaConsensus = *diagViaConsensusOpt;

    EXPECT_EQ(diagDirect.sampleCount, diagViaConsensus.sampleCount);
    EXPECT_EQ(diagDirect.rttMin, diagViaConsensus.rttMin);
    EXPECT_EQ(diagDirect.rttMean, diagViaConsensus.rttMean);
    EXPECT_EQ(diagDirect.offsetMean, diagViaConsensus.offsetMean);
    EXPECT_EQ(diagDirect.stepCount, diagViaConsensus.stepCount);

    // Sanity: sample count should be > 0 and rttMin reasonable.
    EXPECT_GT(diagViaConsensus.sampleCount, 0u);
    EXPECT_GE(diagViaConsensus.rttMin, 5ms);  // 2 * 3ms delay.
    EXPECT_LE(diagViaConsensus.rttMin, 10ms);

    // Return nullopt for unknown peer.
    auto unknown = nodeA.GetSessionDiagnostics("nobody");
    EXPECT_FALSE(unknown.has_value());
}

// ===========================================================================
// H1 — Consensus::HandleSyncPulse passes receivedAt to session (Passive t2)
//
// When B (Passive) calls HandleSyncPulse with an explicit receivedAt, the
// echo_t2 in the reply must equal that timestamp, not the delayed clock time.
// ===========================================================================

TEST(Consensus, HandleSyncPulse_ReceivedAtPassedThrough)
{
    FakeClock clockA;
    FakeClock clockB;
    clockA.SetNow(1s);
    clockB.SetNow(1s);

    Consensus nodeA(clockA);
    Consensus nodeB(clockB);

    nodeA.AddPeer("B");
    nodeB.AddPeer("A");

    // A sends a pulse (Active = "A" < "B").
    auto pulseOpt = nodeA.MakePulse("B");
    ASSERT_TRUE(pulseOpt.has_value());

    // Capture true receive time on B's side before any queue delay.
    Ticks trueT2 = clockB.Now();

    // Simulate 50ms queue delay at B.
    clockB.Advance(50ms);

    // Without receivedAt: _lastReceivedAt = clockB.Now() = trueT2 + 50ms.
    auto replyNoOverride = nodeB.HandleSyncPulse("A", *pulseOpt);
    ASSERT_TRUE(replyNoOverride.has_value());
    // echo_t2 should reflect the delayed clock (biased).
    ASSERT_TRUE(replyNoOverride->echo_t2.has_value());
    EXPECT_GE(*replyNoOverride->echo_t2, trueT2 + 49ms)
        << "Without override, echo_t2 should reflect the delayed processing time";

    // With receivedAt = trueT2: echo_t2 = trueT2, t1 (= t3) >= trueT2 + 50ms.
    auto replyWithOverride = nodeB.HandleSyncPulse("A", *pulseOpt, trueT2);
    ASSERT_TRUE(replyWithOverride.has_value());
    ASSERT_TRUE(replyWithOverride->echo_t2.has_value());
    EXPECT_EQ(*replyWithOverride->echo_t2, trueT2)
        << "With receivedAt, echo_t2 should be the exact receive timestamp";
    EXPECT_GT(replyWithOverride->t1, *replyWithOverride->echo_t2)
        << "t1 (send time) must be after echo_t2 (receive time)";
}

// ===========================================================================
// H1 — Consensus::HandleSyncPulse passes receivedAt to session (Active t4)
//
// When A (Active) calls HandleSyncPulse on a reply with an explicit
// receivedAt, the offset computation uses that time as t4.
// ===========================================================================

TEST(Consensus, HandleSyncPulse_T4ReceivedAtAffectsOffset)
{
    FakeClock clockA;
    FakeClock clockB;
    clockA.SetNow(1s);
    clockB.SetNow(1s + 30ms); // B is 30ms ahead.

    SessionConfig config;
    config.minSamplesForSync = 1;
    config.filterWindowSize = 4;
    config.stepThreshold = 500ms;

    Consensus nodeA(clockA, ConsensusMode::Voter, config);
    Consensus nodeB(clockB, ConsensusMode::Voter, config);

    nodeA.AddPeer("B");
    nodeB.AddPeer("A");

    constexpr Ticks netDelay = 5ms;

    // Round 1: A sends, B replies.
    auto pulse = nodeA.MakePulse("B");
    ASSERT_TRUE(pulse.has_value());

    clockA.Advance(netDelay);
    clockB.Advance(netDelay);

    auto replyOpt = nodeB.HandleSyncPulse("A", *pulse);
    ASSERT_TRUE(replyOpt.has_value());

    clockA.Advance(netDelay);
    clockB.Advance(netDelay);

    // True t4 = clockA.Now() at this point.
    Ticks trueT4 = clockA.Now();

    // Simulate 80ms queue delay — clock advances before processing.
    clockA.Advance(80ms);

    // With trueT4 override: offset should be close to 30ms.
    nodeA.HandleSyncPulse("B", *replyOpt, trueT4, nodeB.OurEpochInfo());

    auto* session = nodeA.GetSession("B");
    ASSERT_NE(session, nullptr);

    // Synced estimate should be close to local + 30ms.
    Ticks synced = session->RemoteNow();
    Ticks local = clockA.Now();
    auto error = std::chrono::abs(synced - (local + 30ms));
    EXPECT_LE(error, 5ms)
        << "With receivedAt, offset estimate should be close to 30ms, got "
        << (synced - local).count() << "ns";
}
