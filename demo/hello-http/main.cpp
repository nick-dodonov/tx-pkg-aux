#include "App/AsioContext.h"
#include "Boot/Boot.h"
#include "Http/LiteClient.h"
#include "Log/Log.h"

int main(int argc, char** argv)
{
    Boot::LogInfo(argc, argv);
    App::AsioContext asio_context;

    auto executor = asio_context.get_executor();
    auto client = Http::LiteClient{std::move(executor)};
    auto TryHttp = [&client](std::string_view url) {
        Log::InfoF("TryHttp: >>> request: '{}'", url);
        client.Get(url, [url](auto result) {
            if (result) {
                const auto& response = *result;
                Log::InfoF("TryHttp: <<< success: '{}':\n{}", url, response);
            } else {
                Log::ErrorF("TryHttp: <<< failed: '{}': {}", url, result.error().message());
            }
        });
    };

    // TryHttp("http://ifconfig.io");
    // TryHttp("https://httpbin.org/headers");

    // TryHttp("url-parse-error");
    // TryHttp("http://localhost:12345/connect-refused");
    TryHttp("http://httpbun.com/status/200");
    // TryHttp("http://httpbun.com/get");
    // TryHttp("https://httpbun.com/get");

    return asio_context.Run();
}
