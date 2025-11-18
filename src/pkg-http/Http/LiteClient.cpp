#include "LiteClient.h"
#if __EMSCRIPTEN__
    #include "Impl/EmFetchLiteClient.h"
    #include "Impl/JsFetchLiteClient.h"
#else
    #include "Impl/AsioLiteClient.h"
#endif

namespace Http
{
    std::shared_ptr<ILiteClient> LiteClient::MakeDefault(Options options)
    {
        auto executor = std::move(options.executor);
#if __EMSCRIPTEN__
        if (options.wasm.useJsFetchClient) {
            return std::make_shared<JsFetchLiteClient>(std::move(executor));
        }
        return std::make_shared<EmFetchLiteClient>(std::move(executor));
#else
        return std::make_shared<AsioLiteClient>(std::move(executor));
#endif
    }
}
