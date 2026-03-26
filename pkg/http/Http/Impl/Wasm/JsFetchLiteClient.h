#pragma once
#if __EMSCRIPTEN__
#include "../../ILiteClient.h"

namespace Http
{
    /// Lite HTTP client implementation using JS raw fetch API that will work both in browser and Node.js (WASI).
    class JsFetchLiteClient : public ILiteClient
    {
    public:
        void Get(std::string_view url, Callback&& handler, std::stop_token stopToken) override;
    };
}
#endif
