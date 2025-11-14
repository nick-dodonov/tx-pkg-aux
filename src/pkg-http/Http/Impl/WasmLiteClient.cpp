#if __EMSCRIPTEN__
#include "WasmLiteClient.h"
#include "Log/Log.h"

namespace Http
{
    boost::asio::awaitable<ILiteClient::Result> WasmLiteClient::GetAsync(std::string url)
    {
        Log::ErrorF("TODO: WasmLiteClient::GetAsync: {}", url);
        co_return std::unexpected(std::make_error_code(std::errc::not_supported));
    }
}
#endif
