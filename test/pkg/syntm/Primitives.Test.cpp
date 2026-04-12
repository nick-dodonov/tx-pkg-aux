#include "SynTm/Clock.h"
#include "SynTm/Probe.h"
#include "SynTm/TruncTime.h"
#include "SynTm/Types.h"

#include <array>
#include <cstddef>
#include <gtest/gtest.h>

using namespace SynTm;
using namespace std::chrono_literals;

// ===========================================================================
// DriftRate
// ===========================================================================

TEST(DriftRate, IdentityRate)
{
    DriftRate r{}; // 0 ppb — no drift.
    EXPECT_EQ(r.Apply(1s), 1s);
}

TEST(DriftRate, PositiveDrift)
{
    auto r = DriftRate{500us}; // 500 µs/s = 500 ppm - remote gains 500 µs per second.
    EXPECT_EQ(r.Apply(1s), 1s + 500us);
}

TEST(DriftRate, NegativeDrift)
{
    auto r = DriftRate{-500us}; // -500 µs/s = -500 ppm - remote loses 500 µs per second.
    EXPECT_EQ(r.Apply(2s), 2s - 1ms);
}

TEST(DriftRate, LargeValuesNoOverflow)
{
    // 100 seconds * 1 ppm drift — should not overflow with __int128.
    auto r = DriftRate{1us}; // 1 µs/s = 1 ppm
    Ticks hundred_sec = 100s;
    Ticks result = r.Apply(hundred_sec);
    EXPECT_EQ(result, 100s + 100us);
}

// ===========================================================================
// FakeClock
// ===========================================================================

TEST(FakeClock, StartsAtZero)
{
    FakeClock clock;
    EXPECT_EQ(clock.Now(), Ticks{});
}

TEST(FakeClock, SetNow)
{
    FakeClock clock;
    clock.SetNow(42ms);
    EXPECT_EQ(clock.Now(), 42ms);
}

TEST(FakeClock, Advance)
{
    FakeClock clock;
    clock.SetNow(1ms);
    clock.Advance(500us);
    EXPECT_EQ(clock.Now(), 1500us);
}

TEST(FakeClock, AdvanceMultipleTimes)
{
    FakeClock clock;
    clock.Advance(100ns);
    clock.Advance(200ns);
    clock.Advance(300ns);
    EXPECT_EQ(clock.Now(), 600ns);
}

// ===========================================================================
// SteadyClock
// ===========================================================================

TEST(SteadyClock, ReturnsPositive)
{
    SteadyClock clock;
    EXPECT_GT(clock.Now(), Ticks{});
}

TEST(SteadyClock, Monotonic)
{
    SteadyClock clock;
    Ticks a = clock.Now();
    Ticks b = clock.Now();
    EXPECT_GE(b, a);
}

// ===========================================================================
// Probe — offset and RTT calculation
// ===========================================================================

TEST(Probe, ComputeResultSymmetric)
{
    // Symmetric delay: 10ms each way, zero offset.
    //   t1=0, t2=10ms, t3=10ms, t4=20ms
    auto result = ComputeProbeResult(Ticks{}, 10ms, 10ms, 20ms);
    EXPECT_EQ(result.offset, Ticks{});
    EXPECT_EQ(result.rtt, 20ms);
}

TEST(Probe, ComputeResultWithOffset)
{
    // Remote clock is 5ms ahead, symmetric 10ms RTT.
    //   t1=0, t2=15ms, t3=15ms, t4=20ms
    //   offset = ((15-0) + (15-20)) / 2 = (15 + (-5)) / 2 = 5ms
    auto result = ComputeProbeResult(Ticks{}, 15ms, 15ms, 20ms);
    EXPECT_EQ(result.offset, 5ms);
    EXPECT_EQ(result.rtt, 20ms);
}

TEST(Probe, ComputeResultWithNegativeOffset)
{
    // Remote clock is 5ms behind, symmetric 10ms RTT.
    //   t1=0, t2=5ms, t3=5ms, t4=20ms
    //   offset = ((5-0) + (5-20)) / 2 = (5 + (-15)) / 2 = -5ms
    auto result = ComputeProbeResult(Ticks{}, 5ms, 5ms, 20ms);
    EXPECT_EQ(result.offset, -5ms);
    EXPECT_EQ(result.rtt, 20ms);
}

