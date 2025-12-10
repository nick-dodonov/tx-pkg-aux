#pragma once
#include "ILiteClient.h"
#include <boost/asio/any_io_executor.hpp>
#include <memory>

namespace Http
{
    class LiteClient
    {
    public:
        struct Options {
            boost::asio::any_io_executor executor;
            struct {
                bool useJsFetchClient = false; // use JsFetchLiteClient instead of EmFetchLiteClient
            } wasm;
        };
        static std::shared_ptr<ILiteClient> MakeDefault(Options options);
    };
}
