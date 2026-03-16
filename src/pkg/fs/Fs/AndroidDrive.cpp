#ifdef __ANDROID__
#include "AndroidDrive.h"
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

    Coro::Task<Drive::ReadAllBytesResult> AndroidDrive::ReadAllBytesAsync(Path path)
    {
        if (!_assetManager) {
            co_return std::unexpected(std::make_error_code(std::errc::not_supported));
        }

        // Open asset from APK
        std::string assetPath = path.generic_string();
        AAsset* asset = AAssetManager_open(_assetManager, assetPath.c_str(), AASSET_MODE_BUFFER);
        if (!asset) {
            Log::Trace("AndroidDrive: Failed to open asset: {}", assetPath);
            co_return std::unexpected(std::make_error_code(std::errc::no_such_file_or_directory));
        }

        // Get asset size
        off_t assetSize = AAsset_getLength(asset);
        if (assetSize < 0) {
            AAsset_close(asset);
            co_return std::unexpected(std::make_error_code(std::errc::io_error));
        }

        // Read asset data
        std::vector<uint8_t> buffer(static_cast<size_t>(assetSize));
        int bytesRead = AAsset_read(asset, buffer.data(), static_cast<size_t>(assetSize));
        AAsset_close(asset);

        if (bytesRead != assetSize) {
            Log::Error("AndroidDrive: Failed to read complete asset: {} (expected {}, got {})", 
                assetPath, assetSize, bytesRead);
            co_return std::unexpected(std::make_error_code(std::errc::io_error));
        }

        Log::Trace("AndroidDrive: Successfully read {} bytes from {}", bytesRead, assetPath);
        co_return buffer;
    }
}

#endif // __ANDROID__
