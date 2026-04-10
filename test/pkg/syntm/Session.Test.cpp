#include "SynTm/Clock.h"
#include "SynTm/Probe.h"
#include "SynTm/Session.h"
#include "SynTm/SessionConfig.h"
#include "SynTm/Types.h"

#include <cstdint>
#include <gtest/gtest.h>

using namespace SynTm;

// ===========================================================================
// Helper: simulate a probe round-trip between two sessions
// ===========================================================================

namespace
{
    /// Simulate one complete probe exchange:
    ///   initiator sends request → responder handles → initiator handles response.
    /// Both clocks advance by `oneWayDelay` for each leg.
    Session::ProbeHandleResult SimulateProbeRound(
        Session& initiator, FakeClock& initiatorClock,
        Session& responder, FakeClock& responderClock,
        Nanos oneWayDelay)
    {
        // Step 1: Initiator creates request.
        auto req = initiator.MakeProbeRequest();

        // Step 2: Network delay (initiator → responder).
        initiatorClock.Advance(oneWayDelay);
        responderClock.Advance(oneWayDelay);

        // Step 3: Responder handles request.
        auto resp = responder.HandleProbeRequest(req);

        // Step 4: Network delay (responder → initiator).
        initiatorClock.Advance(oneWayDelay);
        responderClock.Advance(oneWayDelay);

        // Step 5: Initiator handles response.
        return initiator.HandleProbeResponse(resp);
    }
}

// ===========================================================================
// Session — basic state transitions
// ===========================================================================

TEST(Session, StartsIdle)
{
    FakeClock clock;
    Session session(clock);
    EXPECT_EQ(session.State(), SessionState::Idle);
    EXPECT_EQ(session.Quality(), SyncQuality::None);
}

TEST(Session, TransitionsToProbing)
{
    FakeClock clock;
    Session session(clock);
    [[maybe_unused]] auto req1 = session.MakeProbeRequest();
    EXPECT_EQ(session.State(), SessionState::Probing);
}

TEST(Session, ShouldProbeInitially)
{
    FakeClock clock;
    Session session(clock);
    EXPECT_TRUE(session.ShouldProbe());
}

TEST(Session, ShouldProbeRespectsInterval)
{
    FakeClock clock;
    SessionConfig config;
    config.probeIntervalMin = 100'000'000; // 100ms
    Session session(clock, config);

    [[maybe_unused]] auto req1 = session.MakeProbeRequest();
    EXPECT_FALSE(session.ShouldProbe()); // Just sent.

    clock.Advance(50'000'000); // 50ms — not yet.
    EXPECT_FALSE(session.ShouldProbe());

    clock.Advance(60'000'000); // 110ms total — should probe.
    EXPECT_TRUE(session.ShouldProbe());
}

// ===========================================================================
// Session — two-node convergence (zero offset)
// ===========================================================================

