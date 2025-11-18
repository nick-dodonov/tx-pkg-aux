#pragma once
#include "ILiteClient.h"
#include <boost/asio/any_io_executor.hpp>
#include <memory>

namespace Http
{
    class LiteClient
    {
    public:
        static std::shared_ptr<ILiteClient> MakeDefault(boost::asio::any_io_executor executor);
    };
}
