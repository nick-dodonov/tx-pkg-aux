#pragma once
#if __EMSCRIPTEN__
#include "BaseLiteClient.h"

namespace Http
{
    /// Lite HTTP client implementation using Emscripten Fetch API.
    /// It's based on XMLHttpRequest in browser, so doesn't work in WASI environment (i.e. Node.js),
    ///   so requires Polyfill for XMLHttpRequest to work there (can be linked w/ --post-js <polyfill.js>).
    /// See https://emscripten.org/docs/api_reference/fetch.html
    class WasmLiteClient : public BaseLiteClient
    {
    public:
        using BaseLiteClient::BaseLiteClient;
        boost::asio::awaitable<ILiteClient::Result> GetAsync(std::string url) override;
    };
}
#endif
