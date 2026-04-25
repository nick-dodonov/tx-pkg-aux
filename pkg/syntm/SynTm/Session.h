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
            : _logger("Session", config.parentLogger)
            , _clock(clock)
            , _config(std::move(config))
            , _filter(_config.filterWindowSize)
            , _driftModel(SteerPolicy{
                .stepThreshold = _config.stepThreshold,
                .maxSlewRate   = _config.maxSlewRate,
                .parentLogger  = _logger,
            })
        {
            assert(config.filterWindowSize >= config.minSamplesForSync &&
                   "filterWindowSize must be >= minSamplesForSync");
        }

        [[nodiscard]] const auto& GetLogger() const noexcept { return _logger; }

        /// Whether it's time to send a new probe request.
        [[nodiscard]] bool ShouldProbe() const noexcept
        {
            if (!_everProbed) {
                return true;
            }
            const auto now = _clock.Now();
            return (now - _lastProbeSentAt) >= CurrentProbeInterval();
        }

        /// Create a probe request to send to the remote peer.
        [[nodiscard]] ProbeRequest MakeProbeRequest()
        {
            const auto now = _clock.Now();
            _lastProbeSentAt = now;
            _everProbed = true;
            if (_state == SessionState::Idle) {
                _state = SessionState::Probing;
            }
            _logger.Trace("t1={}ns", Log::Sep{now.count()});
            return ProbeRequest{.t1 = now};
        }

        /// Handle an incoming probe request from a remote peer.
        /// Returns a response to send back.
        [[nodiscard]] ProbeResponse HandleProbeRequest(const ProbeRequest& req) const
        {
            const auto t2 = _clock.Now();
            _logger.Trace("t1={}ns -> t2={}ns", Log::Sep{req.t1.count()}, Log::Sep{t2.count()});
            return ProbeResponse{
                .t1 = req.t1,
                .t2 = t2,
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
            const auto t4 = _clock.Now();
            const auto probe = ComputeProbeResult(resp.t1, resp.t2, resp.t3, t4);
            _logger.Trace("result: offset={}ns rtt={}ns", Log::Sep{probe.offset.count()}, Log::Sep{probe.rtt.count()});

            const auto filterResult = _filter.AddSample(t4, probe);
            const auto sampleCount = filterResult.sampleCount;

            _logger.Trace("filter: offset={}ns rate={}ns/s ({:.6f}) jitter={}ns minRtt={}ns sampleCount={}",
                Log::Sep{filterResult.offset.count()}, filterResult.rate.count(), filterResult.rate.ToDouble(),
                Log::Sep{filterResult.jitter.count()}, Log::Sep{filterResult.minRtt.count()}, sampleCount);

            const auto stepped = _driftModel.Steer(t4, filterResult);

            auto enteredResyncing = false;
            auto exitedResyncing = false;

            // Update state.
            if (stepped) {
                enteredResyncing = (_state != SessionState::Resyncing);
                _state = SessionState::Resyncing;
                // Reset filter so post-step probes build a fresh window —
                // stale pre-step samples corrupt the drift estimate and cause
                // cascading re-steps.
                _filter.Reset();
                _logger.Trace("state: STEPPED -> Resyncing (enteredResyncing={}) filter reset", enteredResyncing);
            } else if (_state == SessionState::Probing && sampleCount >= _config.minSamplesForSync) {
                _state = SessionState::Synced;
                _logger.Trace("state: Probing -> Synced (sampleCount={})", sampleCount);
            } else if (_state == SessionState::Resyncing && sampleCount >= _config.minSamplesForSync) {
                exitedResyncing = true;
                _state = SessionState::Synced;
                _logger.Trace("state: Resyncing -> Synced (sampleCount={})", sampleCount);
            } else {
                _logger.Trace("state: {} sampleCount={}/{}", SessionStateToString(_state), sampleCount, _config.minSamplesForSync);
            }

            return {
                .filterResult = filterResult,
                .stepped = stepped,
                .enteredResyncing = enteredResyncing,
                .exitedResyncing = exitedResyncing,
            };
        }

        /// Convert a local time to the estimated remote peer time.
        [[nodiscard]] Ticks ToRemoteTime(Ticks localTime) const noexcept
        {
            return _driftModel.Convert(localTime);
        }

        /// Current local time (raw clock).
        [[nodiscard]] Ticks LocalNow() const noexcept
        {
            return _clock.Now();
        }

        /// Estimated current time on the remote peer.
        [[nodiscard]] Ticks RemoteNow() const noexcept
        {
            return ToRemoteTime(_clock.Now());
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
            _lastProbeSentAt = {};
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
            const auto sampleCount = _filter.SampleCount();
            if (sampleCount < _config.filterWindowSize / 2) {
                return _config.probeIntervalMin;
            }

            // Linear interpolation between min and max based on fill ratio.
            const auto range = _config.probeIntervalMax - _config.probeIntervalMin;
            auto ratio =
                static_cast<std::int64_t>(sampleCount * 2) /
                static_cast<std::int64_t>(_config.filterWindowSize);
            ratio = std::min(ratio, std::int64_t{2});

            return _config.probeIntervalMin + range * (ratio - 1);
        }

        Log::Logger _logger;
        IClock& _clock;
        SessionConfig _config;
        Filter _filter;
        DriftModel _driftModel;

        SessionState _state = SessionState::Idle;
        Ticks _lastProbeSentAt{};
        bool _everProbed = false;
    };
}
