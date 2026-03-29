#include "WrtcServer.h"

#ifdef __EMSCRIPTEN__
// #include "Js/JsRtcServer.h"  // NYI
#else
#include "Dc/DcRtcServer.h"
#endif

#include <memory>
#include <utility>

namespace Rtt::Rtc
{

std::shared_ptr<ITransport> WrtcServer::MakeDefault(Options options)
{
#ifdef __EMSCRIPTEN__
    // NYI: JsRtcServer — browser RTCPeerConnection via Emscripten JS bridge.
    return nullptr;
#else
    return std::make_shared<DcRtcServer>(DcRtcServer::Options{
        .sigClient = std::move(options.sigClient),
        .localId = std::move(options.localId),
        .iceServers = std::move(options.iceServers),
        .maxMessageSize = options.maxMessageSize,
    });
#endif
}

} // namespace Rtt::Rtc
