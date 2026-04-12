#pragma once
#include <chrono>
#include <cstdint>
#include <string_view>

namespace SynTm
{
    /// Duration with nanosecond resolution.
    /// Using std::chrono::duration enforces unit-safe arithmetic and enables
    /// chrono literals (1ms, 2s) throughout the codebase.
    using Ticks = std::chrono::nanoseconds;

    /// Clock drift rate in parts per billion (ppb).
    ///
    /// Semantics: `ppb = (rate - 1.0) × 10⁹`
    ///   - `ppb = 0`        → no drift (rate = 1.0, local and remote advance equally)
    ///   - `ppb = +1'000`   → +1 ppm — remote gains 1 µs every second
    ///   - `ppb = -500'000` → −500 ppm — remote loses 0.5 ms every second
    ///
    /// Range: int32 covers ±2.1×10⁹ ppb = ±2.1× rate — far beyond any realistic
    /// crystal drift (±1000 ppm = ±10⁶ ppb uses < 0.05% of the range).
    struct DriftRate
    {
        std::int32_t ppb = 0;

        /// Apply this rate to an elapsed duration: result ≈ value × (1 + ppb×10⁻⁹).
        ///
        /// Uses __int128 to handle the intermediate product without overflow.
        ///
        /// TODO(opt): decomposed form avoids __int128 and is safe for all realistic drifts:
        ///   return value + Ticks{value.count() * ppb / 1'000'000'000LL};
        ///   Safety bound: |elapsed.count() * ppb| ≤ INT64_MAX (~9.22×10¹⁸).
        ///   For Ticks=ns and |ppb| ≤ 10⁶ (1000 ppm): safe up to ~2.56 h elapsed.
        ///   In practice elapsed resets on every step/slew (seconds), so always safe.
        ///
        /// TODO(opt): shift variant — eliminates division entirely:
        ///   store ppb_30 = ppb * 2³⁰ / 10⁹, Apply = value + (value * ppb_30) >> 30.
        ///   2³⁰ ≈ 1.07×10⁹ introduces ~7% representational error vs true ppb.
        ///   Consider only if Apply appears as a profiling hot spot (currently
        ///   one call site: DriftModel::Convert).
        [[nodiscard]] constexpr Ticks Apply(Ticks value) const noexcept
        {
            // 10⁹ ppb denominator — exact for the ppb unit.
            constexpr std::int64_t kDen = 1'000'000'000LL;
            auto wide = static_cast<__int128>(value.count()) * (kDen + ppb);
            return Ticks{static_cast<std::int64_t>(wide / kDen)};
        }

        /// Convert to the underlying rate scalar for logging/debugging.
        /// Returns a value near 1.0; significant deviation (e.g. 0.5 or 2.0)
        /// indicates a problem. Not used in the hot path.
        [[nodiscard]] constexpr double ToDouble() const noexcept
        {
            return 1.0 + static_cast<double>(ppb) / 1'000'000'000.0;
        }

        auto operator<=>(const DriftRate&) const = default;
    };

    /// Sync quality reported by a session or the consensus layer.
    enum class SyncQuality : std::uint8_t
    {
        None,   ///< No synchronization data yet.
        Low,    ///< Few samples, high jitter.
        High,   ///< Stable offset and drift estimates.
    };

    constexpr std::string_view SyncQualityToString(SyncQuality q) noexcept
    {
        switch (q)
        {
            case SyncQuality::None:   return "None";
            case SyncQuality::Low:    return "Low";
            case SyncQuality::High:   return "High";
        }
        return "Unknown";
    }

    /// Events emitted by the consensus layer on sync state transitions.
    enum class SyncEvent : std::uint8_t
    {
        SyncAcquired,    ///< Synchronization established for the first time (or re-established after SyncLost).
        EpochChanged,    ///< The sync epoch changed (group merge — this side adopted the other's epoch).
        ResyncStarted,   ///< A step correction was applied — time jumped, quality is uncertain until ResyncCompleted.
        ResyncCompleted, ///< Resyncing converged — time estimate is stable again.
        SyncLost,        ///< Lost all synchronized peers.
    };

    constexpr std::string_view SyncEventToString(SyncEvent e) noexcept
    {
        switch (e)
        {
            case SyncEvent::SyncAcquired:    return "SyncAcquired";
            case SyncEvent::EpochChanged:    return "EpochChanged";
            case SyncEvent::ResyncStarted:   return "ResyncStarted";
            case SyncEvent::ResyncCompleted: return "ResyncCompleted";
            case SyncEvent::SyncLost:        return "SyncLost";
        }
        return "Unknown";
    }
}
