#include "RtcServer.h"

#ifdef __EMSCRIPTEN__
// #include "Js/JsRtcServer.h"  // NYI
#else
#include "Dc/DcRtcServer.h"
#endif

#include <memory>
#include <utility>

namespace Rtt::Rtc
{
    std::shared_ptr<ITransport> RtcServer::MakeDefault(Options options)
    {
#ifdef __EMSCRIPTEN__
        // NYI: JsRtcServer — browser RTCPeerConnection via Emscripten JS bridge.
        return nullptr;
#else
        return std::make_shared<DcRtcServer>(std::move(options));
#endif
    }
}
