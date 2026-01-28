#include "Fs/NativeDrive.h"
#include "Fs/RunfilesDrive.h"
#include "Fs/System.h"
#include "Log/Log.h"
#include <filesystem>
#include <gtest/gtest.h>

TEST(FsTest, DriveOverlay)
{
    Fs::NativeDrive drive("/prefix");

    auto result = drive.GetNativePath("subdir/file.txt");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "/prefix/subdir/file.txt");
}

TEST(FsTest, SystemDefaultDrive)
{
    auto& drive = Fs::System::GetDefaultDrive();

    auto result = drive.GetNativePath("test.txt");
    ASSERT_TRUE(result.has_value());

    auto expectedPath = std::filesystem::current_path() / "test.txt";
    EXPECT_EQ(result.value(), expectedPath.string());
}

TEST(FsTest, RunfilesDrive)
{
    Fs::RunfilesDrive drive("tx-pkg-aux");

#ifdef __EMSCRIPTEN__
    // Runfiles are not supported on WASM platform
    EXPECT_FALSE(drive.IsSupported());
#else
    // Desktop platforms should support runfiles
    EXPECT_TRUE(drive.IsSupported());

    auto result = drive.GetNativePath("data/test.txt");
    if (result.has_value()) {
        Log::Debug("Native path success: {}", result.value());
    } else {
        Log::Debug("Native path error: {}", result.error().message());
    }
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result.value().empty());
#endif
}
