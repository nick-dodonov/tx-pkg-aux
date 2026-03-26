#if __EMSCRIPTEN__
#include "EmFetchLiteClient.h"
#include "Log/Log.h"
#include <emscripten/fetch.h>

namespace Http
{
    struct EmFetchContext
    {
        std::string url;
        ILiteClient::Callback handler;

        EmFetchContext(std::string url_, ILiteClient::Callback handler_)
            : url(std::move(url_))
            , handler(std::move(handler_))
        {}
    };

    void EmFetchLiteClient::Get(std::string_view url, Callback&& handler, std::stop_token stopToken)
    {
        if (stopToken.stop_requested()) {
            handler(std::unexpected(std::system_error{
                std::make_error_code(std::errc::operation_canceled),
                "stop requested before request"
            }));
            return;
        }

        Log::Debug("http: emscripten_fetch: {}", url);
        auto* ctx = new EmFetchContext{std::string{url}, std::move(handler)};

        emscripten_fetch_attr_t attr;
        emscripten_fetch_attr_init(&attr);
        strcpy(attr.requestMethod, "GET");
        attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
        attr.userData = ctx;

        attr.onsuccess = [](emscripten_fetch_t* fetch) {
            auto* ctx = static_cast<EmFetchContext*>(fetch->userData);
            auto status = fetch->status;
            auto numBytes = fetch->numBytes;
            Log::Debug("http: onsuccess: {} ({}) bytes={}", status, std::string_view(fetch->statusText), numBytes);
            std::move(ctx->handler)(ILiteClient::Response{
                .statusCode = static_cast<int>(status),
                .body = std::string(fetch->data, numBytes),
            });
            delete ctx;
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
            delete ctx;
            emscripten_fetch_close(fetch);
        };

        emscripten_fetch(&attr, ctx->url.c_str());
    }
}
#endif
