#pragma once
#if !__EMSCRIPTEN__
#include "../AsioLiteClient.h"

namespace Http
{
    class BeastLiteClient : public AsioLiteClient
    {
    public:
        using AsioLiteClient::AsioLiteClient;
        boost::asio::awaitable<Result> GetAsync(std::string url) override;
    };
}
#endif
