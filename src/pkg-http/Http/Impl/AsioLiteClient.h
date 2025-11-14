#pragma once
#if !__EMSCRIPTEN__
#include "BaseLiteClient.h"

namespace Http
{
    class AsioLiteClient : public BaseLiteClient
    {
    public:
        using BaseLiteClient::BaseLiteClient;
        boost::asio::awaitable<ILiteClient::Result> GetAsync(std::string url) override;
    };
}
#endif
