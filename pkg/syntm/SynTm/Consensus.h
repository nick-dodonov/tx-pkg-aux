#pragma once
#include "Clock.h"
#include "Epoch.h"
#include "Session.h"
#include "SessionConfig.h"
#include "Types.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>

namespace SynTm
{
    /// Consensus participation mode.
    enum class ConsensusMode : std::uint8_t
    {
        Voter,  ///< Full participant — contributes to epoch and syncs bidirectionally.
        Viewer, ///< Passive — observes time from peers without voting.
    };

    /// Multi-link consensus manager.
    ///
    /// Owns per-link Sessions and aggregates their results into a single
    /// synchronized time. Handles epoch propagation and group merges.
    ///
    /// This class is NOT thread-safe. The caller must serialize calls
    /// (typically from a single event loop / exec::Domain).
    class Consensus
    {
    public:
        using EventCallback = std::function<void(SyncEvent)>;

        explicit Consensus(IClock& clock, ConsensusMode mode = ConsensusMode::Voter, SessionConfig config = {})
            : _clock(clock)
            , _mode(mode)
            , _config(config)
        {}

        /// Register a callback for sync events.
        void OnEvent(EventCallback callback)
        {
            _eventCallback = std::move(callback);
        }

        // -------------------------------------------------------------------
        // Link management
        // -------------------------------------------------------------------

        /// Add a new link/peer. Returns false if the peer already exists.
        bool AddPeer(const std::string& peerId)
        {
            if (_peers.contains(peerId)) {
                return false;
            }
            _peers.emplace(peerId, Session(_clock, _config));

            // If we have no epoch yet, create one.
            if (!_epoch.IsValid()) {
                CreateEpoch();
            }

            return true;
        }

        /// Remove a peer. Returns false if not found.
        bool RemovePeer(const std::string& peerId)
        {
            auto erased = _peers.erase(peerId);
            if (erased == 0) {
                return false;
            }

            // If no peers left, emit SyncLost.
            if (_peers.empty() && _synced) {
                _synced = false;
                EmitEvent(SyncEvent::SyncLost);
            }

            return true;
        }

        /// Get a session for a peer (for direct access).
        [[nodiscard]] Session* GetSession(const std::string& peerId)
        {
            auto it = _peers.find(peerId);
            if (it == _peers.end()) {
                return nullptr;
            }
            return &it->second;
        }

        [[nodiscard]] const Session* GetSession(const std::string& peerId) const
        {
            auto it = _peers.find(peerId);
            if (it == _peers.end()) {
                return nullptr;
            }
            return &it->second;
        }

        // -------------------------------------------------------------------
        // Probe exchange (delegated to per-peer Session)
        // -------------------------------------------------------------------

        /// Check if a specific peer needs probing.
        [[nodiscard]] bool ShouldProbe(const std::string& peerId) const
        {
            const auto* session = GetSession(peerId);
            return session && session->ShouldProbe();
        }

        /// Create a probe request for a peer.
        [[nodiscard]] std::optional<ProbeRequest> MakeProbeRequest(const std::string& peerId)
        {
            auto* session = GetSession(peerId);
            if (!session) {
                return std::nullopt;
            }
            return session->MakeProbeRequest();
        }

        /// Handle an incoming probe request from a peer.
        [[nodiscard]] std::optional<ProbeResponse> HandleProbeRequest(const std::string& peerId, const ProbeRequest& req)
        {
            auto* session = GetSession(peerId);
            if (!session) {
                return std::nullopt;
            }
            return session->HandleProbeRequest(req);
        }

        /// Handle a probe response from a peer, plus their epoch info.
        void HandleProbeResponse(
            const std::string& peerId,
            const ProbeResponse& resp,
            std::optional<EpochInfo> remoteEpoch = std::nullopt)
        {
            auto* session = GetSession(peerId);
            if (!session) {
                return;
            }

            auto result = session->HandleProbeResponse(resp);

            // Process epoch merge if the remote provided epoch info.
            if (remoteEpoch) {
                HandleRemoteEpoch(*remoteEpoch);
            }

            // Emit ResyncStarted only when the session first enters Resyncing —
            // not on every individual step within the same episode.
            if (result.enteredResyncing) {
                EmitEvent(SyncEvent::ResyncStarted);
            }

            // Check if we've just acquired sync for the first time (or after SyncLost).
            if (!_synced && HasAnySyncedPeer()) {
                _synced = true;
                EmitEvent(SyncEvent::SyncAcquired);
            } else if (_synced && result.exitedResyncing) {
                // Session returned to Synced after a resync episode.
                EmitEvent(SyncEvent::ResyncCompleted);
            }
        }

        // -------------------------------------------------------------------
        // Epoch management
        // -------------------------------------------------------------------

