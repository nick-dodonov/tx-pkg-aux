#pragma once
#include "Rtt/Acceptor.h"

#include <cstddef>
#include <memory>
#include <span>
#include <vector>

namespace Rtt::Testing
{
    /// Reusable test acceptor that records delivered links, errors, received
    /// packets, and disconnect notifications.
    class TestAcceptor : public ILinkAcceptor
    {
    public:
        LinkHandler OnLink(LinkResult result) override
        {
            if (result.has_value()) {
                links.push_back(*result);
            } else {
                lastError = result.error();
            }

            return LinkHandler{
                .onReceived = [this](std::span<const std::byte> data) {
                    receivedPackets.emplace_back(data.begin(), data.end());
                },
                .onDisconnected = [this]() {
                    disconnected = true;
                },
            };
        }

        std::vector<std::shared_ptr<ILink>> links;
        Error lastError = Error::Unknown;

        std::vector<std::vector<std::byte>> receivedPackets;
        bool disconnected = false;
    };
}
