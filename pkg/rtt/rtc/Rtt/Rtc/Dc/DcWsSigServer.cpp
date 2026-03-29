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

            // Register on the hub; the returned user keeps the slot alive.
            // Capture the user in the ws callbacks (they share ownership).
            auto user = hub->Register(
                PeerId{connId},
                // Outbound: deliver hub messages to this WebSocket client.
                [ws](SigMessage msg) {
                    if (!ws->isOpen()) {
                        return;
                    }
                    const json envelope = {{"id", msg.from.value}, {"payload", std::move(msg.payload)}};
                    ws->send(envelope.dump());
                });

            // Track the connection so we can clean up on close.
            {
                std::lock_guard lock{impl->connMutex};
                impl->connections[connId] = ws;
            }

            ws->onMessage([impl, hub, connId, user](auto raw) {
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

            ws->onClosed([impl, connId, user]() {
                Log::Info("client disconnected: [{}]", connId);
                std::lock_guard lock{impl->connMutex};
                impl->connections.erase(connId);
                // Dropping `user` here unregisters the peer from the hub.
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

} // namespace Rtt::Rtc
#endif
