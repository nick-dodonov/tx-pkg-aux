#if __EMSCRIPTEN__
#include "EmFetchLiteClient.h"
#include "Log/Log.h"
#include <emscripten/fetch.h>
#include <boost/asio.hpp>

namespace Http
{
    struct EmFetchContext
    {
        std::string url;
        boost::asio::executor_work_guard<boost::asio::any_io_executor> work; // don't let executor stop while fetch in progress
        boost::asio::any_completion_handler<void(ILiteClient::Result)> handler;

        explicit EmFetchContext(std::string url_, const boost::asio::any_io_executor& executor)
            : url(std::move(url_))
            , work(boost::asio::make_work_guard(executor))
        {}

        template <typename CompletionToken>
        auto FetchAsync(CompletionToken&& token)
        {
            Log::Debug("http: async initiate: '{}'", url);
            return boost::asio::async_initiate<CompletionToken, void(ILiteClient::Result)>(
                [this](auto handler) {
                    Log::Debug("http: async initiator: '{}'", url);
                    this->handler = std::move(handler);

                    emscripten_fetch_attr_t attr;
                    emscripten_fetch_attr_init(&attr);
                    strcpy(attr.requestMethod, "GET");
                    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
                    attr.userData = this;

                    attr.onsuccess = [](emscripten_fetch_t* fetch) {
                        auto* ctx = static_cast<EmFetchContext*>(fetch->userData);
                        auto status = fetch->status;
                        auto numBytes = fetch->numBytes;
                        Log::Debug("http: onsuccess: {} ({}) bytes={}", status, std::string_view(fetch->statusText), numBytes);
                        std::move(ctx->handler)(ILiteClient::Response{
                            .statusCode = static_cast<int>(status),
                            .body = std::string(fetch->data, numBytes),
                        });
                        emscripten_fetch_close(fetch);
                    };

                    attr.onerror = [](emscripten_fetch_t* fetch) {
                        auto* ctx = static_cast<EmFetchContext*>(fetch->userData);
                        auto status = fetch->status;
                        auto statusView = std::string_view(fetch->statusText); // otherwise std::format fails
                        auto numBytes = fetch->numBytes;
                        Log::Debug("http: onerror: {} ({}) bytes={} ready={} responseUrl={}", 
                            status, statusView, numBytes, fetch->readyState, fetch->responseUrl ? fetch->responseUrl : "<null>");
                        if (status > 0 && fetch->responseUrl) {
                            // HTTP error statuses are reported with their response body allowing to inspect error details in user code
                            std::move(ctx->handler)(ILiteClient::Response{
                                .statusCode = static_cast<int>(status),
                                .body = std::string(fetch->data, numBytes),
                            });
                        } else {
                            // Otherwise it's a network or other error
                            std::move(ctx->handler)(std::unexpected(std::system_error{
                                std::make_error_code(std::errc::not_connected),
                                std::format("emscripten_fetch: onerror: {} ({})", status, statusView)
                            }));
                        }
                        emscripten_fetch_close(fetch);
                    };

                    Log::Debug("http: emscripten_fetch: '{}'", url);
                    emscripten_fetch(&attr, url.c_str());
                },
                std::forward<CompletionToken>(token)
            );
        }
    };

    boost::asio::awaitable<ILiteClient::Result> EmFetchLiteClient::GetAsync(std::string url)
    {
        Log::Debug("http: async: '{}'", url);
        EmFetchContext ctx{std::move(url), co_await boost::asio::this_coro::executor};
        auto result = co_await ctx.FetchAsync(boost::asio::use_awaitable);
        co_return result;
    }
}
#endif
