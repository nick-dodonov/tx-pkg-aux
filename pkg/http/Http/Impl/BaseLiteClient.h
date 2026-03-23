#pragma once
#include "../ILiteClient.h"
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/awaitable.hpp>

namespace Http
{
    class BaseLiteClient : public ILiteClient
    {
    public:
        explicit BaseLiteClient(boost::asio::any_io_executor executor);

        void Get(std::string_view url, Callback&& handler) override;
        virtual boost::asio::awaitable<ILiteClient::Result> GetAsync(std::string url) = 0;

    private:
        boost::asio::any_io_executor _executor;
    };
}
