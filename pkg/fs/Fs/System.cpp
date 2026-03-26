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
        static auto drive = MakeDefaultDrive();
        return *drive;
    }

    std::shared_ptr<Drive> System::MakeDefaultDrive()
    {
        std::vector<Path> prefixes;
        prefixes.emplace_back(std::filesystem::current_path());
        const auto& args = Boot::GetDefaultArgs();
        if (!args.empty()) {
            const auto exePath = Path(args[0]);
            prefixes.emplace_back(exePath.parent_path());
        }
#ifdef __ANDROID__
        auto nativeDrive = std::make_shared<NativeDrive>(std::move(prefixes));
        auto androidDrive = std::make_shared<AndroidDrive>(Droid::Glue::Instance().GetAssetManager());

        // Try Android assets first, then fall back to native filesystem
        return std::make_shared<OverlayDrive>(androidDrive, nativeDrive);
#else
        return NativeDrive::Make(std::move(prefixes));
#endif
    }
}
