#include "WrtcClient.h"

#ifdef __EMSCRIPTEN__
// #include "Js/JsRtcClient.h"  // NYI
#else
#include "Dc/DcRtcClient.h"
#endif

#include <memory>
#include <utility>

namespace Rtt::Rtc
{

std::shared_ptr<ITransport> WrtcClient::MakeDefault(Options options)
{
#ifdef __EMSCRIPTEN__
    // NYI: JsRtcClient — browser RTCPeerConnection via Emscripten JS bridge.
    return nullptr;
#else
    return std::make_shared<DcRtcClient>(DcRtcClient::Options{
        .sigClient = std::move(options.sigClient),
        .localId = std::move(options.localId),
        .remoteId = std::move(options.remoteId),
        .iceServers = std::move(options.iceServers),
        .maxMessageSize = options.maxMessageSize,
    });
#endif
}

} // namespace Rtt::Rtc
