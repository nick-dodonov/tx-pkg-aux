#pragma once
#include "Rtt/Error.h"
#include "Rtt/PeerId.h"

#include <boost/asio/error.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/system/error_code.hpp>
#include <format>

namespace Rtt::Udp
{
    /// Format a UDP endpoint as a PeerId ("ip:port").
    inline PeerId EndpointToPeerId(const boost::asio::ip::udp::endpoint& ep)
    {
        return PeerId{
            .value = std::format("{}:{}", ep.address().to_string(), ep.port())
        };
    }

    /// Map a Boost.Asio error code to the Rtt::Error enum.
    inline Error MapAsioError(const boost::system::error_code& ec)
    {
        namespace asio = boost::asio;
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
