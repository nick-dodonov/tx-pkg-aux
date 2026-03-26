#ifdef __ANDROID__
#include "AndroidDrive.h"
#include "Log/Log.h"

#include <android/asset_manager.h>
#include <android/native_activity.h>

namespace Fs
{
    AndroidDrive::AndroidDrive(AAssetManager* assetManager)
        : _assetManager(assetManager)
    {
        if (!_assetManager) {
            Log::Error("AndroidDrive: AAssetManager is null");
        }
    }

    Drive::PathResult AndroidDrive::GetNativePath(const Path& path)
    {
        // Android assets cannot be accessed as native filesystem paths
        // They exist only within the APK and must be accessed via AAssetManager
        // Return not_supported to indicate this path cannot be converted to native
        return std::unexpected(std::make_error_code(std::errc::not_supported));
    }

    Drive::SizeResult AndroidDrive::GetSize(const Path& path)
    {
        if (!_assetManager) {
            return std::unexpected(std::make_error_code(std::errc::not_supported));
        }

        std::string assetPath = path.generic_string();
        AAsset* asset = AAssetManager_open(_assetManager, assetPath.c_str(), AASSET_MODE_UNKNOWN);
        if (!asset) {
            return std::unexpected(std::make_error_code(std::errc::no_such_file_or_directory));
        }

        auto length = static_cast<size_t>(AAsset_getLength(asset));
        AAsset_close(asset);
        return length;
    }

    Drive::ReadResult AndroidDrive::ReadAllTo(const Path& path, std::vector<uint8_t>& buf)
    {
        if (!_assetManager) {
            return std::unexpected(std::make_error_code(std::errc::not_supported));
        }

        std::string assetPath = path.generic_string();
        AAsset* asset = AAssetManager_open(_assetManager, assetPath.c_str(), AASSET_MODE_BUFFER);
        if (!asset) {
            return std::unexpected(std::make_error_code(std::errc::no_such_file_or_directory));
        }

        auto bytesRead = AAsset_read(asset, buf.data(), buf.size());
        AAsset_close(asset);

        if (bytesRead < 0) {
            return std::unexpected(std::make_error_code(std::errc::io_error));
        }
        return static_cast<size_t>(bytesRead);
    }
}

#endif // __ANDROID__
