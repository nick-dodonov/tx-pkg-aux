#include "MockLink.h"
#include "MockTransport.h"
#include "TestAcceptor.h"
#include "Rtt/Error.h"
#include "Rtt/Handler.h"
#include "Rtt/Link.h"
#include "Rtt/PeerId.h"
#include "Rtt/Transport.h"
#include <cstddef>
#include <cstring>
#include <gtest/gtest.h>
#include <memory>
#include <span>
#include <string>
#include <vector>

using namespace Rtt;
using namespace Rtt::Testing;

// ---------------------------------------------------------------------------
// Concept compliance (compile-time)
// ---------------------------------------------------------------------------

static_assert(LinkLike<MockLink>);
static_assert(LinkLike<ILink>);
static_assert(TransportLike<MockTransport>);
static_assert(TransportLike<ITransport>);
static_assert(LinkAcceptorLike<ILinkAcceptor>);

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace
{
    std::vector<std::byte> ToBytes(std::string_view sv)
    {
        std::vector<std::byte> v(sv.size());
        std::memcpy(v.data(), sv.data(), sv.size());
        return v;
    }
}

// ---------------------------------------------------------------------------
// PeerId
// ---------------------------------------------------------------------------

TEST(PeerId, EqualityAndOrdering)
{
    PeerId a{.value = "alpha"};
    PeerId b{.value = "beta"};
    PeerId a2{.value = "alpha"};

    EXPECT_EQ(a, a2);
    EXPECT_NE(a, b);
    EXPECT_LT(a, b); // "alpha" < "beta"
}

TEST(PeerId, StringViewConversion)
{
    PeerId id{.value = "node-42"};
    std::string_view sv = id;
    EXPECT_EQ(sv, "node-42");
}

// ---------------------------------------------------------------------------
// Error
// ---------------------------------------------------------------------------

TEST(Error, ErrorToStringCoversAll)
{
    EXPECT_EQ(ErrorToString(Error::ConnectionRefused), "ConnectionRefused");
    EXPECT_EQ(ErrorToString(Error::Timeout), "Timeout");
    EXPECT_EQ(ErrorToString(Error::AddressInvalid), "AddressInvalid");
    EXPECT_EQ(ErrorToString(Error::TransportClosed), "TransportClosed");
    EXPECT_EQ(ErrorToString(Error::Unknown), "Unknown");
}

// ---------------------------------------------------------------------------
// Open — success flow
// ---------------------------------------------------------------------------

TEST(Transport, OpenSuccess)
{
    MockTransport transport;
    auto acceptor = std::make_shared<TestAcceptor>();

    transport.Open(acceptor);
    ASSERT_EQ(transport.PendingRequests().size(), 1u);

    auto [link, handler] = transport.SimulateLink(
        0, PeerId{.value = "local-1"}, PeerId{.value = "remote-1"});

    ASSERT_EQ(acceptor->links.size(), 1u);
    EXPECT_EQ(acceptor->links[0]->LocalId().value, "local-1");
    EXPECT_EQ(acceptor->links[0]->RemoteId().value, "remote-1");
    EXPECT_TRUE(handler.onReceived);
    EXPECT_TRUE(handler.onDisconnected);
}

// ---------------------------------------------------------------------------
// Open — error flow
// ---------------------------------------------------------------------------

TEST(Transport, OpenError)
{
    MockTransport transport;
    auto acceptor = std::make_shared<TestAcceptor>();

    transport.Open(acceptor);
    transport.SimulateLinkError(0, Error::AddressInvalid);

    EXPECT_TRUE(acceptor->links.empty());
    EXPECT_EQ(acceptor->lastError, Error::AddressInvalid);
}

// ---------------------------------------------------------------------------
// Open — multiple links (server-like)
// ---------------------------------------------------------------------------

TEST(Transport, OpenMultipleLinks)
{
    MockTransport transport;
    auto acceptor = std::make_shared<TestAcceptor>();

    transport.Open(acceptor);
    ASSERT_EQ(transport.PendingRequests().size(), 1u);

    auto [link, handler] = transport.SimulateLink(
        0, PeerId{.value = "server"}, PeerId{.value = "client-1"});

    ASSERT_EQ(acceptor->links.size(), 1u);
    EXPECT_EQ(acceptor->links[0]->LocalId().value, "server");
    EXPECT_EQ(acceptor->links[0]->RemoteId().value, "client-1");
    EXPECT_TRUE(handler.onReceived);
}

