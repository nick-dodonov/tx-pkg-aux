#include "SynTm/Clock.h"
#include "SynTm/Probe.h"
#include "SynTm/Session.h"
#include "SynTm/SessionConfig.h"
#include "SynTm/Types.h"

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

// ===========================================================================
// Session — step resets filter and result count
// ===========================================================================

// A step correction must flush the filter window and zero the result count so
// that post-step probes build a fresh estimate. Without this, stale pre-step
// samples corrupt the drift rate and cause cascading re-steps.
TEST(Session, StepResetsFilterAndResultCount)
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

    // Converge to Synced.
    for (int i = 0; i < 4; ++i) {
        SimulateProbeRound(sessionA, clockA, sessionB, clockB, delay);
        clockA.Advance(100'000'000);
        clockB.Advance(100'000'000);
    }
    EXPECT_EQ(sessionA.State(), SessionState::Synced);

    // Trigger a step: B jumps 200ms.
    clockB.Advance(200'000'000);

    bool gotStep = false;
    for (int i = 0; i < 5; ++i) {
        auto result = SimulateProbeRound(sessionA, clockA, sessionB, clockB, delay);
        clockA.Advance(10'000'000);
        clockB.Advance(10'000'000);
        if (result.stepped) {
            gotStep = true;
            // Filter must be empty and resultCount reset after the step.
            EXPECT_EQ(sessionA.GetFilter().SampleCount(), 0u);
            break;
        }
    }
    EXPECT_TRUE(gotStep);

    // After the reset, a single further probe must NOT immediately transition
    // to Synced (minSamplesForSync=2 requires 2 post-step results).
    auto r = SimulateProbeRound(sessionA, clockA, sessionB, clockB, delay);
    EXPECT_FALSE(r.stepped);
    EXPECT_EQ(sessionA.State(), SessionState::Resyncing); // still waiting for 2nd result
}

// ===========================================================================
// Session — large initial offset converges without infinite stepping
// ===========================================================================

