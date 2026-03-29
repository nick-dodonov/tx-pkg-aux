#pragma once
#if !defined(__EMSCRIPTEN__)
#include "Rtt/Rtc/ISigClient.h"
#include "Rtt/Rtc/WsSigOptions.h"

namespace Rtt::Rtc
{

/// ISigClient implementation backed by a libdatachannel WebSocket connection.
///
/// Only available on host and droid platforms (not WASM).
/// Use WsSigClient::MakeDefault() for platform-independent code.
class DcWsSigClient : public ISigClient
{
public:
    using Options = WsSigOptions;

    DcWsSigClient();
    explicit DcWsSigClient(Options options);
    ~DcWsSigClient() override;

    // ISigClient
    void Join(PeerId id, SigMessageHandler onMessage, SigJoinHandler onJoined) override;

private:
    Options _options;
};

static_assert(SigClientLike<DcWsSigClient>);

} // namespace Rtt::Rtc
#endif
