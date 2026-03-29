#include "Rtt/Rtc/SigHub.h"

#include "Log/Log.h"

namespace Rtt::Rtc
{

// ---------------------------------------------------------------------------
// HubUser — internal ISigUser implementation
// ---------------------------------------------------------------------------

class SigHub::HubUser : public ISigUser
{
public:
    HubUser(PeerId localId, SigMessageHandler onMessage, const std::shared_ptr<SigHub>& hub)
        : _localId(std::move(localId))
        , _onMessage(std::move(onMessage))
        , _hub(hub)
    {}

    // When the last shared_ptr to this user is dropped the weak_ptr entry in
    // the hub simply stops resolving — no explicit cleanup needed.
    ~HubUser() override = default;

    const PeerId& LocalId() const override { return _localId; }

    void Send(const PeerId& to, std::string payload) override
    {
        // Delegate routing to the hub so the lock is held in one place.
        if (auto hub = _hub.lock()) {
            hub->Dispatch(_localId, to, std::move(payload));
        }
    }

    /// Called by SigHub::Dispatch — deliver a message to this user.
    void Deliver(SigMessage message) const
    {
        _onMessage(std::move(message));
    }

private:
    PeerId _localId;
    SigMessageHandler _onMessage;
    std::weak_ptr<SigHub> _hub; // weak to avoid ISigUser → SigHub → ISigUser cycle
};

// ---------------------------------------------------------------------------
// SigHub
// ---------------------------------------------------------------------------

std::shared_ptr<ISigUser> SigHub::Register(PeerId localId, SigMessageHandler onMessage)
{
    // Create the user while holding a shared_ptr to self so the hub outlives
    // at least one user.  The user captures a weak_ptr to avoid a cycle.
    auto self = shared_from_this();
    auto user = std::make_shared<HubUser>(localId, std::move(onMessage), self);

    {
        std::lock_guard lock{_mutex};
        _peers[localId.value] = user;
    }

    Log::Debug("registered peer {}", localId.value);
    return user;
}

void SigHub::Dispatch(const PeerId& from, const PeerId& to, std::string payload)
{
    // Resolve the target under the lock, then invoke outside to avoid deadlock
    // in case the handler calls Send() (which calls Dispatch() again).
    std::shared_ptr<HubUser> target;
    {
        std::lock_guard lock{_mutex};
        if (auto it = _peers.find(to.value); it != _peers.end()) {
            target = it->second.lock();
        }
    }

    if (!target) {
        Log::Warn("dispatch: peer {} not found (message from {} dropped)", to.value, from.value);
        return;
    }

    target->Deliver(SigMessage{.from = from, .payload = std::move(payload)});
}

} // namespace Rtt::Rtc
