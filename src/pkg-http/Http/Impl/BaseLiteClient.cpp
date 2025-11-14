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
        Log::DebugF("http: query: '{}'", url);
        boost::asio::co_spawn(
            _executor,
            GetAsync(std::string{url}),
            // asio::detached
            [handler = std::move(handler)](const std::exception_ptr& e, Result&& result) {
                if (e) {
                    Log::ErrorF("http: query: exception");
                    handler(std::unexpected(std::make_error_code(std::errc::operation_canceled)));
                } else {
                    Log::DebugF("http: query: succeed"); //TODO: response info?
                    handler(std::move(result));
                }
            }
        );
    }
}
