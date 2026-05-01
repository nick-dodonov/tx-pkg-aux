#pragma once
#include "Clock.h"
#include "Epoch.h"
#include "Session.h"
#include "SessionConfig.h"
#include "Types.h"
#include "pkg/log/Log/Log.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <optional>
#include <ranges>
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
            : _logger("Consensus", config.parentLogger)
            , _clock(clock)
            , _mode(mode)
            , _config(std::move(config))
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
            auto sessionConfig = _config;
            sessionConfig.parentLogger = Log::Logger(std::format("[-{}]", peerId), _config.parentLogger);
            _peers.emplace(peerId, Session(_clock, sessionConfig));

            // If we have no epoch yet, create one.
            if (!_epoch.IsValid()) {
                CreateEpoch();
            }

            return true;
        }

        /// Remove a peer. Returns false if not found.
        bool RemovePeer(const std::string& peerId)
        {
            const auto erased = _peers.erase(peerId);
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
            const auto it = _peers.find(peerId);
            if (it == _peers.end()) {
                return nullptr;
            }
            return &it->second;
        }

        [[nodiscard]] const Session* GetSession(const std::string& peerId) const
        {
            const auto it = _peers.find(peerId);
            if (it == _peers.end()) {
                return nullptr;
            }
            return &it->second;
        }

        // -------------------------------------------------------------------
        // Probe exchange (SyncPulse — delegated to per-peer Session)
        // -------------------------------------------------------------------

        /// Deterministic role assignment: the node with the lexicographically
        /// smaller ID is Active (initiates probes); the other is Passive (replies).
        [[nodiscard]] static bool IsActivePeer(
            const std::string& myId, const std::string& remoteId) noexcept
        {
            return myId < remoteId;
        }

        /// Whether this node should initiate a new probe toward peerId.
        ///
        /// Returns true only when this node is the Active peer AND the session's
        /// probe interval has elapsed.
        [[nodiscard]] bool ShouldInitiateProbe(
            const std::string& myId, const std::string& peerId) const
        {
            if (!IsActivePeer(myId, peerId)) {
                return false;
            }
            const auto* session = GetSession(peerId);
            return session && session->ShouldProbe();
        }

        /// Create a SyncPulse to send toward peerId (Active role).
        ///
        /// Includes an echo of the last pulse received from peerId when available.
        [[nodiscard]] std::optional<SyncPulse> MakePulse(const std::string& peerId)
        {
            auto* session = GetSession(peerId);
            if (!session) {
                return std::nullopt;
            }
            return session->MakePulse();
        }

        /// Handle an incoming SyncPulse from a peer.
        ///
        /// Always processes the pulse (records echo state, computes offset when
        /// echo data is present) and returns a reply pulse for the caller to send.
        /// The caller decides whether to actually send the reply based on role:
        ///   Passive (myId > peerId): send the reply immediately.
        ///   Active  (myId < peerId): discard the reply; send next pulse via MakePulse.
        ///
        /// The reply is built with epoch-relative timestamps for non-epoch-source peers,
        /// matching the epoch-propagation behaviour of the original protocol.
        ///
        /// @param remoteEpoch  Epoch info carried in the incoming wire message.
        /// @param receivedAt   True network-arrival time for the pulse (H1 fix).
        [[nodiscard]] std::optional<SyncPulse> HandleSyncPulse(
            const std::string& peerId,
            const SyncPulse& pulse,
            std::optional<Ticks> receivedAt = std::nullopt,
            std::optional<EpochInfo> remoteEpoch = std::nullopt)
        {
            if (remoteEpoch) {
                HandleRemoteEpoch(*remoteEpoch, peerId);
            }

            auto* session = GetSession(peerId);
            if (!session) {
                return std::nullopt;
            }

            // Process the pulse — pass raw receivedAt to the session so the NTP
            // formula uses consistent raw/epoch-relative timelines (see design notes).
            const auto result = session->HandleSyncPulse(pulse, receivedAt);

            // Emit sync-state events.
            if (result.enteredResyncing) {
                EmitEvent(SyncEvent::ResyncStarted);
            }
            if (!_synced && HasAnySyncedPeer()) {
                _synced = true;
                EmitEvent(SyncEvent::SyncAcquired);
            } else if (_synced && result.exitedResyncing) {
                EmitEvent(SyncEvent::ResyncCompleted);
            }

            // Build a reply pulse with epoch-relative timestamps for non-owners.
            const auto useRaw = _isEpochOwner || peerId == _epochSourcePeerId;
            const auto rawT2 = receivedAt.value_or(_clock.Now());
            const auto t2 = useRaw ? rawT2 : ToSyncedTime(rawT2);
            const auto rawT3 = _clock.Now();
            const auto t3 = useRaw ? rawT3 : ToSyncedTime(rawT3);

            return SyncPulse{
                .t1      = t3,
                .echo_t1 = pulse.t1,
                .echo_t2 = t2,
            };
        }

        /// Return a session's diagnostics snapshot.
        /// Returns std::nullopt if the peer is not found.
        [[nodiscard]] std::optional<SessionDiagnostics> GetSessionDiagnostics(
            const std::string& peerId) const
        {
            const auto* session = GetSession(peerId);
            if (!session) {
                return std::nullopt;
            }
            return session->GetDiagnostics();
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
        ///
        /// @param sourcePeerId  ID of the peer that sent this epoch (empty = not from a probe).
        void HandleRemoteEpoch(const EpochInfo& remote, const std::string& sourcePeerId = {})
        {
            if (!_epoch.IsValid()) {
                // Adopt the remote epoch.
                AdoptEpoch(remote, sourcePeerId);
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
                AdoptEpoch(remote, sourcePeerId);
                EmitEvent(SyncEvent::EpochChanged);
            }
            // Otherwise, keep our epoch. The remote will eventually adopt ours
            // when they receive our epoch info.
        }

        // -------------------------------------------------------------------
        // Synchronized time
        // -------------------------------------------------------------------

        /// Get the best estimate of synchronized time right now.
        /// Epoch owner returns its local time directly — it is the reference.
        /// Non-owners return the best session's remote time estimate.
        [[nodiscard]] Ticks SyncedNow() const noexcept
        {
            if (_isEpochOwner) {
                return _clock.Now();
            }
            if (const auto* session = GetEpochAlignedSession()) {
                return session->RemoteNow();
            }
            // No synced peer — return local time.
            return _clock.Now();
        }

        /// Convert a local time to synced time using the best available session.
        /// Epoch owner maps local → local (identity).
        [[nodiscard]] Ticks ToSyncedTime(Ticks localTime) const noexcept
        {
            if (_isEpochOwner) {
                return localTime;
            }
            if (const auto* session = GetEpochAlignedSession()) {
                return session->ToRemoteTime(localTime);
            }
            return localTime;
        }

        /// Whether we have at least one synced peer.
        [[nodiscard]] bool IsSynced() const noexcept { return _synced; }

        /// Whether this node created and owns the current epoch.
        [[nodiscard]] bool IsEpochOwner() const noexcept { return _isEpochOwner; }

        /// Best sync quality across all peers.
        [[nodiscard]] SyncQuality Quality() const noexcept
        {
            auto best = SyncQuality::None;
            for (const auto& session : _peers | std::views::values) {
                auto q = session.Quality();
                best = std::max(q, best);
            }
            return best;
        }

        /// Number of connected peers.
        [[nodiscard]] std::size_t PeerCount() const noexcept { return _peers.size(); }

        /// Invoke callback for each peer ID.
        template <typename Fn>
        void ForEachPeer(Fn&& fn) const
        {
            for (const auto& [peerId, _] : _peers) {
                fn(peerId);
            }
        }

        /// Consensus mode.
        [[nodiscard]] ConsensusMode Mode() const noexcept { return _mode; }

    private:
        void CreateEpoch()
        {
            const auto now = _clock.Now();
            // Simple epoch ID: hash of local time.
            _epoch = SyncEpoch{
                .id          = static_cast<std::uint64_t>(now.count()) ^ 0x5A5A5A5A'5A5A5A5AULL,
                .baseTime    = now,
                .rate        = DriftRate{},
                .memberCount = 1,
                .createdAt   = now,
            };
            _isEpochOwner = true;
            _epochSourcePeerId.clear();
        }

        void AdoptEpoch(const EpochInfo& info, const std::string& sourcePeerId = {})
        {
            _epoch = SyncEpoch{
                .id          = info.epochId,
                .baseTime    = info.baseTime,
                .rate        = DriftRate{},
                .memberCount = info.memberCount,
                .createdAt   = info.createdAt,
            };

            // Reset all sessions to re-sync under new epoch.
            for (auto& session : _peers | std::views::values) {
                session.Reset();
            }
            _synced = false;
            _isEpochOwner = false;
            _epochSourcePeerId = sourcePeerId;
        }

        [[nodiscard]] bool HasAnySyncedPeer() const noexcept
        {
            return std::ranges::any_of(
                _peers | std::views::values,
                [](const auto& session) {
                    return session.State() == SessionState::Synced;
                });
        }

        /// Find the best synced session (highest quality).
        [[nodiscard]] const Session* BestSyncedSession() const noexcept
        {
            const Session* best = nullptr;
            SyncQuality bestQuality = SyncQuality::None;

            for (const auto& session : _peers | std::views::values) {
                auto q = session.Quality();
                if (q > bestQuality) {
                    bestQuality = q;
                    best = &session;
                }
            }
            return best;
        }

        /// Find the session that is aligned to the epoch source.
        ///
        /// Prefers the session whose peer provided our current epoch, since that
        /// peer's timestamps are already on the epoch owner's timeline. Falls back
        /// to BestSyncedSession when the preferred peer is absent or not yet Synced.
        [[nodiscard]] const Session* GetEpochAlignedSession() const noexcept
        {
            if (!_epochSourcePeerId.empty()) {
                const auto* session = GetSession(_epochSourcePeerId);
                if (session && session->Quality() == SyncQuality::High) {
                    return session;
                }
            }
            return BestSyncedSession();
        }

        void EmitEvent(SyncEvent event) const
        {
            if (_eventCallback) {
                _eventCallback(event);
            }
        }

        Log::Logger _logger;

        IClock& _clock;
        ConsensusMode _mode;
        SessionConfig _config;
        SyncEpoch _epoch;
        bool _synced = false;
        bool _isEpochOwner = false;
        std::string _epochSourcePeerId;

        std::unordered_map<std::string, Session> _peers;
        EventCallback _eventCallback;
    };
}
