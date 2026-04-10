#pragma once
#include "CommandSource.h"
#include "Config.h"
#include "RttBridge.h"
#include "SimHandler.h"
#include "SyncAgent.h"

#include "Exec/RunTask.h"
#include "Log/Log.h"
#include "Rtt/Link.h"
#include "Rtt/Rtc/RtcClient.h"
#include "Rtt/Rtc/RtcServer.h"
#include "Rtt/Transport.h"
#include "RunLoop/CompositeHandler.h"
#include "SynTm/Clock.h"
#include "SynTm/Consensus.h"
#include "SynTm/SyncClock.h"
#include "SynTm/Types.h"

#include <chrono>
#include <exec/timed_scheduler.hpp>
#include <memory>
#include <stdexec/execution.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace Demo
{
    /// Single-participant demo node.
    ///
    /// Owns a Consensus, SyncClock, SimHandler, and per-peer SyncAgents.
    /// Runs as an Exec::RunTask<int> coroutine on the shared Domain.
    /// Registers its SimHandler with the CompositeHandler for frame-driven physics.
    class DemoNode
    {
    public:
        DemoNode(
            std::string nodeId,
            std::shared_ptr<Rtt::Rtc::ISigClient> sigClient,
            ICommandSource& commandSource,
            RunLoop::CompositeHandler& composite,
            const Config& config)
            : _nodeId(std::move(nodeId))
            , _logArea("DemoNode/" + _nodeId)
            , _log(_logArea.c_str())
            , _sigClient(std::move(sigClient))
            , _commandSource(commandSource)
            , _composite(composite)
            , _config(config)
            , _consensus(_clock, config.viewer ? SynTm::ConsensusMode::Viewer : SynTm::ConsensusMode::Voter)
            , _syncClock(_consensus)
            , _sim(std::make_shared<SimHandler>(_nodeId, _syncClock))
            , _mlb(std::make_shared<MultiLinkBridge>())
        {
            _sim->speedMultiplier = config.speed;

            // Subscribe to sync events for introspection logging.
            _consensus.OnEvent([&log = _log](SynTm::SyncEvent event) {
                log.Info("[SYNC] event={}", SynTm::SyncEventToString(event));
            });
        }

        /// The main async coroutine — drives this node's lifecycle.
        Exec::RunTask<int> Run()
        {
            _log.Info("starting");

            // Register SimHandler on the composite for frame-driven updates.
            _composite.Add(*_sim);

            const auto sched = co_await stdexec::read_env(stdexec::get_scheduler);

            // Each DcRtcTransport calls ISigClient::Join() once and registers under localId.
            // Two transports with the same localId create two competing WS connections to the
            // signaling server, so only the first-registered one ever receives routed messages.
            //
            // Rule: at most ONE transport may join the signaling channel per node.
            //
            //  - No targets:  create a pure answerer (RtcServer) — registers on signaling,
            //                 accepts every inbound offer.
            //  - Has targets: create one RtcClient per target — the client already accepts
            //                 inbound offers (maxInboundConnections = 4096 by default), so
            //                 no separate RtcServer is needed or allowed.
            if (_config.connectTargets.empty()) {
                auto server = Rtt::Rtc::RtcServer::MakeDefault({
                    .sigClient = _sigClient,
                    .localId = {_nodeId},
                    .iceServers = _config.iceServers,
                });
                auto acceptor = std::make_shared<MultiBridgeAcceptor>(_mlb);
                server->Open(acceptor);
                _transports.push_back(std::move(server));
                _log.Info("[LINK] server open, waiting for inbound connections");
            } else {
                for (const auto& target : _config.connectTargets) {
                    ConnectTo(target);
                }
            }

            // Schedule first shot.
            auto shotInterval = std::chrono::milliseconds(_config.shotIntervalMs);
            auto statusInterval = std::chrono::milliseconds(_config.statusIntervalMs);
            auto nextShot = _syncClock.NowNanos() + std::chrono::duration_cast<std::chrono::nanoseconds>(shotInterval).count();
            _sim->nextShotTime.store(nextShot, std::memory_order_relaxed);

            auto lastStatus = std::chrono::steady_clock::now();
            auto lastProbe = lastStatus;
            auto lastBroadcast = lastStatus;

            // Main loop — periodic tick driven by schedule_after.
            constexpr auto tick = std::chrono::milliseconds(50);
            while (true) {
                co_await exec::schedule_after(sched, tick);

                // Accept new links.
                while (!_mlb->pendingLinks.empty()) {
                    auto pending = std::move(_mlb->pendingLinks.front());
                    _mlb->pendingLinks.pop();
                    OnNewLink(pending.link, pending.bridge);
                }

                // Process messages from all agents.
                ProcessAllMessages();

                // Probe peers.
                auto now = std::chrono::steady_clock::now();
                if (now - lastProbe >= std::chrono::milliseconds(100)) {
                    lastProbe = now;
                    for (auto& [_, agent] : _agents) {
                        agent.TryProbe();
                    }
                }

                // Broadcast state.
                if (now - lastBroadcast >= std::chrono::milliseconds(200)) {
                    lastBroadcast = now;
                    auto syncNow = _syncClock.NowNanos();
                    for (auto& [_, agent] : _agents) {
                        agent.SendStateUpdate(_sim->position, _sim->velocity, syncNow);
                    }
                }

                // Check shot.
                if (_sim->shotFired.exchange(false, std::memory_order_relaxed)) {
                    auto actual = _sim->shotActualTime.load(std::memory_order_relaxed);
                    auto delta = actual - nextShot;
                    _log.Info("[SHOT] scheduled={} actual={} delta={}ns",
                        nextShot, actual, delta);

                    // Announce shot to peers.
                    for (auto& [_, agent] : _agents) {
                        agent.SendShotFired(nextShot);
                    }

                    // Schedule next shot.
                    nextShot = _syncClock.NowNanos() +
                        std::chrono::duration_cast<std::chrono::nanoseconds>(shotInterval).count();
                    _sim->nextShotTime.store(nextShot, std::memory_order_relaxed);
                }

                // Status dump.
                if (now - lastStatus >= statusInterval) {
                    lastStatus = now;
                    DumpStatus();
                }

                // Process commands.
                ProcessCommands();

                // Check disconnected agents.
                RemoveDisconnectedAgents();
            }

            // Unreachable; node runs until runner exits.
            _composite.Remove(*_sim);

            _log.Info("finished");
            co_return 0;
        }

    private:
        void ConnectTo(const std::string& target)
        {
            _log.Info("[LINK] connecting to {}", target);
            auto client = Rtt::Rtc::RtcClient::MakeDefault({
                .sigClient = _sigClient,
                .localId = {_nodeId},
                .remoteId = {target},
                .iceServers = _config.iceServers,
            });
            auto acceptor = std::make_shared<MultiBridgeAcceptor>(_mlb);
            client->Open(acceptor);
            _transports.push_back(std::move(client));
        }

        void OnNewLink(const std::shared_ptr<Rtt::ILink>& link, std::shared_ptr<LinkBridge> bridge)
        {
            auto peerId = link->RemoteId().value;
            _log.Info("[LINK] connected peer={}", peerId);

            _consensus.AddPeer(peerId);
            _agents.emplace(peerId, SyncAgent(peerId, link, std::move(bridge), _consensus, _syncClock));
        }

        void ProcessAllMessages()
        {
            for (auto& [peerId, agent] : _agents) {
                auto bridge = agent.Bridge();
                while (!bridge->pending.empty()) {
                    auto msg = std::move(bridge->pending.front());
                    bridge->pending.pop();
                    if (!msg) {
                        agent.MarkDisconnected();
                        _log.Info("[LINK] disconnected peer={}", peerId);
                        break;
                    }
                    agent.HandleMessage(*msg);
                }
            }
        }

        void RemoveDisconnectedAgents()
        {
            for (auto it = _agents.begin(); it != _agents.end(); ) {
                if (!it->second.IsConnected()) {
                    auto peerId = it->first;
                    _consensus.RemovePeer(peerId);
                    _mlb->RemoveBridge(peerId);
                    it = _agents.erase(it);
                    _log.Info("[LINK] removed peer={}", peerId);
                } else {
                    ++it;
                }
            }
        }

        void ProcessCommands()
        {
            // Non-blocking: check if a command was delivered.
            _commandSource.Request([this](std::optional<Command> cmd) {
                if (!cmd) {
                    return;
                }
                std::visit([this](auto&& c) { HandleCommand(std::forward<decltype(c)>(c)); }, *cmd);
            });
        }

        void HandleCommand(const CmdConnect& cmd)
        {
            _log.Info("[CMD] connect {}", cmd.peerId);
            ConnectTo(cmd.peerId);
        }

        void HandleCommand(const CmdDisconnect& cmd)
        {
            _log.Info("[CMD] disconnect {}", cmd.peerId);
            auto it = _agents.find(cmd.peerId);
            if (it != _agents.end()) {
                it->second.MarkDisconnected();
            }
        }

        void HandleCommand(const CmdShot&)
        {
            auto now = _syncClock.NowNanos();
            _sim->nextShotTime.store(now, std::memory_order_relaxed);
            _log.Info("[CMD] manual shot at synced={}", now);
        }

        void HandleCommand(const CmdStatus&)
        {
            DumpStatus();
        }

        void HandleCommand(const CmdSetSpeed& cmd)
        {
            _sim->speedMultiplier = cmd.speed;
            _log.Info("[CMD] speed={}", cmd.speed);
        }

        void HandleCommand(const CmdQuit&)
        {
            _log.Info("[CMD] quit");
            _composite.Remove(*_sim);
        }

        void DumpStatus()
        {
            auto syncNow = _syncClock.NowNanos();
            auto quality = _consensus.Quality();
            auto synced = _consensus.IsSynced();
            auto peerCount = _consensus.PeerCount();

            _log.Info("=== synced={} quality={} peers={} syncTime={:.3f}s",
                synced, SynTm::SyncQualityToString(quality), peerCount,
                static_cast<double>(syncNow) / 1e9);

            _log.Info("  SIM: pos={:.2f} vel={:.2f} speed={:.1f}",
                _sim->position, _sim->velocity, _sim->speedMultiplier);

            for (const auto& [peerId, agent] : _agents) {
                const auto& v = agent.View();
                auto estPos = v.EstimatePosition(syncNow);
                auto stale = v.TimeSinceLastUpdate(syncNow);
                _log.Info("  PEER {}: pos={:.2f}(est={:.2f}) offset={}ns rtt={}ns quality={} shot-err={}ns stale={:.1f}s",
                    peerId, v.lastPosition, estPos, v.offset, v.rtt,
                    SynTm::SyncQualityToString(v.syncQuality), v.lastShotError, stale);
            }
        }

        std::string _nodeId;
        std::string _logArea;
        Log::Logger _log;
        std::shared_ptr<Rtt::Rtc::ISigClient> _sigClient;
        ICommandSource& _commandSource;
        RunLoop::CompositeHandler& _composite;
        Config _config;

        SynTm::AppClock _clock; // SynTm::SteadyClock as local also can be used if preferred.
        SynTm::Consensus _consensus;
        SynTm::SyncClock _syncClock;
        std::shared_ptr<SimHandler> _sim;
        std::shared_ptr<MultiLinkBridge> _mlb;

        std::unordered_map<std::string, SyncAgent> _agents;
        std::vector<std::shared_ptr<Rtt::ITransport>> _transports;
    };
}
