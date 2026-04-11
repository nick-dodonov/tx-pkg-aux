#pragma once
#include "Clock.h"
#include "DriftModel.h"
#include "Filter.h"
#include "Probe.h"
#include "SessionConfig.h"
#include "Types.h"

#include "Log/Log.h"
#include "Log/Sep.h"

#include <cassert>
#include <cstdint>

namespace SynTm
{
    /// State of a per-link synchronization session.
    enum class SessionState : std::uint8_t
    {
        Idle,      ///< No probes sent yet.
        Probing,   ///< Collecting samples, not yet stable.
        Synced,    ///< Stable synchronization achieved.
        Resyncing, ///< Re-synchronizing after a disruption.
    };

    constexpr std::string_view SessionStateToString(SessionState s) noexcept
    {
        switch (s)
        {
            case SessionState::Idle:      return "Idle";
            case SessionState::Probing:   return "Probing";
            case SessionState::Synced:    return "Synced";
            case SessionState::Resyncing: return "Resyncing";
        }
        return "Unknown";
    }

    /// Per-link synchronization session.
    ///
    /// Manages the probe exchange protocol and offset/drift estimation
    /// for a single link to a remote peer. The caller drives all I/O —
    /// this class produces byte messages and consumes byte messages,
    /// but never touches a socket or timer directly.
    ///
    /// Typical usage:
    ///   1. Call ShouldProbe(now) periodically.
    ///   2. When true, call MakeProbeRequest() and send the bytes.
    ///   3. When a probe request arrives, call HandleProbeRequest() and send the response.
    ///   4. When a probe response arrives, call HandleProbeResponse().
    class Session
    {
    public:
        explicit Session(IClock& clock, SessionConfig config = {})
            : _clock(clock)
            , _config(config)
            , _filter(config.filterWindowSize)
            , _driftModel(SteerPolicy{
                  .stepThreshold = config.stepThreshold,
                  .maxSlewRate   = config.maxSlewRate,
              })
        {
            assert(config.filterWindowSize >= config.minSamplesForSync &&
                   "filterWindowSize must be >= minSamplesForSync");
        }

        /// Whether it's time to send a new probe request.
        [[nodiscard]] bool ShouldProbe() const noexcept
        {
            if (!_everProbed) {
                return true;
            }
            Ticks now = _clock.Now();
            return (now - _lastProbeSentAt) >= CurrentProbeInterval();
        }

        /// Create a probe request to send to the remote peer.
        /// Records t1 internally for later matching.
        [[nodiscard]] ProbeRequest MakeProbeRequest()
        {
            Ticks now = _clock.Now();
            _lastProbeSentAt = now;
            _pendingT1 = now;
            _everProbed = true;
            if (_state == SessionState::Idle) {
                _state = SessionState::Probing;
            }
            return ProbeRequest{.t1 = now};
        }

        /// Handle an incoming probe request from a remote peer.
        /// Returns a response to send back.
        [[nodiscard]] ProbeResponse HandleProbeRequest(const ProbeRequest& req) const
        {
            Ticks now = _clock.Now();
            return ProbeResponse{
                .t1 = req.t1,
                .t2 = now,
                .t3 = _clock.Now(), // Capture tx time separately for accuracy.
            };
        }

        /// Handle an incoming probe response.
        struct ProbeHandleResult
        {
            FilterResult filterResult;
            /// A step correction was applied to the drift model.
            bool stepped = false;
            /// The session just transitioned INTO Resyncing (first step of a new episode).
            /// False if the session was already in Resyncing before this probe.
            bool enteredResyncing = false;
            /// The session just transitioned OUT OF Resyncing into Synced.
            bool exitedResyncing = false;
        };

