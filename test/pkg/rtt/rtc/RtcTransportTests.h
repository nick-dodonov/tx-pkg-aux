#pragma once
// Shared test scenarios for WebRTC transport implementations.
//
// RtcTransportFixture is an abstract GTest fixture. Concrete subclasses
// provide a makeSigClient() implementation that returns either:
//   - LocalSigClient (in-process, no network)
//   - WsSigClient::MakeDefault() (integration, requires external WS server)
//
// The shared runTest_*() methods exercise the same scenarios regardless
// of the signaling backend, enabling identical coverage across both test modes.

#include "Rtt/Rtc/ISigClient.h"
#include "Rtt/Rtc/WrtcClient.h"
#include "Rtt/Rtc/WrtcServer.h"
#include "Rtt/Acceptor.h"
#include "Rtt/Link.h"
#include "Rtt/Transport.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using namespace Rtt;
using namespace Rtt::Rtc;
using namespace std::chrono_literals;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// Poll until flag is set or timeout is reached.
inline bool awaitFlag(const std::atomic<bool>& flag,
                      std::chrono::milliseconds timeout = 5s)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (flag.load()) {
            return true;
        }
        std::this_thread::sleep_for(10ms);
    }
    return false;
}

inline std::vector<std::byte> toBytes(std::string_view sv)
{
    std::vector<std::byte> v(sv.size());
    std::memcpy(v.data(), sv.data(), sv.size());
    return v;
}

