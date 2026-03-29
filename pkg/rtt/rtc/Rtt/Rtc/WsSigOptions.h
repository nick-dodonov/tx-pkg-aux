#pragma once
#include <cstdint>
#include <string>

namespace Rtt::Rtc
{
    /// Common options shared by all WebSocket-based signaling client implementations.
    struct WsSigOptions
    {
        std::string host = "localhost";
        std::uint16_t port = 8000;
    };
}
