#if !defined(__EMSCRIPTEN__)
#include "DcRtcClient.h"
#include "DcRtcLink.h"
#include "DcRtcConfig.h"

#include "Log/Log.h"
#include "Rtt/Rtc/ISigUser.h"

#include <nlohmann/json.hpp>
#include <rtc/rtc.hpp>

#include <memory>
#include <string>
#include <utility>

namespace Rtt::Rtc
{
    using json = nlohmann::json;

    // ---------------------------------------------------------------------------
    // DcRtcClient::State — holds all negotiation objects, implements ISigHandler
    // ---------------------------------------------------------------------------

    struct DcRtcClient::State: ISigHandler
    {
        Log::Logger logger{"DcRtcClient"};

        std::shared_ptr<ILinkAcceptor> acceptor;
        std::shared_ptr<ISigUser> sigUser;

        std::shared_ptr<DcRtcLink> link;

        PeerId localId;
        PeerId remoteId;
        std::size_t maxMessageSize = 65535;

        // ISigHandler — routes incoming SDP answers and ICE candidates.
        //
        // libdatachannel fires onLocalDescription and onLocalCandidate from
        // independent background threads on the REMOTE peer, so candidates from
        // the server can arrive at the client before the answer — even though the
        // answer is always generated first. DcRtcLink buffers candidates until
        // SetRemoteDescription is called.
        void OnMessage(SigMessage&& msg) override
        {
            const auto& fromPeerId = msg.from;
            if (!link) {
                logger.Warn("no peer connection exists, ignoring message from {}", fromPeerId.value);
                return;
            }
            const auto j = json::parse(std::move(msg).payload, nullptr, /*exceptions=*/false);
            if (j.is_discarded()) {
                logger.Warn("failed to parse message from {}", fromPeerId.value);
                return;
            }
            const auto type = j.value("type", std::string{});
            if (type == "answer") {
                const auto sdp = j.value("description", std::string{});
                link->SetRemoteDescription(sdp, rtc::Description::Type::Answer, fromPeerId);
            } else if (type == "candidate") {
                const auto cand = j.value("candidate", std::string{});
                const auto mid = j.value("mid", std::string{});
                link->AddRemoteCandidate(cand, mid, fromPeerId);
            }
        }

        // ISigHandler — external signaling disconnect.
        void OnLeft(std::error_code ec) override
        {
            if (ec) {
                logger.Warn("signaling connection lost: {}", ec.message());
                acceptor->OnLink(std::unexpected(Error::TransportClosed));
            }
        }
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
        state->maxMessageSize = _options.maxMessageSize;

        _state = state;

        auto iceServers = _options.iceServers;
        auto wstate = std::weak_ptr<State>{state};

        // Factory: called when signaling join completes (success or failure).
        // Returns weak_ptr<ISigHandler> so the signaling layer can route messages.
        auto onJoined = [wstate, config = BuildConfiguration(_options)](SigJoinResult result) mutable -> std::weak_ptr<ISigHandler> {
            auto s = wstate.lock();
            if (!s) {
                return {};
            }
            if (!result) {
                s->logger.Error("signaling join failed: {}", ErrorToString(result.error()));
                s->acceptor->OnLink(std::unexpected(result.error()));
                return {};
            }
            s->sigUser = std::move(*result);

            s->link = std::make_shared<DcRtcLink>(
                std::move(config),
                s->localId,
                s->remoteId,
                std::weak_ptr<ISigUser>{s->sigUser},
                s->maxMessageSize,
                s->logger
            );

            auto dc = s->link->pc.createDataChannel("data");

            dc->onOpen([wstate, dc]() {
                auto s = wstate.lock();
                if (!s) {
                    return;
                }

                auto onClosed = [wstate]() {
                    // PC teardown complete — leave signaling to close the WS connection.
                    if (auto s = wstate.lock(); s && s->sigUser) {
                        s->sigUser->Leave();
                    }
                };
                auto onFailed = [wstate]() {
                    if (auto s = wstate.lock()) {
                        s->logger.Error("peer connection failed");
                        s->acceptor->OnLink(std::unexpected(Error::Unknown));
                        if (s->sigUser) {
                            s->sigUser->Leave();
                        }
                    }
                };

                s->link->Attach(dc, std::move(onClosed), std::move(onFailed));
                auto handler = s->acceptor->OnLink(s->link);
                s->link->SetHandler(std::move(handler));
            });

            // Return State as the ISigHandler — it will receive OnMessage and OnLeft.
            return std::weak_ptr<ISigHandler>{s};
        };

        state->logger.Debug("joining signaling as {}", _options.localId.value);
        _options.sigClient->Join(_options.localId, std::move(onJoined));
    }
}
#endif
