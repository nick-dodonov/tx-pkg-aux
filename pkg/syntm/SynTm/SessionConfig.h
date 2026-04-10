#pragma once
#include "Types.h"

#include <cstdint>

namespace SynTm
{
    /// Tunable parameters for a sync Session.
    struct SessionConfig
    {
        /// Minimum interval between probes (nanoseconds).
        /// Used when the session is unsettled (high jitter).
        Nanos probeIntervalMin = 100'000'000; // 100ms

        /// Maximum interval between probes (nanoseconds).
        /// Used when the session is stable (low jitter).
        Nanos probeIntervalMax = 2'000'000'000; // 2s

        /// Number of samples in the filter sliding window.
        std::size_t filterWindowSize = 8;

        /// Step threshold: corrections larger than this cause a time jump.
        Nanos stepThreshold = 100'000'000; // 100ms

        /// Maximum slew rate (rational).
        Rational maxSlewRate{500, 1'000'000}; // 500ppm

        /// Jitter threshold below which the session is considered stable.
        /// When jitter < this, probe interval increases toward probeIntervalMax.
        Nanos jitterStableThreshold = 1'000'000; // 1ms

        /// Number of consecutive filter results required before
        /// the session transitions from Probing to Synced.
        std::uint32_t minSamplesForSync = 4;
    };

    /// Preset for LAN environments (~1ms RTT).
    constexpr SessionConfig LanSessionConfig()
    {
        return SessionConfig{
            .probeIntervalMin     = 100'000'000,  // 100ms
            .probeIntervalMax     = 2'000'000'000, // 2s
            .filterWindowSize     = 8,
            .stepThreshold        = 100'000'000,   // 100ms
            .maxSlewRate          = {500, 1'000'000},
            .jitterStableThreshold = 1'000'000,    // 1ms
            .minSamplesForSync    = 4,
        };
    }

    /// Preset for WAN environments (~50-200ms RTT).
    constexpr SessionConfig WanSessionConfig()
    {
        return SessionConfig{
            .probeIntervalMin     = 500'000'000,    // 500ms
            .probeIntervalMax     = 10'000'000'000LL, // 10s
            .filterWindowSize     = 16,
            .stepThreshold        = 500'000'000,    // 500ms
            .maxSlewRate          = {200, 1'000'000},
            .jitterStableThreshold = 10'000'000,   // 10ms
            .minSamplesForSync    = 6,
        };
    }
}
