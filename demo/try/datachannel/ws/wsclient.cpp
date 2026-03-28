// WebRTC DataChannel Demo — Native C++ Client (libdatachannel)
//
// Overall flow:
//   1. On startup: generate a random local peer ID and connect to the signaling
//      server via WebSocket at ws://localhost:8000/<localId>.
//   2. OFFERER side: user types a remote peer ID at the prompt.
//      → Creates RTCPeerConnection, creates DataChannel (triggering negotiation),
//        sends SDP offer automatically via onLocalDescription callback.
//   3. ANSWERER side: incoming offer arrives via WebSocket onMessage.
//      → Creates RTCPeerConnection, calls setRemoteDescription — the library
//        auto-generates the answer and fires onLocalDescription to send it back.
//      → Receives the DataChannel through the onDataChannel callback.
//   4. ICE candidates trickle via onLocalCandidate: each candidate is forwarded
//      to the remote peer immediately as it is discovered, in parallel with SDP
//      exchange, so connectivity checks start sooner.
//   5. Once the DataChannel is open both sides exchange text messages P2P —
//      the signaling server is no longer involved.

#include "rtc/rtc.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <future>
#include <iostream>
#include <memory>
#include <random>
#include <stdexcept>
#include <unordered_map>

using namespace std::chrono_literals;
using std::shared_ptr;
using std::weak_ptr;
using nlohmann::json;

// Helper to capture objects in lambdas by weak_ptr instead of shared_ptr.
// Rationale: the rtc:: objects (WebSocket, DataChannel, PeerConnection) store
// their callbacks internally.  If a callback captured the same object by
// shared_ptr it would form a reference cycle and the object would never be
// freed.  A weak_ptr breaks the cycle: the callback does not prolong the
// object's lifetime, and lock() returns nullptr once the object is destroyed.
template <class T>
weak_ptr<T> make_weak_ptr(const shared_ptr<T>& ptr) { return ptr; }

// ---------------------------------------------------------------------------
// Command-line parameters with compile-time defaults.
// ---------------------------------------------------------------------------
class Cmdline
{
    bool _noStun{};
    bool _udpMux{};
public:
    Cmdline(const int argc, char** argv)
    {
        for (int i = 1; i < argc; ++i) {
            std::string arg(argv[i]);
            if (arg == "--no-stun") {
                _noStun = true;
            } else if (arg == "--udp-mux") {
                _udpMux = true;
            } else {
                throw std::runtime_error("Unknown argument: " + arg);
            }
        }
    }
    [[nodiscard]] bool noStun() const { return _noStun; }
    [[nodiscard]] bool udpMux() const { return _udpMux; }
    [[nodiscard]] std::string stunServer() const { return "stun.l.google.com"; }
    [[nodiscard]] int stunPort() const { return 19302; }
    [[nodiscard]] std::string webSocketServer() const { return "localhost"; }
    [[nodiscard]] int webSocketPort() const { return 8000; }
};

// ---------------------------------------------------------------------------
// Global state: active connections keyed by remote peer ID.
// Stored here (not on the stack) so that callbacks fired from library threads
// can safely access them after main() returns to its event loop.
// ---------------------------------------------------------------------------
static std::string localId;
static std::unordered_map<std::string, shared_ptr<rtc::PeerConnection>> peerConnectionMap;
static std::unordered_map<std::string, shared_ptr<rtc::DataChannel>>    dataChannelMap;

static shared_ptr<rtc::PeerConnection> createPeerConnection(
    const rtc::Configuration& config,
    const weak_ptr<rtc::WebSocket>& wws,
    std::string id);

static std::string randomId(size_t length);

