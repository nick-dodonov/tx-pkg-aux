#include "RttBridge.h"

#include "App/Factory.h"
#include "Asio/AsioPoller.h"
#include "Boot/Boot.h"
#include "Exec/Domain.h"
#include "Exec/RunTask.h"
#include "Log/Log.h"
#include "Udp/UdpServer.h"
#include "RunLoop/CompositeHandler.h"

#include <chrono>
#include <format>
#include <memory>
#include <string_view>

using namespace std::chrono;

static std::string Timestamp()
{
    return std::format("{:%H:%M:%S}", floor<seconds>(system_clock::now()));
}

static Exec::RunTask<int> ServerTask(
    boost::asio::any_io_executor executor,
    std::uint16_t port,
    bool keepAlive)
{
    Log::Info("request listening on port {}", port);

    auto bridge = std::make_shared<Demo::RttBridge>();
    auto acceptor = std::make_shared<Demo::BridgeAcceptor>(bridge);
    Rtt::Udp::UdpServer server{{
        .executor = executor,
        .localPort = port
    }};
    server.Open(acceptor);

    Log::Info("opened listening on port {}", server.LocalPort());

    do {
        auto link = co_await Demo::AwaitLink(bridge);
        Log::Info("peer connected: {}", link->RemoteId().value);

        const std::string message = std::format("Hello from server at {}", Timestamp());
        Log::Info("sending message: '{}'", message);
        link->Send([&message](std::span<std::byte> buf) {
            std::memcpy(buf.data(), message.data(), message.size());
            return message.size();
        });

        Log::Info("awaiting message");
        auto payload = co_await Demo::AwaitMessage(bridge);
        if (!payload) {
            Log::Warn("peer disconnected before sending a message");
            if (!keepAlive) {
                co_return 0;
            }
            bridge->onLink = nullptr;
            continue;
        }
        std::string_view msg{reinterpret_cast<const char*>(payload->data()), payload->size()}; //NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        Log::Info("received message: '{}'", msg);

        // //TODO: await sent happened
        // constexpr auto SentDelay = std::chrono::milliseconds(1000);
        // Log::Info("awaiting {}ms to ensure sent", SentDelay.count());
        // {
        //     const auto sched = co_await stdexec::read_env(stdexec::get_scheduler);
        //     co_await exec::schedule_after(sched, SentDelay);
        // }

        Log::Info("disconnecting");
        link->Disconnect();

        if (!keepAlive) {
            Log::Info("exiting");
            co_return 0;
        }

        bridge->onLink = nullptr;
    } while (true);
}

int main(const int argc, const char* argv[])
{
    auto args = Boot::DefaultInit(argc, argv);

    const bool keepAlive = args.Contains("-k");
    std::uint16_t port = Demo::DefaultPort;
    if (args.size() > 1) {
        if (auto p = args.GetIntArg(1)) {
            port = static_cast<std::uint16_t>(*p);
        }
    }

    Asio::AsioPoller asioPoller;
    auto domain = std::make_shared<Exec::Domain>(ServerTask(
        asioPoller.GetExecutor(), 
        port, 
        keepAlive
    ));

    auto composite = std::make_shared<RunLoop::CompositeHandler>();
    composite->Add(asioPoller);
    composite->Add(*domain);

    return App::CreateDefaultRunner(composite)->Run();
}
