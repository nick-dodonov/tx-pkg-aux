#include "System.h"
#include "NativeDrive.h"
#include "Boot/Boot.h"
#include <filesystem>

namespace Fs
{
    Drive& System::GetDefaultDrive()
    {
        static auto drive = []() {
            std::vector<std::string> prefixes;
            prefixes.push_back(std::filesystem::current_path().string());

            const auto& args = Boot::GetDefaultArgs();
            if (args.size() >= 1) {
                auto exePath = std::filesystem::path(args[0]);
                auto exeDir = exePath.parent_path(); // remove executable name to get directory
                prefixes.push_back(exeDir.string());
            }

            return NativeDrive(std::move(prefixes));
        }();

        return drive;
    }
}
