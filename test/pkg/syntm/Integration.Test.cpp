/// End-to-end integration tests for the SynTm time synchronization subsystem.
///
/// These tests simulate multi-node networks with FakeClocks, asymmetric delays,
/// clock offsets, drift rates, and topology changes — verifying convergence,
/// epoch propagation, group merge, and viewer behavior.

#include "SynTm/Clock.h"
#include "SynTm/Consensus.h"
#include "SynTm/Integrate.h"
#include "SynTm/SessionConfig.h"
#include "SynTm/SyncClock.h"
#include "SynTm/TruncTime.h"
#include "SynTm/Types.h"

#include <array>
#include <cstddef>
#include <gtest/gtest.h>
#include <string>
#include <utility>
#include <vector>

using namespace SynTm;
using namespace std::chrono_literals;

// ===========================================================================
// Test harness: simulated node graph
// ===========================================================================

namespace
{
    struct SimNode
    {
        std::string id;
        FakeClock clock;
        Consensus consensus;
        SyncClock syncClock;
        std::vector<SyncEvent> events;

        /// Drift rate applied per AdvanceAll. 1.0 means perfect clock.
        double driftFactor = 1.0;

        SimNode(std::string nodeId, Ticks startTime,
                ConsensusMode mode, SessionConfig config, double drift = 1.0)
            : id(std::move(nodeId))
            , consensus(clock, mode, config)
            , syncClock(consensus)
            , driftFactor(drift)
        {
            clock.SetNow(startTime);
            consensus.OnEvent([this](SyncEvent e) { events.push_back(e); });
        }
    };

    struct SimLink
    {
        std::string nodeA;
        std::string nodeB;
        Ticks delayAtoB;
        Ticks delayBtoA;
    };

    /// Helper: simulate a bidirectional probe round between two SimNodes.
    void ProbeRound(SimNode& a, const std::string& peerOnA,
                    SimNode& b, const std::string& peerOnB,
                    Ticks delayAtoB, Ticks delayBtoA)
    {
        // A → B.
        auto req = a.consensus.MakeProbeRequest(peerOnA);
        if (!req) {
            return;
        }
        a.clock.Advance(delayAtoB);
        b.clock.Advance(delayAtoB);

        auto resp = b.consensus.HandleProbeRequest(peerOnB, *req);
        if (!resp) {
            return;
        }
        a.clock.Advance(delayBtoA);
        b.clock.Advance(delayBtoA);

        a.consensus.HandleProbeResponse(peerOnA, *resp, b.consensus.OurEpochInfo());
    }

    /// Advance all node clocks, applying per-node drift factor.
    void AdvanceAll(std::vector<SimNode*>& nodes, Ticks dt)
    {
        for (auto* n : nodes) {
            auto actual = Ticks{static_cast<std::int64_t>(
                static_cast<double>(dt.count()) * n->driftFactor)};
            n->clock.Advance(actual);
        }
    }

    /// Default fast-converging session config for integration tests.
    SessionConfig FastConfig()
    {
        SessionConfig c;
        c.minSamplesForSync = 2;
        c.filterWindowSize  = 4;
        c.probeIntervalMin  = 10ms;
        c.probeIntervalMax  = 50ms;
        return c;
    }
}

// ===========================================================================
// Two-node convergence with clock offset
// ===========================================================================

TEST(Integration, TwoNodeOffset)
{
    auto config = FastConfig();

    // Node A starts at 1s, node B starts with +50ms offset.
    SimNode nodeA("A", 1s, ConsensusMode::Voter, config);
    SimNode nodeB("B", 1s + 50ms, ConsensusMode::Voter, config);

    nodeA.consensus.AddPeer("B");
    nodeB.consensus.AddPeer("A");

    constexpr Ticks delay = 5ms; // 5ms symmetric.
    std::vector<SimNode*> all = {&nodeA, &nodeB};

    for (int i = 0; i < 8; ++i) {
        ProbeRound(nodeA, "B", nodeB, "A", delay, delay);
        AdvanceAll(all, 50ms);
        ProbeRound(nodeB, "A", nodeA, "B", delay, delay);
        AdvanceAll(all, 50ms);
    }

    EXPECT_TRUE(nodeA.consensus.IsSynced());
    EXPECT_TRUE(nodeB.consensus.IsSynced());

    // Each node converges to its peer's timeline:
    // A.SyncedNow ≈ B.Now (A corrects toward B)
    // B.SyncedNow ≈ A.Now (B corrects toward A)
    nodeA.syncClock.Update();
    nodeB.syncClock.Update();

    // Verify A's synced time is close to B's local time.
    Ticks aSynced = nodeA.syncClock.NowNanos();
    Ticks bLocal  = nodeB.clock.Now();
    auto diffA = std::chrono::abs(aSynced - bLocal);
    EXPECT_LE(diffA, 5ms); // Within 5ms of B's local time.

    // Verify B's synced time is close to A's local time.
    Ticks bSynced = nodeB.syncClock.NowNanos();
    Ticks aLocal  = nodeA.clock.Now();
    auto diffB = std::chrono::abs(bSynced - aLocal);
    EXPECT_LE(diffB, 5ms); // Within 5ms of A's local time.
}

