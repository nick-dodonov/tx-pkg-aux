#include "NativeDrive.h"
#include <filesystem>

namespace Fs
{
    NativeDrive::NativeDrive(std::string_view prefixPath)
        : _prefixPath(prefixPath)
    {}

    Drive::PathResult NativeDrive::GetNativePath(std::string_view path)
    {
        namespace fs = std::filesystem;

        fs::path prefix(_prefixPath);
        fs::path relativePath(path);
        fs::path fullPath = prefix / relativePath;

        return fullPath.string();
    }
}
