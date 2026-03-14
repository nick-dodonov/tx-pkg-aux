#include "Boot/Boot.h"
#include "Log/Log.h"

#include <boost/capy/ex/run_async.hpp>
#include <boost/capy/task.hpp>
#include <boost/corosio.hpp>

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string_view>

namespace corosio = boost::corosio;
namespace capy = boost::capy;

capy::task<void> do_lookup(corosio::io_context& ioc, std::string_view host, std::string_view service)
{
    corosio::resolver r(ioc);

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

    corosio::io_context ioc;
    capy::run_async(ioc.get_executor())(do_lookup(ioc, host, service));
    ioc.run();

    return EXIT_SUCCESS;
}
