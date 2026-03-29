#if !defined(__EMSCRIPTEN__)
#include "DcWsSigServer.h"

#include "Log/Log.h"

#include <nlohmann/json.hpp>
#include <rtc/rtc.hpp>

#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace Rtt::Rtc
{

using json = nlohmann::json;

// ---------------------------------------------------------------------------
// WsClientHandler — ISigHandler for one connected WebSocket client
// ---------------------------------------------------------------------------

struct WsClientHandler : ISigHandler
{
    std::shared_ptr<rtc::WebSocket> ws;
    std::string connId;
    std::shared_ptr<ISigUser> user; // keeps hub registration alive

    void OnMessage(SigMessage&& msg) override
    {
        if (!ws->isOpen()) {
            return;
        }
        const json envelope = {{"id", msg.from.value}, {"payload", std::move(msg.payload)}};
        ws->send(envelope.dump());
    }

    void OnLeft(std::error_code /*ec*/) override
    {
        // No-op: server-side handler — Leave() was called from onClosed.
    }
};

// ---------------------------------------------------------------------------
// DcWsSigServer::Impl — owns the libdatachannel resources
// ---------------------------------------------------------------------------

struct DcWsSigServer::Impl
{
    std::shared_ptr<rtc::WebSocketServer> server;
    std::atomic<std::uint16_t> port{0};

    // Track active connections: connId → weak_ptr<rtc::WebSocket>
    // Used to avoid use-after-free when a client disconnects while a message
    // is in flight.  The map is cleaned up on close events.
    std::mutex connMutex;
    std::unordered_map<std::string, std::weak_ptr<rtc::WebSocket>> connections;
};

// ---------------------------------------------------------------------------
// DcWsSigServer
// ---------------------------------------------------------------------------

DcWsSigServer::DcWsSigServer(std::shared_ptr<SigHub> hub, Options options)
    : _hub(std::move(hub))
    , _options(options)
{}

DcWsSigServer::~DcWsSigServer() = default;

void DcWsSigServer::Start()
{
    _impl = std::make_shared<Impl>();

    rtc::WebSocketServerConfiguration cfg;
    cfg.port = _options.port;

    auto impl = _impl;
    auto hub = _hub;

    _impl->server = std::make_shared<rtc::WebSocketServer>(cfg);

    // onClient: called for each new WebSocket connection.
    // NOTE: path() is only available after state transitions to Open, so we
    // defer peer registration to the onOpen callback.
    _impl->server->onClient([impl, hub](std::shared_ptr<rtc::WebSocket> ws) {

        ws->onOpen([ws, impl, hub]() {
            // Extract the peer ID from the URL path: ws://host:port/<peerId>
            const auto path = ws->path().value_or("");
            const auto connId = path.empty() ? path : path.substr(1); // strip leading '/'

            if (connId.empty()) {
                Log::Warn("client connected with empty peer ID, closing");
                ws->close();
                return;
            }

            Log::Info("client connected: [{}]", connId);

            // Create handler that forwards hub messages to this WS client.
            auto handler = std::make_shared<WsClientHandler>();
            handler->ws = ws;
            handler->connId = connId;

            hub->Register(
                PeerId{connId},
                [handler](SigJoinResult result) -> std::weak_ptr<ISigHandler> {
                    if (!result) {
                        return {};
                    }
                    handler->user = std::move(*result);
                    return handler;
                });

            // Track the connection so we can clean up on close.
            {
                std::lock_guard lock{impl->connMutex};
                impl->connections[connId] = ws;
            }

            ws->onMessage([impl, hub, connId, handler](auto raw) {
                // The signaling protocol uses text frames; ignore binary.
                if (!std::holds_alternative<std::string>(raw)) {
                    return;
                }

                const auto& text = std::get<std::string>(raw);
                const auto msg = json::parse(text, nullptr, /*exceptions=*/false);
                if (msg.is_discarded()) {
                    Log::Warn("[{}] invalid JSON, ignoring", connId);
                    return;
                }

                const auto toIt = msg.find("id");
                const auto plIt = msg.find("payload");
                if (toIt == msg.end() || plIt == msg.end()) {
                    Log::Warn("[{}] missing id or payload field, ignoring", connId);
                    return;
                }

                const std::string to = toIt->template get<std::string>();
                const std::string payload = plIt->template get<std::string>();

                Log::Debug("[{}] -> [{}]  {} bytes", connId, to, payload.size());
                hub->Dispatch(PeerId{connId}, PeerId{to}, payload);
            });

            ws->onClosed([impl, connId, handler]() {
                Log::Info("client disconnected: [{}]", connId);
                {
                    std::lock_guard lock{impl->connMutex};
                    impl->connections.erase(connId);
                }
                // Explicit leave so the hub unregisters the peer.
                if (handler->user) {
                    handler->user->Leave();
                }
            });
        });
    });

    _impl->port = _impl->server->port();
    Log::Info("signaling server listening on port {}", _impl->port.load());
}

std::uint16_t DcWsSigServer::Port() const noexcept
{
    return _impl ? _impl->port.load() : 0;
}

void DcWsSigServer::DisconnectPeer(const PeerId& peerId)
{
    if (!_impl) {
        return;
    }
    std::shared_ptr<rtc::WebSocket> ws;
    {
        std::lock_guard lock{_impl->connMutex};
        if (auto it = _impl->connections.find(peerId.value); it != _impl->connections.end()) {
            ws = it->second.lock();
        }
    }
    if (ws && ws->isOpen()) {
        ws->close();
    }
}

} // namespace Rtt::Rtc
#endif
