#include "Rtt/Rtc/SigHub.h"

#include "Log/Log.h"

namespace Rtt::Rtc
{

// ---------------------------------------------------------------------------
// HubUser — ISigUser registered in the hub
// ---------------------------------------------------------------------------

class SigHub::HubUser : public ISigUser
{
public:
    HubUser(PeerId localId, const std::shared_ptr<SigHub>& hub)
        : _localId(std::move(localId))
        , _hub(hub)
    {}

    ~HubUser() override = default;

    const PeerId& LocalId() const override { return _localId; }

    void Send(const PeerId& to, std::string payload) override
    {
        if (auto hub = _hub.lock()) {
            hub->Dispatch(_localId, to, std::move(payload));
        }
    }

    void Leave() override
    {
        if (_left.exchange(true)) {
            return; // already left
        }
        if (auto hub = _hub.lock()) {
            hub->Unregister(_localId);
        }
        Log::Debug("peer {} left", _localId.value);
        std::weak_ptr<ISigHandler> handler;
        {
            std::lock_guard lock{_handlerMutex};
            handler = _handler;
        }
        if (auto h = handler.lock()) {
            h->OnLeft({});
        }
    }

    // Called after the factory returns. Replays any messages that arrived
    // before the handler was set (can happen with libdatachannel background threads).
    void SetHandler(std::weak_ptr<ISigHandler> handler)
    {
        // Hold the mutex during the entire set+replay sequence.
        // This prevents concurrent Deliver() calls from interleaving with the
        // replay and causing out-of-order OnMessage() invocations 
        //  (fixing the race condition when candidate comes before offer/answer).
        std::lock_guard lock{_handlerMutex};
        _handler = handler;
        auto pending = std::move(_pending);
        if (auto h = handler.lock()) {
            for (auto& msg : pending) {
                h->OnMessage(std::move(msg));
            }
        }
    }

    // May be called from background threads before SetHandler() returns.
    // Messages are buffered until the handler is available.
    void Deliver(SigMessage message)
    {
        std::lock_guard lock{_handlerMutex};
        if (auto h = _handler.lock()) {
            h->OnMessage(std::move(message));
        } else {
            _pending.push_back(std::move(message));
        }
    }

private:
    PeerId _localId;
    std::weak_ptr<SigHub> _hub;

    std::mutex _handlerMutex;
    std::weak_ptr<ISigHandler> _handler;
    std::vector<SigMessage> _pending;

    std::atomic<bool> _left{false};
};

// ---------------------------------------------------------------------------
// SigHub
// ---------------------------------------------------------------------------

void SigHub::Register(PeerId localId, SigJoinHandler onJoined)
{
    auto self = shared_from_this();
    auto user = std::make_shared<HubUser>(localId, self);

    // Register in _peers BEFORE calling the factory so that background threads
    // (e.g. libdatachannel ICE/SDP) can deliver messages into the pending buffer.
    {
        std::lock_guard lock{_mutex};
        _peers[localId.value] = user;
    }

    Log::Debug("registered peer {}", localId.value);

    // Call the factory — may trigger ICE negotiation on background threads.
    auto whandler = onJoined(SigJoinResult{user});

    if (!whandler.lock()) {
        // Caller returned no handler (e.g. join failed) — remove from registry.
        std::lock_guard lock{_mutex};
        _peers.erase(localId.value);
        return;
    }

    // Set the handler; replays any messages buffered during factory execution.
    user->SetHandler(std::move(whandler));
}

void SigHub::Unregister(const PeerId& localId)
{
    std::lock_guard lock{_mutex};
    _peers.erase(localId.value);
}

void SigHub::Dispatch(const PeerId& from, const PeerId& to, std::string payload)
{
    std::shared_ptr<HubUser> target;
    {
        std::lock_guard lock{_mutex};
        if (auto it = _peers.find(to.value); it != _peers.end()) {
            target = it->second.lock();
            if (!target) {
                _peers.erase(it); // lazy cleanup of stale entry
            }
        }
    }

    if (!target) {
        Log::Warn("dispatch: peer {} not found (message from {} dropped)", to.value, from.value);
        return;
    }

    target->Deliver(SigMessage{.from = from, .payload = std::move(payload)});
}

} // namespace Rtt::Rtc
