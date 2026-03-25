#pragma once
#if __EMSCRIPTEN__
#include "../AsioLiteClient.h"

namespace Http
{
    /// Lite HTTP client implementation using JS raw fetch API that will work both in browser and Node.js (WASI).
    class JsFetchLiteClient : public AsioLiteClient
    {
    public:
        using AsioLiteClient::AsioLiteClient;
        boost::asio::awaitable<ILiteClient::Result> GetAsync(std::string url) override;
    };
}
#endif
