#include "rtc/rtc.hpp"

#include <iostream>
#include <thread>

using namespace std::chrono_literals;

int main(int argc, char** argv)
{
    std::cout << "================================================================" << '\n';
    std::cout << "libdatachannel version: " << RTC_VERSION << '\n';

    rtc::Configuration config;

    auto pc = std::make_shared<rtc::PeerConnection>(config);

    pc->onLocalDescription([](rtc::Description description) {
        std::cout << "Local Description (Paste this to the other peer):" << '\n';
        std::cout << std::string(description) << '\n';
    });

    pc->onLocalCandidate([](rtc::Candidate candidate) {
        std::cout << "Local Candidate (Paste this to the other peer after the local description):" << '\n';
        std::cout << std::string(candidate) << '\n' << '\n';
    });

    pc->onStateChange([](rtc::PeerConnection::State state) {
        std::cout << "[State: " << state << "]" << '\n';
    });

    pc->onGatheringStateChange([](rtc::PeerConnection::GatheringState state) {
        std::cout << "[Gathering State: " << state << "]" << '\n';
    });

    auto dc = pc->createDataChannel("test"); // this is the offerer, so create a data channel

    dc->onOpen([&]() {
        std::cout << "[DataChannel open: " << dc->label() << "]" << '\n';
    });

    dc->onClosed([&]() {
        std::cout << "[DataChannel closed: " << dc->label() << "]" << '\n';
    });

    dc->onMessage([](auto data) {
        if (std::holds_alternative<std::string>(data)) {
            std::cout << "[Received: " << std::get<std::string>(data) << "]" << '\n';
        }
    });

    std::this_thread::sleep_for(1s);

    //TODO: include local processing

    if (dc) {
        dc->close();
    }
    if (pc) {
        pc->close();
    }
    return 0;
}
