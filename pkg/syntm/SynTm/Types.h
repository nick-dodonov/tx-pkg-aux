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

    /// Clock drift rate: nanoseconds of correction per second of elapsed time (ns/s).
    ///
    /// Backed by std::chrono::duration<int32_t, std::nano>. The stored count()
    /// is numerically identical to the industry-standard "parts per billion" (ppb),
    /// and 1/1000 of "parts per million" (ppm):
    ///
    ///   count() == 0        → no drift (rate = 1.0, clocks advance equally)
    ///   count() == +1'000   → 1 µs/s = 1 ppm  (remote gains 1 µs per second)
    ///   count() == +500'000 → 500 µs/s = 500 ppm
    ///
    /// Construct from ppm-scale values via duration_cast from microseconds:
    ///   std::chrono::duration_cast<DriftRate>(500us)   // 500 µs/s = 500 ppm
    ///   std::chrono::duration_cast<DriftRate>(1us)     // 1 µs/s   = 1 ppm
    ///
    /// Range: int32_t covers ±2.1×10⁹ ns/s — far beyond any realistic crystal
    /// drift (±1000 ppm = ±10⁶ ns/s uses < 0.05% of the range).
    struct DriftRate : std::chrono::duration<std::int32_t, std::nano>
    {
        using Base = std::chrono::duration<std::int32_t, std::nano>;
        using Base::Base;

        /// Explicit conversion from any duration type (e.g. DriftRate{500us} = 500 µs/s = 500 ppm).
        template <typename Rep2, typename Period2>
        explicit constexpr DriftRate(std::chrono::duration<Rep2, Period2> d) noexcept
            : Base{std::chrono::duration_cast<Base>(d)}
        {
        }

        /// Apply this rate to an elapsed duration: result ≈ elapsed × (1 + count()/period::den).
        ///
        /// Uses __int128 to handle the intermediate product without overflow.
        ///
        /// TODO(opt): decomposed form avoids __int128 and is safe for all realistic drifts:
        ///   return elapsed + Ticks{elapsed.count() * count() / period::den};
        ///   Safety bound: |elapsed.count() * count()| ≤ INT64_MAX (~9.22×10¹⁸).
        ///   For Ticks=ns and |count()| ≤ 10⁶ (1000 ppm): safe up to ~2.56 h elapsed.
        ///   In practice elapsed resets on every step/slew (seconds), so always safe.
        ///
        /// TODO(opt): shift variant — eliminates division entirely:
        ///   store val_30 = count() * 2³⁰ / period::den; Apply = elapsed + (elapsed * val_30) >> 30.
        ///   2³⁰ ≈ 1.07×10⁹ introduces ~7% representational error.
        ///   Consider only if Apply appears as a profiling hot spot (currently
        ///   one call site: DriftModel::Convert).
        [[nodiscard]] constexpr Ticks Apply(Ticks elapsed) const noexcept
        {
            constexpr auto kDen = static_cast<std::int64_t>(period::den);
            auto wide = static_cast<__int128>(elapsed.count()) * (kDen + count());
            return Ticks{static_cast<std::int64_t>(wide / kDen)};
        }

        /// Convert to the underlying rate scalar for logging/debugging.
        /// Returns a value near 1.0; significant deviation (e.g. 0.5 or 2.0)
        /// indicates a problem. Not used in the hot path.
        [[nodiscard]] constexpr double ToDouble() const noexcept
        {
            return 1.0 + static_cast<double>(count()) / static_cast<double>(period::den);
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
