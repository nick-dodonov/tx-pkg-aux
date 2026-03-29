#include "RttBridge.h"

#include "App/Factory.h"
#include "Asio/AsioPoller.h"
#include "Boot/Boot.h"
#include "Exec/Domain.h"
#include "Exec/RunTask.h"
#include "Log/Log.h"
#include "Udp/UdpClient.h"
#include "RunLoop/CompositeHandler.h"

#include <chrono>
#include <cstdint>
#include <format>
#include <memory>
#include <string>
#include <string_view>

using namespace std::chrono;

static std::string Timestamp()
{
    return std::format("{:%H:%M:%S}", floor<seconds>(system_clock::now()));
}

static Exec::RunTask<int> ClientTask(
    boost::asio::any_io_executor executor,
    std::string host,
    std::uint16_t port)
{
    Log::Info("connecting to {}:{}", host, port);

    auto bridge = std::make_shared<Demo::RttBridge>();
    auto acceptor = std::make_shared<Demo::BridgeAcceptor>(bridge);
    Rtt::Udp::UdpClient client{{
        .executor = executor,
        .remoteHost = host,
        .remotePort = port,
    }};
    client.Open(acceptor);

    auto link = co_await Demo::AwaitLink(bridge);
    Log::Info("connected: {} -> {}", link->LocalId().value, link->RemoteId().value);

    const std::string greeting = std::format("Hello from client at {}", Timestamp());
    Log::Info("sending message: '{}'", greeting);
    link->Send([&greeting](std::span<std::byte> buf) {
        std::memcpy(buf.data(), greeting.data(), greeting.size());
        return greeting.size();
    });

    Log::Info("awaiting message");
    auto payload = co_await Demo::AwaitMessage(bridge);
    if (!payload) {
        Log::Warn("server disconnected before sending a reply");
        co_return 1;
    }
    std::string_view reply{reinterpret_cast<const char*>(payload->data()), payload->size()}; //NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    Log::Info("received message: '{}'", reply);

    // //TODO: await sent happened
    // constexpr auto SentDelay = std::chrono::milliseconds(1000);
    // Log::Info("awaiting {}ms to ensure sent", SentDelay.count());
    // {
    //     const auto sched = co_await stdexec::read_env(stdexec::get_scheduler);
    //     co_await exec::schedule_after(sched, SentDelay);
    // }

    Log::Info("disconnecting");
    link->Disconnect();

    Log::Info("exiting");
    co_return 0;
}

int main(const int argc, const char* argv[])
{
    auto args = Boot::DefaultInit(argc, argv);

    std::string host = Demo::DefaultHost;
    auto port = Demo::DefaultPort;
    if (args.size() > 1) {
        host = std::string{args[1]};
        if (args.size() > 2) {
            auto portResult  = args.GetIntArg(2);
            if (!portResult) {
                Log::Error("invalid port: {}", args[2]);
                return 1;
            }
            port = static_cast<std::uint16_t>(*portResult);
        }
    }

    Asio::AsioPoller asioPoller;
    auto domain = std::make_shared<Exec::Domain>(ClientTask(
        asioPoller.GetExecutor(), 
        std::move(host), 
        port
    ));

    auto composite = std::make_shared<RunLoop::CompositeHandler>();
    composite->Add(asioPoller);
    composite->Add(*domain);

    return App::CreateDefaultRunner(composite)->Run();
}
