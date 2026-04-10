#pragma once
#include "Types.h"

#include <cstdint>

namespace SynTm
{
    /// Synchronization epoch — shared time reference for a group of nodes.
    ///
    /// When two groups of previously independent nodes connect, they
    /// compare epochs and the group with the "weaker" epoch adopts the
    /// stronger one. Strength is determined by: older creation time wins;
    /// on tie, larger member count wins; on tie, lower id wins.
    struct SyncEpoch
    {
        /// Unique identifier (e.g., hash of creator node ID + creation time).
        std::uint64_t id = 0;

        /// Base synchronized time — the epoch's "origin" in nanoseconds.
        Nanos baseTime = 0;

        /// Drift rate relative to some agreed reference (typically 1/1).
        Rational rate{.num=1, .den=1};

        /// Number of nodes currently sharing this epoch.
        std::uint32_t memberCount = 0;

        /// Creation timestamp in the creator's local time (for ordering).
        Nanos createdAt = 0;

        /// Whether this epoch has been initialized (non-default).
        [[nodiscard]] bool IsValid() const noexcept { return id != 0; }

        /// Compare epoch strength. Returns true if `this` is stronger than `other`.
        /// Stronger epoch wins during group merges.
        [[nodiscard]] bool IsStrongerThan(const SyncEpoch& other) const noexcept
        {
            // Older epoch wins (lower createdAt).
            if (createdAt != other.createdAt) {
                return createdAt < other.createdAt;
            }
            // Tie-break: larger group wins.
            if (memberCount != other.memberCount) {
                return memberCount > other.memberCount;
            }
            // Tie-break: lower id wins (deterministic).
            return id < other.id;
        }

        auto operator<=>(const SyncEpoch&) const = default;
    };

    /// Metadata piggybacked on probe responses for transitive epoch propagation.
    struct EpochInfo
    {
        std::uint64_t epochId = 0;
        Nanos baseTime = 0;
        Nanos createdAt = 0;
        std::uint32_t memberCount = 0;

        static constexpr std::size_t WireSize = sizeof(std::uint64_t) + sizeof(Nanos) * 2 + sizeof(std::uint32_t);
    };

    /// Create an EpochInfo from a SyncEpoch.
    [[nodiscard]] constexpr EpochInfo ToEpochInfo(const SyncEpoch& epoch) noexcept
    {
        return EpochInfo{
            .epochId = epoch.id,
            .baseTime = epoch.baseTime,
            .createdAt = epoch.createdAt,
            .memberCount = epoch.memberCount,
        };
    }
}
