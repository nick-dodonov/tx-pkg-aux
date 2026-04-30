#include "SynTm/DriftModel.h"
#include "SynTm/Filter.h"
#include "SynTm/Probe.h"
#include "SynTm/Types.h"

#include <gtest/gtest.h>

using namespace SynTm;
using namespace std::chrono_literals;

// ===========================================================================
// Filter — offset estimation
// ===========================================================================

TEST(Filter, OneSampleGivesValidResult)
{
    Filter filter(8);
    auto result = filter.AddSample(Ticks{}, ProbeResult{.offset = 1us, .rtt = 10us});
    EXPECT_EQ(result.sampleCount, 1u);
    EXPECT_EQ(result.offset, 1us);
    EXPECT_EQ(filter.SampleCount(), 1u);
}

TEST(Filter, ProducesResultAfterTwoSamples)
{
    Filter filter(8);
    filter.AddSample(Ticks{}, ProbeResult{.offset = 1us, .rtt = 10us});
    auto result = filter.AddSample(1ms, ProbeResult{.offset = 1us, .rtt = 10us});
    EXPECT_EQ(result.sampleCount, 2u);
    EXPECT_EQ(result.offset, 1us);
}

TEST(Filter, ConvergesWithConsistentSamples)
{
    Filter filter(8);
    constexpr Ticks trueOffset = 5ms;

    FilterResult lastResult{};
    for (int i = 0; i < 8; ++i) {
        Ticks localTime = 100ms * i; // 100ms apart
        lastResult = filter.AddSample(localTime,
            ProbeResult{.offset = trueOffset, .rtt = 2ms}); // 2ms RTT
    }

    EXPECT_EQ(lastResult.offset, trueOffset);
    EXPECT_LE(lastResult.jitter, Ticks{}); // Zero jitter with identical offsets.
}

TEST(Filter, MinRttTracked)
{
    Filter filter(4);
    filter.AddSample(Ticks{}, ProbeResult{.offset = 1us, .rtt = 10ms});
    filter.AddSample(100ms, ProbeResult{.offset = 1us, .rtt = 5ms});
    filter.AddSample(200ms, ProbeResult{.offset = 1us, .rtt = 8ms});
    auto result = filter.AddSample(300ms,
        ProbeResult{.offset = 1us, .rtt = 12ms});

    EXPECT_EQ(result.minRtt, 5ms);
}

TEST(Filter, OutlierRejection)
{
    // Weighted median should reject high-RTT outlier offsets.
    Filter filter(8);

    // 6 good samples with 5ms offset and 2ms RTT.
    for (int i = 0; i < 6; ++i) {
        filter.AddSample(100ms * i,
            ProbeResult{.offset = 5ms, .rtt = 2ms});
    }

    // 2 outlier samples with 50ms offset and 200ms RTT (high latency).
    filter.AddSample(600ms,
        ProbeResult{.offset = 50ms, .rtt = 200ms});
    auto result = filter.AddSample(700ms,
        ProbeResult{.offset = 50ms, .rtt = 200ms});

    // Weighted median should still be near 5ms, not 50ms.
    EXPECT_EQ(result.offset, 5ms);
}

TEST(Filter, WindowEviction)
{
    Filter filter(4);
    for (int i = 0; i < 10; ++i) {
        filter.AddSample(100ms * i,
            ProbeResult{.offset = 1us * i, .rtt = 1ms});
    }
    // Window should only contain last 4 samples.
    EXPECT_EQ(filter.SampleCount(), 4u);
}

TEST(Filter, Reset)
{
    Filter filter(8);
    filter.AddSample(Ticks{}, ProbeResult{.offset = 1us, .rtt = 10us});
    filter.AddSample(100ms, ProbeResult{.offset = 1us, .rtt = 10us});
    EXPECT_EQ(filter.SampleCount(), 2u);

    filter.Reset();
    EXPECT_EQ(filter.SampleCount(), 0u);
}

// ===========================================================================
// Filter — drift estimation
// ===========================================================================

TEST(Filter, DetectsZeroDrift)
{
    Filter filter(8);
    // All offsets are constant → rate should be ~1.
    for (int i = 0; i < 8; ++i) {
        filter.AddSample(1s * i,
            ProbeResult{.offset = 10ms, .rtt = 2ms});
    }
    auto result = filter.AddSample(8s,
        ProbeResult{.offset = 10ms, .rtt = 2ms});

    // Rate should be 0 (no drift).
    EXPECT_EQ(result.rate.count(), 0);
}