// ===========================================================================
// Three-node chain: A ↔ B ↔ C, epoch propagation
// ===========================================================================

TEST(Integration, ThreeNodeChain)
{
    auto config = FastConfig();

    SimNode nodeA("A", 1s, ConsensusMode::Voter, config);
    SimNode nodeB("B", 1s, ConsensusMode::Voter, config);
    SimNode nodeC("C", 1s, ConsensusMode::Voter, config);

    // A ↔ B, B ↔ C. No direct A ↔ C link.
    nodeA.consensus.AddPeer("B");
    nodeB.consensus.AddPeer("A");
    nodeB.consensus.AddPeer("C");
    nodeC.consensus.AddPeer("B");

    constexpr Ticks delay = 3ms;
    std::vector<SimNode*> all = {&nodeA, &nodeB, &nodeC};

    for (int i = 0; i < 10; ++i) {
        ProbeRound(nodeA, "B", nodeB, "A", delay, delay);
        AdvanceAll(all, 30ms);
        ProbeRound(nodeB, "C", nodeC, "B", delay, delay);
        AdvanceAll(all, 30ms);
        ProbeRound(nodeB, "A", nodeA, "B", delay, delay);
        AdvanceAll(all, 30ms);
        ProbeRound(nodeC, "B", nodeB, "C", delay, delay);
        AdvanceAll(all, 30ms);
    }

    // All three should share the same epoch.
    EXPECT_EQ(nodeA.consensus.Epoch().id, nodeB.consensus.Epoch().id);
    EXPECT_EQ(nodeB.consensus.Epoch().id, nodeC.consensus.Epoch().id);

    // B should be synced (has two peers).
    EXPECT_TRUE(nodeB.consensus.IsSynced());
}

// ===========================================================================
// Group merge: two pre-synced pairs connect
// ===========================================================================

TEST(Integration, GroupMerge)
{
    auto config = FastConfig();

    // Group 1: A ↔ B, started earlier (stronger epoch).
    SimNode nodeA("A", 100ms, ConsensusMode::Voter, config);
    SimNode nodeB("B", 100ms, ConsensusMode::Voter, config);

    // Group 2: C ↔ D, started later (weaker epoch).
    SimNode nodeC("C", 500ms, ConsensusMode::Voter, config);
    SimNode nodeD("D", 500ms, ConsensusMode::Voter, config);

    nodeA.consensus.AddPeer("B");
    nodeB.consensus.AddPeer("A");
    nodeC.consensus.AddPeer("D");
    nodeD.consensus.AddPeer("C");

    constexpr Ticks delay = 2ms;

    // Sync each group internally.
    std::vector<SimNode*> group1 = {&nodeA, &nodeB};
    std::vector<SimNode*> group2 = {&nodeC, &nodeD};

    for (int i = 0; i < 5; ++i) {
        ProbeRound(nodeA, "B", nodeB, "A", delay, delay);
        AdvanceAll(group1, 30ms);
        AdvanceAll(group2, 30ms);
        ProbeRound(nodeB, "A", nodeA, "B", delay, delay);
        AdvanceAll(group1, 30ms);
        AdvanceAll(group2, 30ms);
        ProbeRound(nodeC, "D", nodeD, "C", delay, delay);
        AdvanceAll(group1, 30ms);
        AdvanceAll(group2, 30ms);
        ProbeRound(nodeD, "C", nodeC, "D", delay, delay);
        AdvanceAll(group1, 30ms);
        AdvanceAll(group2, 30ms);
    }

    auto epochGroup1 = nodeA.consensus.Epoch().id;
    auto epochGroup2 = nodeC.consensus.Epoch().id;
    EXPECT_NE(epochGroup1, epochGroup2);

    // Now bridge B ↔ C.
    nodeB.consensus.AddPeer("C");
    nodeC.consensus.AddPeer("B");

    nodeC.events.clear();
    nodeD.events.clear();

    std::vector<SimNode*> all = {&nodeA, &nodeB, &nodeC, &nodeD};

    for (int i = 0; i < 5; ++i) {
        ProbeRound(nodeB, "C", nodeC, "B", delay, delay);
        AdvanceAll(all, 30ms);
        ProbeRound(nodeC, "B", nodeB, "C", delay, delay);
        AdvanceAll(all, 30ms);
        // Continue internal probes.
        ProbeRound(nodeC, "D", nodeD, "C", delay, delay);
        AdvanceAll(all, 30ms);
        ProbeRound(nodeD, "C", nodeC, "D", delay, delay);
        AdvanceAll(all, 30ms);
    }

    // Group 2 should have adopted group 1's epoch (it was created earlier).
    EXPECT_EQ(nodeC.consensus.Epoch().id, epochGroup1);
    EXPECT_EQ(nodeD.consensus.Epoch().id, epochGroup1);

    // C or D should have received an EpochChanged event.
    auto hasEpochChanged = [](const std::vector<SyncEvent>& evts) {
        for (auto e : evts) {
            if (e == SyncEvent::EpochChanged) {
                return true;
            }
        }
        return false;
    };
    EXPECT_TRUE(hasEpochChanged(nodeC.events) || hasEpochChanged(nodeD.events));
}

