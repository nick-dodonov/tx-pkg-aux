#include "rtc/rtc.hpp"

#include <iostream>
#include <thread>

using namespace std::chrono_literals;

int main(int argc, char** argv)
{
    std::cout << "================================================================" << '\n';
    std::cout << "[#### libdatachannel] version: " << RTC_VERSION << '\n';

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

    //TODO: include local processing
    std::cout << "[#### Emulate processing]" << '\n';
    std::this_thread::sleep_for(500ms);

    std::cout << "[#### Start closing]" << '\n';
    if (dc) {
        dc->close();
    }
    if (pc) {
        pc->close();
    }

    //TODO: wait closed
    std::cout << "[#### Waiting closed]" << '\n';
    std::this_thread::sleep_for(500ms);
    std::cout << std::flush;
    return 0;
}
