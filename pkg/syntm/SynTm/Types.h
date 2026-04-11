#pragma once
#include <cstdint>
#include <string_view>

namespace SynTm
{
    /// Ticks as a signed 64-bit integer (nanosecond resolution).
    /// Range: ~±292 years — sufficient for any realistic synchronization window.
    using Ticks = std::int64_t;

    /// Rational number for drift rate representation.
    /// Enables purely integer arithmetic in the hot path.
    /// Invariant: den > 0. A rate of 1.0 is {1, 1}.
    struct Rational
    {
        std::int64_t num = 1;
        std::int64_t den = 1;

        /// Apply this rate to a duration: (value * num) / den.
        [[nodiscard]] constexpr Ticks Apply(Ticks value) const noexcept
        {
            // Use __int128 to avoid overflow on large values.
            auto wide = static_cast<__int128>(value) * num;
            return static_cast<Ticks>(wide / den);
        }

        /// Convert to double for logging/debugging (not used in hot path).
        [[nodiscard]] constexpr double ToDouble() const noexcept
        {
            return static_cast<double>(num) / static_cast<double>(den);
        }

        auto operator<=>(const Rational&) const = default;
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
