#if __EMSCRIPTEN__
#include "JsWsSigClient.h"

#include "Log/Log.h"

#include <emscripten/em_js.h>

namespace Rtt::Rtc
{

// ---------------------------------------------------------------------------
// JsWsSigContext — heap-allocated state for one Join() call
//
// Lifecycle:
//   Allocated in Join(), passed as opaque pointer into JavaScript.
//
//   Connection phase (before onOpen):
//     - ctx lives on the heap, referenced only by the JS map
//     - OnOpen: ctx ownership transfers to JsWsSigUser (do NOT delete)
//     - OnError: fires the join handler with failure, then deletes ctx
//
//   Session phase (after onOpen):
//     - ctx is owned by JsWsSigUser (shared_ptr<ISigUser> held by caller)
//     - OnClose (server-side): marks state; ctx is still owned by user
//     - ~JsWsSigUser: calls JsWsContext_Close then deletes ctx
// ---------------------------------------------------------------------------
struct JsWsSigContext;

// clang-format off

/// Open a new WebSocket connection and register it in the shared JS map.
/// All WebSocket events route back to C++ via EMSCRIPTEN_KEEPALIVE callbacks.
EM_JS(void, JsWsContext_Connect, (JsWsSigContext* ctx, const char* urlPtr), {
    if (!Module._rtt_wsMap) Module._rtt_wsMap = new Map();
    const url = UTF8ToString(urlPtr);
    out("rtt: js_ws: connecting to " + url);
    const ws = new WebSocket(url);
    Module._rtt_wsMap.set(ctx, ws);

    // Each handler captures `ws` by closure and checks object identity against
    // the current map entry.  This prevents stale events from a previous
    // WebSocket whose ctx pointer was reused by a new allocation (ABA problem).

    ws.onerror = e => {
        if (Module._rtt_wsMap.get(ctx) !== ws) return;
        Module._rtt_wsMap.delete(ctx);
        const sp = stackSave();
        _JsWsContext_OnError(ctx, stringToUTF8OnStack(e.message || "WebSocket error"));
        stackRestore(sp);
    };

    ws.onclose = () => {
        if (Module._rtt_wsMap.get(ctx) !== ws) return;
        Module._rtt_wsMap.delete(ctx);
        _JsWsContext_OnClose(ctx);
    };

    ws.onopen = () => {
        _JsWsContext_OnOpen(ctx);
    };

    ws.onmessage = e => {
        if (Module._rtt_wsMap.get(ctx) !== ws) return;
        const text = typeof e.data === 'string' ? e.data : String(e.data);
        try {
            const msg = JSON.parse(text);
            const sp = stackSave();
            _JsWsContext_OnMessage(ctx,
                stringToUTF8OnStack(msg.id || ""),
                stringToUTF8OnStack(msg.payload || ""));
            stackRestore(sp);
        } catch (_) {
            out("rtt: js_ws: invalid JSON, ignoring");
        }
    };
});

/// Send a JSON-enveloped signaling message to the given peer.
EM_JS(void, JsWsContext_Send, (JsWsSigContext* ctx, const char* toPtr, const char* payloadPtr), {
    if (!Module._rtt_wsMap) return;
    const ws = Module._rtt_wsMap.get(ctx);
    if (!ws) return;
    const to      = UTF8ToString(toPtr);
    const payload = UTF8ToString(payloadPtr);
    ws.send(JSON.stringify({id: to, payload}));
});

/// Close the WebSocket and remove it from the JS map.
/// After this call, no further C++ callbacks will fire for this ctx.
EM_JS(void, JsWsContext_Close, (JsWsSigContext* ctx), {
    if (!Module._rtt_wsMap) return;
    const ws = Module._rtt_wsMap.get(ctx);
    if (!ws) return;
    Module._rtt_wsMap.delete(ctx);
    if (ws.readyState === WebSocket.OPEN || ws.readyState === WebSocket.CONNECTING) {
        ws.close();
    }
    // onclose will fire but the map entry is already gone — no C++ callback.
});

// clang-format on

// ---------------------------------------------------------------------------
// JsWsSigUser — ISigUser returned to the caller after a successful open.
// The user owns the ctx and deletes it on destruction.
// ---------------------------------------------------------------------------

class JsWsSigUser : public ISigUser
{
public:
    JsWsSigUser(PeerId id, JsWsSigContext* ctx)
        : _id(std::move(id))
        , _ctx(ctx)
    {}

    ~JsWsSigUser() override;

