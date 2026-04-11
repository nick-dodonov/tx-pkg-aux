#pragma once
#include "SynTm/SyncClock.h"
#include "RunLoop/Handler.h"
#include "RunLoop/UpdateCtx.h"

#include <atomic>
#include <cmath>
#include <string>

namespace Demo
{
    /// Frame-driven physics simulation handler.
    ///
    /// Runs on CompositeHandler's update loop alongside Exec::Domain.
    /// Advances local position, updates SyncClock's atomic snapshot,
    /// and checks coordinated shot timing.
    class SimHandler : public RunLoop::Handler
    {
    public:
        explicit SimHandler(std::string nodeId, SynTm::SyncClock& syncClock)
            : _nodeId(std::move(nodeId))
            , _syncClock(syncClock)
        {}

        // -- Sim state (written by SimHandler on Update, read by DemoNode) --

        float position = 0.0f;
        float velocity = 1.0f;
        float speedMultiplier = 1.0f;

        /// Set by DemoNode when a coordinated shot is scheduled.
        std::atomic<SynTm::Ticks> nextShotTime{0};

        /// Set to true by Update when the shot fires. DemoNode reads & clears.
        std::atomic<bool> shotFired{false};

        /// Actual synced time when the shot was detected.
        std::atomic<SynTm::Ticks> shotActualTime{0};

        // -- Handler interface --

        void Update(const RunLoop::UpdateCtx& ctx) override
        {
            // Refresh SyncClock atomic snapshot every frame.
            _syncClock.Update();

            float dt = ctx.frame.deltaSeconds * speedMultiplier;

            // Simple 1D motion with sinusoidal velocity perturbation.
            float t = ctx.session.passedSeconds;
            velocity = 1.0f + 0.5f * std::sin(t * 0.7f);
            position += velocity * dt;

            // Check shot timing.
            auto expected = nextShotTime.load(std::memory_order_relaxed);
            if (expected > 0) {
                auto now = _syncClock.NowNanos();
                if (now >= expected) {
                    shotActualTime.store(now, std::memory_order_relaxed);
                    shotFired.store(true, std::memory_order_relaxed);
                    nextShotTime.store(0, std::memory_order_relaxed);
                }
            }
        }

    private:
        std::string _nodeId;
        SynTm::SyncClock& _syncClock;
    };
}