// ===========================================================================
// Viewer mode: viewer tracks voter without voting
// ===========================================================================

TEST(Integration, ViewerTracksVoter)
{
    auto config = FastConfig();

    SimNode voter("V", 1s, ConsensusMode::Voter, config);
    SimNode viewer("W", 2s, ConsensusMode::Viewer, config);

    voter.consensus.AddPeer("W");
    viewer.consensus.AddPeer("V");

    constexpr Ticks delay = 5ms;
    std::vector<SimNode*> all = {&voter, &viewer};

    for (int i = 0; i < 6; ++i) {
        ProbeRound(viewer, "V", voter, "W", delay, delay);
        AdvanceAll(all, 50ms);
        ProbeRound(voter, "W", viewer, "V", delay, delay);
        AdvanceAll(all, 50ms);
    }

    // Viewer should have adopted the voter's epoch.
    EXPECT_EQ(viewer.consensus.Epoch().id, voter.consensus.Epoch().id);

    // Viewer should be synced.
    EXPECT_TRUE(viewer.consensus.IsSynced());
    EXPECT_EQ(viewer.consensus.Mode(), ConsensusMode::Viewer);
}

// ===========================================================================
// Integrate: full wire round-trip through serialization
// ===========================================================================

TEST(Integration, WireFormatEndToEnd)
{
    auto config = FastConfig();

    SimNode nodeA("A", 1s, ConsensusMode::Voter, config);
    SimNode nodeB("B", 1s, ConsensusMode::Voter, config);

    nodeA.consensus.AddPeer("B");
    nodeB.consensus.AddPeer("A");

    constexpr Ticks delay = 3ms;
    std::vector<SimNode*> all = {&nodeA, &nodeB};

    // Simulate probe exchange using wire-format serialization.
    for (int i = 0; i < 6; ++i) {
        auto req = nodeA.consensus.MakeProbeRequest("B");
        ASSERT_TRUE(req.has_value());

        // Serialize request.
        std::array<std::byte, 128> reqBuf{};
        auto reqBytes = WriteSyncProbeRequest(reqBuf, nodeA.consensus.OurEpochInfo(), *req);
        ASSERT_GT(reqBytes, 0u);

        AdvanceAll(all, delay);

        // Parse request on B's side.
        auto parsedReq = ParseSyncMessage(
            std::span<const std::byte>(reqBuf.data(), reqBytes));
        ASSERT_TRUE(parsedReq.has_value());
        ASSERT_TRUE(parsedReq->request.has_value());

        // Handle epoch from header.
        nodeB.consensus.HandleRemoteEpoch(parsedReq->epoch);

        auto resp = nodeB.consensus.HandleProbeRequest("A", *parsedReq->request);
        ASSERT_TRUE(resp.has_value());

        // Serialize response.
        std::array<std::byte, 128> respBuf{};
        auto respBytes = WriteSyncProbeResponse(
            respBuf, nodeB.consensus.OurEpochInfo(), *resp);
        ASSERT_GT(respBytes, 0u);

        AdvanceAll(all, delay);

        // Parse response on A's side.
        auto parsedResp = ParseSyncMessage(
            std::span<const std::byte>(respBuf.data(), respBytes));
        ASSERT_TRUE(parsedResp.has_value());
        ASSERT_TRUE(parsedResp->response.has_value());

        nodeA.consensus.HandleProbeResponse(
            "B", *parsedResp->response, parsedResp->epoch);

        AdvanceAll(all, 50ms);
    }

    EXPECT_TRUE(nodeA.consensus.IsSynced());
    EXPECT_EQ(nodeA.consensus.Epoch().id, nodeB.consensus.Epoch().id);
}

