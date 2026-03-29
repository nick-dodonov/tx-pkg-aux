#include "Rtt/Rtc/LocalSigClient.h"

#include "Log/Log.h"

namespace Rtt::Rtc
{

LocalSigClient::LocalSigClient(std::shared_ptr<SigHub> hub)
    : _hub(std::move(hub))
{}

void LocalSigClient::Join(PeerId localId,
                           SigMessageHandler onMessage,
                           SigJoinHandler onJoined)
{
    Log::Debug("joining as {}", localId.value);

    // Register synchronously — SigHub::Register() is thread-safe.
    auto user = _hub->Register(localId, std::move(onMessage));

    // Notify the caller immediately; no network round-trip required.
    onJoined(SigJoinResult{std::move(user)});
}

} // namespace Rtt::Rtc
