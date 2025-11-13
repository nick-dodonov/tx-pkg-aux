#pragma once
#include "ILiteClient.h"
#include <boost/asio/any_io_executor.hpp>

namespace Http
{
    class LiteClient final : public ILiteClient
    {
    public:
        explicit LiteClient(boost::asio::any_io_executor executor);

        void Get(std::string_view url, Callback&& handler) override;

    private:
        boost::asio::any_io_executor _executor;
    };
}
