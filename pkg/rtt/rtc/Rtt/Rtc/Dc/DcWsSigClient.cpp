#if !defined(__EMSCRIPTEN__)
#include "DcWsSigClient.h"

#include "Log/Log.h"

#include <nlohmann/json.hpp>
#include <rtc/rtc.hpp>

#include <memory>
#include <utility>

namespace Rtt::Rtc
{

using json = nlohmann::json;

// ---------------------------------------------------------------------------
// DcWsSigUser — ISigUser that owns the WebSocket connection
//
// Dropping the shared_ptr closes the socket and the server cleans up the
// peer registration automatically (it holds a weak_ptr from now on).
// ---------------------------------------------------------------------------

class DcWsSigUser : public ISigUser
{
public:
    DcWsSigUser(const DcWsSigUser&) = default;
    DcWsSigUser(DcWsSigUser&&) = delete;
    DcWsSigUser& operator=(const DcWsSigUser&) = default;
    DcWsSigUser& operator=(DcWsSigUser&&) = delete;

    DcWsSigUser(PeerId id, std::shared_ptr<rtc::WebSocket> ws)
        : _id(std::move(id))
        , _ws(std::move(ws))
    {}

    ~DcWsSigUser() override
    {
        if (_ws && _ws->isOpen()) {
            _ws->close();
        }
    }

    [[nodiscard]] const PeerId& LocalId() const override { return _id; }

    void Send(const PeerId& to, std::string payload) override
    {
        if (!_ws || !_ws->isOpen()) {
            Log::Warn("[{}] cannot send — socket closed", _id.value);
            return;
        }
        const json envelope = {{"id", to.value}, {"payload", std::move(payload)}};
        _ws->send(envelope.dump());
    }

private:
    PeerId _id;
    std::shared_ptr<rtc::WebSocket> _ws;
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

void DcWsSigClient::Join(PeerId id, SigMessageHandler onMessage, SigJoinHandler onJoined)
{
    const std::string url = "ws://" + _options.host + ":" +
                            std::to_string(_options.port) + "/" + id.value;

    Log::Debug("[{}] connecting to {}", id.value, url);

    auto ws = std::make_shared<rtc::WebSocket>();

    // Both onOpen and onError share the same handler via shared_ptr so that
    // only one of them fires it (guarded by the `fired` flag).
    auto fired     = std::make_shared<bool>(false);
    auto onMsgCap  = std::make_shared<SigMessageHandler>(std::move(onMessage));
    auto onJoinCap = std::make_shared<SigJoinHandler>(std::move(onJoined));

    ws->onOpen([ws, id, fired, onJoinCap]() {
        if (*fired) {
            return;
        }
        *fired = true;
        Log::Info("[{}] signaling connected", id.value);
        auto user = std::make_shared<DcWsSigUser>(id, ws);
        (*onJoinCap)(SigJoinResult{std::move(user)});
    });

    ws->onError([id, fired, onJoinCap](std::string err) {
        if (*fired) {
            return;
        }
        *fired = true;
        Log::Error("[{}] signaling error: {}", id.value, err);
        (*onJoinCap)(std::unexpected(Error::ConnectionRefused));
    });

    ws->onClosed([id]() {
        Log::Info("[{}] signaling disconnected", id.value);
    });

    ws->onMessage([id, onMsgCap](auto raw) {
        if (!std::holds_alternative<std::string>(raw)) {
            return;
        }

        const auto& text = std::get<std::string>(raw);
        const auto  msg  = json::parse(text, nullptr, /*exceptions=*/false);
        if (msg.is_discarded()) {
            Log::Warn("[{}] invalid JSON from server, ignoring", id.value);
            return;
        }

        const auto fromIt = msg.find("id");
        const auto plIt   = msg.find("payload");
        if (fromIt == msg.end() || plIt == msg.end()) {
            Log::Warn("[{}] missing id or payload field, ignoring", id.value);
            return;
        }

        (*onMsgCap)(SigMessage{
            .from = PeerId{fromIt->template get<std::string>()},
            .payload = plIt->template get<std::string>(),
        });
    });

    ws->open(url);
}

} // namespace Rtt::Rtc
#endif
