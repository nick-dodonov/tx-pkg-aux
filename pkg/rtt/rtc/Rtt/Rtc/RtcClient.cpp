#include "RtcClient.h"

#ifdef __EMSCRIPTEN__
// #include "Js/JsRtcClient.h"  // NYI
#else
#include "Dc/DcRtcClient.h"
#endif

#include <memory>
#include <utility>

namespace Rtt::Rtc
{
    std::shared_ptr<ITransport> RtcClient::MakeDefault(Options options)
    {
#ifdef __EMSCRIPTEN__
        // NYI: JsRtcClient — browser RTCPeerConnection via Emscripten JS bridge.
        return nullptr;
#else
        return std::make_shared<DcRtcClient>(std::move(options));
#endif
    }
}
