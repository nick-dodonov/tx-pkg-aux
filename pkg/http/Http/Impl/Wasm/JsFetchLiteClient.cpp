#if __EMSCRIPTEN__
#include "JsFetchLiteClient.h"
#include "Log/Log.h"
#include <emscripten/em_js.h>

namespace Http
{
    struct JsFetchContext;

    // clang-format off
    EM_JS(void, JsFetchContext_Fetch, (JsFetchContext* ctx, const char* urlPtr), {
        (async () => {
            const url = UTF8ToString(urlPtr);
            out("http: js_fetch: " + url);

            try {
                // await new Promise(resolve => setTimeout(resolve, 500)); // emulate fetch /w sleep
                const response = await fetch(url);
                // console.log("http: js_fetch: fetch response:", response);

                const bodyText = await response.text();
                // out("http: js_fetch: result: status: " + response.status);
                // out("http: js_fetch: result: body:", bodyText);

                const bodyTextPtr = stringToUTF8OnStack(bodyText);
                _JsFetchContext_OnFetchResult(ctx, response.status, bodyTextPtr);
            } catch (error) {
                console.warn("http: js_fetch: error: ", error);
                const errorMessage = error?.message || String(error); //JSON.stringify(error, Object.getOwnPropertyNames(error)));
                // out("http: js_fetch: error: message: " + errorMessage);
                // out("http: js_fetch: error: stack: " + (error?.stack || "<no stack>"));
                _JsFetchContext_OnFetchError(ctx, stringToUTF8OnStack(errorMessage));
            }
        })();
    });
    // clang-format on

    struct JsFetchContext {
        std::string url;
        ILiteClient::Callback handler;

        JsFetchContext(std::string url_, ILiteClient::Callback handler_)
            : url(std::move(url_))
            , handler(std::move(handler_))
        {}

        void OnFetchResult(int status, const char* body)
        {
            auto bodyStr = body ? std::string(body) : std::string{};
            Log::Trace("http: fetch result: status={} body.size={}", status, bodyStr.size());
            std::move(handler)(ILiteClient::Response{
                .statusCode = status,
                .body = std::move(bodyStr),
            });
            delete this;
        }

        void OnFetchError(const char* error)
        {
            Log::Trace("http: fetch error: {}", error);
            std::move(handler)(std::unexpected(std::system_error{
                std::make_error_code(std::errc::not_connected),
                error
            }));
            delete this;
        }
    };

    extern "C" {
        EMSCRIPTEN_KEEPALIVE void JsFetchContext_OnFetchResult(JsFetchContext* ctx, int status, const char* body)
        {
            ctx->OnFetchResult(status, body);
        }
        EMSCRIPTEN_KEEPALIVE void JsFetchContext_OnFetchError(JsFetchContext* ctx, const char* error)
        {
            ctx->OnFetchError(error);
        }
    }

    void JsFetchLiteClient::Get(std::string_view url, Callback&& handler, std::stop_token stopToken)
    {
        if (stopToken.stop_requested()) {
            handler(std::unexpected(std::system_error{
                std::make_error_code(std::errc::operation_canceled),
                "stop requested before request"
            }));
            return;
        }

        Log::Debug("http: js_fetch: {}", url);
        auto* ctx = new JsFetchContext{std::string{url}, std::move(handler)};
        JsFetchContext_Fetch(ctx, ctx->url.c_str());
    }
}
#endif
