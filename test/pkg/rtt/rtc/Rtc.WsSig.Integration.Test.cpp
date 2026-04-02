// WebRTC transport integration tests using WsSigClient (external signaling server).
//
// Tests any platform-appropriate ITransport implementation via RtcClient/RtcServer
// factory with a real WebSocket signaling server.
//
// Requires a signaling server running at 127.0.0.1:8000.
// Start it with:
//   node demo/try/datachannel/ws/signaling-node/signaling-server.js
//
// Run with:
//   bazel test //test/pkg/rtt/rtc:rtc-ws-sig-integration

#include "Rtc.Client.Tests.h"

#include "Rtt/Rtc/WsSigClient.h"
#include "Rtt/Rtc/WsSigOptions.h"

#include <gtest/gtest.h>
#include <memory>

using namespace Rtt;
using namespace Rtt::Rtc;

#ifdef __ANDROID__
// Android emulator routes loopback traffic from 127.0.0.1 on the host to 10.0.2.2.
static constexpr auto kSigHost = "10.0.2.2";
#else
static constexpr auto kSigHost = "127.0.0.1";
#endif

static constexpr uint16_t kSigPort = 8000;

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

class RtcWsSigIntegrationTest : public RtcTransportFixture
{
protected:
    std::shared_ptr<ISigClient> makeSigClient() override
    {
        return WsSigClient::MakeDefault(WsSigOptions{
            .host = kSigHost,
            .port = kSigPort,
        });
    }
};

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_F(RtcWsSigIntegrationTest, Connect_LinksEstablished)
{
    runTest_Connect_LinksEstablished();
}

TEST_F(RtcWsSigIntegrationTest, Send_ClientToServer)
{
    runTest_Send_ClientToServer();
}

TEST_F(RtcWsSigIntegrationTest, Send_ServerToClient)
{
    runTest_Send_ServerToClient();
}

TEST_F(RtcWsSigIntegrationTest, Send_Bidirectional)
{
    runTest_Send_Bidirectional();
}

TEST_F(RtcWsSigIntegrationTest, Disconnect_ClientSide)
{
    runTest_Disconnect_ClientSide();
}
