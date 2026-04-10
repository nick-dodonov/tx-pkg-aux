#pragma once
#include "SynTm/Types.h"

#include <string>

namespace Demo
{
    /// Representation of a remote peer's observable state.
    struct PeerView
    {
        std::string peerId;

        // Simulation state (from StateUpdate messages)
        float lastPosition = 0.0f;
        float lastVelocity = 0.0f;
        SynTm::Nanos lastUpdateSyncTime = 0;

        // Sync metrics (from Consensus session)
        SynTm::Nanos offset = 0;
        SynTm::Nanos rtt = 0;
        SynTm::Nanos jitter = 0;
        SynTm::SyncQuality syncQuality = SynTm::SyncQuality::None;

        // Shot accuracy
        SynTm::Nanos lastShotError = 0;
        int shotCount = 0;
        SynTm::Nanos totalShotError = 0;

        /// Extrapolate position at the given synced time based on last known state.
        [[nodiscard]] float EstimatePosition(SynTm::Nanos currentSyncTime) const noexcept
        {
            if (lastUpdateSyncTime == 0) {
                return lastPosition;
            }
            float dt = static_cast<float>(currentSyncTime - lastUpdateSyncTime) / 1'000'000'000.0f;
            return lastPosition + lastVelocity * dt;
        }

        /// Seconds since last state update.
        [[nodiscard]] float TimeSinceLastUpdate(SynTm::Nanos currentSyncTime) const noexcept
        {
            if (lastUpdateSyncTime == 0) {
                return 0.0f;
            }
            return static_cast<float>(currentSyncTime - lastUpdateSyncTime) / 1'000'000'000.0f;
        }
    };
}
