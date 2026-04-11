#include "SynTm/DriftModel.h"
#include "SynTm/Filter.h"
#include "SynTm/Probe.h"
#include "SynTm/Types.h"

#include <gtest/gtest.h>

using namespace SynTm;

// ===========================================================================
// Filter — offset estimation
// ===========================================================================

TEST(Filter, OneSampleGivesValidResult)
{
    Filter filter(8);
    auto result = filter.AddSample(0, ProbeResult{.offset = 1000, .rtt = 10000});
    EXPECT_EQ(result.sampleCount, 1u);
    EXPECT_EQ(result.offset, 1000);
    EXPECT_EQ(filter.SampleCount(), 1u);
}

TEST(Filter, ProducesResultAfterTwoSamples)
{
    Filter filter(8);
    filter.AddSample(0, ProbeResult{.offset = 1000, .rtt = 10000});
    auto result = filter.AddSample(1'000'000, ProbeResult{.offset = 1000, .rtt = 10000});
    EXPECT_EQ(result.sampleCount, 2u);
    EXPECT_EQ(result.offset, 1000);
}

TEST(Filter, ConvergesWithConsistentSamples)
{
    Filter filter(8);
    constexpr Nanos trueOffset = 5'000'000; // 5ms

    FilterResult lastResult{};
    for (int i = 0; i < 8; ++i) {
        Nanos localTime = static_cast<Nanos>(i) * 100'000'000; // 100ms apart
        lastResult = filter.AddSample(localTime,
            ProbeResult{.offset = trueOffset, .rtt = 2'000'000}); // 2ms RTT
    }

    EXPECT_EQ(lastResult.offset, trueOffset);
    EXPECT_LE(lastResult.jitter, 0); // Zero jitter with identical offsets.
}

TEST(Filter, MinRttTracked)
{
    Filter filter(4);
    filter.AddSample(0, ProbeResult{.offset = 1000, .rtt = 10'000'000});
    filter.AddSample(100'000'000, ProbeResult{.offset = 1000, .rtt = 5'000'000});
    filter.AddSample(200'000'000, ProbeResult{.offset = 1000, .rtt = 8'000'000});
    auto result = filter.AddSample(300'000'000,
        ProbeResult{.offset = 1000, .rtt = 12'000'000});

    EXPECT_EQ(result.minRtt, 5'000'000);
}

TEST(Filter, OutlierRejection)
{
    // Weighted median should reject high-RTT outlier offsets.
    Filter filter(8);

    // 6 good samples with 5ms offset and 2ms RTT.
    for (int i = 0; i < 6; ++i) {
        filter.AddSample(static_cast<Nanos>(i) * 100'000'000,
            ProbeResult{.offset = 5'000'000, .rtt = 2'000'000});
    }

    // 2 outlier samples with 50ms offset and 200ms RTT (high latency).
    filter.AddSample(600'000'000,
        ProbeResult{.offset = 50'000'000, .rtt = 200'000'000});
    auto result = filter.AddSample(700'000'000,
        ProbeResult{.offset = 50'000'000, .rtt = 200'000'000});

    // Weighted median should still be near 5ms, not 50ms.
    EXPECT_EQ(result.offset, 5'000'000);
}

TEST(Filter, WindowEviction)
{
    Filter filter(4);
    for (int i = 0; i < 10; ++i) {
        filter.AddSample(static_cast<Nanos>(i) * 100'000'000,
            ProbeResult{.offset = static_cast<Nanos>(i) * 1000, .rtt = 1'000'000});
    }
    // Window should only contain last 4 samples.
    EXPECT_EQ(filter.SampleCount(), 4u);
}

TEST(Filter, Reset)
{
    Filter filter(8);
    filter.AddSample(0, ProbeResult{.offset = 1000, .rtt = 10000});
    filter.AddSample(100'000'000, ProbeResult{.offset = 1000, .rtt = 10000});
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
        filter.AddSample(static_cast<Nanos>(i) * 1'000'000'000LL,
            ProbeResult{.offset = 10'000'000, .rtt = 2'000'000});
    }
    auto result = filter.AddSample(8'000'000'000LL,
        ProbeResult{.offset = 10'000'000, .rtt = 2'000'000});

    // Rate should be 1/1 (no drift).
    EXPECT_EQ(result.rate.num, result.rate.den);
}

