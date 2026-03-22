#include "App/Capy/Domain.h"
#include "App/Factory.h"
#include "Boot/Boot.h"
#include "Log/Scope.h"
#include "Fs/System.h"
#include "Fs/Drive.h"

using namespace std::literals;

static Coro::Task<int> MainAsync()
{
    auto _ = Log::Scope{};

    auto& drive = Fs::System::GetDefaultDrive();

    auto result = co_await drive.ReadAllAsync<std::string>("test_data/test.txt");
    if (!result) {
        Log::Error("Failed to read file: {}", result.error().message());
        co_return 1;
    }

    const auto& content = result.value();
    Log::Info("Content:\n{}", content);

    co_return 0;
}

int main(const int argc, const char* argv[])
{
    Boot::DefaultInit(argc, argv);
    
    auto domain = std::make_shared<Coro::Domain>(MainAsync());
    auto runner = App::Loop::CreateDefaultRunner(domain);
    return runner->Run();
}
