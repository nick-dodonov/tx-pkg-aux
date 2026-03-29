#include "WsSigClient.h"

#ifdef __EMSCRIPTEN__
#include "Js/JsWsSigClient.h"
#else
#include "Dc/DcWsSigClient.h"
#endif

#include <memory>

namespace Rtt::Rtc
{
    std::shared_ptr<ISigClient> WsSigClient::MakeDefault(Options options)
    {
#ifdef __EMSCRIPTEN__
        return std::make_shared<JsWsSigClient>(std::move(options));
#else
        return std::make_shared<DcWsSigClient>(std::move(options));
#endif
    }
}
