#pragma once
#include "Types.h"

#include <cstdint>
#include <limits>
#include <type_traits>

namespace SynTm
{
    /// Truncated time representation for compact wire encoding.
    ///
    /// Stores a `Bits`-wide unsigned index representing time quantized
    /// at `Quantum` nanoseconds from an epoch base. Useful for tagging
    /// data objects with a timestamp that occupies fewer bytes than a full
    /// 64-bit nanosecond value.
    ///
    /// Example: TruncTime<16, 100'000> (16-bit, 100µs quantum)
    ///   Range:  65536 × 100µs = ~6.55 seconds before wrap-around.
    ///   Size:   2 bytes on the wire.
    ///
    /// The receiver, knowing the shared epoch base and an approximate
    /// hint of the current time, can reconstruct the full absolute time
    /// via Expand().
    template <int Bits, Nanos Quantum>
        requires (Bits > 0 && Bits <= 64 && Quantum > 0)
    struct TruncTime
    {
        static_assert(Bits <= 64);

        using StorageType = std::conditional_t<
            (Bits <= 8),  std::uint8_t,
            std::conditional_t<
                (Bits <= 16), std::uint16_t,
                std::conditional_t<
                    (Bits <= 32), std::uint32_t,
                    std::uint64_t>>>;

        StorageType index = 0;

        static constexpr Nanos QuantumNs = Quantum;
        static constexpr int BitWidth    = Bits;
        static constexpr StorageType MaxIndex =
            (Bits == 64) ? std::numeric_limits<StorageType>::max()
                         : static_cast<StorageType>((StorageType{1} << Bits) - 1);

        /// Total range in nanoseconds before wrap-around.
        static constexpr Nanos Range = static_cast<Nanos>(MaxIndex + 1) * Quantum;

        auto operator<=>(const TruncTime&) const = default;
    };

    /// Truncate an absolute nanosecond time relative to an epoch base.
    template <int Bits, Nanos Quantum>
    [[nodiscard]] constexpr TruncTime<Bits, Quantum> Truncate(
        Nanos absolute, Nanos epochBase) noexcept
    {
        using TT = TruncTime<Bits, Quantum>;
        Nanos delta = absolute - epochBase;
        auto idx = static_cast<typename TT::StorageType>(
            (static_cast<std::uint64_t>(delta) / Quantum) & TT::MaxIndex);
        return TT{.index = idx};
    }

    /// Expand a truncated time back to an absolute nanosecond value.
    ///
    /// `hint` is an approximate absolute time (e.g., the current synced time)
    /// used to resolve wrap-around ambiguity. The result is the unique absolute
    /// time that, when truncated, produces the given index AND is closest to
    /// the hint.
    template <int Bits, Nanos Quantum>
    [[nodiscard]] constexpr Nanos Expand(
        TruncTime<Bits, Quantum> trunc, Nanos epochBase, Nanos hint) noexcept
    {
        using TT = TruncTime<Bits, Quantum>;
        constexpr Nanos range = TT::Range;
        constexpr Nanos halfRange = range / 2;

        // Candidate: the absolute time for this index in the same "cycle" as the hint.
        Nanos hintDelta = hint - epochBase;
        Nanos hintCycle = (hintDelta / range) * range;
        Nanos candidate = epochBase + hintCycle + static_cast<Nanos>(trunc.index) * Quantum;

        // Pick the candidate closest to hint (might be ±1 cycle away).
        Nanos diff = candidate - hint;
        if (diff > halfRange) {
            candidate -= range;
        } else if (diff < -halfRange) {
            candidate += range;
        }

        return candidate;
    }

    // -----------------------------------------------------------------------
    // Common presets
    // -----------------------------------------------------------------------

    /// 16-bit, 100µs quantum → ~6.55s range, 2 bytes.
    using TruncTime16_100us = TruncTime<16, 100'000>;

    /// 16-bit, 1ms quantum → ~65.5s range, 2 bytes.
    using TruncTime16_1ms = TruncTime<16, 1'000'000>;

    /// 32-bit, 1µs quantum → ~4295s (~71 min) range, 4 bytes.
    using TruncTime32_1us = TruncTime<32, 1'000>;
}
