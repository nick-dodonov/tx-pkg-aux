#if !defined(__EMSCRIPTEN__)
#include "DcRtcServer.h"
#include "DcRtcLink.h"

#include "Rtt/Rtc/ISigUser.h"
#include "Log/Log.h"

#include <nlohmann/json.hpp>
#include <rtc/rtc.hpp>

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Rtt::Rtc
{

using json = nlohmann::json;

// ---------------------------------------------------------------------------
// DcRtcServer::State
//
// Shared by all per-peer callbacks. Holds the server's signaling identity
// and tracks active PeerConnections keyed by remote peer ID.
// ---------------------------------------------------------------------------

struct DcRtcServer::State
{
    std::shared_ptr<ILinkAcceptor> acceptor;
    std::shared_ptr<ISigUser> sigUser;

    PeerId localId;
    std::vector<std::string> iceServers;
    std::size_t maxMessageSize = 65535;

    std::mutex mutex;
    std::unordered_map<std::string, std::shared_ptr<rtc::PeerConnection>> peerPcs;
};

// ---------------------------------------------------------------------------
// DcRtcServer
// ---------------------------------------------------------------------------

DcRtcServer::DcRtcServer(Options options)
    : _options(std::move(options))
{}

DcRtcServer::~DcRtcServer() = default;

void DcRtcServer::Open(std::shared_ptr<ILinkAcceptor> acceptor)
{
    auto state = std::make_shared<State>();
    state->acceptor = std::move(acceptor);
    state->localId = _options.localId;
    state->iceServers = _options.iceServers;
    state->maxMessageSize = _options.maxMessageSize;

    _state = state;

    auto wstate = std::weak_ptr<State>{state};

    // Handle incoming signaling messages from any remote peer.
    auto onSigMsg = [wstate](SigMessage msg) {
        auto s = wstate.lock();
        if (!s) {
            return;
        }

        const auto j = json::parse(msg.payload, nullptr, /*exceptions=*/false);
        if (j.is_discarded()) {
            return;
        }

        const auto type = j.value("type", std::string{});
        const auto remotePeerId = msg.from;

        if (type == "offer") {
            // New peer connecting: create a PeerConnection and answer the offer.
            const auto sdp = j.value("description", std::string{});

            rtc::Configuration config;
            for (const auto& srv : s->iceServers) {
                config.iceServers.emplace_back(srv);
            }

            auto pc = std::make_shared<rtc::PeerConnection>(config);

            {
                std::lock_guard lock{s->mutex};
                s->peerPcs[remotePeerId.value] = pc;
            }

            // Send our SDP answer to the remote peer.
            auto wsigUser = std::weak_ptr<ISigUser>{s->sigUser};
            pc->onLocalDescription([wsigUser, remotePeerId](const rtc::Description& desc) {
                if (auto su = wsigUser.lock()) {
                    const json payload = {{"type", desc.typeString()},
                                          {"description", std::string(desc)}};
                    su->Send(remotePeerId, payload.dump());
                }
            });

            // Forward our local ICE candidates to the remote peer.
            pc->onLocalCandidate([wsigUser, remotePeerId](const rtc::Candidate& cand) {
                if (auto su = wsigUser.lock()) {
                    const json payload = {{"type", "candidate"},
                                          {"candidate", std::string(cand)},
                                          {"mid", cand.mid()}};
                    su->Send(remotePeerId, payload.dump());
                }
            });

            // Clean up the PC entry when the connection is closed.
            pc->onStateChange([wstate, remotePeerId](rtc::PeerConnection::State st) {
                if (st == rtc::PeerConnection::State::Closed ||
                    st == rtc::PeerConnection::State::Failed ||
                    st == rtc::PeerConnection::State::Disconnected) {
                    if (auto s = wstate.lock()) {
                        std::lock_guard lock{s->mutex};
                        s->peerPcs.erase(remotePeerId.value);
                    }
                }
            });

            // Deliver a link when the client's DataChannel arrives.
            const auto localId = s->localId;
            const auto maxMsgSize = s->maxMessageSize;

            pc->onDataChannel(
                [wstate, wpc = std::weak_ptr<rtc::PeerConnection>{pc},
                 localId, remotePeerId, maxMsgSize]
                (std::shared_ptr<rtc::DataChannel> dc) {
                    auto s = wstate.lock();
                    auto pc = wpc.lock();
                    if (!s || !pc) {
                        return;
                    }
                    Log::Debug("data channel open {} -> {}",
                               localId.value, remotePeerId.value);

                    auto link = DcRtcLink::Create(localId, remotePeerId,
                                                  pc, dc, maxMsgSize);
                    auto handler = s->acceptor->OnLink(link);
                    link->SetHandler(std::move(handler));
                });

            // Applying the remote description triggers ICE gathering and the
            // library auto-generates a local answer via onLocalDescription.
            pc->setRemoteDescription(rtc::Description(sdp, "offer"));

        } else if (type == "candidate") {
            // Remote ICE candidate for an existing peer connection.
            const auto candidate = j.value("candidate", std::string{});
            const auto mid = j.value("mid", std::string{});

            std::shared_ptr<rtc::PeerConnection> pc;
            {
                std::lock_guard lock{s->mutex};
                if (auto it = s->peerPcs.find(remotePeerId.value);
                    it != s->peerPcs.end()) {
                    pc = it->second;
                }
            }
            if (pc) {
                pc->addRemoteCandidate(rtc::Candidate(candidate, mid));
            }
        }
    };

    auto onJoined = [wstate](SigJoinResult result) {
        auto s = wstate.lock();
        if (!s) {
            return;
        }
        if (!result) {
            Log::Error("signaling join failed: {}", ErrorToString(result.error()));
            s->acceptor->OnLink(std::unexpected(result.error()));
            return;
        }
        s->sigUser = std::move(*result);
        Log::Debug("signaling ready as {}", s->localId.value);
    };

    Log::Debug("joining signaling as {}", _options.localId.value);
    _options.sigClient->Join(_options.localId, std::move(onSigMsg), std::move(onJoined));
}

} // namespace Rtt::Rtc
#endif