// ---------------------------------------------------------------------------
// Open — transport-level error
// ---------------------------------------------------------------------------

TEST(Transport, OpenTransportError)
{
    MockTransport transport;
    auto acceptor = std::make_shared<TestAcceptor>();

    transport.Open(acceptor);
    transport.SimulateLinkError(0, Error::TransportClosed);

    EXPECT_TRUE(acceptor->links.empty());
    EXPECT_EQ(acceptor->lastError, Error::TransportClosed);
}

// ---------------------------------------------------------------------------
// Send — writer-based zero-copy flow
// ---------------------------------------------------------------------------

TEST(Link, SendWriteFlow)
{
    auto link = std::make_shared<MockLink>(
        PeerId{.value = "A"}, PeerId{.value = "B"});

    const auto payload = ToBytes("hello");

    link->Send([&](std::span<std::byte> buf) -> std::size_t {
        EXPECT_GE(buf.size(), payload.size());
        std::memcpy(buf.data(), payload.data(), payload.size());
        return payload.size();
    });

    ASSERT_EQ(link->SentPackets().size(), 1u);
    EXPECT_EQ(link->SentPackets()[0], payload);
}

TEST(Link, SendMultiplePackets)
{
    auto link = std::make_shared<MockLink>(
        PeerId{.value = "A"}, PeerId{.value = "B"});

    for (int i = 0; i < 3; ++i)
    {
        link->Send([&](std::span<std::byte> buf) -> std::size_t {
            auto byte = static_cast<std::byte>(i);
            buf[0] = byte;
            return 1;
        });
    }

    EXPECT_EQ(link->SentPackets().size(), 3u);
    EXPECT_EQ(link->SentPackets()[0][0], static_cast<std::byte>(0));
    EXPECT_EQ(link->SentPackets()[1][0], static_cast<std::byte>(1));
    EXPECT_EQ(link->SentPackets()[2][0], static_cast<std::byte>(2));
}

// ---------------------------------------------------------------------------
// Receive — data delivery via LinkHandler
// ---------------------------------------------------------------------------

TEST(Link, ReceiveFlow)
{
    MockTransport transport;
    auto acceptor = std::make_shared<TestAcceptor>();

    transport.Open(acceptor);
    auto [link, handler] = transport.SimulateLink(
        0, PeerId{.value = "L"}, PeerId{.value = "R"});

    // Simulate incoming data via the handler
    auto data = ToBytes("world");
    handler.onReceived(std::span<const std::byte>{data});

    ASSERT_EQ(acceptor->receivedPackets.size(), 1u);
    EXPECT_EQ(acceptor->receivedPackets[0], data);
}

// ---------------------------------------------------------------------------
// Disconnect — notification via LinkHandler
// ---------------------------------------------------------------------------

TEST(Link, DisconnectNotification)
{
    MockTransport transport;
    auto acceptor = std::make_shared<TestAcceptor>();

    transport.Open(acceptor);
    auto [link, handler] = transport.SimulateLink(
        0, PeerId{.value = "L"}, PeerId{.value = "R"});

    EXPECT_FALSE(acceptor->disconnected);
    handler.onDisconnected();
    EXPECT_TRUE(acceptor->disconnected);
}

TEST(Link, DisconnectMethod)
{
    auto link = std::make_shared<MockLink>(
        PeerId{.value = "A"}, PeerId{.value = "B"});

    EXPECT_FALSE(link->WasDisconnected());
    link->Disconnect();
    EXPECT_TRUE(link->WasDisconnected());
}

// ---------------------------------------------------------------------------
// Peer identification
// ---------------------------------------------------------------------------

TEST(Link, PeerIdIdentification)
{
    auto link = std::make_shared<MockLink>(
        PeerId{.value = "local-node"}, PeerId{.value = "remote-node"});

    EXPECT_EQ(link->LocalId().value, "local-node");
    EXPECT_EQ(link->RemoteId().value, "remote-node");

    // Verify identity is stable via the ILink interface
    ILink& ilink = *link;
    EXPECT_EQ(ilink.LocalId().value, "local-node");
    EXPECT_EQ(ilink.RemoteId().value, "remote-node");
}
