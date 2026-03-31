#if !defined(__EMSCRIPTEN__)
#include "DcRtcServer.h"
#include "DcRtcLink.h"
#include "DcRtcConfig.h"

#include "Log/Log.h"
#include "Rtt/Rtc/ISigUser.h"

#include <nlohmann/json.hpp>
#include <rtc/rtc.hpp>

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

namespace Rtt::Rtc
{
    using json = nlohmann::json;

    // ---------------------------------------------------------------------------
    // DcRtcServer::State — implements ISigHandler for the server peer identity
    // ---------------------------------------------------------------------------

    struct DcRtcServer::State
        : ISigHandler
        , std::enable_shared_from_this<State>
    {
        Log::Logger logger{"DcRtcServer"};

        std::shared_ptr<ILinkAcceptor> acceptor;
        std::shared_ptr<ISigUser> sigUser;

        PeerId localId;
        rtc::Configuration config;
        std::size_t maxMessageSize = 65535;

        std::mutex mutex;
        std::unordered_map<std::string, std::shared_ptr<DcRtcLink>> peers;

        // ISigHandler — handles offers and ICE candidates from remote peers.
        void OnMessage(SigMessage&& msg) override
        {
            const auto j = json::parse(std::move(msg).payload, nullptr, /*exceptions=*/false);
            if (j.is_discarded()) {
                return;
            }

            const auto type = j.value("type", std::string{});
            const auto& remotePeerId = msg.from;

            if (type == "offer") {
                logger.Trace("offer received from {}", remotePeerId.value);
                HandleOffer(remotePeerId, j);
            } else if (type == "candidate") {
                auto candidate = j.value("candidate", std::string{});
                auto mid = j.value("mid", std::string{});
                std::shared_ptr<DcRtcLink> link;
                {
                    std::lock_guard lock{mutex};
                    if (auto it = peers.find(remotePeerId.value); it != peers.end()) {
                        link = it->second;
                    }
                }
                if (link) {
                    link->AddRemoteCandidate(std::move(candidate), std::move(mid), remotePeerId);
                }
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

    private:
        void HandleOffer(const PeerId& remotePeerId, const json& j)
        {
            const auto sdp = j.value("description", std::string{});

            auto link = std::make_shared<DcRtcLink>(
                config,
                localId,
                remotePeerId,
                std::weak_ptr<ISigUser>{sigUser},
                maxMessageSize,
                logger
            );
            {
                std::lock_guard lock{mutex};
                peers[remotePeerId.value] = link;
            }

            auto wself = std::weak_ptr<State>{shared_from_this()};
            const auto wacceptor = std::weak_ptr<ILinkAcceptor>{acceptor};

            link->pc.onDataChannel(
                [wlink = std::weak_ptr<DcRtcLink>{link}, wself, wacceptor, remotePeerId](std::shared_ptr<rtc::DataChannel> dc) {
                    auto link = wlink.lock();
                    auto acc = wacceptor.lock();
                    if (!link || !acc) {
                        return;
                    }

                    // Both closed and failed paths erase the peer from the map.
                    auto onPeerGone = [wself, remotePeerId]() {
                        if (auto s = wself.lock()) {
                            std::lock_guard lock{s->mutex};
                            s->peers.erase(remotePeerId.value);
                        }
                    };

                    link->Attach(std::move(dc), onPeerGone, onPeerGone);
                    auto handler = acc->OnLink(link);
                    link->SetHandler(std::move(handler));
                }
            );

            link->SetRemoteDescription(sdp, rtc::Description::Type::Offer, remotePeerId);
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
        state->config = BuildConfiguration(_options);
        state->maxMessageSize = _options.maxMessageSize;

        _state = state;

        auto wstate = std::weak_ptr<State>{state};

        auto onJoined = [wstate](SigJoinResult result) -> std::weak_ptr<ISigHandler> {
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
            s->logger.Debug("signaling ready as {}", s->localId.value);
            return std::weak_ptr<ISigHandler>{s};
        };

        state->logger.Debug("joining signaling as {}", _options.localId.value);
        _options.sigClient->Join(_options.localId, std::move(onJoined));
    }
}
#endif
