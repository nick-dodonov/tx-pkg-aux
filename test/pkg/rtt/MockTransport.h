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
    /// Stores pending Connect/Listen requests and exposes Simulate*()
    /// helpers so tests can trigger callbacks deterministically.
    class MockTransport : public ITransport
    {
    public:
        struct PendingRequest
        {
            std::shared_ptr<ILinkAcceptor> acceptor;
        };

        void Connect(std::shared_ptr<ILinkAcceptor> acceptor) override
        {
            _pendingConnects.push_back({std::move(acceptor)});
        }

        void Listen(std::shared_ptr<ILinkAcceptor> acceptor) override
        {
            _pendingListens.push_back({std::move(acceptor)});
        }

        // --- Simulation helpers ---

        struct SimulatedConnection
        {
            std::shared_ptr<MockLink> link;
            LinkHandler handler;
        };

        /// Simulate a successful outbound connection for the i-th pending
        /// Connect request.
        SimulatedConnection SimulateConnect(std::size_t index,
                                            PeerId localId,
                                            PeerId remoteId)
        {
            auto link = std::make_shared<MockLink>(std::move(localId), std::move(remoteId));
            auto handler = _pendingConnects[index].acceptor->OnLink(link);
            return {
                .link=link, 
                .handler=std::move(handler),
            };
        }

        /// Simulate a connection error for the i-th pending Connect request.
        LinkHandler SimulateConnectError(std::size_t index, Error error)
        {
            return _pendingConnects[index].acceptor->OnLink(std::unexpected(error));
        }

        /// Simulate an incoming connection for the i-th pending Listen request.
        SimulatedConnection SimulateAccept(std::size_t index,
                                           PeerId localId,
                                           PeerId remoteId)
        {
            auto link = std::make_shared<MockLink>(std::move(localId), std::move(remoteId));
            auto handler = _pendingListens[index].acceptor->OnLink(link);
            return {
                .link=link, 
                .handler=std::move(handler),
            };
        }

        /// Simulate a listen error for the i-th pending Listen request.
        LinkHandler SimulateAcceptError(std::size_t index, Error error)
        {
            return _pendingListens[index].acceptor->OnLink(std::unexpected(error));
        }

        // --- Test accessors ---

        [[nodiscard]] const std::vector<PendingRequest>& PendingConnects() const { return _pendingConnects; }
        [[nodiscard]] const std::vector<PendingRequest>& PendingListens() const { return _pendingListens; }

    private:
        std::vector<PendingRequest> _pendingConnects;
        std::vector<PendingRequest> _pendingListens;
    };

    static_assert(TransportLike<MockTransport>);
}
