#include "App/Loop/Factory.h"
#include "Boot/Boot.h"
#include "Coro/Domain.h"
#include "Log/Scope.h"
#include "Fs/System.h"
#include "Fs/Drive.h"

using namespace std::literals;

static Coro::Task<int> MainAsync()
{
    auto _ = Log::Scope{};

    auto& drive = Fs::System::GetDefaultDrive();

    auto result = co_await drive.ReadAllBytesAsync("test_data/test.txt");
    if (!result) {
        Log::Error("Failed to read file: {}", result.error().message());
        co_return 1;
    }

    const auto& bytes = result.value();
    std::string_view content(reinterpret_cast<const char*>(bytes.data()), bytes.size()); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
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
