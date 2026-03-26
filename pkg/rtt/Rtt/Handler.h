#pragma once
#include <cstddef>
#include <functional>
#include <span>

namespace Rtt
{
    /// Callbacks returned by the user from OnConnected / OnAccepted to handle
    /// data reception and disconnection for a specific link.
    ///
    /// Returned atomically from the connection callback to avoid races between
    /// handler setup and the first incoming data.
    struct LinkHandler
    {
        /// Called when data arrives on the link.
        std::function<void(std::span<const std::byte>)> onReceived;

        /// Called when the link is disconnected (gracefully or otherwise).
        std::function<void()> onDisconnected;
    };
}
