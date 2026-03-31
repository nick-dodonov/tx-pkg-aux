#include "RtcServer.h"

#ifdef __EMSCRIPTEN__
#include "Js/JsRtcTransport.h"
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
        return std::make_shared<JsRtcTransport>(std::move(options));
#else
        return std::make_shared<DcRtcServer>(std::move(options));
#endif
    }
}
