#pragma once
#include "Rtt/Rtc/ISigClient.h"
#include "Rtt/Rtc/WsSigOptions.h"

#include <memory>

namespace Rtt::Rtc
{
    /// Factory for creating a platform-appropriate ISigClient backed by a WebSocket.
    ///
    /// On host/droid: uses DcWsSigClient (libdatachannel WebSocket).
    /// On wasm:       uses JsWsSigClient (native browser/Node.js WebSocket via JS).
    ///
    /// User code calls WsSigClient::MakeDefault() and holds the returned
    /// std::shared_ptr<ISigClient>.  No #ifdefs are needed in user code.
    class WsSigClient
    {
    public:
        using Options = WsSigOptions;

        /// Create the platform-default ISigClient implementation.
        static std::shared_ptr<ISigClient> MakeDefault(Options options);
    };
}