        [[nodiscard]] ProbeHandleResult HandleProbeResponse(const ProbeResponse& resp)
        {
            Ticks t4 = _clock.Now();
            auto probe = ComputeProbeResult(resp.t1, resp.t2, resp.t3, t4);

            Log::Trace("probe offset={}ns rtt={}ns", Log::Sep{probe.offset}, Log::Sep{probe.rtt});

            auto filterResult = _filter.AddSample(t4, probe);
            const auto sampleCount = filterResult.sampleCount;

            Log::Trace("filter: offset={}ns rate={}/{}({}) jitter={}ns minRtt={}ns sampleCount={}",
                Log::Sep{filterResult.offset}, Log::Sep{filterResult.rate.num}, Log::Sep{filterResult.rate.den}, filterResult.rate.ToDouble(),
                Log::Sep{filterResult.jitter}, Log::Sep{filterResult.minRtt}, sampleCount);

            bool stepped = _driftModel.Steer(t4, filterResult);

            bool enteredResyncing = false;
            bool exitedResyncing = false;

            // Update state.
            if (stepped) {
                enteredResyncing = (_state != SessionState::Resyncing);
                _state = SessionState::Resyncing;
                // Reset filter so post-step probes build a fresh window —
                // stale pre-step samples corrupt the drift estimate and cause
                // cascading re-steps.
                _filter.Reset();
                Log::Trace("state: STEPPED -> Resyncing (enteredResyncing={}) filter reset", enteredResyncing);
            } else if (_state == SessionState::Probing && sampleCount >= _config.minSamplesForSync) {
                _state = SessionState::Synced;
                Log::Trace("state: Probing -> Synced (sampleCount={})", sampleCount);
            } else if (_state == SessionState::Resyncing && sampleCount >= _config.minSamplesForSync) {
                exitedResyncing = true;
                _state = SessionState::Synced;
                Log::Trace("state: Resyncing -> Synced (sampleCount={})", sampleCount);
            } else {
                Log::Trace("state: {} sampleCount={}/{}", SessionStateToString(_state), sampleCount, _config.minSamplesForSync);
            }

            return {.filterResult = filterResult,
                    .stepped = stepped,
                    .enteredResyncing = enteredResyncing,
                    .exitedResyncing = exitedResyncing};
        }

        /// Convert a local time to synchronized time using the current model.
        [[nodiscard]] Ticks ToSyncedTime(Ticks localTime) const noexcept
        {
            return _driftModel.Convert(localTime);
        }

        /// Get the current synchronized time.
        [[nodiscard]] Ticks SyncedNow() const noexcept
        {
            return ToSyncedTime(_clock.Now());
        }

        /// Current session state.
        [[nodiscard]] SessionState State() const noexcept { return _state; }

        /// Current sync quality derived from state and jitter.
        [[nodiscard]] SyncQuality Quality() const noexcept
        {
            switch (_state)
            {
                case SessionState::Idle:
                    return SyncQuality::None;
                case SessionState::Probing:
                case SessionState::Resyncing:
                    return SyncQuality::Low;
                case SessionState::Synced:
                    return SyncQuality::High;
            }
            return SyncQuality::None;
        }

        /// Access the underlying filter (for inspection/testing).
        [[nodiscard]] const Filter& GetFilter() const noexcept { return _filter; }

        /// Access the underlying drift model (for inspection/testing).
        [[nodiscard]] const DriftModel& GetDriftModel() const noexcept { return _driftModel; }

        /// Access config.
        [[nodiscard]] const SessionConfig& Config() const noexcept { return _config; }

        /// Reset the session (e.g., on epoch change).
        void Reset()
        {
            _state = SessionState::Idle;
            _lastProbeSentAt = 0;
            _pendingT1 = 0;
            _everProbed = false;
            _filter.Reset();
            _driftModel.Reset();
        }

    private:
        /// Adaptive probe interval: shorter when unsettled, longer when stable.
        [[nodiscard]] Ticks CurrentProbeInterval() const noexcept
        {
            if (_state == SessionState::Idle || _state == SessionState::Probing) {
                return _config.probeIntervalMin;
            }

            // Once synced, scale interval based on filter sample count.
            // More samples → more stable → longer interval.
            auto sampleCount = _filter.SampleCount();
            if (sampleCount < _config.filterWindowSize / 2) {
                return _config.probeIntervalMin;
            }

            // Linear interpolation between min and max based on fill ratio.
            Ticks range = _config.probeIntervalMax - _config.probeIntervalMin;
            auto ratio = static_cast<Ticks>(sampleCount * 2) /
                         static_cast<Ticks>(_config.filterWindowSize);
            if (ratio > 2) { //NOLINT(readability-use-std-min-max)
                ratio = 2;
            }
            return _config.probeIntervalMin + (range * (ratio - 1)) / 1;
        }

        IClock& _clock;
        SessionConfig _config;
        Filter _filter;
        DriftModel _driftModel;

        SessionState _state = SessionState::Idle;
        Ticks _lastProbeSentAt = 0;
        Ticks _pendingT1 = 0;
        bool _everProbed = false;
    };
}
