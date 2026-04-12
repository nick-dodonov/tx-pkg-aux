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
