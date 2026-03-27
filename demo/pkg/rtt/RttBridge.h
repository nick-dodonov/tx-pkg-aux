#pragma once
#include "Rtt/Acceptor.h"
#include "Rtt/Handler.h"
#include "Rtt/Link.h"

#include <exec/create.hpp>
#include <cstddef>
#include <functional>
#include <memory>
#include <queue>
#include <stdexec/execution.hpp>
#include <vector>

namespace Demo
{
    static constexpr const char* DefaultHost = "127.0.0.1";
    static constexpr std::uint16_t DefaultPort = 6000;

    /// Shared bridge between Asio callbacks and exec coroutines.
    ///
    /// Holds one-shot function slots that are filled by AwaitLink /
    /// AwaitMessage and fired from BridgeAcceptor / LinkHandler.
    struct RttBridge
    {
        std::function<void(std::shared_ptr<Rtt::ILink>)> onLink;
        std::function<void(std::vector<std::byte>)> onMessage;
        std::queue<std::vector<std::byte>> pending;

        /// Returns a LinkHandler that delivers received data to onMessage or
        /// accumulates it in the pending queue if no waiter is registered yet.
        [[nodiscard]] Rtt::LinkHandler MakeHandler()
        {
            return {
                .onReceived = [this](std::span<const std::byte> data) {
                    if (onMessage) {
                        onMessage({data.begin(), data.end()});
                    } else {
                        pending.emplace(data.begin(), data.end());
                    }
                },
            };
        }
    };

    /// ILinkAcceptor implementation that forwards link results to RttBridge.
    class BridgeAcceptor : public Rtt::ILinkAcceptor
    {
    public:
        explicit BridgeAcceptor(std::shared_ptr<RttBridge> bridge)
            : _bridge(std::move(bridge))
        {}

        Rtt::LinkHandler OnLink(Rtt::LinkResult result) override
        {
            if (!result) {
                return {};
            }
            if (_bridge->onLink) {
                _bridge->onLink(*result);
            }
            return _bridge->MakeHandler();
        }

    private:
        std::shared_ptr<RttBridge> _bridge;
    };

    /// Returns a sender that completes with the next established ILink.
    ///
    /// The coroutine suspends until BridgeAcceptor::OnLink fires inside
    /// AsioPoller::Update() → io_context::poll(). No polling loop needed.
    inline auto AwaitLink(std::shared_ptr<RttBridge> bridge)
    {
        return exec::create<stdexec::set_value_t(std::shared_ptr<Rtt::ILink>)>(
            [](auto& ctx) noexcept {
                auto& [bridge] = ctx.args;
                bridge->onLink = [&ctx, &bridge](std::shared_ptr<Rtt::ILink> link) {
                    bridge->onLink = nullptr;
                    stdexec::set_value(std::move(ctx.receiver), std::move(link));
                };
            },
            std::move(bridge));
    }

    /// Returns a sender that completes with the next received message bytes.
    ///
    /// The coroutine suspends until LinkHandler::onReceived fires inside
    /// AsioPoller::Update() → io_context::poll(). No polling loop needed.
    inline auto AwaitMessage(std::shared_ptr<RttBridge> bridge)
    {
        return exec::create<stdexec::set_value_t(std::vector<std::byte>)>(
            [](auto& ctx) noexcept {
                auto& [bridge] = ctx.args;
                // Deliver immediately if a message arrived before AwaitMessage was called.
                if (!bridge->pending.empty()) {
                    auto data = std::move(bridge->pending.front());
                    bridge->pending.pop();
                    stdexec::set_value(std::move(ctx.receiver), std::move(data));
                    return;
                }
                bridge->onMessage = [&ctx, &bridge](std::vector<std::byte> data) {
                    bridge->onMessage = nullptr;
                    stdexec::set_value(std::move(ctx.receiver), std::move(data));
                };
            },
            std::move(bridge));
    }

} // namespace Demo