int main(int argc, char** argv) try //NOLINT(readability-function-cognitive-complexity)
{
    Cmdline params(argc, argv);

    rtc::InitLogger(rtc::LogLevel::Info);

    // -------------------------------------------------------------------------
    // Build RTCPeerConnection configuration.
    // -------------------------------------------------------------------------
    rtc::Configuration config;
    if (params.noStun()) {
        std::cout << "No STUN server is configured. Only local hosts and public IP addresses supported." << '\n';
    } else {
        std::string stunServer;
        if (!params.stunServer().starts_with("stun:")) {
            stunServer = "stun:";
        }
        stunServer += params.stunServer() + ":" + std::to_string(params.stunPort());
        std::cout << "STUN server is " << stunServer << '\n';
        config.iceServers.emplace_back(stunServer);
    }

    if (params.udpMux()) {
        // ICE UDP mux multiplexes all peer connections on a single UDP port,
        // which simplifies firewall rules in server deployments.
        std::cout << "ICE UDP mux enabled" << '\n';
        config.enableIceUdpMux = true;
    }

    localId = randomId(4);
    std::cout << "The local ID is " << localId << '\n';

    // -------------------------------------------------------------------------
    // Signaling: connect to the WebSocket server.
    // ws->open() is non-blocking — the connection happens asynchronously.
    // We use a promise/future to block main() until onOpen or onError fires.
    // -------------------------------------------------------------------------
    auto ws = std::make_shared<rtc::WebSocket>();

    std::promise<void> wsPromise;
    auto wsFuture = wsPromise.get_future();

    ws->onOpen([&wsPromise]() {
        std::cout << "WebSocket connected, signaling ready" << '\n';
        wsPromise.set_value(); // unblocks wsFuture.get() below
    });

    ws->onError([&wsPromise](const std::string& s) {
        std::cout << "WebSocket error" << '\n';
        wsPromise.set_exception(std::make_exception_ptr(std::runtime_error(s))); // rethrown in wsFuture.get()
    });

    ws->onClosed([]() { std::cout << "WebSocket closed" << '\n'; });

    // Route incoming signaling messages to the appropriate PeerConnection.
    // Captures ws as weak_ptr: ws owns this callback, so capturing it by
    // shared_ptr would form a cycle preventing destruction.
    ws->onMessage([&config, wws = make_weak_ptr(ws)](auto data) {
        // The signaling protocol uses text frames only; ignore binary.
        if (!std::holds_alternative<std::string>(data)) {
            return;
        }

        const auto& str = std::get<std::string>(data);
        json message = json::parse(str.data(), str.data() + str.size(), nullptr, true, false);

        auto it = message.find("id");
        if (it == message.end()) {
            return;
        }
        auto id = it->get<std::string>(); // sender's peer ID (rewritten by server)

        it = message.find("type");
        if (it == message.end()) {
            return;
        }
        auto type = it->get<std::string>();

        // Look up an existing PeerConnection for this peer.
        // If none exists and this is an offer, create one (ANSWERER path).
        // Any other message for an unknown peer is stale — discard it.
        shared_ptr<rtc::PeerConnection> pc;
        if (auto jt = peerConnectionMap.find(id); jt != peerConnectionMap.end()) {
            pc = jt->second;
        } else if (type == "offer") {
            std::cout << "Answering to " + id << '\n';
            pc = createPeerConnection(config, wws, id);
        } else {
            std::cout << "Unexpected message type \"" << type << "\" from [" << id << "] with no existing connection\n";
            return;
        }

        if (type == "offer" || type == "answer") {
            // Apply the remote SDP.  For the ANSWERER: the library automatically
            // generates a local answer and fires onLocalDescription to send it back.
            // For the OFFERER: just stores the answer, no reply needed.
            auto sdp = message["description"].get<std::string>();
            pc->setRemoteDescription(rtc::Description(sdp, type));
        } else if (type == "candidate") {
            // Trickle ICE: add each remote candidate as it arrives so
            // connectivity checks can start before all candidates are gathered.
            auto sdp = message["candidate"].get<std::string>();
            auto mid = message["mid"].get<std::string>();
            pc->addRemoteCandidate(rtc::Candidate(sdp, mid));
        }
    });

    const std::string wsPrefix = !params.webSocketServer().contains("://") ? "ws://" : "";
    const auto url = wsPrefix + params.webSocketServer() + ":" +
                     std::to_string(params.webSocketPort()) + "/" + localId;

    std::cout << "WebSocket URL is " << url << '\n';
    ws->open(url);

    // Block until the WebSocket handshake completes (or throws on error).
    std::cout << "Waiting for signaling to be connected..." << '\n';
    wsFuture.get();

    // -------------------------------------------------------------------------
    // Main loop: OFFERER side — user enters the remote peer ID to call.
    // The ANSWERER path runs entirely inside ws->onMessage above.
    // -------------------------------------------------------------------------
    while (true) {
        std::cout << "Enter a remote ID to send an offer:" << '\n';
        std::string id;
        std::cin >> id;
        std::cin.ignore();

        if (id.empty()) {
            break;
        }

        if (id == localId) {
            std::cout << "Invalid remote ID (This is the local ID)" << '\n';
            continue;
        }

        std::cout << "Offering to " + id << '\n';
        auto pc = createPeerConnection(config, ws, id);

        // The offerer must create the DataChannel before sending the offer so
        // the channel description is embedded in the SDP.  Creating it here
        // also triggers ICE negotiation — the library calls setLocalDescription
        // internally and fires onLocalDescription to send the offer via WS.
        const std::string label = "test";
        std::cout << "Creating DataChannel with label \"" << label << "\"" << '\n';
        auto dc = pc->createDataChannel(label);

        dc->onOpen([id, wdc = make_weak_ptr(dc)]() {
            std::cout << "DataChannel from " << id << " open" << '\n';
            // Greet the answerer on the P2P channel (signaling is no longer used).
            if (auto dc = wdc.lock()) {
                dc->send("Hello from " + localId);
            }
        });

        dc->onClosed([id]() { std::cout << "DataChannel from " << id << " closed" << '\n'; });

        dc->onMessage([id](auto data) {
            if (std::holds_alternative<std::string>(data)) {
                std::cout << "Message from " << id << " received: " << std::get<std::string>(data) << '\n';
            } else {
                std::cout << "Binary message from " << id
                          << " received, size=" << std::get<rtc::binary>(data).size() << '\n';
            }
        });

        // Keep the DataChannel alive — without storing the shared_ptr here
        // it would be destroyed immediately when dc goes out of scope.
        dataChannelMap.emplace(id, dc);
    }

    std::cout << "Cleaning up..." << '\n';
    dataChannelMap.clear();
    peerConnectionMap.clear();
    return 0;

} catch (const std::exception& e) {
    std::cout << "Error: " << e.what() << '\n';
    dataChannelMap.clear();
    peerConnectionMap.clear();
    return -1;
}

