#if __EMSCRIPTEN__
#include "JsRtcTransport.h"

#include "Log/Log.h"
#include "Rtt/Rtc/ISigUser.h"

#include <nlohmann/json.hpp>

#include <emscripten/em_js.h>

#include <format>
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
    // Forward declarations
    // ---------------------------------------------------------------------------

    struct JsRtcPeerContext;

    // ---------------------------------------------------------------------------
    // EM_JS bridge functions (C++ → JavaScript)
    //
    // Module._rtt_pcMap: Map<ctx*, {pc, dc, descSet, pendingCandidates}>
    //   pc  — RTCPeerConnection
    //   dc  — RTCDataChannel (set after DC is created/received)
    //   descSet — bool: true once setRemoteDescription resolved
    //   pendingCandidates — [{candidate, sdpMid}] buffered before descSet
    // ---------------------------------------------------------------------------

    // clang-format off

    /// Create an RTCPeerConnection as offerer, create a DataChannel, wire ICE/state
    /// events, then createOffer → setLocalDescription.
    /// Fires _JsRtcPc_OnLocalDescription and _JsRtcPc_OnLocalCandidate async.
    /// Fires _JsRtcPc_OnDataChannelOpen when the DataChannel opens.
    /// Fires _JsRtcPc_OnDataChannelClose when the DataChannel closes.
    EM_JS(void, JsRtcPc_SetupOfferer, (JsRtcPeerContext* ctx, const char* iceJsonPtr), {
        if (!Module._rtt_pcMap) Module._rtt_pcMap = new Map();
        const iceServers = JSON.parse(UTF8ToString(iceJsonPtr));
        const pc = new RTCPeerConnection({iceServers});
        const dc = pc.createDataChannel('data');
        dc.binaryType = 'arraybuffer';
        const entry = {pc, dc, descSet: false, pendingCandidates: []};
        Module._rtt_pcMap.set(ctx, entry);

        pc.onicecandidate = e => {
            if (!e.candidate) return;
            const sp = stackSave();
            _JsRtcPc_OnLocalCandidate(ctx,
                stringToUTF8OnStack(e.candidate.candidate),
                stringToUTF8OnStack(e.candidate.sdpMid || ""));
            stackRestore(sp);
        };

        dc.onopen = () => {
            _JsRtcPc_OnDataChannelOpen(ctx);
        };

        dc.onclose = () => {
            _JsRtcPc_OnDataChannelClose(ctx);
        };

        dc.onmessage = e => {
            const buf = e.data;
            if (!(buf instanceof ArrayBuffer)) return;
            const view = new Uint8Array(buf);
            const ptr = _malloc(view.byteLength);
            HEAPU8.set(view, ptr);
            _JsRtcPc_OnMessage(ctx, ptr, view.byteLength);
            _free(ptr);
        };

        pc.createOffer()
            .then(desc => pc.setLocalDescription(desc))
            .then(() => {
                const sp = stackSave();
                _JsRtcPc_OnLocalDescription(ctx,
                    stringToUTF8OnStack(pc.localDescription.type),
                    stringToUTF8OnStack(pc.localDescription.sdp));
                stackRestore(sp);
            });
    });

    /// Create an RTCPeerConnection as answerer, wire ICE/state events,
    /// setRemoteDescription(offer) → createAnswer → setLocalDescription.
    /// Buffers incoming ICE candidates until setRemoteDescription resolves.
    /// Fires _JsRtcPc_OnLocalDescription and _JsRtcPc_OnLocalCandidate async.
    /// Fires _JsRtcPc_OnDataChannelOpen/_OnDataChannelClose via ondatachannel.
    EM_JS(void, JsRtcPc_SetupAnswerer, (JsRtcPeerContext* ctx, const char* offerSdpPtr, const char* iceJsonPtr), {
        if (!Module._rtt_pcMap) Module._rtt_pcMap = new Map();
        const iceServers = JSON.parse(UTF8ToString(iceJsonPtr));
        const offerSdp   = UTF8ToString(offerSdpPtr);
        const pc = new RTCPeerConnection({iceServers});
        const entry = {pc, dc: null, descSet: false, pendingCandidates: []};
        Module._rtt_pcMap.set(ctx, entry);

        pc.onicecandidate = e => {
            if (!e.candidate) return;
            const sp = stackSave();
            _JsRtcPc_OnLocalCandidate(ctx,
                stringToUTF8OnStack(e.candidate.candidate),
                stringToUTF8OnStack(e.candidate.sdpMid || ""));
            stackRestore(sp);
        };

        pc.ondatachannel = e => {
            const dc = e.channel;
            dc.binaryType = 'arraybuffer';
            entry.dc = dc;

            dc.onopen = () => {
                _JsRtcPc_OnDataChannelOpen(ctx);
            };

            dc.onclose = () => {
                _JsRtcPc_OnDataChannelClose(ctx);
            };

            dc.onmessage = ev => {
                const buf = ev.data;
                if (!(buf instanceof ArrayBuffer)) return;
                const view = new Uint8Array(buf);
                const ptr = _malloc(view.byteLength);
                HEAPU8.set(view, ptr);
                _JsRtcPc_OnMessage(ctx, ptr, view.byteLength);
                _free(ptr);
            };
        };

        pc.setRemoteDescription({type: 'offer', sdp: offerSdp})
            .then(() => {
                entry.descSet = true;
                const pending = entry.pendingCandidates.splice(0);
                pending.forEach(c => pc.addIceCandidate(c));
                return pc.createAnswer();
            })
            .then(desc => pc.setLocalDescription(desc))
            .then(() => {
                const sp = stackSave();
                _JsRtcPc_OnLocalDescription(ctx,
                    stringToUTF8OnStack(pc.localDescription.type),
                    stringToUTF8OnStack(pc.localDescription.sdp));
                stackRestore(sp);
            });
    });

    /// Apply a remote SDP answer to the offerer's PeerConnection and flush
    /// any ICE candidates that arrived before the answer.
    EM_JS(void, JsRtcPc_SetRemoteAnswer, (JsRtcPeerContext* ctx, const char* answerSdpPtr), {
        if (!Module._rtt_pcMap) return;
        const entry = Module._rtt_pcMap.get(ctx);
        if (!entry) return;
        const answerSdp = UTF8ToString(answerSdpPtr);
        entry.pc.setRemoteDescription({type: 'answer', sdp: answerSdp})
            .then(() => {
                entry.descSet = true;
                const pending = entry.pendingCandidates.splice(0);
                pending.forEach(c => entry.pc.addIceCandidate(c));
            });
    });

    /// Add a remote ICE candidate. Buffers it if the remote description is not
    /// yet set (i.e. the answer has not been applied), otherwise applies immediately.
    EM_JS(void, JsRtcPc_AddIceCandidate, (JsRtcPeerContext* ctx, const char* candPtr, const char* midPtr), {
        if (!Module._rtt_pcMap) return;
        const entry = Module._rtt_pcMap.get(ctx);
        if (!entry) return;
        const cand = {candidate: UTF8ToString(candPtr), sdpMid: UTF8ToString(midPtr)};
        if (entry.descSet) {
            entry.pc.addIceCandidate(cand);
        } else {
            entry.pendingCandidates.push(cand);
        }
    });

    /// Send binary data over the DataChannel.
    /// Copies the given WASM heap slice into a Uint8Array and calls dc.send().
    EM_JS(void, JsRtcPc_Send, (JsRtcPeerContext* ctx, const void* dataPtr, int size), {
        if (!Module._rtt_pcMap) return;
        const entry = Module._rtt_pcMap.get(ctx);
        if (!entry || !entry.dc || entry.dc.readyState !== 'open') return;
        entry.dc.send(HEAPU8.slice(dataPtr, dataPtr + size));
    });

    /// Close the PeerConnection and remove the entry from the JS map.
    /// After this call no further C++ callbacks will fire for this ctx.
    EM_JS(void, JsRtcPc_Close, (JsRtcPeerContext* ctx), {
        if (!Module._rtt_pcMap) return;
        const entry = Module._rtt_pcMap.get(ctx);
        if (!entry) return;
        Module._rtt_pcMap.delete(ctx);
        try { entry.pc.close(); } catch (_) {}
    });

    // clang-format on

    // ---------------------------------------------------------------------------
    // JsRtcLink — ILink over a browser DataChannel / PeerConnection
    //
    // Lifetime:
    //   Created inside JsRtcPc_OnDataChannelOpen (EMSCRIPTEN_KEEPALIVE).
    //   Owns JsRtcPeerContext* and deletes it on destruction.
    //   _onGone: called from destructor to remove ctx from the State::peers map.
    // ---------------------------------------------------------------------------

    class JsRtcLink: public ILink
    {
    public:
        JsRtcLink(PeerId localId, PeerId remoteId, JsRtcPeerContext* ctx,
                  std::size_t maxMessageSize, std::function<void()> onGone)
            : _localId(std::move(localId))
            , _remoteId(std::move(remoteId))
            , _ctx(ctx)
            , _maxMessageSize(maxMessageSize)
            , _onGone(std::move(onGone))
        {}

        ~JsRtcLink() override;

        JsRtcLink(const JsRtcLink&) = delete;
        JsRtcLink& operator=(const JsRtcLink&) = delete;
        JsRtcLink(JsRtcLink&&) = delete;
        JsRtcLink& operator=(JsRtcLink&&) = delete;

        [[nodiscard]] const PeerId& LocalId() const override { return _localId; }
        [[nodiscard]] const PeerId& RemoteId() const override { return _remoteId; }

        void Send(WriteCallback writer) override;
        void Disconnect() override;

        void SetHandler(LinkHandler handler) { _handler = std::move(handler); }

        /// Called from EMSCRIPTEN_KEEPALIVE JsRtcPc_OnDataChannelClose.
        void OnClose()
        {
            if (_closedFired.exchange(true)) {
                return;
            }
            if (_handler.onDisconnected) {
                _handler.onDisconnected();
            }
        }

        /// Called from EMSCRIPTEN_KEEPALIVE JsRtcPc_OnMessage.
        void OnMessage(const void* data, int size)
        {
            if (_handler.onReceived) {
                _handler.onReceived(std::span<const std::byte>{
                    static_cast<const std::byte*>(data),
                    static_cast<std::size_t>(size)});
            }
        }

    private:
        PeerId _localId;
        PeerId _remoteId;
        JsRtcPeerContext* _ctx;
        std::size_t _maxMessageSize;
        std::function<void()> _onGone;
        LinkHandler _handler;
        std::atomic<bool> _closedFired{false};
        bool _disconnectRequested{false};
    };

    // ---------------------------------------------------------------------------
    // JsRtcPeerContext — heap-allocated state for one peer connection.
    //
    // Lifecycle (offerer / client):
    //   Allocated in State::OpenOfferer(), passed as opaque pointer into JS.
    //   Before DC opens: lives on the heap, referenced only by Module._rtt_pcMap.
    //   OnDataChannelOpen: ownership transfers to JsRtcLink — do NOT delete here.
    //   OnError (JS onerror before open): fires acceptor error, then deletes ctx.
    //
    // Lifecycle (answerer / server):
    //   Allocated in State::HandleOffer() per incoming peer.
    //   State::peers[peerId] holds a non-owning raw pointer for ICE routing.
    //   OnDataChannelOpen: JsRtcLink takes ownership; _onGone erases from peers map.
    //   OnError before open: deletes ctx directly.
    // ---------------------------------------------------------------------------

    struct JsRtcPeerContext
    {
        PeerId localId;
        PeerId remoteId;
        std::weak_ptr<ISigUser> sigUser;
        bool opened = false;
        bool closedFired = false;

        // Non-owning raw pointer to the link, set after DC opens.
        // Used to route message/close events after ownership transfers to JsRtcLink.
        JsRtcLink* link = nullptr;

        // [offerer only] called exactly once when DC opens — transfers ctx ownership to JsRtcLink.
        // [answerer] same — called from State::peers routing.
        std::function<void(JsRtcPeerContext*)> onDcOpen;

        // [offerer only] called on signaling error before DC opens.
        std::function<void()> onError;
    };

    // JsRtcLink destructor + Send/Disconnect defined after JsRtcPeerContext is complete.

    JsRtcLink::~JsRtcLink()
    {
        JsRtcPc_Close(_ctx);
        delete _ctx;
        if (_onGone) {
            _onGone();
        }
    }

    void JsRtcLink::Send(WriteCallback writer)
    {
        if (_disconnectRequested || !_ctx) {
            return;
        }
        std::vector<std::byte> buf(_maxMessageSize);
        const auto written = writer(std::span<std::byte>{buf});
        if (written == 0) {
            return;
        }
        JsRtcPc_Send(_ctx, buf.data(), static_cast<int>(written));
    }

    void JsRtcLink::Disconnect()
    {
        if (_disconnectRequested) {
            return;
        }
        _disconnectRequested = true;
        JsRtcPc_Close(_ctx);
        // OnClose() will fire from the DC onclose event.
    }

    // ---------------------------------------------------------------------------
    // C → C++ bridge (EMSCRIPTEN_KEEPALIVE)
    // ---------------------------------------------------------------------------

    extern "C" {
        /// Called when the DataChannel opens. Transfers ctx ownership to JsRtcLink.
        EMSCRIPTEN_KEEPALIVE void JsRtcPc_OnDataChannelOpen(JsRtcPeerContext* ctx)
        {
            if (ctx->opened) {
                return;
            }
            ctx->opened = true;
            if (ctx->onDcOpen) {
                ctx->onDcOpen(ctx); // ownership of ctx transfers to JsRtcLink
            }
        }

        /// Called when the DataChannel closes. Routes to the link if available.
        EMSCRIPTEN_KEEPALIVE void JsRtcPc_OnDataChannelClose(JsRtcPeerContext* ctx)
        {
            if (ctx->link) {
                ctx->link->OnClose();
            }
        }

        /// Called for each incoming binary message.
        EMSCRIPTEN_KEEPALIVE void JsRtcPc_OnMessage(JsRtcPeerContext* ctx, const void* data, int size)
        {
            if (ctx->link) {
                ctx->link->OnMessage(data, size);
            }
        }

        /// Called when a local SDP description is ready (offer or answer).
        /// Routes it to the remote peer via signaling.
        EMSCRIPTEN_KEEPALIVE void JsRtcPc_OnLocalDescription(JsRtcPeerContext* ctx,
                                                              const char* typeStr,
                                                              const char* sdp)
        {
            if (auto su = ctx->sigUser.lock()) {
                const json payload = {{"type", typeStr}, {"description", sdp}};
                su->Send(ctx->remoteId, payload.dump());
            }
        }

        /// Called for each local ICE candidate. Routes it to the remote peer.
        EMSCRIPTEN_KEEPALIVE void JsRtcPc_OnLocalCandidate(JsRtcPeerContext* ctx,
                                                            const char* cand,
                                                            const char* mid)
        {
            if (auto su = ctx->sigUser.lock()) {
                const json payload = {{"type", "candidate"}, {"candidate", cand}, {"mid", mid}};
                su->Send(ctx->remoteId, payload.dump());
            }
        }
    }

    // ---------------------------------------------------------------------------
    // Build iceServers JSON string from vector<string> — e.g.
    //   [{"urls":"stun:stun.l.google.com:19302"},{"urls":"turn:..."}]
    // ---------------------------------------------------------------------------

    static std::string BuildIceJson(const std::vector<std::string>& iceServers)
    {
        json arr = json::array();
        for (const auto& srv : iceServers) {
            arr.push_back({{"urls", srv}});
        }
        return arr.dump();
    }

    // ---------------------------------------------------------------------------
    // JsRtcTransport::State — ISigHandler, manages peer contexts
    // ---------------------------------------------------------------------------

    struct JsRtcTransport::State
        : ISigHandler
        , std::enable_shared_from_this<State>
    {
        explicit State(std::string_view localIdValue)
            : _logName(std::format("JsRtc/{}", localIdValue))
            , logger(_logName.c_str())
        {}

        // _logName must be declared before logger so the c_str() pointer stays valid.
        std::string _logName;
        Log::Logger logger;

        std::shared_ptr<ILinkAcceptor> acceptor;
        std::shared_ptr<ISigUser> sigUser;

        PeerId localId;
        PeerId remoteId; // non-empty → offerer; empty → answerer
        std::vector<std::string> iceServers;
        std::size_t maxMessageSize = 65535;
        std::size_t maxInboundConnections = 4096;

        // Non-owning raw pointers to in-progress peer contexts.
        // Entries removed when JsRtcLink::_onGone fires (after DC open) or on early error.
        std::mutex mutex;
        std::unordered_map<std::string, JsRtcPeerContext*> peers;
        std::size_t inboundCount = 0; // protected by mutex; counts active inbound peer connections

        bool isOfferer() const { return !remoteId.value.empty(); }

        // ISigHandler — routes SDP/ICE to the appropriate PeerContext.
        void OnMessage(SigMessage&& msg) override
        {
            const auto j = json::parse(std::move(msg).payload, nullptr, /*exceptions=*/false);
            if (j.is_discarded()) {
                logger.Warn("failed to parse signaling message from {}", msg.from.value);
                return;
            }
            const auto type = j.value("type", std::string{});
            const auto& fromId = msg.from;

            if (type == "offer") {
                {
                    std::lock_guard lock{mutex};
                    if (maxInboundConnections == 0 || inboundCount >= maxInboundConnections) {
                        logger.Warn("inbound limit reached, rejecting offer from {}", fromId.value);
                        //TODO: notify the offerer of the rejection (e.g. with a specific error message) instead of silently dropping the offer
                        return;
                    }
                }
                const auto sdp = j.value("description", std::string{});
                logger.Trace("offer from {}", fromId.value);
                HandleOffer(fromId, sdp);
            } else if (type == "answer") {
                if (!isOfferer()) {
                    logger.Warn("answerer received unexpected answer from {}", fromId.value);
                    return;
                }
                const auto sdp = j.value("description", std::string{});
                logger.Trace("answer from {}", fromId.value);
                // For offerer there is exactly one peer context.
                JsRtcPeerContext* ctx = nullptr;
                {
                    std::lock_guard lock{mutex};
                    auto it = peers.find(fromId.value);
                    if (it != peers.end()) {
                        ctx = it->second;
                    }
                }
                if (ctx) {
                    JsRtcPc_SetRemoteAnswer(ctx, sdp.c_str());
                }
            } else if (type == "candidate") {
                const auto cand = j.value("candidate", std::string{});
                const auto mid = j.value("mid", std::string{});
                JsRtcPeerContext* ctx = nullptr;
                {
                    std::lock_guard lock{mutex};
                    auto it = peers.find(fromId.value);
                    if (it != peers.end()) {
                        ctx = it->second;
                    }
                }
                if (ctx) {
                    logger.Trace("ICE candidate from {}", fromId.value);
                    JsRtcPc_AddIceCandidate(ctx, cand.c_str(), mid.c_str());
                }
            }
        }

        // ISigHandler — signaling disconnected.
        void OnLeft(std::error_code ec) override
        {
            if (ec) {
                logger.Warn("signaling connection lost: {}", ec.message());
                acceptor->OnLink(std::unexpected(Error::TransportClosed));
            }
        }

        // Start offerer flow: allocate ctx, register in peers map, call JS.
        void OpenOfferer()
        {
            const std::string iceJson = BuildIceJson(iceServers);

            auto wself = std::weak_ptr<State>{shared_from_this()};
            auto waccept = std::weak_ptr<ILinkAcceptor>{acceptor};

            auto* ctx = new JsRtcPeerContext{
                .localId = localId,
                .remoteId = remoteId,
                .sigUser = std::weak_ptr<ISigUser>{sigUser},
            };

            ctx->onDcOpen = [wself, waccept, maxMsgSize = maxMessageSize](JsRtcPeerContext* c) {
                auto s = wself.lock();
                auto acc = waccept.lock();
                if (!s || !acc) {
                    // State is gone — the link was never delivered; clean up.
                    JsRtcPc_Close(c);
                    delete c;
                    return;
                }

                auto onGone = [wself, remId = c->remoteId]() {
                    if (auto ss = wself.lock()) {
                        std::lock_guard lock{ss->mutex};
                        ss->peers.erase(remId.value);
                    }
                };

                s->logger.Debug("data channel open {} -> {}", c->localId.value, c->remoteId.value);
                auto link = std::make_shared<JsRtcLink>(
                    c->localId, c->remoteId, c, maxMsgSize, std::move(onGone));
                c->link = link.get(); // non-owning backref
                auto handler = acc->OnLink(link);
                link->SetHandler(std::move(handler));
            };

            ctx->onError = [wself, waccept]() {
                if (auto s = wself.lock()) {
                    s->logger.Error("peer connection error before DC opened");
                }
                if (auto acc = waccept.lock()) {
                    acc->OnLink(std::unexpected(Error::ConnectionRefused));
                }
            };

            {
                std::lock_guard lock{mutex};
                peers[remoteId.value] = ctx;
            }

            logger.Debug("starting offerer {} -> {}", localId.value, remoteId.value);
            JsRtcPc_SetupOfferer(ctx, iceJson.c_str());
        }

        // Handle an incoming offer: allocate ctx, call JS, wire DC-open callback.
        void HandleOffer(const PeerId& fromId, const std::string& offerSdp)
        {
            const std::string iceJson = BuildIceJson(iceServers);

            auto wself = std::weak_ptr<State>{shared_from_this()};
            auto waccept = std::weak_ptr<ILinkAcceptor>{acceptor};

            auto* ctx = new JsRtcPeerContext{
                .localId = localId,
                .remoteId = fromId,
                .sigUser = std::weak_ptr<ISigUser>{sigUser},
            };

            ctx->onDcOpen = [wself, waccept, maxMsgSize = maxMessageSize](JsRtcPeerContext* c) {
                auto s = wself.lock();
                auto acc = waccept.lock();
                if (!s || !acc) {
                    JsRtcPc_Close(c);
                    delete c;
                    return;
                }

                auto onGone = [wself, remId = c->remoteId]() {
                    if (auto ss = wself.lock()) {
                        std::lock_guard lock{ss->mutex};
                        ss->peers.erase(remId.value);
                        if (ss->inboundCount > 0) {
                            --ss->inboundCount;
                        }
                    }
                };

                s->logger.Debug("data channel open {} -> {}", c->localId.value, c->remoteId.value);
                auto link = std::make_shared<JsRtcLink>(
                    c->localId, c->remoteId, c, maxMsgSize, std::move(onGone));
                c->link = link.get();
                auto handler = acc->OnLink(link);
                link->SetHandler(std::move(handler));
            };

            {
                std::lock_guard lock{mutex};
                peers[fromId.value] = ctx;
                ++inboundCount;
            }

            logger.Debug("answering offer from {}", fromId.value);
            JsRtcPc_SetupAnswerer(ctx, offerSdp.c_str(), iceJson.c_str());
        }
    };

    // ---------------------------------------------------------------------------
    // JsRtcTransport
    // ---------------------------------------------------------------------------

    JsRtcTransport::JsRtcTransport(Options options)
        : _options(std::move(options))
    {}

    JsRtcTransport::~JsRtcTransport() = default;

    void JsRtcTransport::Open(std::shared_ptr<ILinkAcceptor> acceptor)
    {
        auto state = std::make_shared<State>(_options.localId.value);
        state->acceptor = std::move(acceptor);
        state->localId = _options.localId;
        state->remoteId = _options.remoteId;
        state->iceServers = _options.iceServers;
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
            s->logger.Debug("signaling ready as {}", s->localId.value);

            if (s->isOfferer()) {
                s->OpenOfferer();
            }
            // Answerer: waits for OnMessage with type=="offer".

            return std::weak_ptr<ISigHandler>{s};
        };

        state->logger.Debug("joining signaling as {}", _options.localId.value);
        _options.sigClient->Join(_options.localId, std::move(onJoined));
    }
}
#endif
