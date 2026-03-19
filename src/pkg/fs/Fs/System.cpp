#include "System.h"
#include "NativeDrive.h"
#include "Boot/Boot.h"
#include <filesystem>

#ifdef __ANDROID__
#include "AndroidDrive.h"
#include "OverlayDrive.h"
#include "Droid/Glue.h"
#endif

namespace Fs
{
    Drive& System::GetDefaultDrive()
    {
#ifdef __ANDROID__
        static OverlayDrive drive = []() {
            std::vector<Path> prefixes;
            prefixes.push_back(std::filesystem::current_path());

            const auto& args = Boot::GetDefaultArgs();
            if (args.size() >= 1) {
                auto exePath = Path(args[0]);
                prefixes.push_back(exePath.parent_path());
            }

            static NativeDrive nativeDrive(std::move(prefixes));
            static AndroidDrive androidDrive(Droid::Glue::Instance().GetAssetManager());

            // Try Android assets first, then fall back to native filesystem
            return OverlayDrive({&androidDrive, &nativeDrive});
        }();
#else
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
#endif

        return drive;
    }
}
