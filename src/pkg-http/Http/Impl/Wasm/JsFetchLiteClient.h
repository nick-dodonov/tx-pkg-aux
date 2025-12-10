#pragma once
#if __EMSCRIPTEN__
#include "../BaseLiteClient.h"

namespace Http
{
    /// Lite HTTP client implementation using JS raw fetch API that will work both in browser and Node.js (WASI).
    class JsFetchLiteClient : public BaseLiteClient
    {
    public:
        using BaseLiteClient::BaseLiteClient;
        boost::asio::awaitable<ILiteClient::Result> GetAsync(std::string url) override;
    };
}
#endif
