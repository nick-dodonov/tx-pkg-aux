// WebRTC transport tests using LocalSigClient (in-process signaling).
//
// Uses LocalSigClient backed by a shared SigHub so the entire test — including
// WebRTC signaling — runs inside a single process without any network access.
// Tests any platform-appropriate ITransport implementation via RtcClient/RtcServer
// factory (DcRtcClient/DcRtcServer on host/droid).
//
// Note: even in-process WebRTC negotiation is asynchronous (libdatachannel
// uses internal threads for ICE gathering), so all tests poll for completion.

#include "Rtc.Client.Tests.h"

#include "Rtt/Rtc/SigHub.h"
#include "Rtt/Rtc/LocalSigClient.h"

#include <gtest/gtest.h>
#include <memory>

using namespace Rtt;
using namespace Rtt::Rtc;

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

class RtcLocalSigTest : public RtcTransportFixture
{
protected:
    // All peers in a test share a single hub so routing works without a server.
    std::shared_ptr<SigHub> _hub = std::make_shared<SigHub>();

    std::shared_ptr<ISigClient> makeSigClient() override
    {
        return std::make_shared<LocalSigClient>(_hub);
    }
};

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_F(RtcLocalSigTest, Connect_LinksEstablished)
{
    runTest_Connect_LinksEstablished();
}

TEST_F(RtcLocalSigTest, Send_ClientToServer)
{
    runTest_Send_ClientToServer();
}

TEST_F(RtcLocalSigTest, Send_ServerToClient)
{
    runTest_Send_ServerToClient();
}

TEST_F(RtcLocalSigTest, Send_Bidirectional)
{
    runTest_Send_Bidirectional();
}

TEST_F(RtcLocalSigTest, Disconnect_ClientSide)
{
    runTest_Disconnect_ClientSide();
}
