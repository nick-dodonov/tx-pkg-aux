#if !defined(__EMSCRIPTEN__)
#include "DcRtcClient.h"
#include "DcRtcLink.h"

#include "Rtt/Rtc/ISigUser.h"
#include "Log/Log.h"

#include <nlohmann/json.hpp>
#include <rtc/rtc.hpp>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace Rtt::Rtc
{

using json = nlohmann::json;

// ---------------------------------------------------------------------------
// DcRtcClient::State — holds all negotiation objects
//
// Kept alive by DcRtcClient::_state for the connection lifetime.
// The PC and DC are also held by DcRtcLink once the DataChannel is open;
// DcRtcClient can be destroyed after that and the link continues working.
// ---------------------------------------------------------------------------

struct DcRtcClient::State
{
    std::shared_ptr<ILinkAcceptor> acceptor;
    std::shared_ptr<ISigUser> sigUser;
    std::shared_ptr<rtc::PeerConnection> pc;
    std::shared_ptr<rtc::DataChannel> dc;

    PeerId localId;
    PeerId remoteId;
    std::vector<std::string> iceServers;
    std::size_t maxMessageSize = 65535;
};

// ---------------------------------------------------------------------------
// DcRtcClient
// ---------------------------------------------------------------------------

DcRtcClient::DcRtcClient(Options options)
    : _options(std::move(options))
{}

DcRtcClient::~DcRtcClient() = default;

void DcRtcClient::Open(std::shared_ptr<ILinkAcceptor> acceptor)
{
    auto state = std::make_shared<State>();
    state->acceptor = std::move(acceptor);
    state->localId = _options.localId;
    state->remoteId = _options.remoteId;
    state->iceServers = _options.iceServers;
    state->maxMessageSize = _options.maxMessageSize;

    _state = state;

    auto wstate = std::weak_ptr<State>{state};

    // Route incoming SDP answers and ICE candidates from the remote peer.
    auto onSigMsg = [wstate](SigMessage msg) {
        auto s = wstate.lock();
        if (!s || !s->pc) {
            return;
        }
        const auto j = json::parse(msg.payload, nullptr, /*exceptions=*/false);
        if (j.is_discarded()) {
            return;
        }
        const auto type = j.value("type", std::string{});
        if (type == "answer") {
            const auto sdp = j.value("description", std::string{});
            s->pc->setRemoteDescription(rtc::Description(sdp, type));
        } else if (type == "candidate") {
            const auto candidate = j.value("candidate", std::string{});
            const auto mid = j.value("mid", std::string{});
            s->pc->addRemoteCandidate(rtc::Candidate(candidate, mid));
        }
    };

    // On successful join: create RTCPeerConnection and initiate negotiation.
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

        rtc::Configuration config;
        for (const auto& srv : s->iceServers) {
            config.iceServers.emplace_back(srv);
        }

        auto pc = std::make_shared<rtc::PeerConnection>(config);
        s->pc = pc;

        auto wsigUser = std::weak_ptr<ISigUser>{s->sigUser};
        const auto remoteId = s->remoteId;

        // Forward our SDP offer to the remote peer.
        pc->onLocalDescription([wsigUser, remoteId](const rtc::Description& desc) {
            if (auto su = wsigUser.lock()) {
                const json payload = {{"type", desc.typeString()},
                                      {"description", std::string(desc)}};
                su->Send(remoteId, payload.dump());
            }
        });

        // Forward each local ICE candidate to the remote peer (trickle ICE).
        pc->onLocalCandidate([wsigUser, remoteId](const rtc::Candidate& cand) {
            if (auto su = wsigUser.lock()) {
                const json payload = {{"type", "candidate"},
                                      {"candidate", std::string(cand)},
                                      {"mid", cand.mid()}};
                su->Send(remoteId, payload.dump());
            }
        });

        pc->onStateChange([wstate](rtc::PeerConnection::State st) {
            if (st == rtc::PeerConnection::State::Failed) {
                if (auto s = wstate.lock()) {
                    Log::Error("peer connection failed");
                    s->acceptor->OnLink(std::unexpected(Error::Unknown));
                }
            }
        });

        // Creating the DataChannel triggers SDP negotiation — the library
        // fires onLocalDescription automatically with the generated offer.
        const auto maxMsgSize = s->maxMessageSize;
        const auto localId = s->localId;

        auto dc = pc->createDataChannel("data");
        s->dc = dc;

        dc->onOpen([wstate, localId, remoteId, maxMsgSize]() {
            auto s = wstate.lock();
            if (!s) {
                return;
            }
            Log::Debug("data channel open {} -> {}", localId.value, remoteId.value);

            auto link = DcRtcLink::Create(localId, remoteId, s->pc, s->dc, maxMsgSize);
            auto handler = s->acceptor->OnLink(link);
            link->SetHandler(std::move(handler));
        });
    };

    Log::Debug("joining signaling as {}", _options.localId.value);
    _options.sigClient->Join(_options.localId, std::move(onSigMsg), std::move(onJoined));
}

} // namespace Rtt::Rtc
#endif
