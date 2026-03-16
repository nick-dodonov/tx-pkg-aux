#include "System.h"
#include "NativeDrive.h"
#include "Boot/Boot.h"
#include <filesystem>

namespace Fs
{
    Drive& System::GetDefaultDrive()
    {
        static auto drive = []() {
            std::vector<Path> prefixes;
            prefixes.push_back(std::filesystem::current_path());

            const auto& args = Boot::GetDefaultArgs();
            if (args.size() >= 1) {
                auto exePath = Path(args[0]);
                prefixes.push_back(exePath.parent_path());
            }

            return NativeDrive(std::move(prefixes));
        }();

        return drive;
    }
}
