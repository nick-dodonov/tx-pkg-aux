#include "App/AsioContext.h"
#include "Boot/Boot.h"
#include "Http/LiteClient.h"
#include "Log/Log.h"

static App::AsioContext asioContext;

int main(int argc, char** argv)
{
    Boot::LogHeader(argc, argv);

    auto client = Http::LiteClient::MakeDefault(asioContext.get_executor());
    auto TryHttp = [client](std::string_view url) {
        Log::Info("TryHttp: >>> request: '{}'", url);
        client->Get(url, [url](auto result) {
            if (result) {
                const auto& response = *result;
                Log::Info("TryHttp: <<< success: '{}':\n{}", url, response);
            } else {
                Log::Error("TryHttp: <<< failed: '{}': {}", url, result.error().message());
            }
        });
    };

    // TryHttp("url-parse-error");
    TryHttp("http://localhost:12345/connect-refused");

    // TryHttp("http://ifconfig.io/ip");
    // TryHttp("http://httpbin.org/headers");
    // TryHttp("http://jsonplaceholder.typicode.com/todos/1");

    // TryHttp("http://httpbun.com/status/200");
    // TryHttp("http://httpbun.com/get");
    // TryHttp("https://httpbun.com/get");

    return asioContext.Run();
}
