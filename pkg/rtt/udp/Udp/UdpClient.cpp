#include "UdpClient.h"
#include "UdpCommon.h"
#include "UdpLink.h"

#include "Log/Log.h"
#include <boost/asio/ip/udp.hpp>
#include <memory>
#include <utility>

namespace Rtt::Udp
{
    namespace asio = boost::asio;
    using udp = asio::ip::udp;

    UdpClient::UdpClient(Options options)
        : _options(std::move(options))
    {}

    std::shared_ptr<IConnector> UdpClient::Open(std::shared_ptr<ILinkAcceptor> acceptor)
    {
        auto executor = _options.executor;
        auto host = _options.remoteHost;
        auto port = std::to_string(_options.remotePort);
        auto maxDgSize = _options.maxDatagramSize;

        Log::Trace("resolving {}:{}", host, port);

        auto resolver = std::make_shared<udp::resolver>(executor);
        resolver->async_resolve(
            host, port,
            [resolver, executor, maxDgSize, acceptor = std::move(acceptor), host, port]
            (boost::system::error_code ec, udp::resolver::results_type results) mutable {
                if (ec) {
                    Log::Trace("resolve failed for {}:{} — {}", host, port, ec.message());
                    acceptor->OnLink(std::unexpected(MapAsioError(ec)));
                    return;
                }

                auto remoteEp = *results.begin();
                udp::socket socket(executor, udp::v4());

                boost::system::error_code connectEc;
                auto _ = socket.connect(remoteEp, connectEc);
                if (connectEc) {
                    Log::Trace("connect failed to {}:{} — {}", host, port, connectEc.message());
                    acceptor->OnLink(std::unexpected(MapAsioError(connectEc)));
                    return;
                }

                auto localEp = socket.local_endpoint();
                auto localId = EndpointToPeerId(localEp);
                auto remoteId = EndpointToPeerId(remoteEp);

                Log::Trace("connected {} -> {}", localId.value, remoteId.value);

                auto link = std::make_shared<UdpLink>(
                    std::move(socket), std::move(localId), std::move(remoteId), maxDgSize);

                auto handler = acceptor->OnLink(link);
                link->StartReceive(std::move(handler));
            });
        return nullptr;
    }
}
