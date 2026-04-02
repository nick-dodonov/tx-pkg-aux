#if !defined(__EMSCRIPTEN__)
#include "DcRtcConfig.h"

namespace Rtt::Rtc
{
    rtc::Configuration BuildConfiguration(const RtcOptions& options)
    {
        rtc::Configuration config;
        for (const auto& uri : options.iceServers) {
            config.iceServers.emplace_back(uri);
        }
        return config;
    }
}
#endif