TEST(Session, ConvergesWithZeroOffset)
{
    FakeClock clockA;
    FakeClock clockB;
    clockA.SetNow(1'000'000'000LL);
    clockB.SetNow(1'000'000'000LL); // Same time.

    SessionConfig config;
    config.minSamplesForSync = 3;
    config.filterWindowSize = 4;

    Session sessionA(clockA, config);
    Session sessionB(clockB, config);

    constexpr Nanos delay = 5'000'000; // 5ms one-way.

    for (int i = 0; i < 5; ++i) {
        SimulateProbeRound(sessionA, clockA, sessionB, clockB, delay);

        // Advance time between probes.
        clockA.Advance(100'000'000);
        clockB.Advance(100'000'000);
    }

    EXPECT_EQ(sessionA.State(), SessionState::Synced);

    // Synced time should be very close to local time (zero offset).
    Nanos synced = sessionA.SyncedNow();
    Nanos local  = clockA.Now();
    Nanos diff   = synced - local;
    if (diff < 0) {
        diff = -diff;
    }
    EXPECT_LE(diff, 1'000'000); // Within 1ms tolerance.
}

// ===========================================================================
// Session — two-node convergence with offset
// ===========================================================================

TEST(Session, ConvergesWithOffset)
{
    FakeClock clockA;
    FakeClock clockB;
    clockA.SetNow(1'000'000'000LL);
    clockB.SetNow(1'050'000'000LL); // B is 50ms ahead.

    SessionConfig config;
    config.minSamplesForSync = 3;
    config.filterWindowSize = 4;

    Session sessionA(clockA, config);
    Session sessionB(clockB, config);

    constexpr Nanos delay = 5'000'000; // 5ms one-way.

    for (int i = 0; i < 5; ++i) {
        SimulateProbeRound(sessionA, clockA, sessionB, clockB, delay);
        clockA.Advance(100'000'000);
        clockB.Advance(100'000'000);
    }

    EXPECT_EQ(sessionA.State(), SessionState::Synced);

    // Session A's synced time should reflect B's offset.
    // syncedTime ≈ localTimeA + 50ms
    Nanos synced = sessionA.SyncedNow();
    Nanos expected = clockA.Now() + 50'000'000;
    Nanos diff = synced - expected;
    if (diff < 0) {
        diff = -diff;
    }
    EXPECT_LE(diff, 5'000'000); // Within 5ms tolerance.
}

// ===========================================================================
// Session — packet loss resilience
// ===========================================================================

TEST(Session, HandlesPacketLoss)
{
    FakeClock clockA;
    FakeClock clockB;
    clockA.SetNow(1'000'000'000LL);
    clockB.SetNow(1'000'000'000LL);

    SessionConfig config;
    config.minSamplesForSync = 3;
    config.filterWindowSize = 4;

    Session sessionA(clockA, config);
    Session sessionB(clockB, config);

    constexpr Nanos delay = 5'000'000;

    // Send 2 good probes.
    SimulateProbeRound(sessionA, clockA, sessionB, clockB, delay);
    clockA.Advance(100'000'000);
    clockB.Advance(100'000'000);
    SimulateProbeRound(sessionA, clockA, sessionB, clockB, delay);
    clockA.Advance(100'000'000);
    clockB.Advance(100'000'000);

    // Simulate "lost" probe — just advance time without exchanging.
    clockA.Advance(200'000'000);
    clockB.Advance(200'000'000);

    // Send 3 more good probes.
    for (int i = 0; i < 3; ++i) {
        SimulateProbeRound(sessionA, clockA, sessionB, clockB, delay);
        clockA.Advance(100'000'000);
        clockB.Advance(100'000'000);
    }

    // Should still reach synced state.
    EXPECT_EQ(sessionA.State(), SessionState::Synced);
}

// ===========================================================================
// Session — drift compensation
// ===========================================================================

TEST(Session, CompensatesDrift)
{
    FakeClock clockA;
    FakeClock clockB;
    clockA.SetNow(1'000'000'000LL);
    clockB.SetNow(1'000'000'000LL);

    SessionConfig config;
    config.minSamplesForSync = 3;
    config.filterWindowSize = 8;

    Session sessionA(clockA, config);
    Session sessionB(clockB, config);

    constexpr Nanos delay = 5'000'000;

    // B drifts +100ppm relative to A.
    // After each 100ms interval, B advances 100ms + 10µs.
    for (int i = 0; i < 10; ++i) {
        SimulateProbeRound(sessionA, clockA, sessionB, clockB, delay);
        clockA.Advance(100'000'000);
        clockB.Advance(100'010'000); // 100ppm faster.
    }

    EXPECT_EQ(sessionA.State(), SessionState::Synced);

    // The drift model should track B's drift.
    // After 10 rounds of 100ms + 2×5ms delay = ~1.1s, B has drifted ~110µs.
    // The session should have partially compensated for this.
    Nanos synced = sessionA.SyncedNow();
    Nanos local  = clockA.Now();
    // With drift compensation, the offset should be tracked.
    // We just verify the model is initialized and producing reasonable output.
    EXPECT_TRUE(sessionA.GetDriftModel().IsInitialized());
    EXPECT_NE(synced, local); // Should show some offset due to drift.
}

// ===========================================================================
// Session — step detection
// ===========================================================================

TEST(Session, DetectsStep)
{
    FakeClock clockA;
    FakeClock clockB;
    clockA.SetNow(1'000'000'000LL);
    clockB.SetNow(1'000'000'000LL);

    SessionConfig config;
    config.minSamplesForSync = 2;
    config.filterWindowSize = 4;
    config.stepThreshold = 50'000'000; // 50ms

    Session sessionA(clockA, config);
    Session sessionB(clockB, config);

    constexpr Nanos delay = 1'000'000;

    // Converge first.
    for (int i = 0; i < 4; ++i) {
        SimulateProbeRound(sessionA, clockA, sessionB, clockB, delay);
        clockA.Advance(100'000'000);
        clockB.Advance(100'000'000);
    }
    EXPECT_EQ(sessionA.State(), SessionState::Synced);

    // Suddenly B jumps 200ms ahead — should trigger step.
    clockB.Advance(200'000'000);

    // Feed enough probes with the new offset to overcome the filter window.
    bool gotStep = false;
    for (int i = 0; i < 5; ++i) {
        auto result = SimulateProbeRound(sessionA, clockA, sessionB, clockB, delay);
        if (result.stepped) {
            gotStep = true;
            break;
        }
        clockA.Advance(10'000'000);
        clockB.Advance(10'000'000);
    }
    EXPECT_TRUE(gotStep);
    EXPECT_EQ(sessionA.State(), SessionState::Resyncing);
}

// ===========================================================================
// Session — reset
// ===========================================================================

TEST(Session, ResetClearsState)
{
    FakeClock clock;
    Session session(clock);

    [[maybe_unused]] auto req = session.MakeProbeRequest();
    EXPECT_NE(session.State(), SessionState::Idle);

    session.Reset();
    EXPECT_EQ(session.State(), SessionState::Idle);
    EXPECT_EQ(session.GetFilter().SampleCount(), 0u);
    EXPECT_FALSE(session.GetDriftModel().IsInitialized());
}