    JsWsSigUser(const JsWsSigUser&) = delete;
    JsWsSigUser& operator=(const JsWsSigUser&) = delete;
    JsWsSigUser(JsWsSigUser&&) = delete;
    JsWsSigUser& operator=(JsWsSigUser&&) = delete;

    [[nodiscard]] const PeerId& LocalId() const override { return _id; }

    void Send(const PeerId& to, std::string payload) override
    {
        JsWsContext_Send(_ctx, to.value.c_str(), payload.c_str());
    }

    void Leave() override;

private:
    PeerId _id;
    JsWsSigContext* _ctx;
};

// ---------------------------------------------------------------------------
// JsWsSigContext — full definition (after JsWsSigUser is complete)
// ---------------------------------------------------------------------------

struct JsWsSigContext
{
    PeerId id;
    SigJoinHandler onJoined;
    std::weak_ptr<ISigHandler> handler;
    bool fired = false;
    bool leaving = false;

    void OnOpen(std::shared_ptr<JsWsSigUser> user)
    {
        if (fired) {
            return;
        }
        fired = true;
        Log::Info("[{}] signaling connected", id.value);
        handler = std::move(onJoined)(SigJoinResult{std::move(user)});
        onJoined = {};
    }

    void OnError(const char* err)
    {
        if (fired) {
            // onOpen already fired and JsWsSigUser owns ctx — do not delete.
            return;
        }
        fired = true;
        Log::Error("[{}] signaling error: {}", id.value, err);
        std::move(onJoined)(std::unexpected(Error::ConnectionRefused));
        delete this;
    }

    void OnClose()
    {
        if (leaving) {
            return; // Leave() already notified OnLeft({})
        }
        Log::Info("[{}] signaling disconnected", id.value);
        if (auto h = handler.lock()) {
            h->OnLeft(SigError::ConnectionLost);
        }
    }

    void OnMessage(const char* fromId, const char* payload)
    {
        if (auto h = handler.lock()) {
            h->OnMessage(SigMessage{PeerId{fromId}, payload});
        }
    }
};

// JsWsSigUser destructor and Leave() defined here where JsWsSigContext is complete.
JsWsSigUser::~JsWsSigUser()
{
    JsWsContext_Close(_ctx);
    delete _ctx;
}

void JsWsSigUser::Leave()
{
    if (_ctx->leaving) {
        return;
    }
    _ctx->leaving = true;
    auto h = _ctx->handler.lock();
    JsWsContext_Close(_ctx); // removes from JS map — onclose won't call OnClose
    if (h) {
        h->OnLeft({});
    }
}

// ---------------------------------------------------------------------------
// C → C++ bridge  (exported so EM_JS JS code can call back into C++)
// ---------------------------------------------------------------------------

extern "C"
{
    EMSCRIPTEN_KEEPALIVE void JsWsContext_OnOpen(JsWsSigContext* ctx)
    {
        // Ownership of ctx transfers to JsWsSigUser — do NOT delete here.
        auto user = std::make_shared<JsWsSigUser>(ctx->id, ctx);
        ctx->OnOpen(std::move(user));
    }

    EMSCRIPTEN_KEEPALIVE void JsWsContext_OnError(JsWsSigContext* ctx, const char* err)
    {
        ctx->OnError(err);
    }

    EMSCRIPTEN_KEEPALIVE void JsWsContext_OnClose(JsWsSigContext* ctx)
    {
        ctx->OnClose();
    }

    EMSCRIPTEN_KEEPALIVE void JsWsContext_OnMessage(
        JsWsSigContext* ctx,
        const char* fromId,
        const char* payload)
    {
        ctx->OnMessage(fromId, payload);
    }
}

// ---------------------------------------------------------------------------
// JsWsSigClient
// ---------------------------------------------------------------------------

JsWsSigClient::JsWsSigClient()
    : JsWsSigClient(Options{})
{}

JsWsSigClient::JsWsSigClient(Options options)
    : _options(std::move(options))
{}

JsWsSigClient::~JsWsSigClient() = default;

void JsWsSigClient::Join(PeerId id, SigJoinHandler onJoined)
{
    const std::string url = "ws://" + _options.host + ":" +
                            std::to_string(_options.port) + "/" + id.value;

    Log::Debug("[{}] connecting to {}", id.value, url);

    auto* ctx = new JsWsSigContext{
        .id = id,
        .onJoined = std::move(onJoined),
    };
    JsWsContext_Connect(ctx, url.c_str());
}

} // namespace Rtt::Rtc
#endif
