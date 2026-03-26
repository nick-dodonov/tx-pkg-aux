#include "App/Factory.h"
#include "Asio/AsioPoller.h"
#include "Boot/Boot.h"
#include "Exec/Domain.h"
#include "Exec/RunTask.h"
#include "Http/ILiteClient.h"
#include "Http/LiteClient.h"
#include "Log/Log.h"
#include "RunLoop/CompositeHandler.h"

#include <exec/create.hpp>

/// Returns a sender that performs a single HTTP GET.
/// The callback fires directly from AsioPoller::Update() → io_context.poll(), resuming the
/// awaiting coroutine with the Result — no polling loops, no extra frames.
static auto HttpGet(std::shared_ptr<Http::ILiteClient> client, std::string url)
{
    return exec::create<stdexec::set_value_t(Http::ILiteClient::Result)>(
        [](auto& ctx) noexcept {
            auto& [client, url] = ctx.args;
            client->Get(url, [&ctx](Http::ILiteClient::Result r) {
                stdexec::set_value(std::move(ctx.receiver), std::move(r));
            });
        },
        std::move(client), std::move(url));
}

static Exec::RunTask<int> ExecMain(std::shared_ptr<Http::ILiteClient> client)
{
    auto result = co_await HttpGet(std::move(client), "http://httpbun.com/get");
    if (result) {
        Log::Info("HTTP {}\n{}", result->statusCode, result->body);
    } else {
        Log::Error("HTTP failed: {}", result.error().what());
        co_return 1;
    }
    co_return 0;
}

int main(const int argc, const char* argv[])
{
    Boot::LogHeader({argc, argv});

    // AsioPoller drives the io_context each frame. Its executor is only used to create the HTTP client —
    // ExecMain and HttpGet are pure exec and know nothing about asio.
    Asio::AsioPoller asioPoller;
    const auto client = Http::LiteClient::MakeDefault({.executor = asioPoller.GetExecutor()});

    auto execDomain = std::make_shared<Exec::Domain>(ExecMain(client));

    auto composite = std::make_shared<RunLoop::CompositeHandler>();
    composite->Add(asioPoller);
    composite->Add(*execDomain);

    return App::CreateDefaultRunner(composite)->Run();
}
