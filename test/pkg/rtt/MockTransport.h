#pragma once
#include "MockLink.h"
#include "Rtt/Transport.h"

#include <memory>
#include <utility>
#include <vector>

namespace Rtt::Testing
{
    /// Minimal mock transport for unit tests.
    ///
    /// Stores pending Open() requests and exposes Simulate*() helpers
    /// so tests can trigger callbacks deterministically.
    class MockTransport : public ITransport
    {
    public:
        struct PendingRequest
        {
            std::shared_ptr<ILinkAcceptor> acceptor;
        };

        std::shared_ptr<IConnector> Open(std::shared_ptr<ILinkAcceptor> acceptor) override
        {
            _pendingRequests.push_back({std::move(acceptor)});
            return nullptr;
        }

        // --- Simulation helpers ---

        struct SimulatedLink
        {
            std::shared_ptr<MockLink> link;
            LinkHandler handler;
        };

        /// Simulate a successful link for the i-th pending Open() request.
        SimulatedLink SimulateLink(std::size_t index,
                                   PeerId localId,
                                   PeerId remoteId)
        {
            auto link = std::make_shared<MockLink>(std::move(localId), std::move(remoteId));
            auto handler = _pendingRequests[index].acceptor->OnLink(link);
            return {
                .link = link,
                .handler = std::move(handler),
            };
        }

        /// Simulate a link error for the i-th pending Open() request.
        LinkHandler SimulateLinkError(std::size_t index, Error error)
        {
            return _pendingRequests[index].acceptor->OnLink(std::unexpected(error));
        }

        // --- Test accessors ---

        [[nodiscard]] const std::vector<PendingRequest>& PendingRequests() const { return _pendingRequests; }

    private:
        std::vector<PendingRequest> _pendingRequests;
    };

    static_assert(TransportLike<MockTransport>);
}