inline std::string fromBytes(std::span<const std::byte> data)
{
    return {reinterpret_cast<const char*>(data.data()), data.size()}; //NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

// ---------------------------------------------------------------------------
// TestLinkAcceptor
//
// Captures the first link delivered by a transport and records the
// first received packet and disconnect notification.
// ---------------------------------------------------------------------------

struct TestLinkAcceptor : public ILinkAcceptor
{
    std::shared_ptr<ILink> link;
    std::vector<std::byte> lastPacket;
    std::atomic<bool> linkReady{false};
    std::atomic<bool> packetReceived{false};
    std::atomic<bool> disconnected{false};

    LinkHandler OnLink(LinkResult result) override
    {
        if (result.has_value()) {
            link = *result;
            linkReady = true;
        }
        return LinkHandler{
            .onReceived = [this](std::span<const std::byte> data) {
                lastPacket.assign(data.begin(), data.end());
                packetReceived = true;
            },
            .onDisconnected = [this]() {
                disconnected = true;
            },
        };
    }
};

// ---------------------------------------------------------------------------
// RtcTransportFixture
// ---------------------------------------------------------------------------

class RtcTransportFixture : public ::testing::Test
{
protected:
    void TearDown() override
    {
        // libdatachannel uses background threads for ICE and SCTP.
        // Allow them to finish teardown before the next test creates new
        // PeerConnections — avoids resource contention between tests.
        std::this_thread::sleep_for(500ms);
    }

    /// Return a signaling client for the given role.
    /// Called twice per test (once for client, once for server).
    /// Both calls must return clients that share the same signaling channel.
    virtual std::shared_ptr<ISigClient> makeSigClient() = 0;

    /// ICE servers; override to add STUN when testing through NAT.
    virtual std::vector<std::string> iceServers() { return {}; }

    std::shared_ptr<ITransport> makeClient(PeerId localId, PeerId remoteId)
    {
        return WrtcClient::MakeDefault(WrtcClient::Options{
            .sigClient = makeSigClient(),
            .localId = std::move(localId),
            .remoteId = std::move(remoteId),
            .iceServers = iceServers(),
        });
    }

    std::shared_ptr<ITransport> makeServer(PeerId localId)
    {
        return WrtcServer::MakeDefault(WrtcServer::Options{
            .sigClient = makeSigClient(),
            .localId = std::move(localId),
            .iceServers = iceServers(),
        });
    }

    // -----------------------------------------------------------------------
    // Shared test scenarios
    // -----------------------------------------------------------------------

    void runTest_Connect_LinksEstablished()
    {
        auto serverAcceptor = std::make_shared<TestLinkAcceptor>();
        auto clientAcceptor = std::make_shared<TestLinkAcceptor>();

        auto server = makeServer(PeerId{"server"});
        auto client = makeClient(PeerId{"client"}, PeerId{"server"});

        server->Open(serverAcceptor);
        client->Open(clientAcceptor);

        ASSERT_TRUE(awaitFlag(clientAcceptor->linkReady));
        ASSERT_TRUE(awaitFlag(serverAcceptor->linkReady));

        EXPECT_EQ(clientAcceptor->link->LocalId().value, "client");
        EXPECT_EQ(clientAcceptor->link->RemoteId().value, "server");
        EXPECT_EQ(serverAcceptor->link->LocalId().value, "server");
        EXPECT_EQ(serverAcceptor->link->RemoteId().value, "client");
    }

    void runTest_Send_ClientToServer()
    {
        auto serverAcceptor = std::make_shared<TestLinkAcceptor>();
        auto clientAcceptor = std::make_shared<TestLinkAcceptor>();

        auto server = makeServer(PeerId{"server"});
        auto client = makeClient(PeerId{"client"}, PeerId{"server"});

        server->Open(serverAcceptor);
        client->Open(clientAcceptor);

        ASSERT_TRUE(awaitFlag(clientAcceptor->linkReady));
        ASSERT_TRUE(awaitFlag(serverAcceptor->linkReady));

        const std::string msg = "hello from client";
        const auto expected = toBytes(msg);

        clientAcceptor->link->Send([&](std::span<std::byte> buf) {
            std::memcpy(buf.data(), expected.data(), expected.size());
            return expected.size();
        });

        ASSERT_TRUE(awaitFlag(serverAcceptor->packetReceived));
        EXPECT_EQ(fromBytes(serverAcceptor->lastPacket), msg);
    }

    void runTest_Send_ServerToClient()
    {
        auto serverAcceptor = std::make_shared<TestLinkAcceptor>();
        auto clientAcceptor = std::make_shared<TestLinkAcceptor>();

        auto server = makeServer(PeerId{"server"});
        auto client = makeClient(PeerId{"client"}, PeerId{"server"});

        server->Open(serverAcceptor);
        client->Open(clientAcceptor);

        ASSERT_TRUE(awaitFlag(clientAcceptor->linkReady));
        ASSERT_TRUE(awaitFlag(serverAcceptor->linkReady));

        const std::string msg = "hello from server";
        const auto expected = toBytes(msg);

        serverAcceptor->link->Send([&](std::span<std::byte> buf) {
            std::memcpy(buf.data(), expected.data(), expected.size());
            return expected.size();
        });

        ASSERT_TRUE(awaitFlag(clientAcceptor->packetReceived));
        EXPECT_EQ(fromBytes(clientAcceptor->lastPacket), msg);
    }

    void runTest_Send_Bidirectional()
    {
        auto serverAcceptor = std::make_shared<TestLinkAcceptor>();
        auto clientAcceptor = std::make_shared<TestLinkAcceptor>();

        auto server = makeServer(PeerId{"server"});
        auto client = makeClient(PeerId{"client"}, PeerId{"server"});

        server->Open(serverAcceptor);
        client->Open(clientAcceptor);

        ASSERT_TRUE(awaitFlag(clientAcceptor->linkReady));
        ASSERT_TRUE(awaitFlag(serverAcceptor->linkReady));

        const auto c2s = toBytes("client->server");
        const auto s2c = toBytes("server->client");

        clientAcceptor->link->Send([&](std::span<std::byte> buf) {
            std::memcpy(buf.data(), c2s.data(), c2s.size());
            return c2s.size();
        });
        serverAcceptor->link->Send([&](std::span<std::byte> buf) {
            std::memcpy(buf.data(), s2c.data(), s2c.size());
            return s2c.size();
        });

        ASSERT_TRUE(awaitFlag(serverAcceptor->packetReceived));
        ASSERT_TRUE(awaitFlag(clientAcceptor->packetReceived));
        EXPECT_EQ(fromBytes(serverAcceptor->lastPacket), "client->server");
        EXPECT_EQ(fromBytes(clientAcceptor->lastPacket), "server->client");
    }

    void runTest_Disconnect_ClientSide()
    {
        auto serverAcceptor = std::make_shared<TestLinkAcceptor>();
        auto clientAcceptor = std::make_shared<TestLinkAcceptor>();

        auto server = makeServer(PeerId{"server"});
        auto client = makeClient(PeerId{"client"}, PeerId{"server"});

        server->Open(serverAcceptor);
        client->Open(clientAcceptor);

        ASSERT_TRUE(awaitFlag(clientAcceptor->linkReady));
        ASSERT_TRUE(awaitFlag(serverAcceptor->linkReady));

        clientAcceptor->link->Disconnect();

        ASSERT_TRUE(awaitFlag(serverAcceptor->disconnected));
    }
};
