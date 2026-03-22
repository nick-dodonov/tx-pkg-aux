#include "App/Capy/Domain.h"
#include "App/Factory.h"
#include "Boot/Boot.h"
#include "Log/Scope.h"

#include <boost/capy/delay.hpp>

using namespace std::literals;

static Coro::Task<> FooAsync()
{
    auto _ = Log::Scope{};

    co_await boost::capy::delay(300ms);
}

static Coro::Task<int> MainAsync(int exitCode)
{
    auto _ = Log::Scope{};

    co_await FooAsync();

    Log::Info("exiting with code: {}", exitCode);
    co_return exitCode;
}

int main(const int argc, const char* argv[])
{
    auto args = Boot::DefaultInit(argc, argv);
    auto exitCode = args.GetIntArg(1).value_or(0);
    
    auto domain = std::make_shared<Coro::Domain>(MainAsync(exitCode));
    auto runner = App::Loop::CreateDefaultRunner(domain);
    return runner->Run();
}
