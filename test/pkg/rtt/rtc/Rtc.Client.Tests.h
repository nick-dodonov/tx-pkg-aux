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

#include "Log/Log.h"
#include "Rtt/Rtc/ISigClient.h"
#include "Rtt/Rtc/RtcClient.h"
#include "Rtt/Rtc/RtcServer.h"
#include "Rtt/Acceptor.h"
#include "Rtt/Link.h"
#include "Rtt/Transport.h"

#include <gtest/gtest.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <atomic>
#include <chrono>
#include <cstring>
#include <memory>
#include <random>
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
#ifdef __EMSCRIPTEN__
        emscripten_sleep(10);
#else
        std::this_thread::sleep_for(10ms);
#endif
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
    std::shared_ptr<TestLinkAcceptor> serverAcceptor;
    std::shared_ptr<TestLinkAcceptor> clientAcceptor;
    std::shared_ptr<ITransport> server;
    std::shared_ptr<ITransport> client;
    std::shared_ptr<IConnector> _clientConnector;
    PeerId _clientId;
    PeerId _serverId;

    void SetUp() override
    {
        serverAcceptor = std::make_shared<TestLinkAcceptor>();
        clientAcceptor = std::make_shared<TestLinkAcceptor>();
    }

    void TearDown() override
    {
        // Disconnect any live links so libdatachannel fires onClosed on both
        // sides. We wait for both disconnected flags before returning, which
        // guarantees that the SCTP/ICE background threads are done before the
        // next test creates new PeerConnections.
        if (clientAcceptor->link) {
            clientAcceptor->link->Disconnect();
        }
        if (serverAcceptor->link) {
            serverAcceptor->link->Disconnect();
        }
        if (clientAcceptor->linkReady) {
            EXPECT_TRUE(awaitFlag(clientAcceptor->disconnected));
        }
        if (serverAcceptor->linkReady) {
            EXPECT_TRUE(awaitFlag(serverAcceptor->disconnected));
        }
    }

    /// Return a signaling client for the given role.
    /// Called twice per test (once for client, once for server).
    /// Both calls must return clients that share the same signaling channel.
    virtual std::shared_ptr<ISigClient> makeSigClient() = 0;

    /// ICE servers; override to add STUN when testing through NAT.
    virtual std::vector<std::string> iceServers() { return {}; }

    void openClientServer()
    {
        // Unique per-process token (from random_device) combined with a
        // per-process counter ensures peer IDs are globally unique across
        // concurrent test processes (e.g. --runs_per_test > 1).
        static const auto sProcToken = []() {
            std::mt19937 rng{std::random_device{}()};
            return std::uniform_int_distribution<unsigned>(0, 0xFFFFu)(rng);
        }();
        static std::atomic<int> sCounter{0};
        const int n = ++sCounter;

        _clientId = PeerId{"c-" + std::to_string(sProcToken) + "-" + std::to_string(n)};
        _serverId = PeerId{"s-" + std::to_string(sProcToken) + "-" + std::to_string(n)};

        server = RtcServer::MakeDefault(RtcServer::Options{
            .sigClient = makeSigClient(),
            .localId = _serverId,
            .iceServers = iceServers(),
        });
        client = RtcClient::MakeDefault(RtcClient::Options{
            .sigClient = makeSigClient(),
            .localId = _clientId,
            .iceServers = iceServers(),
        });
        [[maybe_unused]] auto _ = server->Open(serverAcceptor);
        _clientConnector = client->Open(clientAcceptor);
        _clientConnector->Connect(_serverId);
    }

    // -----------------------------------------------------------------------
    // Shared test scenarios
    // -----------------------------------------------------------------------

    void runTest_Connect_LinksEstablished()
    {
        openClientServer();

        ASSERT_TRUE(awaitFlag(clientAcceptor->linkReady));
        ASSERT_TRUE(awaitFlag(serverAcceptor->linkReady));

        EXPECT_EQ(clientAcceptor->link->LocalId().value, _clientId.value);
        EXPECT_EQ(clientAcceptor->link->RemoteId().value, _serverId.value);
        EXPECT_EQ(serverAcceptor->link->LocalId().value, _serverId.value);
        EXPECT_EQ(serverAcceptor->link->RemoteId().value, _clientId.value);
    }

    void runTest_Send_ClientToServer()
    {
        openClientServer();

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
        openClientServer();

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
        openClientServer();

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
        openClientServer();

        ASSERT_TRUE(awaitFlag(clientAcceptor->linkReady));
        ASSERT_TRUE(awaitFlag(serverAcceptor->linkReady));

        clientAcceptor->link->Disconnect();

        // Both sides observe onDisconnected — from _dc->onClosed() callback.
        ASSERT_TRUE(awaitFlag(clientAcceptor->disconnected));
        ASSERT_TRUE(awaitFlag(serverAcceptor->disconnected));
    }
};
