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
    /// epoch-relative synchronized time. Handles epoch propagation and group merges.
    ///
    /// ## Time architecture
    ///
    /// Each peer has an independent steady clock with an arbitrary origin.
    /// These clocks are never directly comparable.
    ///
    ///   Session::RemoteNow()   — remote peer's local steady time (estimated via
    ///                            NTP formula). NOT comparable to local time;
    ///                            the raw difference is always large and arbitrary.
    ///
    ///   Consensus::SyncedNow() — epoch-relative time shared across all peers in
    ///                            the group. Computed as:
    ///                              session.RemoteNow() + peerEpochOffset
    ///                            where peerEpochOffset = remote.(SyncedNow - LocalNow)
    ///                            propagated via EpochInfo.epochOffset.
    ///
    ///   epochOffset = SyncedNow() - LocalNow() — unique per peer (reflects the
    ///                            difference between this peer's clock origin and
    ///                            the epoch owner's). Zero for the epoch owner.
    ///                            To compare sync quality across peers, compare
    ///                            SyncedNow() values directly (it's possible only in tests/labs where all peers are in the same process).
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
        /// Both Active and Passive peers may initiate — the role distinction only
        /// governs reply sending (a reply to a reply is suppressed by returning
        /// std::nullopt from HandleSyncPulse when hadEchoData is true).  Gating
        /// on role here would prevent the Passive side from ever updating its own
        /// DriftModel, breaking SyncedNow() on that side.
        [[nodiscard]] bool ShouldInitiateProbe(const std::string& peerId) const
        {
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
        /// Processes the pulse and, if it was a fresh probe (not a reply to our own
        /// outstanding probe), returns a reply pulse for the caller to send.
        /// Returns std::nullopt in two cases:
        ///   — peer not found;
        ///   — the pulse was a reply to our probe (hadEchoData=true): the caller
        ///     must not send another reply, preventing infinite reply chains when
        ///     both sides probe each other symmetrically.
        ///
        /// Transitivity is achieved through the epochOffset field of EpochInfo:
        ///   remoteEpoch.epochOffset = (remote.SyncedNow - remote.LocalNow)
        /// This is stored in _peerEpochOffsets and added to session.RemoteNow()
        /// in SyncedNow(), so a relay node automatically converts its peer's local
        /// time to epoch owner time across any number of hops.
        ///
        /// All SyncPulse timestamps remain raw local times — Session is unchanged.
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
                // Store the sender's epoch offset for transitive SyncedNow().
                _peerEpochOffsets[peerId] = remoteEpoch->epochOffset;
            }

            auto* session = GetSession(peerId);
            if (!session) {
                return std::nullopt;
            }

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

            // The pulse carried echo data that matched our pending probe — it was
            // a reply to us.  Do not reply again; the caller should not send
            // anything back (prevents infinite reply chains).
            if (result.hadEchoData) {
                return std::nullopt;
            }

            // Build reply with raw timestamps only.  Transitivity is handled via
            // epochOffset in OurEpochInfo(), not by manipulating timestamps here.
            const auto rawT2 = receivedAt.value_or(_clock.Now());
            const auto rawT3 = _clock.Now();

            return SyncPulse{
                .t1      = rawT3,
                .echo_t1 = pulse.t1,
                .echo_t2 = rawT2,
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
        ///
        /// Includes the current epochOffset (SyncedNow - LocalNow) so that
        /// receivers can compute transitively correct synced time.
        [[nodiscard]] EpochInfo OurEpochInfo() const noexcept
        {
            auto info = ToEpochInfo(_epoch);
            info.epochOffset = SyncedNow() - _clock.Now();
            return info;
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
        ///
        /// Epoch owner: returns local time directly (it is the reference).
        /// Non-owner: returns session.RemoteNow() + peerEpochOffset, where
        ///   session.RemoteNow() ≈ peer's local time
        ///   peerEpochOffset    = peer's (SyncedNow - LocalNow) ≈ (epoch - peer_local)
        ///   sum                ≈ epoch owner's time  (transitive over any hop count)
        [[nodiscard]] Ticks SyncedNow() const noexcept
        {
            if (_isEpochOwner) {
                return _clock.Now();
            }

            // Prefer epoch-source session with its known epochOffset.
            if (!_epochSourcePeerId.empty()) {
                const auto* session = GetSession(_epochSourcePeerId);
                const auto it = _peerEpochOffsets.find(_epochSourcePeerId);
                if (session
                    && session->Quality() >= SyncQuality::Low
                    && it != _peerEpochOffsets.end()) {
                    return session->RemoteNow() + it->second;
                }
            }

            // Fall back: best session among peers with a known epochOffset.
            const Session* bestSession = nullptr;
            Ticks bestOffset{};
            auto bestQuality = SyncQuality::None;
            for (const auto& [pid, sess] : _peers) {
                const auto q = sess.Quality();
                const auto it = _peerEpochOffsets.find(pid);
                if (q > bestQuality && it != _peerEpochOffsets.end()) {
                    bestSession = &sess;
                    bestOffset = it->second;
                    bestQuality = q;
                }
            }
            if (bestSession) {
                return bestSession->RemoteNow() + bestOffset;
            }

            return _clock.Now();
        }

        /// Convert a local time to synced time.
        /// Epoch owner maps local → local (identity).
        [[nodiscard]] Ticks ToSyncedTime(Ticks localTime) const noexcept
        {
            if (_isEpochOwner) {
                return localTime;
            }
            return localTime + (SyncedNow() - _clock.Now());
        }

        /// ID of the peer from whom we received our current epoch, or empty if we
        /// own the epoch ourselves.
        [[nodiscard]] std::string_view EpochSourcePeerId() const noexcept
        {
            return _epochSourcePeerId;
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

            _isEpochOwner = false;
            _epochSourcePeerId = sourcePeerId;
            _synced = false;

            // Clear stale epoch offsets — they were relative to the old epoch.
            // Fresh offsets will arrive with the next probe cycle.
            // Sessions are intentionally preserved: raw DriftModels (local→remote
            // in physical clock units) remain valid regardless of epoch changes.
            _peerEpochOffsets.clear();
        }

        [[nodiscard]] bool HasAnySyncedPeer() const noexcept
        {
            return std::ranges::any_of(
                _peers | std::views::values,
                [](const auto& session) {
                    return session.State() == SessionState::Synced;
                });
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

        /// Per-peer epoch offsets: (peer.SyncedNow - peer.LocalNow) at the time
        /// of their last probe.  Used by SyncedNow() to compute transitive time.
        std::unordered_map<std::string, Ticks> _peerEpochOffsets;

        std::unordered_map<std::string, Session> _peers;
        EventCallback _eventCallback;
    };
}
