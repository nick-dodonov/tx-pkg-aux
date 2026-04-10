#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace Demo
{
    /// Parsed CLI configuration for the rtt-syntm demo.
    struct Config
    {
        /// Local peer name. Empty = auto-generated.
        std::string name;

        /// Signaling server host.
        std::string sigHost = "localhost";

        /// Signaling server port.
        std::uint16_t sigPort = 8000;

        /// Remote peer IDs to connect to on startup.
        std::vector<std::string> connectTargets;

        /// Number of in-process peers to spawn (0 = external mode).
        int inProcess = 0;

        /// Viewer mode — observe only, no epoch voting.
        bool viewer = false;

        /// Coordinated shot interval in milliseconds.
        int shotIntervalMs = 9000;

        /// Status dump interval in milliseconds.
        int statusIntervalMs = 2000;

        /// Simulation speed multiplier.
        float speed = 1.0f;

        /// ICE server URIs.
        std::vector<std::string> iceServers = {"stun:stun.l.google.com:19302"};

        /// Predefined experiment scenario name (empty = interactive).
        std::string scenario;
    };

    /// Parse CLI arguments into Config.
    /// Supports: --name, --sig-host, --sig-port, --connect, --in-process,
    ///           --viewer, --shot-interval, --status-interval, --speed, --ice, --scenario
    inline Config ParseConfig(int argc, const char* argv[])
    {
        Config cfg;
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            auto next = [&]() -> std::string {
                return (i + 1 < argc) ? argv[++i] : "";
            };

            if (arg == "--name") {
                cfg.name = next();
            } else if (arg == "--sig-host") {
                cfg.sigHost = next();
            } else if (arg == "--sig-port") {
                cfg.sigPort = static_cast<std::uint16_t>(std::stoi(next()));
            } else if (arg == "--connect") {
                cfg.connectTargets.push_back(next());
            } else if (arg == "--in-process") {
                cfg.inProcess = std::stoi(next());
            } else if (arg == "--viewer") {
                cfg.viewer = true;
            } else if (arg == "--shot-interval") {
                cfg.shotIntervalMs = std::stoi(next());
            } else if (arg == "--status-interval") {
                cfg.statusIntervalMs = std::stoi(next());
            } else if (arg == "--speed") {
                cfg.speed = std::stof(next());
            } else if (arg == "--ice") {
                cfg.iceServers.clear();
                cfg.iceServers.push_back(next());
            } else if (arg == "--scenario") {
                cfg.scenario = next();
            }
        }
        return cfg;
    }
}
