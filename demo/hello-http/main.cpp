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

static std::vector<std::string> ChooseTryUrls(int argc, char** argv)
{
    static const auto urls = std::map<std::string, std::string>{
        {"ep", "url-parse-error"},
        {"ec", "http://localhost:12345/connect-refused"},
        {"tif", "http://ifconfig.io/ip"},
        {"tjp", "http://jsonplaceholder.typicode.com/todos/1"},
        {"h200", "http://httpbun.com/status/200"},
        {"h400", "http://httpbun.com/status/400"},
        {"hget", "http://httpbun.com/get"},
        {"get", "https://httpbun.com/get"},
    };

    std::vector<std::string> selectedUrls;
    for (int i = 1; i < argc; ++i) {
        auto* arg = argv[i];
        if (auto it = urls.find(arg); it != urls.end()) {
            selectedUrls.push_back(it->second);
        }
    }

    if (selectedUrls.empty()) {
        Log::Info("No valid URL keys specified. Setting default.");
        selectedUrls.emplace_back("https://httpbun.com/get");
    }

    return selectedUrls;
}

int main(int argc, char** argv)
{
    Boot::LogHeader(argc, argv);

    auto client = Http::LiteClient::MakeDefault({
        .executor = asioContext.get_executor(), 
        .wasm = {.useJsFetchClient = TakeJsFlag(argc, argv)}
    });

    auto TryHttp = [client](std::string_view url) {
        Log::Info("TryHttp: >>> request: {}", url);
        client->Get(url, [url](auto result) {
            if (result) {
                const auto& response = *result;
                Log::Info("TryHttp: <<< success: {}: {} \n{}", url, response.statusCode, response.body);
            } else {
                const auto& error = result.error();
                Log::Error("TryHttp: <<< failed: {}: {}", url, error.what());
            }
        });
    };

    auto selectedUrls = ChooseTryUrls(argc, argv);

    Log::Info("Running selected tests:");
    for (const auto& url : selectedUrls) {
        TryHttp(url);
    }

    return asioContext.Run();
}
