#include "LiteClient.h"
#if __EMSCRIPTEN__
    #include "Impl/WasmLiteClient.h"
#else
    #include "Impl/AsioLiteClient.h"
#endif

namespace Http
{
    LiteClient::LiteClient(boost::asio::any_io_executor executor)
        : _impl(
#if __EMSCRIPTEN__
              std::make_shared<WasmLiteClient>(std::move(executor))
#else
              std::make_shared<AsioLiteClient>(std::move(executor))
#endif
          )
    {
    }

    void LiteClient::Get(const std::string_view url, Callback&& handler)
    {
        _impl->Get(url, std::move(handler));
    }
}
