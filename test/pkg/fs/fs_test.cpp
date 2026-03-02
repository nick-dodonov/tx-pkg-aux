#include "Fs/NativeDrive.h"
#include "Fs/OverlayDrive.h"
#include "Fs/RunfilesDrive.h"
#include "Fs/System.h"
#include "Log/Log.h"
#include <filesystem>
#include <gtest/gtest.h>

TEST(FsTest, DriveOverlay)
{
    Fs::NativeDrive drive("/prefix");

    const auto result = drive.GetNativePath("subdir/file.txt");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "/prefix/subdir/file.txt");
}

TEST(FsTest, SystemDefaultDrive)
{
    auto& drive = Fs::System::GetDefaultDrive();

    const auto result = drive.GetNativePath("test.txt");
    ASSERT_TRUE(result.has_value());

    const auto expectedPath = std::filesystem::current_path() / "test.txt";
    EXPECT_EQ(result.value(), expectedPath.generic_string());
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

TEST(FsTest, OverlayDrive)
{
    Fs::NativeDrive drive1("/first");
    Fs::NativeDrive drive2("/second");
    Fs::NativeDrive drive3("/third");

    Fs::OverlayDrive overlay({&drive1, &drive2, &drive3});

    const auto result = overlay.GetNativePath("file.txt");
    ASSERT_TRUE(result.has_value());
    // Should use first drive
    EXPECT_EQ(result.value(), "/first/file.txt");
}

TEST(FsTest, OverlayDriveWithRunfiles)
{
    Fs::RunfilesDrive runfiles("tx-pkg-aux");
    Fs::NativeDrive fallback("/fallback");

    Fs::OverlayDrive overlay({&runfiles, &fallback});

#ifdef __EMSCRIPTEN__
    // On WASM, runfiles not supported, should fall back to native drive
    auto result = overlay.GetNativePath("test.txt");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "/fallback/test.txt");
#else
    // On desktop, should use runfiles if available
    if (runfiles.IsSupported())
    {
        auto result = overlay.GetNativePath("data/test.txt");
        ASSERT_TRUE(result.has_value());
        EXPECT_FALSE(result.value().empty());
        EXPECT_NE(result.value(), "/fallback/data/test.txt");
    }
#endif
}
