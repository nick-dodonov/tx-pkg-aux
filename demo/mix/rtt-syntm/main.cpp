#include "CommandSource.h"
#include "Config.h"
#include "DemoNode.h"

#include "App/Factory.h"
#include "Boot/Boot.h"
#include "Exec/Domain.h"
#include "Log/Log.h"
#include "Rtt/Rtc/LocalSigClient.h"
#include "Rtt/Rtc/SigHub.h"
#include "Rtt/Rtc/WsSigClient.h"
#include "RunLoop/CompositeHandler.h"

#include <memory>
#include <string>
#include <vector>

using namespace Demo;

/// Generate a node name like "node-0", "node-1", etc.
static std::string MakeNodeName(int index)
{
    return "node-" + std::to_string(index);
}

int main(int argc, const char* argv[])
{
    Boot::DefaultInit(argc, argv);
    auto config = ParseConfig(argc, argv);

    auto composite = std::make_shared<RunLoop::CompositeHandler>();

    if (config.inProcess > 0) {
        // In-process mode: create N nodes with LocalSigClient, chain topology.
        // Each node gets its own Domain; all are multiplexed on one CompositeHandler.
        NullCommandSource nullCmd;
        auto hub = std::make_shared<Rtt::Rtc::SigHub>();
        int n = config.inProcess;
        Log::Info("starting {} in-process nodes", n);

        std::vector<std::unique_ptr<DemoNode>> nodes;
        std::vector<std::shared_ptr<Exec::Domain>> domains;

        for (int i = 0; i < n; ++i) {
            auto name = MakeNodeName(i);
            auto sigClient = std::make_shared<Rtt::Rtc::LocalSigClient>(hub);

            Config nodeConfig = config;
            nodeConfig.name = name;
            nodeConfig.connectTargets.clear();
            if (i > 0) {
                nodeConfig.connectTargets.push_back(MakeNodeName(i - 1));
            }

            nodes.push_back(std::make_unique<DemoNode>(
                name, sigClient, nullCmd, *composite, nodeConfig));
        }

        // Create a Domain per node and add each to the composite.
        for (auto& node : nodes) {
            auto domain = std::make_shared<Exec::Domain>(node->Run());
            composite->Add(*domain);
            domains.push_back(std::move(domain));
        }

        return App::CreateDefaultRunner(composite)->Run();
    } else {
        // External mode: single node connecting to a signaling server.
        StdinCommandSource stdinCmd;
        auto sigClient = Rtt::Rtc::WsSigClient::MakeDefault({
            .host = config.sigHost,
            .port = config.sigPort,
        });

        auto nodeId = config.name.empty() ? "peer" : config.name;
        Log::Info("starting external node {}", nodeId);

        auto node = std::make_unique<DemoNode>(
            nodeId, sigClient, stdinCmd, *composite, config);

        auto domain = std::make_shared<Exec::Domain>(node->Run());
        composite->Add(*domain);

        return App::CreateDefaultRunner(composite)->Run();
    }
}
