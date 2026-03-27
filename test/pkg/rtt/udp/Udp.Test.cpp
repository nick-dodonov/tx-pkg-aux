#include "TestAcceptor.h"
#include "Udp/UdpClient.h"
#include "Udp/UdpServer.h"
#include <boost/asio.hpp>
#include <cstddef>
#include <cstring>
#include <gtest/gtest.h>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace asio = boost::asio;

using namespace Rtt;
using namespace Rtt::Testing;
using namespace Rtt::Udp;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace
{
    std::vector<std::byte> ToBytes(std::string_view sv)
    {
        std::vector<std::byte> buf(sv.size());
        std::memcpy(buf.data(), sv.data(), sv.size());
        return buf;
    }

    std::string FromBytes(std::span<const std::byte> data)
    {
        return {
            reinterpret_cast<const char*>(data.data()), data.size() //NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        };
    }

    /// Run io_context until a predicate is satisfied, with a safety timeout.
    void RunUntil(asio::io_context& io, auto predicate)
    {
        asio::steady_timer timeout{io, std::chrono::seconds(5)};
        timeout.async_wait([&](boost::system::error_code ec) {
            if (!ec) {
                io.stop();
            }
        });

        while (!predicate() && !io.stopped()) {
            io.run_one();
        }

        timeout.cancel();
        io.restart();
    }
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST(UdpTransport, ConnectAndSendReceive)
{
    asio::io_context io;

    // Server: listen on OS-assigned port
    auto serverAcceptor = std::make_shared<TestAcceptor>();
    UdpServer server{{.executor = io.get_executor(), .localPort = 0}};
    server.Open(serverAcceptor);
    auto serverPort = server.LocalPort();
    ASSERT_NE(serverPort, 0);

    // Client: connect to the server
    auto clientAcceptor = std::make_shared<TestAcceptor>();
    UdpClient client{{
        .executor = io.get_executor(),
        .remoteHost = "127.0.0.1",
        .remotePort = serverPort,
    }};
    client.Open(clientAcceptor);

    // Drive until client link is established
    RunUntil(io, [&] { return clientAcceptor->links.size() == 1; });
    ASSERT_EQ(clientAcceptor->links.size(), 1);

    auto& clientLink = clientAcceptor->links[0];

    // Client sends data to server
    auto payload = ToBytes("hello-server");
    clientLink->Send([&](std::span<std::byte> buf) -> std::size_t {
        auto n = std::min(buf.size(), payload.size());
        std::memcpy(buf.data(), payload.data(), n);
        return n;
    });

    // Drive until server receives the packet and creates a link
    RunUntil(io, [&] { return serverAcceptor->receivedPackets.size() == 1; });
    ASSERT_EQ(serverAcceptor->links.size(), 1);
    ASSERT_EQ(serverAcceptor->receivedPackets.size(), 1);
    EXPECT_EQ(FromBytes(serverAcceptor->receivedPackets[0]), "hello-server");
}

TEST(UdpTransport, BidirectionalCommunication)
{
    asio::io_context io;

    auto serverAcceptor = std::make_shared<TestAcceptor>();
    UdpServer server{{.executor = io.get_executor(), .localPort = 0}};
    server.Open(serverAcceptor);

    auto clientAcceptor = std::make_shared<TestAcceptor>();
    UdpClient client{{
        .executor = io.get_executor(),
        .remoteHost = "127.0.0.1",
        .remotePort = server.LocalPort(),
    }};
    client.Open(clientAcceptor);

    // Client -> Server
    RunUntil(io, [&] { return clientAcceptor->links.size() == 1; });
    auto& clientLink = clientAcceptor->links[0];

    auto request = ToBytes("ping");
    clientLink->Send([&](std::span<std::byte> buf) -> std::size_t {
        auto n = std::min(buf.size(), request.size());
        std::memcpy(buf.data(), request.data(), n);
        return n;
    });

    RunUntil(io, [&] { return serverAcceptor->links.size() == 1; });

    // Server -> Client
    auto& serverLink = serverAcceptor->links[0];
    auto response = ToBytes("pong");
    serverLink->Send([&](std::span<std::byte> buf) -> std::size_t {
        auto n = std::min(buf.size(), response.size());
        std::memcpy(buf.data(), response.data(), n);
        return n;
    });

    RunUntil(io, [&] { return clientAcceptor->receivedPackets.size() == 1; });
    EXPECT_EQ(FromBytes(clientAcceptor->receivedPackets[0]), "pong");
    EXPECT_EQ(FromBytes(serverAcceptor->receivedPackets[0]), "ping");
}

TEST(UdpTransport, ListenMultipleClients)
{
    asio::io_context io;

    auto serverAcceptor = std::make_shared<TestAcceptor>();
    UdpServer server{{.executor = io.get_executor(), .localPort = 0}};
    server.Open(serverAcceptor);

    // Two clients connect to the same server
    auto acceptorA = std::make_shared<TestAcceptor>();
    UdpClient clientA{{
        .executor = io.get_executor(),
        .remoteHost = "127.0.0.1",
        .remotePort = server.LocalPort(),
    }};
    clientA.Open(acceptorA);

    auto acceptorB = std::make_shared<TestAcceptor>();
    UdpClient clientB{{
        .executor = io.get_executor(),
        .remoteHost = "127.0.0.1",
        .remotePort = server.LocalPort(),
    }};
    clientB.Open(acceptorB);

    RunUntil(io, [&] {
        return acceptorA->links.size() == 1 && acceptorB->links.size() == 1;
    });

    // Both clients send
    auto payloadA = ToBytes("from-A");
    acceptorA->links[0]->Send([&](std::span<std::byte> buf) -> std::size_t {
        auto n = std::min(buf.size(), payloadA.size());
        std::memcpy(buf.data(), payloadA.data(), n);
        return n;
    });

    auto payloadB = ToBytes("from-B");
    acceptorB->links[0]->Send([&](std::span<std::byte> buf) -> std::size_t {
        auto n = std::min(buf.size(), payloadB.size());
        std::memcpy(buf.data(), payloadB.data(), n);
        return n;
    });

    // Server should get two links and two packets
    RunUntil(io, [&] { return serverAcceptor->receivedPackets.size() == 2; });
    EXPECT_EQ(serverAcceptor->links.size(), 2);

    // Verify both payloads arrived (order may vary)
    std::vector<std::string> received;
    for (auto& pkt : serverAcceptor->receivedPackets)
    {
        received.push_back(FromBytes(pkt));
    }
    EXPECT_TRUE(std::find(received.begin(), received.end(), "from-A") != received.end());
    EXPECT_TRUE(std::find(received.begin(), received.end(), "from-B") != received.end());
}

TEST(UdpTransport, PeerIdsArePopulated)
{
    asio::io_context io;

    auto serverAcceptor = std::make_shared<TestAcceptor>();
    UdpServer server{{.executor = io.get_executor(), .localPort = 0}};
    server.Open(serverAcceptor);

    auto clientAcceptor = std::make_shared<TestAcceptor>();
    UdpClient client{{
        .executor = io.get_executor(),
        .remoteHost = "127.0.0.1",
        .remotePort = server.LocalPort(),
    }};
    client.Open(clientAcceptor);

    RunUntil(io, [&] { return clientAcceptor->links.size() == 1; });
    auto& clientLink = clientAcceptor->links[0];

    // Client link should have non-empty local and remote IDs
    EXPECT_FALSE(clientLink->LocalId().value.empty());
    EXPECT_FALSE(clientLink->RemoteId().value.empty());

    // Send a packet so the server creates a link
    auto payload = ToBytes("id-check");
    clientLink->Send([&](std::span<std::byte> buf) -> std::size_t {
        auto n = std::min(buf.size(), payload.size());
        std::memcpy(buf.data(), payload.data(), n);
        return n;
    });

    RunUntil(io, [&] { return serverAcceptor->links.size() == 1; });
    auto& serverLink = serverAcceptor->links[0];

    EXPECT_FALSE(serverLink->LocalId().value.empty());
    EXPECT_FALSE(serverLink->RemoteId().value.empty());

    // The server's remote should match the client's local and vice versa
    EXPECT_EQ(clientLink->RemoteId().value,
              std::string("127.0.0.1:") + std::to_string(server.LocalPort()));
}

TEST(UdpTransport, ClientDisconnect)
{
    asio::io_context io;

    auto serverAcceptor = std::make_shared<TestAcceptor>();
    UdpServer server{{.executor = io.get_executor(), .localPort = 0}};
    server.Open(serverAcceptor);

    auto clientAcceptor = std::make_shared<TestAcceptor>();
    UdpClient client{{
        .executor = io.get_executor(),
        .remoteHost = "127.0.0.1",
        .remotePort = server.LocalPort(),
    }};
    client.Open(clientAcceptor);

    RunUntil(io, [&] { return clientAcceptor->links.size() == 1; });
    auto& clientLink = clientAcceptor->links[0];

    // Disconnect the client
    clientLink->Disconnect();

    // The client's disconnect callback should fire
    EXPECT_TRUE(clientAcceptor->disconnected);
}
