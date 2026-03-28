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

template <class T>
weak_ptr<T> make_weak_ptr(const shared_ptr<T>& ptr) { return ptr; }

using nlohmann::json;


class Cmdline
{
    bool _noStun{};
    bool _udpMux{};
public:
    Cmdline(const int argc, char** argv)
    {
        (void)argc;
        (void)argv;
        if (argc > 1) {
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
    }

    [[nodiscard]] bool noStun() const { return _noStun; }
    [[nodiscard]] bool udpMux() const { return _udpMux; }
    [[nodiscard]] std::string stunServer() const { return "stun.l.google.com"; }
    [[nodiscard]] int stunPort() const { return 19302; }
    [[nodiscard]] std::string webSocketServer() const { return "localhost"; }
    [[nodiscard]] int webSocketPort() const { return 8000; }
};

static std::string localId;
static std::unordered_map<std::string, shared_ptr<rtc::PeerConnection>> peerConnectionMap;
static std::unordered_map<std::string, shared_ptr<rtc::DataChannel>> dataChannelMap;

static shared_ptr<rtc::PeerConnection> createPeerConnection(
    const rtc::Configuration& config,
    const weak_ptr<rtc::WebSocket>& wws,
    std::string id);

static std::string randomId(size_t length);

int main(int argc, char** argv) try { //NOLINT(readability-function-cognitive-complexity)
    Cmdline params(argc, argv);

    rtc::InitLogger(rtc::LogLevel::Info);

    rtc::Configuration config;
    std::string stunServer;
    if (params.noStun()) {
        std::cout << "No STUN server is configured. Only local hosts and public IP addresses supported." << '\n';
    } else {
        if (!params.stunServer().starts_with("stun:")) {
            stunServer = "stun:";
        }
        stunServer += params.stunServer() + ":" + std::to_string(params.stunPort());
        std::cout << "STUN server is " << stunServer << '\n';
        config.iceServers.emplace_back(stunServer);
    }

    if (params.udpMux()) {
        std::cout << "ICE UDP mux enabled" << '\n';
        config.enableIceUdpMux = true;
    }

    localId = randomId(4);
    std::cout << "The local ID is " << localId << '\n';

    auto ws = std::make_shared<rtc::WebSocket>();

    std::promise<void> wsPromise;
    auto wsFuture = wsPromise.get_future();

    ws->onOpen([&wsPromise]() {
        std::cout << "WebSocket connected, signaling ready" << '\n';
        wsPromise.set_value();
    });

    ws->onError([&wsPromise](const std::string& s) {
        std::cout << "WebSocket error" << '\n';
        wsPromise.set_exception(std::make_exception_ptr(std::runtime_error(s)));
    });

    ws->onClosed([]() { std::cout << "WebSocket closed" << '\n'; });

    ws->onMessage([&config, wws = make_weak_ptr(ws)](auto data) {
        // data holds either std::string or rtc::binary
        if (!std::holds_alternative<std::string>(data)) {
            return;
        }
        const auto& str = std::get<std::string>(data);
        json message = json::parse(str.data(), str.data() + str.size(), nullptr, true, false);

        auto it = message.find("id");
        if (it == message.end()) {
            return;
        }

        auto id = it->get<std::string>();

        it = message.find("type");
        if (it == message.end()) {
            return;
        }

        auto type = it->get<std::string>();

        shared_ptr<rtc::PeerConnection> pc;
        if (auto jt = peerConnectionMap.find(id); jt != peerConnectionMap.end()) {
            pc = jt->second;
        } else if (type == "offer") {
            std::cout << "Answering to " + id << '\n';
            pc = createPeerConnection(config, wws, id);
        } else {
            return;
        }

        if (type == "offer" || type == "answer") {
            auto sdp = message["description"].get<std::string>();
            pc->setRemoteDescription(rtc::Description(sdp, type));
        } else if (type == "candidate") {
            auto sdp = message["candidate"].get<std::string>();
            auto mid = message["mid"].get<std::string>();
            pc->addRemoteCandidate(rtc::Candidate(sdp, mid));
        }
    });

    const std::string wsPrefix = !params.webSocketServer().contains("://") ? "ws://" : "";
    const auto url = wsPrefix + params.webSocketServer() + ":" + std::to_string(params.webSocketPort()) + "/" + localId;

    std::cout << "WebSocket URL is " << url << '\n';
    ws->open(url);

    std::cout << "Waiting for signaling to be connected..." << '\n';
    wsFuture.get();

    while (true) {
        std::string id;
        std::cout << "Enter a remote ID to send an offer:" << '\n';
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

        // We are the offerer, so create a data channel to initiate the process
        const std::string label = "test";
        std::cout << "Creating DataChannel with label \"" << label << "\"" << '\n';
        auto dc = pc->createDataChannel(label);

        dc->onOpen([id, wdc = make_weak_ptr(dc)]() {
            std::cout << "DataChannel from " << id << " open" << '\n';
            if (auto dc = wdc.lock()) {
                dc->send("Hello from " + localId);
            }
        });

        dc->onClosed([id]() { std::cout << "DataChannel from " << id << " closed" << '\n'; });

        dc->onMessage([id, wdc = make_weak_ptr(dc)](auto data) {
            // data holds either std::string or rtc::binary
            if (std::holds_alternative<std::string>(data)) {
                std::cout << "Message from " << id << " received: " << std::get<std::string>(data)
                    << '\n';
            } else {
                std::cout << "Binary message from " << id
                    << " received, size=" << std::get<rtc::binary>(data).size() << '\n';
            }
        });

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

// Create and set up a PeerConnection
shared_ptr<rtc::PeerConnection> createPeerConnection(
    const rtc::Configuration& config,
    const weak_ptr<rtc::WebSocket>& wws,
    std::string id)
{
    auto pc = std::make_shared<rtc::PeerConnection>(config);

    pc->onStateChange(
        [](rtc::PeerConnection::State state) { std::cout << "State: " << state << '\n'; });

    pc->onGatheringStateChange([](rtc::PeerConnection::GatheringState state) {
        std::cout << "Gathering State: " << state << '\n';
    });

    pc->onLocalDescription([wws, id](const rtc::Description& description) {
        json message = {{"id", id},
                        {"type", description.typeString()},
                        {"description", std::string(description)}};

        if (auto ws = wws.lock()) {
            ws->send(message.dump());
        }
    });

    pc->onLocalCandidate([wws, id](const rtc::Candidate& candidate) {
        json message = {{"id", id},
                        {"type", "candidate"},
                        {"candidate", std::string(candidate)},
                        {"mid", candidate.mid()}};

        if (auto ws = wws.lock()) {
            ws->send(message.dump());
        }
    });

    pc->onDataChannel([id](const shared_ptr<rtc::DataChannel>& dc) {
        std::cout << "DataChannel from " << id << " received with label \"" << dc->label() << "\""
            << '\n';

        dc->onOpen([wdc = make_weak_ptr(dc)]() {
            if (auto dc = wdc.lock()) {
                dc->send("Hello from " + localId);
            }
        });

        dc->onClosed([id]() { std::cout << "DataChannel from " << id << " closed" << '\n'; });

        dc->onMessage([id](auto data) {
            // data holds either std::string or rtc::binary
            if (std::holds_alternative<std::string>(data)) {
                std::cout << "Message from " << id << " received: " << std::get<std::string>(data)
                    << '\n';
            } else {
                std::cout << "Binary message from " << id
                    << " received, size=" << std::get<rtc::binary>(data).size() << '\n';
            }
        });

        dataChannelMap.emplace(id, dc);
    });

    peerConnectionMap.emplace(id, pc);
    return pc;
};

// Helper function to generate a random ID
// ReSharper disable once CppDFAConstantParameter
std::string randomId(const size_t length)
{
    using std::chrono::high_resolution_clock;
    static thread_local std::mt19937 rng(
        static_cast<unsigned int>(high_resolution_clock::now().time_since_epoch().count()));
    static const std::string characters("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
    std::string id(length, '0');
    std::uniform_int_distribution<int> uniform(0, static_cast<int>(characters.size() - 1));
    std::ranges::generate(id, [&]() { return characters.at(uniform(rng)); });
    return id;
}
