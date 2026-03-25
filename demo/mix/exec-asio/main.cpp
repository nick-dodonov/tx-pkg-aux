#include "App/Factory.h"
#include "Asio/AsioPoller.h"
#include "Boot/Boot.h"
#include "Exec/Domain.h"
#include "Exec/RunTask.h"
#include "Log/Log.h"
#include "Log/Scope.h"
#include "RunLoop/CompositeHandler.h"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <exec/asio/completion_token.hpp>

namespace asio = boost::asio;

static asio::awaitable<std::string> AsioSub(int input)
{
    auto _ = Log::Scope{"({})", input};
    auto timer = asio::steady_timer(co_await asio::this_coro::executor);
    timer.expires_after(std::chrono::milliseconds(50));
    co_await timer.async_wait(asio::use_awaitable);
    co_return "sync result " + std::to_string(input);
}

static exec::task<void> ExecSub()
{
    auto _ = Log::Scope{};
    const auto sched = co_await stdexec::read_env(stdexec::get_scheduler);
    co_return;
}

static Exec::RunTask<int> ExecMain(asio::io_context::executor_type asioExec)
{
    auto _ = Log::Scope{};
    co_await ExecSub();

    asio::steady_timer timer{asioExec};
    timer.expires_after(std::chrono::milliseconds(50));
    const auto ec = co_await timer.async_wait(exec::asio::completion_token);
    Log::Info("Timer fired, ec={}", ec.what());

    // co_spawn completion signature is void(exception_ptr, string), so completion_token
    // delivers both args as set_value -> co_await yields tuple<exception_ptr, string>.
    auto [exc, result] = co_await asio::co_spawn(asioExec, AsioSub(22), exec::asio::completion_token);
    if (exc) {
        std::rethrow_exception(exc);
    }
    Log::Info("AsioSub result: {}", result);

    co_return 33;
}

int main(const int argc, const char* argv[])
{
    Boot::LogHeader({argc, argv});

    // CompositeHandler drives both the asio poller and the exec domain in a single runner loop — no extra threads needed.
    Asio::AsioPoller asioPoller;
    auto execDomain = std::make_shared<Exec::Domain>(ExecMain(asioPoller.GetExecutor()));

    auto composite = std::make_shared<RunLoop::CompositeHandler>();
    composite->Add(asioPoller);
    composite->Add(*execDomain);

    return App::CreateDefaultRunner(composite)->Run();
}
