#include "System.h"
#include "NativeDrive.h"
#include <filesystem>

namespace Fs
{
    Drive& System::GetDefaultDrive()
    {
        static NativeDrive defaultDrive(std::filesystem::current_path().string());
        return defaultDrive;
    }
}
