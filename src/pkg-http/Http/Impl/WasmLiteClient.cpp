#if __EMSCRIPTEN__
#include "WasmLiteClient.h"
#include "Log/Log.h"
#include <emscripten/fetch.h>
#include <boost/asio.hpp>

namespace Http
{
    struct FetchContext
    {
        explicit FetchContext(std::string url_, const boost::asio::any_io_executor& executor)
            : url(std::move(url_))
            , work(boost::asio::make_work_guard(executor))
        {}

        std::string url;
        boost::asio::executor_work_guard<boost::asio::any_io_executor> work; // don't let executor stop while fetch in progress
        boost::asio::any_completion_handler<void(ILiteClient::Result)> handler;
    };

    template <typename CompletionToken>
    static auto FetchAsync(FetchContext& ctx, CompletionToken&& token)
    {
        Log::Debug("http: async initiate: '{}'", ctx.url);
        return boost::asio::async_initiate<CompletionToken, void(ILiteClient::Result)>(
            [&ctx](auto handler) {
                Log::Debug("http: async initiator: '{}'", ctx.url);
                ctx.handler = std::move(handler);

                emscripten_fetch_attr_t attr;
                emscripten_fetch_attr_init(&attr);
                strcpy(attr.requestMethod, "GET");
                attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
                attr.userData = &ctx;

                attr.onsuccess = [](emscripten_fetch_t* fetch) {
                    Log::Debug("http: onsuccess: {} ({}) numBytes={}", fetch->status, std::string_view(fetch->statusText), fetch->numBytes);
                    auto* ctx = static_cast<FetchContext*>(fetch->userData);
                    std::move(ctx->handler)(ILiteClient::Response{
                        .statusCode = static_cast<int>(fetch->status),
                        .body = std::string(fetch->data, fetch->numBytes),
                    });
                    emscripten_fetch_close(fetch);
                };

                attr.onerror = [](emscripten_fetch_t* fetch) {
                    Log::Debug("http: onerror: {} ({})", fetch->status, std::string_view(fetch->statusText));
                    auto* ctx = static_cast<FetchContext*>(fetch->userData);
                    std::move(ctx->handler)(std::unexpected(std::system_error{
                        std::make_error_code(std::errc::not_connected),
                        std::format("emscripten_fetch: onerror: {} ({})", fetch->status, std::string_view(fetch->statusText))
                    }));
                    emscripten_fetch_close(fetch);
                };

                Log::Debug("http: emscripten_fetch: '{}'", ctx.url);
                emscripten_fetch(&attr, ctx.url.c_str());
            },
            std::forward<CompletionToken>(token)
        );
    }

    boost::asio::awaitable<ILiteClient::Result> WasmLiteClient::GetAsync(std::string url)
    {
        Log::Debug("http: async: '{}'", url);
        FetchContext ctx{std::move(url), co_await boost::asio::this_coro::executor};
        auto result = co_await FetchAsync(ctx, boost::asio::use_awaitable);
        co_return result;
    }
}
#endif
