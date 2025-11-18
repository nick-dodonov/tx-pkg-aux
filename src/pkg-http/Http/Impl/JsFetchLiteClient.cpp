#include <expected>
#include <system_error>
#if __EMSCRIPTEN__
#include "JsFetchLiteClient.h"
#include "Log/Log.h"
#include <emscripten/em_js.h>

namespace Http
{
    boost::asio::awaitable<ILiteClient::Result> JsFetchLiteClient::GetAsync(std::string url)
    {
        Log::Error("TODO: http: async: '{}'", url);
        co_return std::unexpected{std::system_error{
            std::make_error_code(std::errc::not_supported),
            "JsFetchLiteClient::GetAsync: not implemented yet"
        }};
    }
}
#endif