// Nodes starting with a multi-second clock difference must converge to Synced
// within a bounded number of probes and stop stepping once stable.
TEST(Session, LargeOffset_ConvergesWithoutInfiniteStepping)
{
    FakeClock clockA;
    FakeClock clockB;
    clockA.SetNow(0LL);
    clockB.SetNow(20'000'000'000LL); // B is 20s ahead.

    SessionConfig config;
    config.minSamplesForSync = 3;
    config.filterWindowSize = 4;
    config.stepThreshold = 100'000'000; // 100ms

    Session sessionA(clockA, config);
    Session sessionB(clockB, config);

    constexpr Nanos delay = 5'000'000; // 5ms one-way.

    // Run plenty of probes.
    for (int i = 0; i < 40; ++i) {
        SimulateProbeRound(sessionA, clockA, sessionB, clockB, delay);
        clockA.Advance(100'000'000);
        clockB.Advance(100'000'000);
    }

    EXPECT_EQ(sessionA.State(), SessionState::Synced);

    // Once synced, no more stepping should occur.
    int stepsAfterSync = 0;
    for (int i = 0; i < 10; ++i) {
        auto result = SimulateProbeRound(sessionA, clockA, sessionB, clockB, delay);
        if (result.stepped) {
            ++stepsAfterSync;
        }
        clockA.Advance(100'000'000);
        clockB.Advance(100'000'000);
    }
    EXPECT_EQ(stepsAfterSync, 0);
}

// ===========================================================================
// Session — enteredResyncing fires only on the first step of an episode
// ===========================================================================

TEST(Session, EnteredResyncing_OnlyOnFirstStepOfEpisode)
{
    FakeClock clockA;
    FakeClock clockB;
    clockA.SetNow(1'000'000'000LL);
    clockB.SetNow(1'000'000'000LL);

    SessionConfig config;
    config.minSamplesForSync = 3;
    config.filterWindowSize = 4;
    config.stepThreshold = 50'000'000; // 50ms

    Session sessionA(clockA, config);
    Session sessionB(clockB, config);

    constexpr Nanos delay = 1'000'000;

    // Converge.
    for (int i = 0; i < 5; ++i) {
        SimulateProbeRound(sessionA, clockA, sessionB, clockB, delay);
        clockA.Advance(100'000'000);
        clockB.Advance(100'000'000);
    }
    EXPECT_EQ(sessionA.State(), SessionState::Synced);

    // Jump B by 200ms to force a step.
    clockB.Advance(200'000'000);

    // Collect the first step.
    Session::ProbeHandleResult firstStep;
    for (int i = 0; i < 5; ++i) {
        firstStep = SimulateProbeRound(sessionA, clockA, sessionB, clockB, delay);
        clockA.Advance(10'000'000);
        clockB.Advance(10'000'000);
        if (firstStep.stepped) {
            break;
        }
    }
    ASSERT_TRUE(firstStep.stepped);
    EXPECT_TRUE(firstStep.enteredResyncing); // First step of episode.

    // Another jump while still in Resyncing — subsequent step must NOT set enteredResyncing.
    clockB.Advance(200'000'000);
    ASSERT_EQ(sessionA.State(), SessionState::Resyncing);

    for (int i = 0; i < 5; ++i) {
        auto r = SimulateProbeRound(sessionA, clockA, sessionB, clockB, delay);
        clockA.Advance(10'000'000);
        clockB.Advance(10'000'000);
        if (r.stepped) {
            EXPECT_FALSE(r.enteredResyncing); // Already in Resyncing.
            break;
        }
    }
}

// ===========================================================================
// Filter drift-rate regression — int64 overflow with long probe intervals
// ===========================================================================
//
// Reproduces an integer overflow bug in ComputeDriftRate(). When the sample
// window spans several seconds (e.g. 8 samples including one at a 2s gap),
// squared time differences reach ~1.4e21, which overflows int64 max (9.2e18).
// The final cast `static_cast<int64_t>(sumXX / n)` wraps to a garbage value,
// producing rates like 0.52 instead of ~1.0. DriftModel then accumulates ~1s
// of error over 2s and triggers a cascading step (ResyncStarted) loop.
//
// Sample values are reproduced from real p0 logs where this was observed.

TEST(Filter, DriftRate_NoOverflowWhenWindowSpansSeconds)
{
    // Eight samples from the real p0 log trace (approximate localTimes
    // reconstructed from wall-clock timestamps; offsets taken verbatim).
    // Samples 1-7 are fast-probe results (~120ms apart, offset ~-4910ms).
    // Sample 8 is the first 2-second-gap probe with a +44ms outlier offset.
    // The resulting window spans ~5s (localTime delta ≈ 5054ms).
    struct S { Nanos localTime; Nanos offset; };
    constexpr std::array<S, 8> samples = {{
        { .localTime=5'118'360'000LL, .offset=-4'911'739'872LL },
        { .localTime=5'233'884'000LL, .offset=-4'907'421'709LL },
        { .localTime=5'353'914'000LL, .offset=-4'907'789'371LL },
        { .localTime=5'476'340'000LL, .offset=-4'908'687'968LL },
        { .localTime=5'720'116'000LL, .offset=-4'912'144'349LL },
        { .localTime=5'836'254'000LL, .offset=-4'911'507'230LL },
        { .localTime=5'956'149'000LL, .offset=-4'910'067'616LL },
        {.localTime=10'172'818'000LL, .offset=-4'866'559'264LL }, // 2s-gap probe: 44ms outlier
    }};

    Filter filter(8);
    FilterResult result{};
    for (const auto& s : samples) {
        result = filter.AddSample(s.localTime, ProbeResult{.offset = s.offset, .rtt = 60'000'000LL});
    }

    double rate = static_cast<double>(result.rate.num) / static_cast<double>(result.rate.den);

    // With the overflow bug the rate was ~0.521; the DriftModel then diverges
    // by ~960ms over the next 2s probe interval, triggering a step.
    EXPECT_NEAR(rate, 1.0, 0.01)
        << "computed rate=" << result.rate.num << "/" << result.rate.den
        << " (" << rate << ") — likely int64 overflow in ComputeDriftRate";
}
