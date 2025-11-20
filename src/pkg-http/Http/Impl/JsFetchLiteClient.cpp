#if __EMSCRIPTEN__
#include "JsFetchLiteClient.h"
#include "Log/Log.h"
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/any_completion_handler.hpp>
#include <emscripten/em_js.h>

namespace Http
{
    // clang-format off
    EM_JS(void, js_fetch, (const char* ptrUrl, void* callback, void* userData), {
        (async () => {
            try {
                const url = UTF8ToString(ptrUrl);
                out("http: js_fetch: fetching URL: " + url);
                // emulate fetch with sleep
                await new Promise(resolve => setTimeout(resolve, 500));
                const text = "Simulated fetch result for URL: " + url;
                out("http: js_fetch: fetch complete, size=" + text.length);
                //const response = await fetch(UTF8ToString(url));
                //const text = await response.text();
                const ptr = stringToNewUTF8(text);
                out("http: js_fetch: result:", ptr.toString(16));
                dynCall('vpp', callback, [ptr, userData]);
            } catch (error) {
                out("http: js_fetch: error:", error);
                dynCall('vpp', callback, [null, userData]);
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

        static void OnFetchCompleteImpl(char* resultPtr, void* userData)
        {
            auto* self = static_cast<JsFetchContext*>(userData);
            self->OnFetchComplete(resultPtr);
        }

        void OnFetchComplete(char* resultPtr)
        {
            if (resultPtr) {
                std::unique_ptr<char, decltype(&free)> result(resultPtr, &free);
                Log::Trace("http: fetch result: ptr={} size={} '{}'", (void*)result.get(), strlen(result.get()), result.get());
                std::move(handler)(ILiteClient::Response{
                    .statusCode = 333, // simulated status code
                    .body = std::string(result.get()),
                });
            } else {
                Log::Trace("http: fetch failed: ptr=<null>");
                std::move(handler)(std::unexpected(std::system_error{
                    std::make_error_code(std::errc::not_connected),
                    "TODO: provide error details from JS side"
                }));
            }
        }

        template <typename CompletionToken>
        auto FetchAsync(CompletionToken&& token)
        {
            Log::Debug("http: async initiate: '{}'", url);
            return boost::asio::async_initiate<CompletionToken, void(ILiteClient::Result)>(
                [this](auto handler) {
                    Log::Debug("http: async initiator: '{}'", url);
                    this->handler = std::move(handler);
                    js_fetch(
                        url.c_str(),
                        (void*)&JsFetchContext::OnFetchCompleteImpl,
                        (void*)this
                    );
                },
                std::forward<CompletionToken>(token)
            );
        }
    };

    boost::asio::awaitable<ILiteClient::Result> JsFetchLiteClient::GetAsync(std::string url)
    {
        Log::Debug("http: async: '{}'", url);
        JsFetchContext ctx{std::move(url), co_await boost::asio::this_coro::executor};
        auto result = co_await ctx.FetchAsync(boost::asio::use_awaitable);
        co_return result;
    }
}
#endif
