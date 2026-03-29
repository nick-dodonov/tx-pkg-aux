#pragma once
#include "Rtt/Rtc/ISigClient.h"
#include <string>

namespace Rtt::Rtc
{

// ---------------------------------------------------------------------------
// WsSigClient — ISigClient backed by a live WebSocket connection
//
// Usage:
//   WsSigClient client;
//   client.Join("alice", onMsg, [](SigJoinResult r) {
//       auto user = *r;         // ISigUser — send via user->Send(...)
//       user->Send("bob", sdp); // routed through the server
//   });
//
// The returned ISigUser keeps the WebSocket alive.  Dropping the user
// closes the connection and unregisters from the server.
// ---------------------------------------------------------------------------

class WsSigClient : public ISigClient
{
public:
    struct Options
    {
        std::string host = "localhost";
        std::uint16_t port = 8000;
    };

    WsSigClient();
    explicit WsSigClient(Options options);
    ~WsSigClient() override;

    // ISigClient
    void Join(PeerId id, SigMessageHandler onMessage, SigJoinHandler onJoined) override;

private:
    Options _options;
};

static_assert(SigClientLike<WsSigClient>);

} // namespace Rtt::Rtc
