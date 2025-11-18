#include "BaseLiteClient.h"
#include "Log/Log.h"
#include <boost/asio/co_spawn.hpp>

namespace Http
{
    BaseLiteClient::BaseLiteClient(boost::asio::any_io_executor executor)
        : _executor(std::move(executor))
    {}

    void BaseLiteClient::Get(std::string_view url, Callback&& handler)
    {
        Log::Debug("http: query: '{}'", url);
        boost::asio::co_spawn(
            _executor,
            GetAsync(std::string{url}),
            // asio::detached
            [handler = std::move(handler)](const std::exception_ptr& e, Result&& result) {
                if (e) {
                    // code never here with -fno-exceptions
                    Log::Error("http: query: exception");
                    handler(std::unexpected(std::system_error{
                        std::make_error_code(std::errc::state_not_recoverable),
                        "co_spawn: not handled exception"
                    }));
                } else {
                    if (result) {
                        const auto& response = *result;
                        Log::Debug("http: query: succeed: {} size={}", response.statusCode, response.body.size());
                    } else {
                        const auto& error = result.error();
                        Log::Error("http: query: failed: {}", error.what());
                    }
                    handler(std::move(result));
                }
            }
        );
    }
}
