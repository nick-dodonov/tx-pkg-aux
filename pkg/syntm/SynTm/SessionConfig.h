#pragma once
#include "Types.h"

#include <chrono>
#include <cstdint>

namespace SynTm
{
    using namespace std::chrono_literals;
    /// Tunable parameters for a sync Session.
    struct SessionConfig
    {
        /// Minimum interval between probes (ticks).
        /// Used when the session is unsettled (high jitter).
        Ticks probeIntervalMin = 100ms;

        /// Maximum interval between probes (ticks).
        /// Used when the session is stable (low jitter).
        Ticks probeIntervalMax = 2s;

        /// Number of samples in the filter sliding window.
        std::size_t filterWindowSize = 8;

        /// Step threshold: corrections larger than this cause a time jump.
        Ticks stepThreshold = 100ms;

        /// Maximum slew rate (ppb). Default: 500'000 ppb = 500 ppm.
        DriftRate maxSlewRate{.ppb = 500'000};

        /// Jitter threshold below which the session is considered stable.
        /// When jitter < this, probe interval increases toward probeIntervalMax.
        Ticks jitterStableThreshold = 1ms;

        /// Number of consecutive filter results required before
        /// the session transitions from Probing to Synced.
        std::uint32_t minSamplesForSync = 4;
    };

    /// Preset for LAN environments (~1ms RTT).
    constexpr SessionConfig LanSessionConfig()
    {
        return SessionConfig{
            .probeIntervalMin      = 100ms,
            .probeIntervalMax      = 2s,
            .filterWindowSize      = 8,
            .stepThreshold         = 100ms,
            .maxSlewRate           = DriftRate{.ppb = 500'000},
            .jitterStableThreshold = 1ms,
            .minSamplesForSync     = 4,
        };
    }

    /// Preset for WAN environments (~50-200ms RTT).
    constexpr SessionConfig WanSessionConfig()
    {
        return SessionConfig{
            .probeIntervalMin      = 500ms,
            .probeIntervalMax      = 10s,
            .filterWindowSize      = 16,
            .stepThreshold         = 500ms,
            .maxSlewRate           = DriftRate{.ppb = 200'000},
            .jitterStableThreshold = 10ms,
            .minSamplesForSync     = 6,
        };
    }
}
