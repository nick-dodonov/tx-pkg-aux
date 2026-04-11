#pragma once
#include "PeerView.h"
#include "Protocol.h"
#include "RttBridge.h"

#include "Rtt/Link.h"
#include "SynTm/Consensus.h"
#include "SynTm/Integrate.h"
#include "SynTm/SyncClock.h"
#include "Log/Log.h"

#include <array>
#include <cstddef>
#include <memory>
#include <string>

namespace Demo
{
    /// Per-peer synchronization and data agent.
    ///
    /// Wraps a single RTT link and its associated LinkBridge.
    /// Handles probe exchange (via SynTm Integrate.h), state updates,
    /// and shot messages for one remote peer.
    class SyncAgent
    {
    public:
        SyncAgent(
            std::string peerId,
            std::shared_ptr<Rtt::ILink> link,
            std::shared_ptr<LinkBridge> bridge,
            SynTm::Consensus& consensus,
            SynTm::SyncClock& syncClock)
            : _peerId(std::move(peerId))
            , _link(std::move(link))
            , _bridge(std::move(bridge))
            , _consensus(consensus)
            , _syncClock(syncClock)
        {
            _view.peerId = _peerId;
        }

        [[nodiscard]] const std::string& PeerId() const noexcept { return _peerId; }
        [[nodiscard]] PeerView& View() noexcept { return _view; }
        [[nodiscard]] const PeerView& View() const noexcept { return _view; }
        [[nodiscard]] std::shared_ptr<LinkBridge> Bridge() const noexcept { return _bridge; }
        [[nodiscard]] bool IsConnected() const noexcept { return _connected; }

        /// Send a probe request if the session says it's time.
        void TryProbe()
        {
            if (!_consensus.ShouldProbe(_peerId)) {
                return;
            }
            auto req = _consensus.MakeProbeRequest(_peerId);
            if (!req) {
                return;
            }
            auto epoch = _consensus.OurEpochInfo();
            std::array<std::byte, 128> raw{};
            auto n = SynTm::WriteSyncProbeRequest(raw, epoch, *req);
            if (n == 0) {
                return;
            }
            auto wrapped = WrapSyncProbe(std::span<const std::byte>(raw.data(), n));
            SendBytes(wrapped);
        }

        /// Process an incoming raw message from the transport.
        void HandleMessage(std::span<const std::byte> data)
        {
            auto appMsg = ParseAppMessage(data);
            if (!appMsg) {
                return;
            }

            switch (appMsg->type) {
                case MsgType::SyncProbe:
                    HandleSyncProbe(appMsg->payload);
                    break;
                case MsgType::StateUpdate:
                    HandleStateUpdate(appMsg->payload);
                    break;
                case MsgType::ShotFired:
                    HandleShotFired(appMsg->payload);
                    break;
                case MsgType::ShotAck:
                    HandleShotAck(appMsg->payload);
                    break;
            }
        }

        /// Broadcast local simulation state to this peer.
        void SendStateUpdate(float pos, float vel, SynTm::Ticks syncTime)
        {
            auto msg = SerializeStateUpdate({.syncTime = syncTime, .position = pos, .velocity = vel});
            SendBytes(msg);
        }

        /// Send a shot-fired announcement.
        void SendShotFired(SynTm::Ticks scheduledTime)
        {
            auto msg = SerializeShotFired({.scheduledSyncTime = scheduledTime});
            SendBytes(msg);
        }

        /// Send a shot acknowledgment.
        void SendShotAck(SynTm::Ticks scheduledTime, SynTm::Ticks receivedTime)
        {
            auto msg = SerializeShotAck({.scheduledSyncTime = scheduledTime, .receivedSyncTime = receivedTime});
            SendBytes(msg);
        }

        void MarkDisconnected() { _connected = false; }

    private:
        void HandleSyncProbe(std::span<const std::byte> payload)
        {
            auto parsed = SynTm::ParseSyncMessage(payload);
            if (!parsed) {
                return;
            }

            // Always process the remote epoch.
            _consensus.HandleRemoteEpoch(parsed->epoch);

            if (parsed->type == SynTm::SyncMessageType::ProbeRequest && parsed->request) {
                auto resp = _consensus.HandleProbeRequest(_peerId, *parsed->request);
                if (resp) {
                    auto epoch = _consensus.OurEpochInfo();
                    std::array<std::byte, 128> raw{};
                    auto n = SynTm::WriteSyncProbeResponse(raw, epoch, *resp);
                    if (n > 0) {
                        auto wrapped = WrapSyncProbe(std::span<const std::byte>(raw.data(), n));
                        SendBytes(wrapped);
                    }
                }
            } else if (parsed->type == SynTm::SyncMessageType::ProbeResponse && parsed->response) {
                _consensus.HandleProbeResponse(_peerId, *parsed->response, parsed->epoch);
            }
        }

        void HandleStateUpdate(std::span<const std::byte> payload)
        {
            auto msg = ReadStateUpdate(payload);
            if (!msg) {
                return;
            }
            _view.lastPosition = msg->position;
            _view.lastVelocity = msg->velocity;
            _view.lastUpdateSyncTime = msg->syncTime;
        }

        void HandleShotFired(std::span<const std::byte> payload)
        {
            auto msg = ReadShotFired(payload);
            if (!msg) {
                return;
            }
            auto now = _syncClock.NowNanos();
            auto delta = now - msg->scheduledSyncTime;
            Log::Info("[SHOT] received from={} scheduled={} actual={} delta={}ns",
                _peerId, msg->scheduledSyncTime, now, delta);

            // Send acknowledgment.
            SendShotAck(msg->scheduledSyncTime, now);
        }

        void HandleShotAck(std::span<const std::byte> payload)
        {
            auto msg = ReadShotAck(payload);
            if (!msg) {
                return;
            }
            auto error = msg->receivedSyncTime - msg->scheduledSyncTime;
            _view.lastShotError = error;
            ++_view.shotCount;
            _view.totalShotError += std::chrono::abs(error);
            Log::Info("[SHOT] ack from={} error={}ns avg={}ns",
                _peerId, error,
                _view.shotCount > 0 ? _view.totalShotError / _view.shotCount : SynTm::Ticks{});
        }

        void SendBytes(std::span<const std::byte> data)
        {
            if (!_link || !_connected) {
                return;
            }
            auto copy = std::vector<std::byte>(data.begin(), data.end());
            _link->Send([copy = std::move(copy)](std::span<std::byte> buf) -> std::size_t {
                auto n = std::min(buf.size(), copy.size());
                std::memcpy(buf.data(), copy.data(), n);
                return n;
            });
        }

        std::string _peerId;
        std::shared_ptr<Rtt::ILink> _link;
        std::shared_ptr<LinkBridge> _bridge;
        SynTm::Consensus& _consensus;
        SynTm::SyncClock& _syncClock;
        PeerView _view;
        bool _connected = true;
    };
}
