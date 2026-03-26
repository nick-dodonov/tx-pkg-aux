#pragma once
#include <string_view>

namespace Rtt
{
    /// Transport-level error codes.
    enum class Error
    {
        ConnectionRefused,
        Timeout,
        AddressInvalid,
        TransportClosed,
        Unknown,
    };

    constexpr std::string_view ErrorToString(Error e) noexcept
    {
        switch (e)
        {
            case Error::ConnectionRefused: return "ConnectionRefused";
            case Error::Timeout:           return "Timeout";
            case Error::AddressInvalid:    return "AddressInvalid";
            case Error::TransportClosed:   return "TransportClosed";
            case Error::Unknown:           return "Unknown";
        }
        return "Unknown";
    }
}
