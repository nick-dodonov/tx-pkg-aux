#include "LiteClient.h"
#if __EMSCRIPTEN__
    #include "Impl/Wasm/EmFetchLiteClient.h"
    #include "Impl/Wasm/JsFetchLiteClient.h"
#else
    #include "Impl/Beast/BeastLiteClient.h"
#endif

namespace Http
{
    std::shared_ptr<ILiteClient> LiteClient::MakeDefault(Options options)
    {
#if __EMSCRIPTEN__
        if (options.wasm.useJsFetchClient) {
            return std::make_shared<JsFetchLiteClient>();
        }
        return std::make_shared<EmFetchLiteClient>();
#else
        return std::make_shared<BeastLiteClient>(std::move(options.executor));
#endif
    }
}