TEST(Filter, DetectsPositiveDrift)
{
    Filter filter(8);
    // Offsets increase linearly → remote clock is faster.
    // offset(t) = 10ms + t * 100ppm = 10ms + t * 1e-4
    // At t = i seconds: offset = 10ms + i * 100us
    for (int i = 0; i < 8; ++i) {
        Ticks localTime = 1s * i;
        Ticks offset = 10ms + 100us * i;
        filter.AddSample(localTime, ProbeResult{.offset = offset, .rtt = 2ms});
    }

    // The filter should detect rate > 1.
    auto result = filter.Samples();
    EXPECT_EQ(result.size(), 8u);

    // Feed one more to trigger a fresh result.
    auto fr = filter.AddSample(8s,
        ProbeResult{.offset = 10ms + 800us, .rtt = 2ms});

    // Rate ppb > 0 (remote is faster).
    double rate = fr.rate.ToDouble();
    EXPECT_GT(rate, 1.0);
    EXPECT_LT(rate, 1.01); // Should be in the ballpark of 100ppm (1.0001).
}

// ===========================================================================
// DriftModel — initialization and conversion
// ===========================================================================

TEST(DriftModel, UninitializedPassthrough)
{
    DriftModel model;
    EXPECT_FALSE(model.IsInitialized());
    EXPECT_EQ(model.Convert(1ms), 1ms);
}

TEST(DriftModel, InitializeAndConvert)
{
    DriftModel model;
    model.Initialize(1s, 5ms); // offset 5ms

    EXPECT_TRUE(model.IsInitialized());

    // At baseLocal: should return baseSynced.
    EXPECT_EQ(model.Convert(1s), 1s + 5ms);

    // 1 second later with rate 1/1: still 5ms offset.
    EXPECT_EQ(model.Convert(2s), 2s + 5ms);
}

TEST(DriftModel, SteerSmallCorrection)
{
    DriftModel model;
    model.Initialize(Ticks{}, 5ms); // 5ms offset.

    // Filter says offset is 7ms (2ms correction).
    FilterResult fr{
        .offset = 7ms,
        .rate   = DriftRate{},
        .jitter = 100us,
        .minRtt = 2ms,
    };

    bool stepped = model.Steer(1s, fr);
    EXPECT_FALSE(stepped); // Small correction → slew, not step.

    // After steering, synced time at current local time should be
    // closer to localTime + 7ms than before.
    Ticks synced = model.Convert(1s);
    Ticks target = 1s + 7ms;
    auto diff = std::chrono::abs(synced - target);
    // Should be within 2ms of target (half correction applied).
    EXPECT_LE(diff, 2ms);
}

TEST(DriftModel, SteerLargeCorrection)
{
    DriftModel model(SteerPolicy{.stepThreshold = 50ms}); // 50ms threshold
    model.Initialize(Ticks{}, 5ms); // 5ms offset.

    // Filter says offset is 200ms (195ms correction > 50ms threshold).
    FilterResult fr{
        .offset = 200ms,
        .rate   = DriftRate{},
        .jitter = 100us,
        .minRtt = 2ms,
    };

    bool stepped = model.Steer(1s, fr);
    EXPECT_TRUE(stepped); // Large correction → step.

    // After step, synced time should be exactly at target.
    EXPECT_EQ(model.Convert(1s), 1s + 200ms);
}

TEST(DriftModel, Reset)
{
    DriftModel model;
    model.Initialize(Ticks{}, 5ms);
    EXPECT_TRUE(model.IsInitialized());

    model.Reset();
    EXPECT_FALSE(model.IsInitialized());
    EXPECT_EQ(model.Convert(1ms), 1ms); // Back to passthrough.
}

TEST(DriftModel, RateApplied)
{
    DriftModel model;
    // Remote is 100ppm faster: rate = 1'000'100 / 1'000'000.
    model.Initialize(Ticks{}, Ticks{});

    FilterResult fr{
        .offset = Ticks{},
        .rate   = DriftRate{100us}, // 100 µs/s = 100 ppm - remote is faster.
        .jitter = 100us,
        .minRtt = 2ms,
    };

    model.Steer(Ticks{}, fr);

    // After 10 seconds with 100ppm drift, synced time should be ~1ms ahead.
    Ticks synced = model.Convert(10s);
    Ticks expected = 10s + 1ms;
    auto diff = std::chrono::abs(synced - expected);
    EXPECT_LE(diff, 100us); // Within 0.1ms tolerance.
}

// ===========================================================================
// H1 — Queuing delay noise model
//
// Confirms numerically that random queuing delays on t4 introduce
// ~std(D)/2 noise into the computed offset.
// This test uses raw ProbeResult construction to inject the delay directly.
// ===========================================================================

