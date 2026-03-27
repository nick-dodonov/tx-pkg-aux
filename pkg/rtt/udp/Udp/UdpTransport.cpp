#include "UdpLink.h"
#include "UdpTransport.h"
#include <boost/asio/ip/udp.hpp>
#include <format>
#include <map>
#include <memory>
#include <utility>

namespace Rtt::Udp
{
    namespace asio = boost::asio;
    using udp = asio::ip::udp;

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------

    namespace
    {
        PeerId EndpointToPeerId(const udp::endpoint& ep)
        {
            return PeerId{
                .value = std::format("{}:{}", ep.address().to_string(), ep.port())
            };
        }

        Error MapAsioError(const boost::system::error_code& ec)
        {
            if (ec == asio::error::connection_refused) {
                return Error::ConnectionRefused;
            }
            if (ec == asio::error::timed_out) {
                return Error::Timeout;
            }
            if (ec == asio::error::host_not_found ||
                ec == asio::error::host_not_found_try_again) {
                return Error::AddressInvalid;
            }
            return Error::Unknown;
        }
    }

    // -----------------------------------------------------------------------
    // ListenState — shared state for the listener receive loop
    // -----------------------------------------------------------------------

    struct UdpTransport::ListenState
    {
        std::shared_ptr<udp::socket> socket;
        std::shared_ptr<ILinkAcceptor> acceptor;
        std::size_t maxDatagramSize{};
        udp::endpoint senderEndpoint;
        std::vector<std::byte> recvBuf;
        std::map<udp::endpoint, std::shared_ptr<UdpLink>> links;
        PeerId localId;
        bool stopped = false;

        void StartReceive()
        {
            if (stopped) { 
                return;
            }

            socket->async_receive_from(
                asio::buffer(recvBuf.data(), recvBuf.size()),
                senderEndpoint,
                [this, self = socket](boost::system::error_code ec, std::size_t bytesReceived) {
                    if (ec || stopped) return;
                    OnReceived(bytesReceived);
                    StartReceive();
                });
        }

        void OnReceived(std::size_t bytesReceived)
        {
            auto data = std::span<const std::byte>{recvBuf.data(), bytesReceived};
            auto it = links.find(senderEndpoint);

            if (it != links.end()) {
                // Known peer — dispatch to existing link
                it->second->DeliverReceived(data);
                return;
            }

            // New peer — create link and notify acceptor
            auto remoteId = EndpointToPeerId(senderEndpoint);
            auto remoteEp = senderEndpoint;

            auto link = std::make_shared<UdpLink>(
                socket, remoteEp, localId, remoteId,
                maxDatagramSize,
                [this, remoteEp]() { links.erase(remoteEp); });

            links[remoteEp] = link;

            auto handler = acceptor->OnLink(link);
            link->SetHandler(std::move(handler));

            // Deliver the first datagram that triggered link creation
            link->DeliverReceived(data);
        }
    };

    // -----------------------------------------------------------------------
    // UdpTransport
    // -----------------------------------------------------------------------

    UdpTransport::UdpTransport(Options options)
        : _options(std::move(options))
    {}

    UdpTransport::~UdpTransport()
    {
        if (_listenState) {
            _listenState->stopped = true;
            if (_listenState->socket && _listenState->socket->is_open()) {
                boost::system::error_code ec;
                auto _ = _listenState->socket->close(ec);
            }
        }
    }

    // -----------------------------------------------------------------------
    // Connect
    // -----------------------------------------------------------------------

    void UdpTransport::Connect(std::shared_ptr<ILinkAcceptor> acceptor)
    {
        auto executor = _options.executor;
        auto host = _options.remoteHost;
        auto port = std::to_string(_options.remotePort);
        auto maxDgSize = _options.maxDatagramSize;

        auto resolver = std::make_shared<udp::resolver>(executor);
        resolver->async_resolve(
            host, port,
            [resolver, executor, maxDgSize, acceptor = std::move(acceptor)]
            (boost::system::error_code ec, udp::resolver::results_type results) mutable {
                if (ec) {
                    acceptor->OnLink(std::unexpected(MapAsioError(ec)));
                    return;
                }

                auto remoteEp = *results.begin();
                udp::socket socket(executor, udp::v4());

                boost::system::error_code connectEc;
                auto _ = socket.connect(remoteEp, connectEc);
                if (connectEc) {
                    acceptor->OnLink(std::unexpected(MapAsioError(connectEc)));
                    return;
                }

                auto localEp = socket.local_endpoint();
                auto localId = EndpointToPeerId(localEp);
                auto remoteId = EndpointToPeerId(remoteEp);

                auto link = std::make_shared<UdpLink>(
                    std::move(socket), std::move(localId), std::move(remoteId), maxDgSize);

                auto handler = acceptor->OnLink(link);
                link->StartReceive(std::move(handler));
            });
    }

    // -----------------------------------------------------------------------
    // Listen
    // -----------------------------------------------------------------------

    void UdpTransport::Listen(std::shared_ptr<ILinkAcceptor> acceptor)
    {
        auto executor = _options.executor;
        auto port = _options.localPort;
        auto maxDgSize = _options.maxDatagramSize;

        boost::system::error_code ec;
        auto socket = std::make_shared<udp::socket>(executor);

        auto _ = socket->open(udp::v4(), ec);
        if (ec) {
            acceptor->OnLink(std::unexpected(MapAsioError(ec)));
            return;
        }

        _ = socket->set_option(asio::socket_base::reuse_address(true), ec);
        _ = socket->bind(udp::endpoint(udp::v4(), port), ec);
        if (ec) {
            acceptor->OnLink(std::unexpected(MapAsioError(ec)));
            return;
        }

        _localPort = socket->local_endpoint().port();

        _listenState = std::make_shared<ListenState>();
        _listenState->socket = std::move(socket);
        _listenState->acceptor = std::move(acceptor);
        _listenState->maxDatagramSize = maxDgSize;
        _listenState->recvBuf.resize(maxDgSize);
        _listenState->localId = EndpointToPeerId(_listenState->socket->local_endpoint());

        _listenState->StartReceive();
    }
}
