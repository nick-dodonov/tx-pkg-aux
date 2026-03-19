#pragma once
#ifdef __ANDROID__
#include "Drive.h"

// Forward declarations for Android types
struct AAssetManager;

namespace Fs
{
    /// Drive implementation for reading files from Android assets
    /// Assets are packaged in the APK and accessed via AAssetManager API
    class AndroidDrive: public Drive
    {
    public:
        /// Create AndroidDrive from AAssetManager pointer directly
        /// The asset manager pointer must remain valid for the lifetime of this drive
        explicit AndroidDrive(AAssetManager* assetManager);

        [[nodiscard]] PathResult GetNativePath(const Path& path) override;
        [[nodiscard]] SizeResult GetSize(const Path& path) override;
        [[nodiscard]] ReadResult ReadAllTo(const Path& path, boost::capy::mutable_buffer buf) override;
        [[nodiscard]] Coro::Task<OpenResult> OpenAsync(Path path) override;

    private:
        AAssetManager* _assetManager = nullptr;
    };
}

#endif // __ANDROID__
