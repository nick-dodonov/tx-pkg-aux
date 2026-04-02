#pragma once
#include "Rtt/Rtc/RtcOptions.h"
#include <rtc/configuration.hpp>

namespace Rtt::Rtc
{
    rtc::Configuration BuildConfiguration(const RtcOptions& options);
}
