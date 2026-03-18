#ifdef __ANDROID__
#include "AndroidDrive.h"
#include "AndroidAssetSource.h"
#include "Log/Log.h"

#include <android/asset_manager.h>
#include <android/native_activity.h>

namespace Fs
{
    AndroidDrive::AndroidDrive(ANativeActivity* activity)
        : _assetManager(activity ? activity->assetManager : nullptr)
    {
        if (!_assetManager) {
            Log::Error("AndroidDrive: ANativeActivity has no assetManager");
        }
    }

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

    Coro::Task<Drive::OpenResult> AndroidDrive::OpenAsync(Path path)
    {
        if (!_assetManager) {
            co_return std::unexpected(std::make_error_code(std::errc::not_supported));
        }

        std::string assetPath = path.generic_string();
        AAsset* asset = AAssetManager_open(_assetManager, assetPath.c_str(), AASSET_MODE_STREAMING);
        if (!asset) {
            Log::Trace("AndroidDrive: Failed to open asset: {}", assetPath);
            co_return std::unexpected(std::make_error_code(std::errc::no_such_file_or_directory));
        }

        AndroidAssetSource source(asset);
        co_return boost::capy::any_read_source(std::move(source));
    }
}

#endif // __ANDROID__