// ===========================================================================
// Clock drift: two nodes with different drift rates converge
// ===========================================================================

TEST(Integration, DriftCompensation)
{
    auto config = FastConfig();

    // Node A runs at 1x speed. Node B runs at 1.0001x (100 ppm fast).
    SimNode nodeA("A", 1s, ConsensusMode::Voter, config, 1.0);
    SimNode nodeB("B", 1s, ConsensusMode::Voter, config, 1.0001);

    nodeA.consensus.AddPeer("B");
    nodeB.consensus.AddPeer("A");

    constexpr Ticks delay = 3ms;
    std::vector<SimNode*> all = {&nodeA, &nodeB};

    for (int i = 0; i < 12; ++i) {
        ProbeRound(nodeA, "B", nodeB, "A", delay, delay);
        AdvanceAll(all, 100ms);
        ProbeRound(nodeB, "A", nodeA, "B", delay, delay);
        AdvanceAll(all, 100ms);
    }

    EXPECT_TRUE(nodeA.consensus.IsSynced());
    EXPECT_TRUE(nodeB.consensus.IsSynced());

    // After convergence, synced times should be close.
    nodeA.syncClock.Update();
    nodeB.syncClock.Update();

    auto diff = std::chrono::abs(nodeA.syncClock.NowNanos() - nodeB.syncClock.NowNanos());
    // 10ms tolerance — generous, accounting for 100 ppm drift over ~2.4s.
    EXPECT_LE(diff, 10ms);
}

// ===========================================================================
// SyncAcquired event fires on first convergence
// ===========================================================================

TEST(Integration, SyncAcquiredEvent)
{
    auto config = FastConfig();

    SimNode nodeA("A", 1s, ConsensusMode::Voter, config);
    SimNode nodeB("B", 1s, ConsensusMode::Voter, config);

    nodeA.consensus.AddPeer("B");
    nodeB.consensus.AddPeer("A");

    constexpr Ticks delay = 5ms;
    std::vector<SimNode*> all = {&nodeA, &nodeB};

    for (int i = 0; i < 6; ++i) {
        ProbeRound(nodeA, "B", nodeB, "A", delay, delay);
        AdvanceAll(all, 50ms);
        ProbeRound(nodeB, "A", nodeA, "B", delay, delay);
        AdvanceAll(all, 50ms);
    }

    auto hasSyncAcquired = [](const std::vector<SyncEvent>& evts) {
        for (auto e : evts) {
            if (e == SyncEvent::SyncAcquired) {
                return true;
            }
        }
        return false;
    };
    EXPECT_TRUE(hasSyncAcquired(nodeA.events));
}

// ===========================================================================
// TruncTime cross-node round-trip
// ===========================================================================

TEST(Integration, TruncTimeCrossNode)
{
    auto config = FastConfig();

    SimNode nodeA("A", 1s, ConsensusMode::Voter, config);
    SimNode nodeB("B", 1s, ConsensusMode::Voter, config);

    nodeA.consensus.AddPeer("B");
    nodeB.consensus.AddPeer("A");

    constexpr Ticks delay = 2ms;
    std::vector<SimNode*> all = {&nodeA, &nodeB};

    for (int i = 0; i < 6; ++i) {
        ProbeRound(nodeA, "B", nodeB, "A", delay, delay);
        AdvanceAll(all, 50ms);
        ProbeRound(nodeB, "A", nodeA, "B", delay, delay);
        AdvanceAll(all, 50ms);
    }

    nodeA.syncClock.Update();
    nodeB.syncClock.Update();

    // A truncates the current time.
    auto trunc = nodeA.syncClock.Truncate<TruncTime16_1ms>();

    // B expands it.
    Ticks expanded = nodeB.syncClock.Expand(trunc);
    Ticks original = nodeA.syncClock.NowNanos();

    auto diff = std::chrono::abs(expanded - original);
    // Within 5ms tolerance (1 quantum + sync error).
    EXPECT_LE(diff, 5ms);
}

// ===========================================================================
// Two nodes with large initial clock offset (e.g. AppClock scenario)
// ===========================================================================

