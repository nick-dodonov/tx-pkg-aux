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
// DcRtcServer::State — implements ISigHandler for the server peer identity
// ---------------------------------------------------------------------------

struct DcRtcServer::State : ISigHandler, std::enable_shared_from_this<State>
{
    std::shared_ptr<ILinkAcceptor> acceptor;
    std::shared_ptr<ISigUser> sigUser;

    PeerId localId;
    std::vector<std::string> iceServers;
    std::size_t maxMessageSize = 65535;

    std::mutex mutex;

    struct PeerState
    {
        std::shared_ptr<rtc::PeerConnection> pc;
        bool remoteDescSet{false};
        std::vector<std::pair<std::string, std::string>> pendingCandidates;
    };
    std::unordered_map<std::string, PeerState> peers;

    // ISigHandler — handles offers and ICE candidates from remote peers.
    void OnMessage(SigMessage&& msg) override
    {
        const auto j = json::parse(std::move(msg).payload, nullptr, /*exceptions=*/false);
        if (j.is_discarded()) {
            return;
        }

        const auto type = j.value("type", std::string{});
        const auto remotePeerId = msg.from;

        if (type == "offer") {
            handleOffer(remotePeerId, j);
        } else if (type == "candidate") {
            auto candidate = j.value("candidate", std::string{});
            auto mid = j.value("mid", std::string{});
            std::shared_ptr<rtc::PeerConnection> pc;
            {
                std::lock_guard lock{mutex};
                if (auto it = peers.find(remotePeerId.value); it != peers.end()) {
                    auto& peer = it->second;
                    if (peer.remoteDescSet) {
                        pc = peer.pc;
                    } else {
                        peer.pendingCandidates.emplace_back(std::move(candidate), std::move(mid));
                    }
                }
            }
            if (pc) {
                pc->addRemoteCandidate(rtc::Candidate(candidate, mid));
            }
        }
    }

    // ISigHandler — external signaling disconnect.
    void OnLeft(std::error_code ec) override
    {
        if (ec) {
            Log::Warn("signaling connection lost: {}", ec.message());
            acceptor->OnLink(std::unexpected(Error::TransportClosed));
        }
    }

private:
    void handleOffer(const PeerId& remotePeerId, const json& j)
    {
        const auto sdp = j.value("description", std::string{});

        rtc::Configuration config;
        for (const auto& srv : iceServers) {
            config.iceServers.emplace_back(srv);
        }

        auto pc = std::make_shared<rtc::PeerConnection>(config);
        {
            std::lock_guard lock{mutex};
            peers[remotePeerId.value].pc = pc;
        }

        auto wsigUser = std::weak_ptr<ISigUser>{sigUser};
        pc->onLocalDescription([wsigUser, remotePeerId](const rtc::Description& desc) {
            if (auto su = wsigUser.lock()) {
                const json payload = {{"type", desc.typeString()},
                                      {"description", std::string(desc)}};
                su->Send(remotePeerId, payload.dump());
            }
        });

        pc->onLocalCandidate([wsigUser, remotePeerId](const rtc::Candidate& cand) {
            if (auto su = wsigUser.lock()) {
                const json payload = {{"type", "candidate"},
                                      {"candidate", std::string(cand)},
                                      {"mid", cand.mid()}};
                su->Send(remotePeerId, payload.dump());
            }
        });

        auto wself = std::weak_ptr<State>{shared_from_this()};

        const auto myLocalId = localId;
        const auto maxMsgSize = maxMessageSize;
        const auto wacceptor = std::weak_ptr<ILinkAcceptor>{acceptor};

        pc->onDataChannel(
            [wpc = std::weak_ptr<rtc::PeerConnection>{pc},
             wself, myLocalId, remotePeerId, maxMsgSize, wacceptor]
            (std::shared_ptr<rtc::DataChannel> dc) {
                auto pc = wpc.lock();
                auto acc = wacceptor.lock();
                if (!pc || !acc) {
                    return;
                }
                Log::Debug("data channel open {} -> {}",
                           myLocalId.value, remotePeerId.value);

                // Both closed and failed paths erase the peer from the map.
                auto onPeerGone = [wself, remotePeerId]() {
                    if (auto s = wself.lock()) {
                        std::lock_guard lock{s->mutex};
                        s->peers.erase(remotePeerId.value);
                    }
                };

                auto link = DcRtcLink::Create(myLocalId, remotePeerId, pc, std::move(dc),
                                              maxMsgSize, onPeerGone, onPeerGone);
                auto handler = acc->OnLink(link);
                link->SetHandler(std::move(handler));
            });

        pc->setRemoteDescription(rtc::Description(sdp, "offer"));

        // Flush any candidates that arrived before setRemoteDescription.
        std::vector<std::pair<std::string, std::string>> pending;
        {
            std::lock_guard lock{mutex};
            if (auto it = peers.find(remotePeerId.value); it != peers.end()) {
                it->second.remoteDescSet = true;
                pending = std::move(it->second.pendingCandidates);
            }
        }
        for (auto& [cand, mid] : pending) {
            pc->addRemoteCandidate(rtc::Candidate(cand, mid));
        }
    }
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

    auto onJoined = [wstate](SigJoinResult result) -> std::weak_ptr<ISigHandler> {
        auto s = wstate.lock();
        if (!s) {
            return {};
        }
        if (!result) {
            Log::Error("signaling join failed: {}", ErrorToString(result.error()));
            s->acceptor->OnLink(std::unexpected(result.error()));
            return {};
        }
        s->sigUser = std::move(*result);
        Log::Debug("signaling ready as {}", s->localId.value);
        return std::weak_ptr<ISigHandler>{s};
    };

    Log::Debug("joining signaling as {}", _options.localId.value);
    _options.sigClient->Join(_options.localId, std::move(onJoined));
}

} // namespace Rtt::Rtc
#endif