TEST(Probe, ComputeResultAsymmetric)
{
    // Asymmetric delay: forward 5ms, return 15ms. Zero real offset.
    //   t1=0, t2=5ms, t3=5ms, t4=20ms
    //   True offset = 0, but estimated offset = -5ms (NTP asymmetry bias).
    auto result = ComputeProbeResult(Ticks{}, 5ms, 5ms, 20ms);
    // RTT = (20-0) - (5-5) = 20ms
    EXPECT_EQ(result.rtt, 20ms);
    // Offset has asymmetry bias — expected for NTP-style estimation.
    EXPECT_EQ(result.offset, -5ms);
}

TEST(Probe, ComputeResultWithProcessingDelay)
{
    // Responder takes 2ms to process: t3 - t2 = 2ms.
    //   t1=0, t2=10ms, t3=12ms, t4=22ms
    //   rtt = (22-0) - (12-10) = 20ms
    //   offset = ((10-0) + (12-22)) / 2 = (10 - 10) / 2 = 0
    auto result = ComputeProbeResult(Ticks{}, 10ms, 12ms, 22ms);
    EXPECT_EQ(result.rtt, 20ms);
    EXPECT_EQ(result.offset, Ticks{});
}

// ===========================================================================
// Probe — serialization round-trip
// ===========================================================================

TEST(ProbeSerialization, RequestRoundTrip)
{
    ProbeRequest req{.t1 = Ticks{123'456'789'012'345LL}};
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
        .t1 = Ticks{111'111'111'111LL},
        .t2 = Ticks{222'222'222'222LL},
        .t3 = Ticks{333'333'333'333LL},
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
    ProbeRequest req{.t1 = Ticks{42}};
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
    ProbeRequest req{.t1 = Ticks{-999'999'999LL}};
    std::array<std::byte, ProbeRequest::WireSize> buf{};
    [[maybe_unused]] auto written = WriteTo(buf, req);
    auto decoded = ReadProbeRequest(buf);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->t1, Ticks{-999'999'999LL});
}

// ===========================================================================
// TruncTime — truncation and expansion
// ===========================================================================

TEST(TruncTime, BasicTruncateExpand)
{
    constexpr Ticks epoch = 1000s; // 1000 seconds
    constexpr Ticks time = epoch + 500us; // +500µs
    constexpr std::int64_t quantum = 100'000; // 100µs

    auto trunc = Truncate<16, quantum>(time, epoch);
    EXPECT_EQ(trunc.index, 5u); // 500'000 / 100'000 = 5

    Ticks expanded = Expand(trunc, epoch, time);
    EXPECT_EQ(expanded, epoch + Ticks{5 * quantum}); // Exact quantum boundary.
}

TEST(TruncTime, WrapAround)
{
    constexpr Ticks epoch{};

    // Time is at 300ms → wraps around (300 mod 256 = 44).
    constexpr Ticks time = 300ms;
    auto trunc = Truncate<8, 1'000'000>(time, epoch);
    EXPECT_EQ(trunc.index, 44u);

    // Expand with hint near 300ms should give back ~300ms.
    Ticks expanded = Expand(trunc, epoch, 295ms);
    EXPECT_EQ(expanded, 300ms);
}

TEST(TruncTime, ExpandChoosesClosestToHint)
{
    constexpr Ticks epoch{};

    // Index 10 → within first cycle: 10ms.
    TruncTime<8, 1'000'000> trunc{.index = 10};

    // Hint near 10ms → should expand to 10ms.
    EXPECT_EQ(Expand(trunc, epoch, 8ms), 10ms);

    // Hint near 266ms (= 256 + 10) → should expand to 266ms.
    EXPECT_EQ(Expand(trunc, epoch, 264ms), 266ms);

    // Hint near 522ms (= 512 + 10) → should expand to 522ms.
    EXPECT_EQ(Expand(trunc, epoch, 520ms), 522ms);
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
    constexpr Ticks epoch = 5s;
    constexpr std::int64_t quantum = 100'000;

    for (int i = 0; i < 1000; ++i) {
        Ticks time = epoch + Ticks{static_cast<std::int64_t>(i) * quantum};
        auto trunc = Truncate<16, quantum>(time, epoch);
        Ticks expanded = Expand(trunc, epoch, time);
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