// ---------------------------------------------------------------------------
// PeerConnection factory: wires up all ICE and DataChannel callbacks.
// Called both by the OFFERER (before createDataChannel) and by the ANSWERER
// (in response to receiving an offer via the signaling WebSocket).
// ---------------------------------------------------------------------------
shared_ptr<rtc::PeerConnection> createPeerConnection(
    const rtc::Configuration& config,
    const weak_ptr<rtc::WebSocket>& wws,
    std::string id)
{
    auto pc = std::make_shared<rtc::PeerConnection>(config);

    pc->onStateChange([](rtc::PeerConnection::State state) {
        std::cout << "State: " << state << '\n';
    });

    pc->onGatheringStateChange([](rtc::PeerConnection::GatheringState state) {
        std::cout << "Gathering State: " << state << '\n';
    });

    // Fired automatically by the library when the local SDP is ready:
    //   • OFFERER:  after createDataChannel() kicks off negotiation.
    //   • ANSWERER: after setRemoteDescription() auto-generates the answer.
    // We forward the SDP to the remote peer via the signaling channel.
    pc->onLocalDescription([wws, id](const rtc::Description& description) {
        json message = {
            {"id",          id},
            {"type",        description.typeString()},
            {"description", std::string(description)},
        };
        if (auto ws = wws.lock()) {
            ws->send(message.dump());
        }
    });

    // Trickle ICE: send each local candidate to the remote peer immediately
    // instead of waiting for all candidates to be gathered, so the remote
    // side can start connectivity checks sooner.
    pc->onLocalCandidate([wws, id](const rtc::Candidate& candidate) {
        json message = {
            {"id",        id},
            {"type",      "candidate"},
            {"candidate", std::string(candidate)},
            {"mid",       candidate.mid()},
        };
        if (auto ws = wws.lock()) {
            ws->send(message.dump());
        }
    });

    // ANSWERER receives the DataChannel created by the offerer here.
    // The shared_ptr<DataChannel> passed in must be stored in dataChannelMap —
    // otherwise it goes out of scope and the channel is destroyed immediately.
    pc->onDataChannel([id](const shared_ptr<rtc::DataChannel>& dc) {
        std::cout << "DataChannel from " << id << " received with label \"" << dc->label() << "\"\n";

        dc->onOpen([wdc = make_weak_ptr(dc)]() {
            // Greet the offerer once the P2P channel is established.
            if (auto dc = wdc.lock())
                dc->send("Hello from " + localId);
        });

        dc->onClosed([id]() { std::cout << "DataChannel from " << id << " closed" << '\n'; });

        dc->onMessage([id](auto data) {
            if (std::holds_alternative<std::string>(data)) {
                std::cout << "Message from " << id << " received: " << std::get<std::string>(data) << '\n';
            } else {
                std::cout << "Binary message from " << id
                          << " received, size=" << std::get<rtc::binary>(data).size() << '\n';
            }
        });

        // Keep the channel alive — must be stored before leaving this callback.
        dataChannelMap.emplace(id, dc);
    });

    peerConnectionMap.emplace(id, pc);
    return pc;
}

// ---------------------------------------------------------------------------
// Utility: generate a random alphanumeric ID of the given length.
// Uses a thread-local Mersenne Twister seeded from the high-resolution clock
// to avoid contention when called from multiple threads.
// ---------------------------------------------------------------------------
std::string randomId(const size_t length)
{
    using std::chrono::high_resolution_clock;
    static thread_local std::mt19937 rng(static_cast<unsigned int>(high_resolution_clock::now().time_since_epoch().count()));
    static const std::string characters("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
    std::uniform_int_distribution<int> uniform(0, static_cast<int>(characters.size() - 1));
    std::string id(length, '0');
    std::ranges::generate(id, [&]() { return characters.at(uniform(rng)); });
    return id;
}
