#include "LiteClient.h"
#if __EMSCRIPTEN__
    #include "Impl/WasmLiteClient.h"
#else
    #include "Impl/AsioLiteClient.h"
#endif

namespace Http
{
    std::shared_ptr<ILiteClient> LiteClient::MakeDefault(boost::asio::any_io_executor executor)
    {
#if __EMSCRIPTEN__
        return std::make_shared<WasmLiteClient>(std::move(executor));
#else
        return std::make_shared<AsioLiteClient>(std::move(executor));
#endif
    }
}
