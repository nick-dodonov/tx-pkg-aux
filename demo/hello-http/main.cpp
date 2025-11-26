#include "App/AsioContext.h"
#include "Boot/Boot.h"
#include "Http/LiteClient.h"
#include "Log/Log.h"
#include <span>

static App::AsioContext asioContext;

static bool HasCommandLineFlag(int argc, char** argv, std::string_view flag)
{
    return std::ranges::any_of(
        std::span(argv + 1, argc - 1),
        [flag](const char* arg) { return std::string_view(arg) == flag; }
    );
}

static bool TakeJsFlag(int argc, char** argv)
{
    bool flag = HasCommandLineFlag(argc, argv, "--js");
    Log::Info("JsFetchLiteClient: {}", flag);
    return flag;
}

int main(int argc, char** argv)
{
    Boot::LogHeader(argc, argv);

    auto client = Http::LiteClient::MakeDefault({
        .executor = asioContext.get_executor(), 
        .wasm = {.useJsFetchClient = TakeJsFlag(argc, argv)}
    });

    auto TryHttp = [client](std::string_view url) {
        Log::Info("TryHttp: >>> request: '{}'", url);
        client->Get(url, [url](auto result) {
            if (result) {
                const auto& response = *result;
                Log::Info("TryHttp: <<< success: '{}': {} \n{}", url, response.statusCode, response.body);
            } else {
                const auto& error = result.error();
                Log::Error("TryHttp: <<< failed: '{}': {}", url, error.what());
            }
        });
    };

    // TryHttp("url-parse-error");
    // TryHttp("http://localhost:12345/connect-refused");

    // TryHttp("http://ifconfig.io/ip");
    // TryHttp("http://httpbin.org/headers");
    // TryHttp("http://jsonplaceholder.typicode.com/todos/1");

    // TryHttp("http://httpbun.com/status/400");
    // TryHttp("http://httpbun.com/get");
    TryHttp("https://httpbun.com/get");

    return asioContext.Run();
}
