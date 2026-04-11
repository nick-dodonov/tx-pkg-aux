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
        SynTm::Ticks lastUpdateSyncTime{};

        // Sync metrics (from Consensus session)
        SynTm::Ticks offset{};
        SynTm::Ticks rtt{};
        SynTm::Ticks jitter{};
        SynTm::SyncQuality syncQuality = SynTm::SyncQuality::None;

        // Shot accuracy
        SynTm::Ticks lastShotError{};
        int shotCount = 0;
        SynTm::Ticks totalShotError{};

        /// Extrapolate position at the given synced time based on last known state.
        [[nodiscard]] float EstimatePosition(SynTm::Ticks currentSyncTime) const noexcept
        {
            if (lastUpdateSyncTime == SynTm::Ticks{}) {
                return lastPosition;
            }
            float dt = std::chrono::duration<float>(currentSyncTime - lastUpdateSyncTime).count();
            return lastPosition + lastVelocity * dt;
        }

        /// Seconds since last state update.
        [[nodiscard]] float TimeSinceLastUpdate(SynTm::Ticks currentSyncTime) const noexcept
        {
            if (lastUpdateSyncTime == SynTm::Ticks{}) {
                return 0.0f;
            }
            return std::chrono::duration<float>(currentSyncTime - lastUpdateSyncTime).count();
        }
    };
}
