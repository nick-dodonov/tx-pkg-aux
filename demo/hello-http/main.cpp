#include "App/AsioContext.h"
#include "Http/LiteClient.h"
#include "Log/Log.h"
#include <span>
#include <boost/asio/experimental/channel.hpp>

// ReSharper disable once CppDFAConstantParameter
static bool HasCommandLineFlag(const int argc, const char** argv, std::string_view flag)
{
    return std::ranges::any_of(
        std::span(argv + 1, argc - 1),
        [flag](const char* arg) { return std::string_view(arg) == flag; }
    );
}

static bool TakeJsFlag(const int argc, const char** argv)
{
    const auto flag = HasCommandLineFlag(argc, argv, "--em");
    Log::Info("useJsFetchClient: {}", !flag);
    return !flag;
}

static std::vector<std::string> ChooseTryUrls(const int argc, const char** argv)
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
        const auto* arg = argv[i];
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

static boost::asio::awaitable<int> CoroMain(const int argc, const char** argv)
{
    auto executor = co_await boost::asio::this_coro::executor;

    auto client = Http::LiteClient::MakeDefault({
        .executor = executor,
        .wasm = {.useJsFetchClient = TakeJsFlag(argc, argv)}
    });

    Log::Info("Running selected tests:");
    const auto selectedUrls = ChooseTryUrls(argc, argv);
    const auto selectedSize = selectedUrls.size();

    // emulating semaphore slim
    using done_channel = boost::asio::experimental::channel<void(boost::system::error_code)>;
    auto completions = std::make_shared<done_channel>(executor, selectedSize);

    for (size_t i = 0; i < selectedSize; ++i) {
        const auto& url = selectedUrls[i];
        Log::Info("TryHttp: >>> [{}] request: {}", i, url);
        client->Get(url, [i, completions, url = std::string{url}](auto result) {
            if (result) {
                const auto& response = *result;
                Log::Info("TryHttp: <<< [{}] success: {}: {} \n{}", i, url, response.statusCode, response.body);
            } else {
                const auto& error = result.error();
                Log::Error("TryHttp: <<< [{}] failed: {}: {}", i, url, error.what());
            }
            completions->try_send(boost::system::error_code{});
        });
    }

    // await N completions
    Log::Info("Awaiting tests: count={}", selectedSize);
    for (size_t i = 0; i < selectedSize; ++i) {
        co_await completions->async_receive(boost::asio::use_awaitable);
    }

    Log::Info("Await finished");
    co_return 0;
}

int main(const int argc, const char* argv[])
{
    static App::AsioContext asioContext{argc, argv};
    return asioContext.RunCoroMain(CoroMain(argc, argv));
}
