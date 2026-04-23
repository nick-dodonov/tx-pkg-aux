#if !defined(__EMSCRIPTEN__)
#include "DcRtcTransport.h"
#include "DcRtcLink.h"
#include "DcRtcConfig.h"

#include "Log/Log.h"
#include "Rtt/Rtc/ISigUser.h"

#include <nlohmann/json.hpp>
#include <rtc/rtc.hpp>

#include <format>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

namespace Rtt::Rtc
{
    using json = nlohmann::json;

    // ---------------------------------------------------------------------------
    // DcRtcTransport::State — ISigHandler, manages offerer and answerer connections.
    //
    // peers map holds all active DcRtcLink instances:
    //   - outbound link keyed on remoteId.value  (offerer mode)
    //   - inbound links keyed on each peer's ID  (answerer mode)
    // ---------------------------------------------------------------------------

    struct DcRtcTransport::State
        : ISigHandler
        , std::enable_shared_from_this<State>
    {
        explicit State(std::string_view localIdValue)
            : _logName(std::format("DcRtc/{}", localIdValue))
            , logger(_logName.c_str())
        {}

        // _logName must be declared before logger so the c_str() pointer stays valid.
        std::string _logName;
        Log::Logger logger;

        std::shared_ptr<ILinkAcceptor> acceptor;
        std::shared_ptr<ISigUser> sigUser;

        PeerId localId;
        rtc::Configuration config;
        std::size_t maxMessageSize = 65535;
        std::size_t maxInboundConnections = 4096;

        std::mutex mutex;
        std::unordered_map<std::string, std::shared_ptr<DcRtcLink>> peers;
        std::size_t inboundCount = 0; // protected by mutex; counts active inbound peer connections
        std::vector<PeerId> pendingConnects; // connections queued before signaling is ready; protected by mutex

        // ISigHandler — routes SDP and ICE messages to the appropriate link.
        //
        // libdatachannel fires signaling callbacks from internal background threads,
        // so ICE candidates can arrive before the SDP answer. DcRtcLink buffers
        // candidates until SetRemoteDescription is called.
        void OnMessage(SigMessage&& msg) override
        {
            const auto& fromId = msg.from;
            const auto j = json::parse(std::move(msg).payload, nullptr, /*exceptions=*/false);
            if (j.is_discarded()) {
                logger.Warn("failed to parse signaling message from {}", fromId.value);
                return;
            }

            const auto type = j.value("type", std::string{});

            if (type == "offer") {
                {
                    std::lock_guard lock{mutex};
                    if (maxInboundConnections == 0 || inboundCount >= maxInboundConnections) {
                        logger.Warn("inbound limit reached, rejecting offer from {}", fromId.value);
                        //TODO: notify the offerer of the rejection (e.g. with a specific error message) instead of silently dropping the offer
                        return;
                    }
                }
                logger.Trace("offer from {}", fromId.value);
                HandleOffer(fromId, j);
            } else if (type == "answer") {
                const auto sdp = j.value("description", std::string{});
                logger.Trace("answer from {}", fromId.value);
                std::shared_ptr<DcRtcLink> link;
                {
                    std::lock_guard lock{mutex};
                    if (auto it = peers.find(fromId.value); it != peers.end()) {
                        link = it->second;
                    }
                }
                if (link) {
                    link->SetRemoteDescription(sdp, rtc::Description::Type::Answer, fromId);
                } else {
                    logger.Warn("unexpected answer from {}: no matching offer found", fromId.value);
                }
            } else if (type == "candidate") {
                auto cand = j.value("candidate", std::string{});
                auto mid = j.value("mid", std::string{});
                std::shared_ptr<DcRtcLink> link;
                {
                    std::lock_guard lock{mutex};
                    if (auto it = peers.find(fromId.value); it != peers.end()) {
                        link = it->second;
                    }
                }
                if (link) {
                    link->AddRemoteCandidate(std::move(cand), std::move(mid), fromId);
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

        // Start the offerer flow: create a DcRtcLink, open a DataChannel to remoteId.
        // Called from DcRtcTransport::Connect() once signaling is ready.
        void OpenOfferer(PeerId remoteId)
        {
            auto link = std::make_shared<DcRtcLink>(
                config,
                localId,
                remoteId,
                std::weak_ptr<ISigUser>{sigUser},
                maxMessageSize,
                logger
            );
            {
                std::lock_guard lock{mutex};
                // Check and insert atomically to avoid TOCTOU: HandleOffer() is called
                // from libdatachannel background threads and may insert peers[remoteId]
                // between a separate check and insert.  If a link already exists (inbound
                // answerer PC or a previous outbound still closing), discard the newly
                // created link and bail out.
                if (peers.contains(remoteId.value)) {
                    logger.Debug("skipping outbound to {} — link already exists", remoteId.value);
                    return;
                }
                peers[remoteId.value] = link;
            }

            auto dc = link->pc.createDataChannel("data");
            auto wself = std::weak_ptr<State>{shared_from_this()};
            auto wlink = std::weak_ptr<DcRtcLink>{link};
            auto waccept = std::weak_ptr<ILinkAcceptor>{acceptor};
            const auto remId = remoteId;

            dc->onOpen([wself, wlink, waccept, dc, remId]() {
                auto s = wself.lock();
                auto l = wlink.lock();
                auto acc = waccept.lock();
                if (!s || !l || !acc) {
                    return;
                }

                auto onClosed = [wself, remId]() {
                    if (auto s = wself.lock()) {
                        std::lock_guard lock{s->mutex};
                        s->peers.erase(remId.value);
                    }
                };
                auto onFailed = [wself, waccept, remId]() {
                    if (auto s = wself.lock()) {
                        s->logger.Error("peer connection failed for {}", remId.value);
                        std::lock_guard lock{s->mutex};
                        s->peers.erase(remId.value);
                    }
                    if (auto acc = waccept.lock()) {
                        acc->OnLink(std::unexpected(Error::Unknown));
                    }
                };

                l->Attach(dc, std::move(onClosed), std::move(onFailed));
                auto handler = acc->OnLink(l);
                l->SetHandler(std::move(handler));
            });

            logger.Debug("starting offerer -> {}", remId.value);
        }

        // Handle an incoming offer: create a DcRtcLink, answer the offer.
        // One new peer is added per call; inboundCount is incremented here and
        // decremented when the peer connection eventually closes (onGone).
        void HandleOffer(const PeerId& fromId, const json& j)
        {
            const auto sdp = j.value("description", std::string{});

            // RFC 8829 §5.2 — Glare resolution ("perfect negotiation" pattern).
            //
            // Glare arises when both sides call Connect() to each other concurrently:
            // each side runs OpenOfferer() and simultaneously receives the other's offer.
            // Without resolution, HandleOffer() would overwrite the outbound entry in
            // `peers`, silently destroying the outbound PeerConnection.  The subsequently
            // delivered answer would then be fed to the inbound (answerer) PC, which never
            // sent an offer, causing libdatachannel to throw — and the process to abort.
            //
            // Resolution: assign each side a fixed role using a deterministic, symmetric
            // tiebreaker — lexicographic comparison of string peer IDs:
            //
            //   "polite" side   (localId < remoteId): rolls back its outbound offer,
            //                   removes the outbound link, and continues as answerer.
            //
            //   "impolite" side (localId > remoteId): ignores the incoming offer,
            //                   keeps its outbound link; the polite side will answer and
            //                   the impolite side will receive that answer shortly.
            //
            // Because the comparison is strict and IDs are unique, exactly one side is
            // polite and the other is impolite — no tie is possible.
            {
                std::lock_guard lock{mutex};
                if (peers.contains(fromId.value)) {
                    const bool polite = localId.value < fromId.value;
                    if (polite) {
                        logger.Warn(
                            "glare with {} — rolling back outbound offer and becoming answerer (polite, {} < {})",
                            fromId.value, localId.value, fromId.value
                        );
                        // Remove the outbound link; its PC will be destroyed and the
                        // onOpen callback becomes a no-op (wlink.lock() returns null).
                        // inboundCount was never incremented for this outbound entry.
                        peers.erase(fromId.value);
                    } else {
                        logger.Warn(
                            "glare with {} — ignoring incoming offer, keeping outbound offer (impolite, {} > {})",
                            fromId.value, localId.value, fromId.value
                        );
                        return;
                    }
                }
            }

            auto link = std::make_shared<DcRtcLink>(
                config,
                localId,
                fromId,
                std::weak_ptr<ISigUser>{sigUser},
                maxMessageSize,
                logger
            );
            {
                std::lock_guard lock{mutex};
                peers[fromId.value] = link;
                ++inboundCount;
            }

            auto wself = std::weak_ptr<State>{shared_from_this()};
            auto waccept = std::weak_ptr<ILinkAcceptor>{acceptor};

            link->pc.onDataChannel(
                [wlink = std::weak_ptr<DcRtcLink>{link}, wself, waccept, fromId](
                    std::shared_ptr<rtc::DataChannel> dc) {
                    auto l = wlink.lock();
                    auto acc = waccept.lock();
                    if (!l || !acc) {
                        return;
                    }

                    auto onGone = [wself, fromId]() {
                        if (auto s = wself.lock()) {
                            std::lock_guard lock{s->mutex};
                            s->logger.Trace("onGone: removing inbound peer {} (inboundCount: {} -> {})",
                                fromId.value, s->inboundCount, s->inboundCount > 0 ? s->inboundCount - 1 : 0);
                            s->peers.erase(fromId.value);
                            // Guard against underflow: if this fires more than once the
                            // counter would wrap (size_t is unsigned), silently pushing
                            // inboundCount past maxInboundConnections and blocking all
                            // future inbound offers.  The root cause (double-fire) is
                            // fixed in DcRtcLink::Attach via std::exchange, but log an
                            // error here as a belt-and-suspenders canary.
                            if (s->inboundCount > 0) {
                                --s->inboundCount;
                            } else {
                                s->logger.Error("inboundCount underflow for {} — onGone fired more than once", fromId.value);
                            }
                        }
                    };

                    l->Attach(std::move(dc), onGone, onGone);
                    auto handler = acc->OnLink(l);
                    l->SetHandler(std::move(handler));
                }
            );

            link->SetRemoteDescription(sdp, rtc::Description::Type::Offer, fromId);
        }
    };

    // ---------------------------------------------------------------------------
    // DcRtcTransport
    // ---------------------------------------------------------------------------

    DcRtcTransport::DcRtcTransport(Options options)
        : _options(std::move(options))
    {}

    DcRtcTransport::~DcRtcTransport() = default;

    std::shared_ptr<IConnector> DcRtcTransport::Open(std::shared_ptr<ILinkAcceptor> acceptor)
    {
        auto state = std::make_shared<State>(_options.localId.value);
        state->acceptor = std::move(acceptor);
        state->localId = _options.localId;
        state->config = BuildConfiguration(_options);
        state->maxMessageSize = _options.maxMessageSize;
        state->maxInboundConnections = _options.maxInboundConnections;

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
            s->logger.Debug("signaling ready");
            // Flush any Connect() calls that arrived before signaling was ready.
            std::vector<PeerId> pending;
            {
                std::lock_guard lock{s->mutex};
                pending = std::move(s->pendingConnects);
            }
            for (auto& id : pending) {
                s->OpenOfferer(std::move(id));
            }
            // Answerer waits for OnMessage("offer").
            return std::weak_ptr<ISigHandler>{s};
        };

        state->logger.Debug("joining signaling as {}", _options.localId.value);
        _options.sigClient->Join(_options.localId, std::move(onJoined));

        return std::shared_ptr<IConnector>(shared_from_this(), static_cast<IConnector*>(this));
    }

    void DcRtcTransport::Connect(PeerId remoteId)
    {
        if (!_state) {
            return;
        }
        {
            std::lock_guard lock{_state->mutex};
            if (!_state->sigUser) {
                _state->pendingConnects.push_back(std::move(remoteId));
                return;
            }
        }
        _state->OpenOfferer(std::move(remoteId));
    }
}
#endif
