#include "App/Domain.h"
#include "Http/LiteClient.h"
#include "Log/Log.h"
#include <boost/asio/experimental/channel.hpp>

static std::vector<std::string> ChooseTryUrls(const Boot::CliArgs& args)
{
    static const auto urls = std::map<std::string, std::string, std::less<>>{
        {"ep", "url-parse-error"},
        {"ec", "http://localhost:12345/connect-refused"},
        {"tif", "http://ifconfig.io/ip"},
        {"tjp", "http://jsonplaceholder.typicode.com/todos/1"},
        {"h200", "http://httpbun.com/status/200"},
        {"h400", "http://httpbun.com/status/400"},
        {"hget", "http://httpbun.com/get"},
        {"get", "https://httpbun.com/get"},
        {"eget", " https://self-signed-cert.httpbun.com/get"},
    };

    std::vector<std::string> selectedUrls;
    for (const auto& arg: args) {
        if (arg.starts_with("http://") or arg.starts_with("https://")) {
            selectedUrls.push_back(std::string{arg});
            continue;
        }

        if (auto it = urls.find(arg); it != urls.end()) {
            selectedUrls.push_back(it->second);
        }
    }

    return selectedUrls;
}

static boost::asio::awaitable<int> CoroMain(const std::shared_ptr<App::Domain> domain)
{
    auto executor = co_await boost::asio::this_coro::executor;

    const auto& args = domain->GetCliArgs();
    auto useJsFetchClient = !args.Contains("--em");
    const auto client = Http::LiteClient::MakeDefault({
        .executor = executor,
        .wasm = {.useJsFetchClient = useJsFetchClient}
    });

    Log::Info("Running selected tests (useJsFetchClient={}):", useJsFetchClient);
    const auto selectedUrls = ChooseTryUrls(args);
    const auto selectedSize = selectedUrls.size();
    if (selectedSize <= 0) {
        Log::Error("No valid URL keys specified.");
        co_return 1;
    }

    // emulating semaphore slim
    using done_channel = boost::asio::experimental::channel<void(boost::system::error_code)>;
    auto completions = std::make_shared<done_channel>(executor, selectedSize);

    for (size_t i = 0; i < selectedSize; ++i) {
        const auto& url = selectedUrls.at(i);
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
    const auto domain = std::make_shared<App::Domain>(argc, argv);
    return domain->RunCoroMain(CoroMain(domain));
}
