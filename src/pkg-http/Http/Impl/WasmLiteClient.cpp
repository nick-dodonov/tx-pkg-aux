#if __EMSCRIPTEN__
#include "WasmLiteClient.h"
#include "Log/Log.h"
#include <emscripten/fetch.h>
#include <boost/asio.hpp>

namespace Http
{
    struct FetchResult {
        int status {};
        std::string data;
        std::string error;
    };

    template <typename CompletionToken>
    auto FetchAsync(std::string url, CompletionToken&& token)
    {
        Log::Debug("http: async_initiate: '{}'", url);
        return boost::asio::async_initiate<CompletionToken, void(boost::system::error_code, FetchResult)>(
            [url = std::move(url)](auto handler) {
                Log::Debug("http: initiation: '{}'", url);
                auto executor = boost::asio::get_associated_executor(handler);
                auto work = boost::asio::make_work_guard(executor);
                struct Context {
                    std::decay_t<decltype(handler)> handler;
                    std::decay_t<decltype(work)> work;
                };
                auto ctx = new Context{std::move(handler), std::move(work)};

                emscripten_fetch_attr_t attr;
                emscripten_fetch_attr_init(&attr);
                strcpy(attr.requestMethod, "GET");
                attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;

                // Transfer ownership to userData
                attr.userData = ctx;

                attr.onsuccess = [](emscripten_fetch_t* fetch) {
                    Log::Debug("http: onsuccess");
                    std::unique_ptr<Context> ctx(static_cast<Context*>(fetch->userData));
                    std::move(ctx->handler)(
                        boost::system::error_code{}, 
                        FetchResult{.status = fetch->status, .data = std::string(fetch->data, fetch->numBytes), .error = ""}
                    );
                    emscripten_fetch_close(fetch);
                };

                attr.onerror = [](emscripten_fetch_t* fetch) {
                    Log::Debug("http: onerror");
                    std::unique_ptr<Context> ctx(static_cast<Context*>(fetch->userData));
                    std::move(ctx->handler)(
                        boost::system::error_code(fetch->status, boost::system::generic_category()),
                        FetchResult{.status = fetch->status, .data = "", .error = "Fetch failed"}
                    );
                    emscripten_fetch_close(fetch);
                };
                
                Log::Debug("http: emscripten_fetch: '{}'", url);
                emscripten_fetch(&attr, url.c_str());
            },
            std::forward<CompletionToken>(token)
        );
    }
    boost::asio::awaitable<ILiteClient::Result> WasmLiteClient::GetAsync(std::string url)
    {
        Log::Debug("http: async: '{}'", url);
        auto result = co_await FetchAsync(url, boost::asio::use_awaitable);
        if (result.status == 200) {
            co_return result.data;
        } else {
            co_return std::unexpected(std::make_error_code(std::errc::connection_aborted));
        }
    }
}
#endif
