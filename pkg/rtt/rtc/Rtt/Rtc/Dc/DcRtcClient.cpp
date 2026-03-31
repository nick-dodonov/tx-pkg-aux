#if !defined(__EMSCRIPTEN__)
#include "DcRtcClient.h"
#include "DcRtcLink.h"

#include "Log/Log.h"
#include "Rtt/Rtc/ISigUser.h"

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
    // DcRtcClient::State — holds all negotiation objects, implements ISigHandler
    // ---------------------------------------------------------------------------

    struct DcRtcClient::State: ISigHandler
    {
        std::shared_ptr<ILinkAcceptor> acceptor;
        std::shared_ptr<ISigUser> sigUser;
        std::shared_ptr<rtc::PeerConnection> pc;
        std::shared_ptr<rtc::DataChannel> dc;

        PeerId localId;
        PeerId remoteId;
        std::vector<std::string> iceServers;
        std::size_t maxMessageSize = 65535;

        // ISigHandler — routes incoming SDP answers and ICE candidates.
        //
        // libdatachannel fires onLocalDescription and onLocalCandidate from
        // independent background threads on the REMOTE peer, so candidates from
        // the server can arrive at the client before the answer — even though the
        // answer is always generated first. We buffer candidates until the answer
        // has been applied to the local PeerConnection.
        void OnMessage(SigMessage&& msg) override
        {
            if (!pc) {
                Log::Warn("message received but no peer connection exists, ignoring");
                return;
            }
            const auto j = json::parse(std::move(msg).payload, nullptr, /*exceptions=*/false);
            if (j.is_discarded()) {
                return;
            }
            const auto type = j.value("type", std::string{});
            if (type == "answer") {
                Log::Trace("answer received from {}", msg.from.value);
                const auto sdp = j.value("description", std::string{});
                pc->setRemoteDescription(rtc::Description(sdp, type));
                _remoteDescriptionSet = true;
                if (!_pendingCandidates.empty()) {
                    Log::Trace("flushing {} pending ICE candidates", _pendingCandidates.size());
                    for (auto& [cand, mid] : _pendingCandidates) {
                        pc->addRemoteCandidate(rtc::Candidate(cand, mid));
                    }
                    _pendingCandidates.clear();
                }
            } else if (type == "candidate") {
                auto cand = j.value("candidate", std::string{});
                auto mid = j.value("mid", std::string{});
                if (_remoteDescriptionSet) {
                    Log::Trace("ICE candidate from {} (direct)", msg.from.value);
                    pc->addRemoteCandidate(rtc::Candidate(cand, mid));
                } else {
                    Log::Trace("ICE candidate from {} (buffered, no remote desc yet)", msg.from.value);
                    _pendingCandidates.emplace_back(std::move(cand), std::move(mid));
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
        bool _remoteDescriptionSet{false};
        std::vector<std::pair<std::string, std::string>> _pendingCandidates;
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

        // Factory: called when signaling join completes (success or failure).
        // Returns weak_ptr<ISigHandler> so the signaling layer can route messages.
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

            rtc::Configuration config;
            for (const auto& srv : s->iceServers) {
                config.iceServers.emplace_back(srv);
            }

            auto pc = std::make_shared<rtc::PeerConnection>(config);
            s->pc = pc;

            auto wsigUser = std::weak_ptr<ISigUser>{s->sigUser};
            const auto remoteId = s->remoteId;

            pc->onLocalDescription([wsigUser, remoteId](const rtc::Description& desc) {
                if (auto su = wsigUser.lock()) {
                    Log::Trace("sending {} to {}", desc.typeString(), remoteId.value);
                    const json payload = {{"type", desc.typeString()}, {"description", std::string(desc)}};
                    su->Send(remoteId, payload.dump());
                }
            });

            pc->onLocalCandidate([wsigUser, remoteId](const rtc::Candidate& cand) {
                if (auto su = wsigUser.lock()) {
                    Log::Trace("sending ICE candidate to {}", remoteId.value);
                    const json payload = {{"type", "candidate"}, {"candidate", std::string(cand)}, {"mid", cand.mid()}};
                    su->Send(remoteId, payload.dump());
                }
            });

            const auto maxMsgSize = s->maxMessageSize;
            const auto localId = s->localId;

            // Log gathering and connection state changes during ICE negotiation
            // (before DcRtcLink is created). DcRtcLink::Create will override
            // onStateChange once the data channel opens.
            pc->onGatheringStateChange([localId, remoteId](rtc::PeerConnection::GatheringState st) {
                using G = rtc::PeerConnection::GatheringState;
                const std::string_view name = st == G::New         ? "New"
                                            : st == G::InProgress  ? "InProgress"
                                            : st == G::Complete    ? "Complete"
                                                                   : "?";
                Log::Trace("ICE gathering [{} -> {}]: {}", localId.value, remoteId.value, name);
            });
            pc->onStateChange([localId, remoteId](rtc::PeerConnection::State st) {
                using S = rtc::PeerConnection::State;
                const std::string_view name = st == S::New          ? "New"
                                            : st == S::Connecting   ? "Connecting"
                                            : st == S::Connected    ? "Connected"
                                            : st == S::Disconnected ? "Disconnected"
                                            : st == S::Failed       ? "Failed"
                                            : st == S::Closed       ? "Closed"
                                                                    : "?";
                Log::Trace("PC state [{} -> {}]: {}", localId.value, remoteId.value, name);
            });

            auto dc = pc->createDataChannel("data");
            s->dc = dc;

            dc->onOpen([wstate, localId, remoteId, maxMsgSize]() {
                auto s = wstate.lock();
                if (!s) {
                    return;
                }
                Log::Debug("data channel open {} -> {}", localId.value, remoteId.value);

                auto onClosed = [wstate]() {
                    // PC teardown complete — leave signaling to close the WS connection.
                    if (auto s = wstate.lock(); s && s->sigUser) {
                        s->sigUser->Leave();
                    }
                };
                auto onFailed = [wstate]() {
                    if (auto s = wstate.lock()) {
                        Log::Error("peer connection failed");
                        s->acceptor->OnLink(std::unexpected(Error::Unknown));
                        if (s->sigUser) {
                            s->sigUser->Leave();
                        }
                    }
                };

                auto link = DcRtcLink::Create(localId, remoteId, s->pc, s->dc, maxMsgSize, std::move(onClosed), std::move(onFailed));
                auto handler = s->acceptor->OnLink(link);
                link->SetHandler(std::move(handler));
            });

            // Return State as the ISigHandler — it will receive OnMessage and OnLeft.
            return std::weak_ptr<ISigHandler>{s};
        };

        Log::Debug("joining signaling as {}", _options.localId.value);
        _options.sigClient->Join(_options.localId, std::move(onJoined));
    }
}
#endif