        /// Get the current epoch.
        [[nodiscard]] const SyncEpoch& Epoch() const noexcept { return _epoch; }

        /// Get our epoch info for sending to peers.
        [[nodiscard]] EpochInfo OurEpochInfo() const noexcept
        {
            return ToEpochInfo(_epoch);
        }

        /// Handle epoch info received from a remote peer.
        void HandleRemoteEpoch(const EpochInfo& remote)
        {
            if (!_epoch.IsValid()) {
                // Adopt the remote epoch.
                AdoptEpoch(remote);
                return;
            }

            if (remote.epochId == _epoch.id) {
                // Same epoch — update member count if larger.
                _epoch.memberCount = std::max(_epoch.memberCount, remote.memberCount);
                return;
            }

            // Different epoch — compare strength.
            SyncEpoch remoteEpoch{
                .id          = remote.epochId,
                .baseTime    = remote.baseTime,
                .rate        = DriftRate{},
                .memberCount = remote.memberCount,
                .createdAt   = remote.createdAt,
            };

            if (_mode == ConsensusMode::Viewer || remoteEpoch.IsStrongerThan(_epoch)) {
                // Adopt the stronger epoch.
                AdoptEpoch(remote);
                EmitEvent(SyncEvent::EpochChanged);
            }
            // Otherwise, keep our epoch. The remote will eventually adopt ours
            // when they receive our epoch info.
        }

        // -------------------------------------------------------------------
        // Synchronized time
        // -------------------------------------------------------------------

        /// Get the best estimate of synchronized time right now.
        [[nodiscard]] Ticks SyncedNow() const noexcept
        {
            // Use the best peer session's synced time.
            const Session* bestSession = BestSyncedSession();
            if (bestSession) {
                return bestSession->SyncedNow();
            }
            // No synced peer — return local time.
            return _clock.Now();
        }

        /// Convert a local time to synced time using the best available session.
        [[nodiscard]] Ticks ToSyncedTime(Ticks localTime) const noexcept
        {
            const Session* bestSession = BestSyncedSession();
            if (bestSession) {
                return bestSession->ToSyncedTime(localTime);
            }
            return localTime;
        }

        /// Whether we have at least one synced peer.
        [[nodiscard]] bool IsSynced() const noexcept { return _synced; }

        /// Best sync quality across all peers.
        [[nodiscard]] SyncQuality Quality() const noexcept
        {
            SyncQuality best = SyncQuality::None;
            for (const auto& [_, session] : _peers) {
                auto q = session.Quality();
                best = std::max(q, best);
            }
            return best;
        }

        /// Number of connected peers.
        [[nodiscard]] std::size_t PeerCount() const noexcept { return _peers.size(); }

        /// Consensus mode.
        [[nodiscard]] ConsensusMode Mode() const noexcept { return _mode; }

    private:
        void CreateEpoch()
        {
            Ticks now = _clock.Now();
            // Simple epoch ID: hash of local time.
            _epoch = SyncEpoch{
                .id          = static_cast<std::uint64_t>(now.count()) ^ 0x5A5A5A5A'5A5A5A5AULL,
                .baseTime    = now,
                .rate        = DriftRate{},
                .memberCount = 1,
                .createdAt   = now,
            };
        }

        void AdoptEpoch(const EpochInfo& info)
        {
            _epoch = SyncEpoch{
                .id          = info.epochId,
                .baseTime    = info.baseTime,
                .rate        = DriftRate{},
                .memberCount = info.memberCount,
                .createdAt   = info.createdAt,
            };

            // Reset all sessions to re-sync under new epoch.
            for (auto& [_, session] : _peers) {
                session.Reset();
            }
            _synced = false;
        }

        [[nodiscard]] bool HasAnySyncedPeer() const noexcept
        {
            for (const auto& [_, session] : _peers) {
                if (session.State() == SessionState::Synced) {
                    return true;
                }
            }
            return false;
        }

        /// Find the best synced session (highest quality).
        [[nodiscard]] const Session* BestSyncedSession() const noexcept
        {
            const Session* best = nullptr;
            SyncQuality bestQuality = SyncQuality::None;

            for (const auto& [_, session] : _peers) {
                auto q = session.Quality();
                if (q > bestQuality) {
                    bestQuality = q;
                    best = &session;
                }
            }
            return best;
        }

        void EmitEvent(SyncEvent event) const
        {
            if (_eventCallback) {
                _eventCallback(event);
            }
        }

        IClock& _clock;
        ConsensusMode _mode;
        SessionConfig _config;
        SyncEpoch _epoch;
        bool _synced = false;

        std::unordered_map<std::string, Session> _peers;
        EventCallback _eventCallback;
    };
}
