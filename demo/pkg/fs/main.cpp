#include "Boot/Boot.h"
#include "Log/Scope.h"
#include "Fs/System.h"
#include "Fs/Drive.h"

#include <cstdlib>
#include <vector>

int main(const int argc, const char* argv[])
{
    Boot::DefaultInit(argc, argv);

    auto _ = Log::Scope{};

    auto& drive = Fs::System::GetDefaultDrive();

    auto sizeResult = drive.GetSize("test_data/test.txt");
    if (!sizeResult) {
        Log::Error("Failed to get file size: {}", sizeResult.error().message());
        return EXIT_FAILURE;
    }

    std::vector<uint8_t> buf(*sizeResult);
    auto readResult = drive.ReadAllTo("test_data/test.txt", buf);
    if (!readResult) {
        Log::Error("Failed to read file: {}", readResult.error().message());
        return EXIT_FAILURE;
    }

    buf.resize(*readResult);
    std::string_view content(reinterpret_cast<const char*>(buf.data()), buf.size()); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    Log::Info("Content:\n{}", content);

    return EXIT_SUCCESS;
}
