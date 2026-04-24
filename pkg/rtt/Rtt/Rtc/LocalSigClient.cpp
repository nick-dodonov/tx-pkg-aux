#include "Rtt/Rtc/LocalSigClient.h"

#include "Log/Log.h"

namespace Rtt::Rtc
{

LocalSigClient::LocalSigClient(std::shared_ptr<SigHub> hub)
    : _hub(std::move(hub))
{}

void LocalSigClient::Join(PeerId localId, SigJoinHandler onJoined)
{
    // SigHub::Register calls onJoined synchronously with the new user.
    Log::Debug("registering as {}", localId.value);
    _hub->Register(std::move(localId), std::move(onJoined));
}

} // namespace Rtt::Rtc