// Verifies that nodes starting with a multi-second clock difference converge
// correctly and do NOT emit ResyncStarted events after the initial convergence.
TEST(Integration, TwoNodeLargeOffset_ConvergesAndStabilizes)
{
    auto config = FastConfig();

    // A starts at 0, B starts at 20s — simulates two processes each with
    // their own AppClock that was started at different wall-clock moments.
    SimNode nodeA("A", Ticks{}, ConsensusMode::Voter, config);
    SimNode nodeB("B", 20s, ConsensusMode::Voter, config);

    nodeA.consensus.AddPeer("B");
    nodeB.consensus.AddPeer("A");

    constexpr Ticks delay = 5ms; // 5ms symmetric.
    std::vector<SimNode*> all = {&nodeA, &nodeB};

    // Run until both sides are Synced (up to 60 rounds).
    int rounds = 0;
    for (; rounds < 60; ++rounds) {
        ProbeRound(nodeA, "B", nodeB, "A", delay, delay);
        AdvanceAll(all, 100ms);
        ProbeRound(nodeB, "A", nodeA, "B", delay, delay);
        AdvanceAll(all, 100ms);
        if (nodeA.consensus.IsSynced() && nodeB.consensus.IsSynced()) {
            break;
        }
    }

    EXPECT_TRUE(nodeA.consensus.IsSynced()) << "A not synced after " << rounds << " rounds";
    EXPECT_TRUE(nodeB.consensus.IsSynced()) << "B not synced after " << rounds << " rounds";

    // Clear event history and run 20 more rounds — no ResyncStarted should fire.
    nodeA.events.clear();
    nodeB.events.clear();

    for (int i = 0; i < 20; ++i) {
        ProbeRound(nodeA, "B", nodeB, "A", delay, delay);
        AdvanceAll(all, 100ms);
        ProbeRound(nodeB, "A", nodeA, "B", delay, delay);
        AdvanceAll(all, 100ms);
    }

    auto hasResyncStarted = [](const std::vector<SyncEvent>& evts) {
        for (auto e : evts) {
            if (e == SyncEvent::ResyncStarted) {
                return true;
            }
        }
        return false;
    };

    EXPECT_FALSE(hasResyncStarted(nodeA.events)) << "A emitted spurious ResyncStarted after stable sync";
    EXPECT_FALSE(hasResyncStarted(nodeB.events)) << "B emitted spurious ResyncStarted after stable sync";
}

// ===========================================================================
// Resynced fires exactly once per resync episode, not per-step
// ===========================================================================

// Verifies that even if the filter produces multiple consecutive step-sized
// corrections, Resynced is emitted only when the session first enters
// Resyncing, not on every subsequent step within the same episode.
TEST(Integration, Resynced_FiresOncePerEpisode)
{
    auto config = FastConfig();
    config.stepThreshold = 50ms; // lower threshold to make steps easier to trigger

    SimNode nodeA("A", 1s, ConsensusMode::Voter, config);
    SimNode nodeB("B", 1s, ConsensusMode::Voter, config);

    nodeA.consensus.AddPeer("B");
    nodeB.consensus.AddPeer("A");

    constexpr Ticks delay = 1ms;
    std::vector<SimNode*> all = {&nodeA, &nodeB};

    // Converge first (bidirectional).
    for (int i = 0; i < 10; ++i) {
        ProbeRound(nodeA, "B", nodeB, "A", delay, delay);
        AdvanceAll(all, 50ms);
        ProbeRound(nodeB, "A", nodeA, "B", delay, delay);
        AdvanceAll(all, 50ms);
    }
    ASSERT_TRUE(nodeA.consensus.IsSynced());
    ASSERT_TRUE(nodeB.consensus.IsSynced());

    nodeA.events.clear();
    nodeB.events.clear();

    // Jump B by 200ms to force A into Resyncing.
    nodeB.clock.Advance(200ms);

    // Run a bunch of probes through the resync episode (bidirectional).
    for (int i = 0; i < 20; ++i) {
        ProbeRound(nodeA, "B", nodeB, "A", delay, delay);
        AdvanceAll(all, 50ms);
        ProbeRound(nodeB, "A", nodeA, "B", delay, delay);
        AdvanceAll(all, 50ms);
    }

    // Count ResyncStarted and ResyncCompleted events on A: each should be exactly 1 per episode.
    int resyncStartedCount = 0;
    int resyncCompletedCount = 0;
    for (auto e : nodeA.events) {
        if (e == SyncEvent::ResyncStarted) {
            ++resyncStartedCount;
        } else if (e == SyncEvent::ResyncCompleted) {
            ++resyncCompletedCount;
        }
    }
    EXPECT_EQ(resyncStartedCount, 1)
        << "ResyncStarted fired " << resyncStartedCount << " times for one resync episode";
    EXPECT_EQ(resyncCompletedCount, 1)
        << "ResyncCompleted fired " << resyncCompletedCount << " times for one resync episode";
}
