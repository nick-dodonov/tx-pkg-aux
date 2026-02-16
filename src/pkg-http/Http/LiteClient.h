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
                // use JsFetchLiteClient instead of EmFetchLiteClient implementation
                //  EmFetchLiteClient impl requires XMLHttpRequest to be available (not in Node.js)
                bool useJsFetchClient = true;
            } wasm;
        };
        static std::shared_ptr<ILiteClient> MakeDefault(Options options);
    };
}
