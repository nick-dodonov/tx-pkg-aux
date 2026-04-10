#pragma once
#include "Rtt/Acceptor.h"
#include "Rtt/Handler.h"
#include "Rtt/Link.h"

#include <exec/create.hpp>
#include <stdexec/execution.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

namespace Demo
{
    /// Per-link bridge between transport callbacks and exec coroutines.
    /// Holds one-shot callback slots that are filled by AwaitMessage
    /// and fired from the LinkHandler returned by MakeHandler().
    struct LinkBridge
    {
        using Msg = std::optional<std::vector<std::byte>>;

        std::function<void(Msg)> onMessage;
        std::queue<Msg> pending;

        /// Create a LinkHandler that routes data to this bridge.
        [[nodiscard]] Rtt::LinkHandler MakeHandler()
        {
            return {
                .onReceived = [this](std::span<const std::byte> data) {
                    Deliver(std::vector<std::byte>{data.begin(), data.end()});
                },
                .onDisconnected = [this]() {
                    Deliver(std::nullopt);
                },
            };
        }

    private:
        void Deliver(Msg payload)
        {
            if (onMessage) {
                onMessage(std::move(payload));
            } else {
                pending.push(std::move(payload));
            }
        }
    };

    /// Returns a sender that completes with the next received message (or nullopt on disconnect).
    inline auto AwaitMessage(std::shared_ptr<LinkBridge> bridge)
    {
        return exec::create<stdexec::set_value_t(LinkBridge::Msg)>(
            [](auto& ctx) noexcept {
                auto& [bridge] = ctx.args;
                if (!bridge->pending.empty()) {
                    auto payload = std::move(bridge->pending.front());
                    bridge->pending.pop();
                    stdexec::set_value(std::move(ctx.receiver), std::move(payload));
                    return;
                }
                bridge->onMessage = [&ctx, &bridge](LinkBridge::Msg payload) {
                    bridge->onMessage = nullptr;
                    stdexec::set_value(std::move(ctx.receiver), std::move(payload));
                };
            },
            std::move(bridge));
    }

    /// Multi-link bridge: manages per-peer LinkBridges and a link arrival queue.
    /// Transport callbacks (from libdatachannel threads) are routed here;
    /// exec coroutines consume them via AwaitNewLink / AwaitMessage.
    struct MultiLinkBridge
    {
        using LinkCallback = std::function<void(std::shared_ptr<Rtt::ILink>, std::shared_ptr<LinkBridge>)>;

        /// One-shot callback fired when a new link arrives.
        LinkCallback onNewLink;

        /// Queue of arrived links (if no waiter was registered).
        struct PendingLink
        {
            std::shared_ptr<Rtt::ILink> link;
            std::shared_ptr<LinkBridge> bridge;
        };
        std::queue<PendingLink> pendingLinks;

        /// Get or create a LinkBridge for a peer.
        std::shared_ptr<LinkBridge> GetOrCreateBridge(const std::string& peerId)
        {
            auto it = bridges.find(peerId);
            if (it != bridges.end()) {
                return it->second;
            }
            auto bridge = std::make_shared<LinkBridge>();
            bridges[peerId] = bridge;
            return bridge;
        }

        /// Remove a bridge for a peer.
        void RemoveBridge(const std::string& peerId)
        {
            bridges.erase(peerId);
        }

        /// Called by the acceptor when a new link arrives.
        void DeliverLink(std::shared_ptr<Rtt::ILink> link, std::shared_ptr<LinkBridge> bridge)
        {
            if (onNewLink) {
                onNewLink(std::move(link), std::move(bridge));
            } else {
                pendingLinks.push({std::move(link), std::move(bridge)});
            }
        }

        std::unordered_map<std::string, std::shared_ptr<LinkBridge>> bridges;
    };

    /// Returns a sender that completes when a new link arrives.
    inline auto AwaitNewLink(std::shared_ptr<MultiLinkBridge> mlb)
    {
        using Result = MultiLinkBridge::PendingLink;
        return exec::create<stdexec::set_value_t(Result)>(
            [](auto& ctx) noexcept {
                auto& [mlb] = ctx.args;
                if (!mlb->pendingLinks.empty()) {
                    auto pending = std::move(mlb->pendingLinks.front());
                    mlb->pendingLinks.pop();
                    stdexec::set_value(std::move(ctx.receiver), std::move(pending));
                    return;
                }
                mlb->onNewLink = [&ctx, &mlb](std::shared_ptr<Rtt::ILink> link, std::shared_ptr<LinkBridge> bridge) {
                    mlb->onNewLink = nullptr;
                    stdexec::set_value(std::move(ctx.receiver), Result{std::move(link), std::move(bridge)});
                };
            },
            std::move(mlb));
    }

    /// ILinkAcceptor that routes links into a MultiLinkBridge.
    class MultiBridgeAcceptor : public Rtt::ILinkAcceptor
    {
    public:
        explicit MultiBridgeAcceptor(std::shared_ptr<MultiLinkBridge> mlb)
            : _mlb(std::move(mlb))
        {}

        Rtt::LinkHandler OnLink(Rtt::LinkResult result) override
        {
            if (!result) {
                return {};
            }
            auto link = *result;
            auto peerId = link->RemoteId().value;
            auto bridge = _mlb->GetOrCreateBridge(peerId);
            _mlb->DeliverLink(link, bridge);
            return bridge->MakeHandler();
        }

    private:
        std::shared_ptr<MultiLinkBridge> _mlb;
    };
}
