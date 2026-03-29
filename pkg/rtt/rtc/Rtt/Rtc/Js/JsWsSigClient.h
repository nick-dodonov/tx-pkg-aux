#pragma once
#if __EMSCRIPTEN__
#include "Rtt/Rtc/ISigClient.h"
#include "Rtt/Rtc/WsSigOptions.h"

namespace Rtt::Rtc
{

/// ISigClient implementation using the native browser/Node.js WebSocket API
/// via Emscripten EM_JS.  Only available on the wasm platform.
///
/// Use WsSigClient::MakeDefault() for platform-independent code.
class JsWsSigClient : public ISigClient
{
public:
    using Options = WsSigOptions;

    JsWsSigClient();
    explicit JsWsSigClient(Options options);
    ~JsWsSigClient() override;

    // ISigClient
    void Join(PeerId id, SigMessageHandler onMessage, SigJoinHandler onJoined) override;

private:
    Options _options;
};

static_assert(SigClientLike<JsWsSigClient>);

} // namespace Rtt::Rtc
#endif