TEST(Filter, DetectsPositiveDrift)
{
    Filter filter(8);
    // Offsets increase linearly → remote clock is faster.
    // offset(t) = 10ms + t * 100ppm = 10ms + t * 1e-4
    // At t = i seconds: offset = 10ms + i * 100'000 ns
    for (int i = 0; i < 8; ++i) {
        Nanos localTime = static_cast<Nanos>(i) * 1'000'000'000LL;
        Nanos offset = 10'000'000 + static_cast<Nanos>(i) * 100'000;
        filter.AddSample(localTime, ProbeResult{.offset = offset, .rtt = 2'000'000});
    }

    // The filter should detect rate > 1.
    auto result = filter.Samples();
    EXPECT_EQ(result.size(), 8u);

    // Feed one more to trigger a fresh result.
    auto fr = filter.AddSample(8'000'000'000LL,
        ProbeResult{.offset = 10'000'000 + 800'000, .rtt = 2'000'000});

    // Rate num > den (remote is faster).
    double rate = static_cast<double>(fr.rate.num) / static_cast<double>(fr.rate.den);
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
    EXPECT_EQ(model.Convert(1'000'000), 1'000'000);
}

TEST(DriftModel, InitializeAndConvert)
{
    DriftModel model;
    model.Initialize(1'000'000'000LL, 5'000'000); // offset 5ms

    EXPECT_TRUE(model.IsInitialized());

    // At baseLocal: should return baseSynced.
    EXPECT_EQ(model.Convert(1'000'000'000LL), 1'005'000'000LL);

    // 1 second later with rate 1/1: still 5ms offset.
    EXPECT_EQ(model.Convert(2'000'000'000LL), 2'005'000'000LL);
}

TEST(DriftModel, SteerSmallCorrection)
{
    DriftModel model;
    model.Initialize(0, 5'000'000); // 5ms offset.

    // Filter says offset is 7ms (2ms correction).
    FilterResult fr{
        .offset = 7'000'000,
        .rate   = Rational{1, 1},
        .jitter = 100'000,
        .minRtt = 2'000'000,
    };

    bool stepped = model.Steer(1'000'000'000LL, fr);
    EXPECT_FALSE(stepped); // Small correction → slew, not step.

    // After steering, synced time at current local time should be
    // closer to localTime + 7ms than before.
    Nanos synced = model.Convert(1'000'000'000LL);
    Nanos target = 1'000'000'000LL + 7'000'000;
    Nanos diff = synced - target;
    if (diff < 0) {
        diff = -diff;
    }
    // Should be within 2ms of target (half correction applied).
    EXPECT_LE(diff, 2'000'000);
}

TEST(DriftModel, SteerLargeCorrection)
{
    DriftModel model(SteerPolicy{.stepThreshold = 50'000'000}); // 50ms threshold
    model.Initialize(0, 5'000'000); // 5ms offset.

    // Filter says offset is 200ms (195ms correction > 50ms threshold).
    FilterResult fr{
        .offset = 200'000'000,
        .rate   = Rational{1, 1},
        .jitter = 100'000,
        .minRtt = 2'000'000,
    };

    bool stepped = model.Steer(1'000'000'000LL, fr);
    EXPECT_TRUE(stepped); // Large correction → step.

    // After step, synced time should be exactly at target.
    EXPECT_EQ(model.Convert(1'000'000'000LL), 1'200'000'000LL);
}

TEST(DriftModel, Reset)
{
    DriftModel model;
    model.Initialize(0, 5'000'000);
    EXPECT_TRUE(model.IsInitialized());

    model.Reset();
    EXPECT_FALSE(model.IsInitialized());
    EXPECT_EQ(model.Convert(1'000'000), 1'000'000); // Back to passthrough.
}

TEST(DriftModel, RateApplied)
{
    DriftModel model;
    // Remote is 100ppm faster: rate = 1'000'100 / 1'000'000.
    model.Initialize(0, 0);

    FilterResult fr{
        .offset = 0,
        .rate   = Rational{.num=1'000'100, .den=1'000'000},
        .jitter = 100'000,
        .minRtt = 2'000'000,
    };

    model.Steer(0, fr);

    // After 10 seconds with 100ppm drift, synced time should be ~1ms ahead.
    Nanos synced = model.Convert(10'000'000'000LL);
    Nanos expected = 10'001'000'000LL; // 10s + 1ms
    Nanos diff = synced - expected;
    if (diff < 0) {
        diff = -diff;
    }
    EXPECT_LE(diff, 100'000); // Within 0.1ms tolerance.
}
