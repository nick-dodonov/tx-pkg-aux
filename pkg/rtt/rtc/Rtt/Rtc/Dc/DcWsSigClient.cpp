#if !defined(__EMSCRIPTEN__)
#include "DcWsSigClient.h"

#include "Log/Log.h"

#include <nlohmann/json.hpp>
#include <rtc/rtc.hpp>

#include <atomic>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

namespace Rtt::Rtc
{
    using json = nlohmann::json;

    // ---------------------------------------------------------------------------
    // DcWsSigUser — ISigUser that owns the WebSocket connection
    // ---------------------------------------------------------------------------

    class DcWsSigUser: public ISigUser
    {
    public:
        DcWsSigUser(PeerId id, std::shared_ptr<rtc::WebSocket> ws)
            : _id(std::move(id))
            , _ws(std::move(ws))
        {}

        ~DcWsSigUser() override
        {
            // Fire-and-forget if Leave() was never called.
            if (_ws && _ws->isOpen()) {
                _ws->close();
            }
        }

        [[nodiscard]] const PeerId& LocalId() const override { return _id; }

        void Send(const PeerId& to, std::string payload) override
        {
            if (!_ws || !_ws->isOpen()) {
                Log::Warn("[{}] cannot send to {} — socket closed", _id.value, to.value);
                return;
            }
            Log::Trace("[{}] -> [{}] send {} bytes", _id.value, to.value, payload.size());
            const json envelope = {{"id", to.value}, {"payload", std::move(payload)}};
            _ws->send(envelope.dump());
        }

        void Leave() override
        {
            if (_leaving.exchange(true)) {
                // Log::Trace("[{}] already leaving, ignoring", _id.value);
                return;
            }
            if (_ws && _ws->isOpen()) {
                _ws->close(); // onClosed will call NotifyClosed()
            } else {
                NotifyClosed(); // already closed, notify immediately
            }
        }

        void SetHandler(std::weak_ptr<ISigHandler> handler)
        {
            std::vector<SigMessage> pending;
            {
                std::lock_guard lock{_handlerMutex};
                _handler = std::move(handler);
                pending = std::move(_pending);
            }
            if (auto h = _handler.lock()) {
                for (auto& msg : pending) {
                    h->OnMessage(std::move(msg));
                }
            }
        }

        void Deliver(SigMessage msg)
        {
            std::lock_guard lock{_handlerMutex};
            if (auto h = _handler.lock()) {
                h->OnMessage(std::move(msg));
            } else {
                _pending.push_back(std::move(msg));
            }
        }

        /// Called from the WebSocket onClosed callback.
        void NotifyClosed()
        {
            if (auto h = _handler.lock()) {
                const auto ec = _leaving.load() ? std::error_code{}                          // clean (Leave() initiated)
                                                : make_error_code(SigError::ConnectionLost); // external drop
                h->OnLeft(ec);
            }
        }

    private:
        PeerId _id;
        std::shared_ptr<rtc::WebSocket> _ws;
        std::mutex _handlerMutex;
        std::weak_ptr<ISigHandler> _handler;
        std::vector<SigMessage> _pending;
        std::atomic<bool> _leaving{false};
    };

    // ---------------------------------------------------------------------------
    // DcWsSigClient
    // ---------------------------------------------------------------------------

    DcWsSigClient::DcWsSigClient()
        : DcWsSigClient(Options{})
    {}

    DcWsSigClient::DcWsSigClient(Options options)
        : _options(std::move(options))
    {}

    DcWsSigClient::~DcWsSigClient() = default;

    void DcWsSigClient::Join(PeerId id, SigJoinHandler onJoined)
    {
        const std::string url = "ws://" + _options.host + ":" + std::to_string(_options.port) + "/" + id.value;

        Log::Debug("[{}] connecting to {}", id.value, url);

        auto ws = std::make_shared<rtc::WebSocket>();

        // Shared slot to pass the created user from onOpen to onClosed callback.
        auto sharedUser = std::make_shared<std::weak_ptr<DcWsSigUser>>();

        // Guard ensuring only one of onOpen/onError fires the factory.
        auto fired = std::make_shared<bool>(false);
        auto onJoinedCap = std::make_shared<SigJoinHandler>(std::move(onJoined));

        ws->onOpen([ws, id, fired, onJoinedCap, sharedUser]() {
            if (*fired) {
                Log::Trace("[{}] onOpen: already fired, ignoring", id.value);
                return;
            }
            *fired = true;
            Log::Info("[{}] signaling connected", id.value);
            auto user = std::make_shared<DcWsSigUser>(id, ws);
            // Register the user in sharedUser BEFORE calling onJoinedCap.
            // onJoinedCap may synchronously start WebRTC negotiation, triggering
            // onLocalDescription on a libdatachannel background thread. On fast
            // loopback the remote's answer can arrive via onMessage before
            // onJoinedCap returns. Without this, sharedUser->lock() would return
            // nullptr and the answer would be silently dropped.
            *sharedUser = user;
            auto whandler = (*onJoinedCap)(SigJoinResult{user});
            // SetHandler replays any messages buffered during onJoinedCap execution.
            user->SetHandler(std::move(whandler));
        });

        ws->onError([id, fired, onJoinedCap](std::string err) {
            if (*fired) {
                Log::Trace("[{}] onError: already fired, ignoring ({})", id.value, err);
                return;
            }
            *fired = true;
            Log::Error("[{}] signaling error: {}", id.value, err);
            (*onJoinedCap)(std::unexpected(Error::ConnectionRefused));
        });

        ws->onClosed([id, sharedUser]() {
            Log::Info("[{}] signaling disconnected", id.value);
            if (auto user = sharedUser->lock()) {
                user->NotifyClosed();
            }
        });

        ws->onMessage([id, sharedUser](auto raw) {
            if (!std::holds_alternative<std::string>(raw)) {
                Log::Trace("[{}] onMessage: ignoring non-text frame", id.value);
                return;
            }
            auto user = sharedUser->lock();
            if (!user) {
                Log::Warn("[{}] onMessage: user not ready, dropping message", id.value);
                return;
            }

            const auto& text = std::get<std::string>(raw);
            const auto msg = json::parse(text, nullptr, /*exceptions=*/false);
            if (msg.is_discarded()) {
                Log::Warn("[{}] invalid JSON from server, ignoring", id.value);
                return;
            }

            const auto fromIt = msg.find("id");
            const auto plIt = msg.find("payload");
            if (fromIt == msg.end() || plIt == msg.end()) {
                Log::Warn("[{}] missing id or payload field, ignoring", id.value);
                return;
            }

            // Deliver via the user's handler (routes through DcRtcClient/Server State).

            user->Deliver(
                SigMessage{
                    .from = PeerId{fromIt->template get<std::string>()},
                    .payload = plIt->template get<std::string>(),
                }
            );
        });

        ws->open(url);
    }
}
#endif
