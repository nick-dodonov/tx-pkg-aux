#include "Boot/Boot.h"
#include "Log/Log.h"

#include <boost/capy/ex/thread_pool.hpp>
#include <boost/capy/ex/run_async.hpp>
#include <boost/capy/task.hpp>
#include <boost/capy/delay.hpp>
#include <boost/corosio.hpp>

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string_view>

namespace corosio = boost::corosio;
namespace capy = boost::capy;

using namespace std::literals;

capy::task<void> do_lookup(capy::execution_context& ctx, std::string_view host, std::string_view service)
{
    auto executor = co_await capy::this_coro::executor;
    corosio::resolver r(executor);

    co_await capy::delay(500ms); // simulate some work before starting the lookup

    auto [ec, results] = co_await r.resolve(host, service);
    if (ec) {
        Log::Error("Resolve failed: {}", ec.message());
        co_return;
    }

    std::ostringstream s;
    s << "Results for " << host;
    if (!service.empty()) {
        s << ":" << service;
    }
    Log::Info("{}", s.str());

    for (auto const& entry : results) {
        auto ep = entry.get_endpoint();
        if (ep.is_v4()) {
            Log::Info("  IPv4: {}:{}", ep.v4_address().to_string(), ep.port());
        } else {
            Log::Info("  IPv6: {}:{}", ep.v6_address().to_string(), ep.port());
        }
    }

    Log::Info("Total: {} addresses", results.size());
}

int main(const int argc, const char** argv)
{
    Boot::DefaultInit(argc, argv);

    if (argc < 2 || argc > 3) {
        std::cerr << "Usage: nslookup <hostname> [service]\n"
                     "Examples:\n"
                     "    nslookup www.google.com\n"
                     "    nslookup www.google.com https\n"
                     "    nslookup localhost 8080\n";
        return EXIT_FAILURE;
    }

    std::string_view host = argv[1];
    std::string_view service = (argc == 3) ? argv[2] : "";

    {
        corosio::io_context ioc;
        auto executor = ioc.get_executor();
        capy::run_async(executor)(do_lookup(ioc, host, service));
        ioc.run();
    }

    // {
    //     capy::thread_pool pool(1);
    //     auto executor = pool.get_executor();
    //     capy::run_async(executor, []() { Log::Info("COMPLETE"); },
    //         [](std::exception_ptr ep) {
    //             Log::Info("ERROR");
    //             // try { std::rethrow_exception(ep); }
    //             // catch (std::exception const& e) {
    //             //     std::cout << "Error: " << e.what() << "\n";
    //             // }
    //         })(do_lookup(pool, host, service));
    //     pool.join();
    // }
    return EXIT_SUCCESS;
}
