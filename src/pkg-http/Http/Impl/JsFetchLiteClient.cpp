#if __EMSCRIPTEN__
#include "JsFetchLiteClient.h"
#include "Log/Log.h"
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/any_completion_handler.hpp>
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
                console.log("http: js_fetch: fetch response:", response);

                const bodyText = await response.text();
                // out("http: js_fetch: result: status: " + response.status);
                // out("http: js_fetch: result: body:", bodyText);

                const bodyTextPtr = stringToUTF8OnStack(bodyText);
                _JsFetchContext_OnFetchResult(ctx, response.status, bodyTextPtr);
            } catch (error) {
                console.warn("http: js_fetch: fetch error:", error);
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
        boost::asio::executor_work_guard<boost::asio::any_io_executor> work; // don't let executor stop while fetch in progress
        boost::asio::any_completion_handler<void(ILiteClient::Result)> handler;

        explicit JsFetchContext(std::string url_, const boost::asio::any_io_executor& executor)
            : url(std::move(url_))
            , work(boost::asio::make_work_guard(executor))
        {}

        template <typename CompletionToken>
        auto FetchAsync(CompletionToken&& token)
        {
            //Log::Debug("http: async initiate: {}", url);
            return boost::asio::async_initiate<CompletionToken, void(ILiteClient::Result)>(
                [this](auto handler) {
                    //Log::Debug("http: async initiator: {}", url);
                    this->handler = std::move(handler);
                    JsFetchContext_Fetch(
                        this,
                        url.c_str()
                    );
                },
                std::forward<CompletionToken>(token)
            );
        }

        void OnFetchResult(int status, const char* body)
        {
            //std::unique_ptr<char, decltype(&free)> result(resultPtr, &free); // when stringToUTF8 is used with _malloc
            Log::Trace("http: fetch result: status={} '{}'", status, body);
            CompleteHandler(ILiteClient::Response{
                .statusCode = status,
                .body = body ? std::string(body) : std::string{},
            });
        }

        void OnFetchError(const char* error)
        {
            Log::Trace("http: fetch error: {}", error);
            CompleteHandler(std::unexpected(std::system_error{
                std::make_error_code(std::errc::not_connected),
                error
            }));
        }

        void CompleteHandler(ILiteClient::Result&& result)
        {
            //Log::Debug("(debug) skip completion w/ {}", std::move(result).has_value() ? "success" : "error");
            std::move(handler)(std::move(result));
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

    boost::asio::awaitable<ILiteClient::Result> JsFetchLiteClient::GetAsync(std::string url)
    {
        Log::Debug("http: async: {}", url);
        JsFetchContext ctx{std::move(url), co_await boost::asio::this_coro::executor};
        auto result = co_await ctx.FetchAsync(boost::asio::use_awaitable);
        co_return result;
    }
}
#endif
