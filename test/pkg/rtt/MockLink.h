#pragma once
#include "Rtt/Link.h"
#include <cstddef>
#include <span>
#include <vector>

namespace Rtt::Testing
{
    /// Minimal mock link for unit tests.
    ///
    /// Captures Send writers and tracks Disconnect calls so tests can
    /// verify the full send/disconnect flow without a real transport.
    class MockLink : public ILink
    {
    public:
        MockLink(PeerId local, PeerId remote)
            : _local(std::move(local))
            , _remote(std::move(remote))
        {}

        const PeerId& LocalId() const override { return _local; }
        const PeerId& RemoteId() const override { return _remote; }

        void Send(WriteCallback writer) override
        {
            // Provide a scratch buffer for the writer, mimicking a transport
            // handing out a packet buffer.
            std::vector<std::byte> buf(_sendBufferSize);
            auto bytesWritten = writer(std::span<std::byte>{buf});
            buf.resize(bytesWritten);
            _sentPackets.push_back(std::move(buf));
        }

        void Disconnect() override { _disconnected = true; }

        // --- Test accessors ---

        [[nodiscard]] const std::vector<std::vector<std::byte>>& SentPackets() const { return _sentPackets; }
        [[nodiscard]] bool WasDisconnected() const { return _disconnected; }

        void SetSendBufferSize(std::size_t size) { _sendBufferSize = size; }

    private:
        PeerId _local;
        PeerId _remote;
        std::vector<std::vector<std::byte>> _sentPackets;
        bool _disconnected = false;
        std::size_t _sendBufferSize = 1024;
    };

    static_assert(LinkLike<MockLink>);
}