TEST(Filter, QueuingDelayIntroducesOffsetNoise)
{
    // The NTP formula: offset = ((t2-t1) + (t3-t4)) / 2.
    // If t4 is delayed by D (processing lag), the measured offset shifts by +D/2.
    // For D uniform in [0, Dmax], std(D/2) = Dmax / (2 * sqrt(12)) ≈ Dmax / 6.9.
    // With Dmax = 200ms → std(measured_offset) ≈ 29ms.
    //
    // We simulate this by adding a random queuing delay to each t4
    // and measuring how much the reported offset deviates from the true offset.

    constexpr Ticks trueOffset = 50ms;
    constexpr Ticks maxQueueDelay = 200ms;
    constexpr int kRounds = 40;

    // Fixed seed PRNG (xorshift32) for deterministic test.
    std::uint32_t rng = 0xDEAD'BEEF;
    auto nextRand = [&]() -> std::uint32_t {
        rng ^= rng << 13;
        rng ^= rng >> 17;
        rng ^= rng << 5;
        return rng;
    };

    Ticks sumError{};
    Ticks sumErrorSq{};

    for (int i = 0; i < kRounds; ++i) {
        // Queuing delay for this round: uniform in [0, maxQueueDelay].
        auto queueNs = static_cast<std::int64_t>(
            static_cast<std::uint64_t>(nextRand()) % static_cast<std::uint64_t>(maxQueueDelay.count()));
        Ticks queueDelay{queueNs};

        // True timestamps:
        //   t1 = 1s (initiator sends)
        //   t2 = 1s + 5ms + trueOffset   (responder receives after 5ms network delay)
        //   t3 = t2                        (responder sends immediately)
        //   t4_true = 1s + 10ms           (initiator receives after another 5ms)
        //   t4_observed = t4_true + queueDelay (processing happens later)
        Ticks t1{1'000'000'000LL + static_cast<std::int64_t>(i) * 100'000'000LL};
        Ticks t2 = t1 + 5ms + trueOffset;
        Ticks t3 = t2;
        Ticks t4_true = t1 + 10ms;
        Ticks t4_observed = t4_true + queueDelay;

        auto result = ComputeProbeResult(t1, t2, t3, t4_observed);

        Ticks error = result.offset - trueOffset;
        sumError += error;
        sumErrorSq += Ticks{error.count() / 1'000 * (error.count() / 1'000)}; // (error/1us)^2 in us^2
    }

    // Mean error ≈ maxQueueDelay / 2 (positive bias from queuing).
    Ticks meanError = sumError / kRounds;
    // We don't assert the exact mean — just that it's substantial (>= 30ms).
    EXPECT_GE(std::chrono::abs(meanError), 30ms)
        << "Queuing delay should introduce at least 30ms mean offset bias";
}

// ===========================================================================
// H2 — Slew rate: DriftModel must honour maxSlewRate
//
// With the current implementation maxSlewRate is unused — correction/2 is
// applied directly. For correction=50ms and maxSlewRate=100us this test FAILS
// (RED) until maxSlewRate enforcement is implemented.
// ===========================================================================

TEST(DriftModel, SlewRateCapped)
{
    // maxSlewRate = 100 µs/s.  Probe interval = 200ms.
    // Max allowed delta per step = 100us/s * 0.2s = 20us.
    SteerPolicy policy{
        .stepThreshold = 200ms, // High threshold: no steps, only slew.
        .maxSlewRate   = DriftRate{100us},
    };
    DriftModel model(policy);
    model.Initialize(Ticks{}, Ticks{});

    constexpr Ticks probeInterval = 200ms;
    constexpr Ticks correction = 50ms; // Large noisy correction — should be clamped.

    Ticks localTime{};
    Ticks prevSynced = model.Convert(localTime);

    for (int i = 0; i < 10; ++i) {
        localTime += probeInterval;

        FilterResult fr{
            .offset = correction, // Constant large noisy offset.
            .rate   = DriftRate{},
            .jitter = 1ms,
            .minRtt = 5ms,
        };
        model.Steer(localTime, fr);

        Ticks newSynced = model.Convert(localTime);
        Ticks delta = std::chrono::abs(newSynced - prevSynced - probeInterval);

        // The step in synced time due to slewing must not exceed
        // maxSlewRate * probeInterval + small epsilon for rate application.
        // maxSlewRate=100us/s * 0.2s = 20us. We allow 2x margin.
        EXPECT_LE(delta, 40us)
            << "Slew at step " << i << " was " << delta.count() << "ns, expected <= 40us";

        prevSynced = newSynced;
    }
}

// ===========================================================================
// H2 — Slew rate: step still applies when correction exceeds threshold
// ===========================================================================

TEST(DriftModel, StepAppliedWhenAboveThreshold)
{
    SteerPolicy policy{
        .stepThreshold = 50ms,
        .maxSlewRate   = DriftRate{100us},
    };
    DriftModel model(policy);
    model.Initialize(Ticks{}, Ticks{});

    FilterResult fr{
        .offset = 200ms, // Exceeds stepThreshold — step must still occur.
        .rate   = DriftRate{},
        .jitter = 1ms,
        .minRtt = 5ms,
    };
    bool stepped = model.Steer(1s, fr);
    EXPECT_TRUE(stepped);
    EXPECT_EQ(model.Convert(1s), 1s + 200ms);
}
