#pragma once
#include "Rtt/PeerId.h"

#include <functional>
#include <string>

namespace Rtt::Rtc
{

// ---------------------------------------------------------------------------
// SigMessage
// ---------------------------------------------------------------------------

/// A signaling message delivered to a registered user.
/// The payload is opaque to the signaling layer — it may carry SDP, ICE
/// candidates, or any other application-defined text.
struct SigMessage
{
    PeerId from;          ///< Sender's peer ID (set by the routing layer).
    std::string payload;  ///< Opaque text payload; format is application-defined.
};

/// Called whenever a signaling message is delivered to a user.
using SigMessageHandler = std::function<void(SigMessage)>;

} // namespace Rtt::Rtc
