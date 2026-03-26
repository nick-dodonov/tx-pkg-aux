#pragma once
#include <string>
#include <string_view>

namespace Rtt
{
    /// Universal peer identity for transport links.
    ///
    /// String-based to accommodate heterogeneous transport schemes
    /// (UUIDs, IP:port, overlay node IDs, etc.).
    struct PeerId
    {
        std::string value;

        auto operator<=>(const PeerId&) const = default;

        operator std::string_view() const noexcept { return value; }
    };
}
