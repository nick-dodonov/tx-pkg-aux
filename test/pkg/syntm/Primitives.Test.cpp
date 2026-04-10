#include "SynTm/Clock.h"
#include "SynTm/Probe.h"
#include "SynTm/TruncTime.h"
#include "SynTm/Types.h"

#include <array>
#include <cstddef>
#include <gtest/gtest.h>
#include <limits>

using namespace SynTm;

// ===========================================================================
// Rational
// ===========================================================================

TEST(Rational, IdentityRate)
{
    Rational r{1, 1};
    EXPECT_EQ(r.Apply(1'000'000'000), 1'000'000'000);
}

TEST(Rational, ScaleUp)
{
    Rational r{3, 2}; // 1.5x
    EXPECT_EQ(r.Apply(1'000'000'000), 1'500'000'000);
}

TEST(Rational, ScaleDown)
{
    Rational r{2, 3}; // ~0.667x
    EXPECT_EQ(r.Apply(3'000'000'000), 2'000'000'000);
}

TEST(Rational, LargeValuesNoOverflow)
{
    // 100 seconds * rate close to 1.0 — should not overflow with __int128.
    Rational r{1'000'001, 1'000'000}; // 1.000001x (1ppm drift)
    Nanos hundred_sec = 100'000'000'000LL;
    Nanos result = r.Apply(hundred_sec);
    EXPECT_EQ(result, 100'000'100'000LL);
}

// ===========================================================================
// SyncQuality / SyncEvent to-string
// ===========================================================================

TEST(Types, SyncQualityToString)
{
    EXPECT_EQ(SyncQualityToString(SyncQuality::None),   "None");
    EXPECT_EQ(SyncQualityToString(SyncQuality::Low),    "Low");
    EXPECT_EQ(SyncQualityToString(SyncQuality::Medium), "Medium");
    EXPECT_EQ(SyncQualityToString(SyncQuality::High),   "High");
}

TEST(Types, SyncEventToString)
{
    EXPECT_EQ(SyncEventToString(SyncEvent::SyncAcquired), "SyncAcquired");
    EXPECT_EQ(SyncEventToString(SyncEvent::EpochChanged),  "EpochChanged");
    EXPECT_EQ(SyncEventToString(SyncEvent::Resynced),      "Resynced");
    EXPECT_EQ(SyncEventToString(SyncEvent::SyncLost),      "SyncLost");
}

// ===========================================================================
// FakeClock
// ===========================================================================

TEST(FakeClock, StartsAtZero)
{
    FakeClock clock;
    EXPECT_EQ(clock.Now(), 0);
}

TEST(FakeClock, SetNow)
{
    FakeClock clock;
    clock.SetNow(42'000'000);
    EXPECT_EQ(clock.Now(), 42'000'000);
}

TEST(FakeClock, Advance)
{
    FakeClock clock;
    clock.SetNow(1'000'000);
    clock.Advance(500'000);
    EXPECT_EQ(clock.Now(), 1'500'000);
}

TEST(FakeClock, AdvanceMultipleTimes)
{
    FakeClock clock;
    clock.Advance(100);
    clock.Advance(200);
    clock.Advance(300);
    EXPECT_EQ(clock.Now(), 600);
}

// ===========================================================================
// SteadyClock
// ===========================================================================

TEST(SteadyClock, ReturnsPositive)
{
    SteadyClock clock;
    EXPECT_GT(clock.Now(), 0);
}

TEST(SteadyClock, Monotonic)
{
    SteadyClock clock;
    Nanos a = clock.Now();
    Nanos b = clock.Now();
    EXPECT_GE(b, a);
}

// ===========================================================================
// Probe — offset and RTT calculation
// ===========================================================================

TEST(Probe, ComputeResultSymmetric)
{
    // Symmetric delay: 10ms each way, zero offset.
    //   t1=0, t2=10ms, t3=10ms, t4=20ms
    constexpr Nanos ms = 1'000'000;
    auto result = ComputeProbeResult(0, 10 * ms, 10 * ms, 20 * ms);
    EXPECT_EQ(result.offset, 0);
    EXPECT_EQ(result.rtt, 20 * ms);
}

TEST(Probe, ComputeResultWithOffset)
{
    // Remote clock is 5ms ahead, symmetric 10ms RTT.
    //   t1=0, t2=15ms, t3=15ms, t4=20ms
    //   offset = ((15-0) + (15-20)) / 2 = (15 + (-5)) / 2 = 5ms
    constexpr Nanos ms = 1'000'000;
    auto result = ComputeProbeResult(0, 15 * ms, 15 * ms, 20 * ms);
    EXPECT_EQ(result.offset, 5 * ms);
    EXPECT_EQ(result.rtt, 20 * ms);
}

TEST(Probe, ComputeResultWithNegativeOffset)
{
    // Remote clock is 5ms behind, symmetric 10ms RTT.
    //   t1=0, t2=5ms, t3=5ms, t4=20ms
    //   offset = ((5-0) + (5-20)) / 2 = (5 + (-15)) / 2 = -5ms
    constexpr Nanos ms = 1'000'000;
    auto result = ComputeProbeResult(0, 5 * ms, 5 * ms, 20 * ms);
    EXPECT_EQ(result.offset, -5 * ms);
    EXPECT_EQ(result.rtt, 20 * ms);
}

TEST(Probe, ComputeResultAsymmetric)
{
    // Asymmetric delay: forward 5ms, return 15ms. Zero real offset.
    //   t1=0, t2=5ms, t3=5ms, t4=20ms
    //   True offset = 0, but estimated offset = -5ms (NTP asymmetry bias).
    constexpr Nanos ms = 1'000'000;
    auto result = ComputeProbeResult(0, 5 * ms, 5 * ms, 20 * ms);
    // RTT = (20-0) - (5-5) = 20ms
    EXPECT_EQ(result.rtt, 20 * ms);
    // Offset has asymmetry bias — expected for NTP-style estimation.
    EXPECT_EQ(result.offset, -5 * ms);
}

TEST(Probe, ComputeResultWithProcessingDelay)
{
    // Responder takes 2ms to process: t3 - t2 = 2ms.
    //   t1=0, t2=10ms, t3=12ms, t4=22ms
    //   rtt = (22-0) - (12-10) = 20ms
    //   offset = ((10-0) + (12-22)) / 2 = (10 - 10) / 2 = 0
    constexpr Nanos ms = 1'000'000;
    auto result = ComputeProbeResult(0, 10 * ms, 12 * ms, 22 * ms);
    EXPECT_EQ(result.rtt, 20 * ms);
    EXPECT_EQ(result.offset, 0);
}

// ===========================================================================
// Probe — serialization round-trip
// ===========================================================================

TEST(ProbeSerialization, RequestRoundTrip)
{
    ProbeRequest req{.t1 = 123'456'789'012'345LL};
    std::array<std::byte, ProbeRequest::WireSize> buf{};

    auto written = WriteTo(buf, req);
    EXPECT_EQ(written, ProbeRequest::WireSize);

    auto decoded = ReadProbeRequest(buf);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->t1, req.t1);
}

TEST(ProbeSerialization, ResponseRoundTrip)
{
    ProbeResponse resp{
        .t1 = 111'111'111'111LL,
        .t2 = 222'222'222'222LL,
        .t3 = 333'333'333'333LL,
    };
    std::array<std::byte, ProbeResponse::WireSize> buf{};

    auto written = WriteTo(buf, resp);
    EXPECT_EQ(written, ProbeResponse::WireSize);

    auto decoded = ReadProbeResponse(buf);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->t1, resp.t1);
    EXPECT_EQ(decoded->t2, resp.t2);
    EXPECT_EQ(decoded->t3, resp.t3);
}

TEST(ProbeSerialization, RequestBufferTooSmall)
{
    ProbeRequest req{.t1 = 42};
    std::array<std::byte, 4> buf{}; // Too small.
    EXPECT_EQ(WriteTo(buf, req), 0u);
    EXPECT_FALSE(ReadProbeRequest(std::span<const std::byte>(buf)).has_value());
}

TEST(ProbeSerialization, ResponseBufferTooSmall)
{
    ProbeResponse resp{};
    std::array<std::byte, 16> buf{}; // Too small (need 24).
    EXPECT_EQ(WriteTo(buf, resp), 0u);
    EXPECT_FALSE(ReadProbeResponse(std::span<const std::byte>(buf)).has_value());
}

TEST(ProbeSerialization, NegativeValues)
{
    ProbeRequest req{.t1 = -999'999'999LL};
    std::array<std::byte, ProbeRequest::WireSize> buf{};
    [[maybe_unused]] auto written = WriteTo(buf, req);
    auto decoded = ReadProbeRequest(buf);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->t1, -999'999'999LL);
}

// ===========================================================================
// TruncTime — truncation and expansion
// ===========================================================================

TEST(TruncTime, BasicTruncateExpand)
{
    constexpr Nanos epoch = 1'000'000'000'000LL; // 1 second
    constexpr Nanos time  = epoch + 500'000;      // +500µs
    constexpr Nanos quantum = 100'000;             // 100µs

    auto trunc = Truncate<16, quantum>(time, epoch);
    EXPECT_EQ(trunc.index, 5u); // 500'000 / 100'000 = 5

    Nanos expanded = Expand(trunc, epoch, time);
    EXPECT_EQ(expanded, epoch + 5 * quantum); // Exact quantum boundary.
}

TEST(TruncTime, WrapAround)
{
    constexpr Nanos epoch = 0;

    // Time is at 300ms → wraps around (300 mod 256 = 44).
    constexpr Nanos time = 300'000'000;
    auto trunc = Truncate<8, 1'000'000>(time, epoch);
    EXPECT_EQ(trunc.index, 44u);

    // Expand with hint near 300ms should give back ~300ms.
    Nanos expanded = Expand(trunc, epoch, 295'000'000);
    EXPECT_EQ(expanded, 300'000'000);
}

TEST(TruncTime, ExpandChoosesClosestToHint)
{
    constexpr Nanos epoch = 0;

    // Index 10 → within first cycle: 10ms.
    TruncTime<8, 1'000'000> trunc{.index = 10};

    // Hint near 10ms → should expand to 10ms.
    EXPECT_EQ(Expand(trunc, epoch, Nanos{8'000'000}), 10'000'000);

    // Hint near 266ms (= 256 + 10) → should expand to 266ms.
    EXPECT_EQ(Expand(trunc, epoch, Nanos{264'000'000}), 266'000'000);

    // Hint near 522ms (= 512 + 10) → should expand to 522ms.
    EXPECT_EQ(Expand(trunc, epoch, Nanos{520'000'000}), 522'000'000);
}

TEST(TruncTime, Presets)
{
    // Verify preset type properties.
    static_assert(TruncTime16_100us::BitWidth == 16);
    static_assert(TruncTime16_100us::QuantumNs == 100'000);
    static_assert(TruncTime16_1ms::QuantumNs == 1'000'000);
    static_assert(TruncTime32_1us::BitWidth == 32);
    static_assert(TruncTime32_1us::QuantumNs == 1'000);
}

TEST(TruncTime, ExactRoundTrip)
{
    // If time is exactly on a quantum boundary, Truncate+Expand is identity.
    constexpr Nanos epoch   = 5'000'000'000LL;
    constexpr Nanos quantum = 100'000;

    for (int i = 0; i < 1000; ++i) {
        Nanos time = epoch + static_cast<Nanos>(i) * quantum;
        auto trunc = Truncate<16, quantum>(time, epoch);
        Nanos expanded = Expand(trunc, epoch, time);
        EXPECT_EQ(expanded, time);
    }
}

TEST(TruncTime, StorageTypes)
{
    static_assert(std::is_same_v<TruncTime<8, 1>::StorageType, std::uint8_t>);
    static_assert(std::is_same_v<TruncTime<16, 1>::StorageType, std::uint16_t>);
    static_assert(std::is_same_v<TruncTime<32, 1>::StorageType, std::uint32_t>);
    static_assert(std::is_same_v<TruncTime<64, 1>::StorageType, std::uint64_t>);
}
