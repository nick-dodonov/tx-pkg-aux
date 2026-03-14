#include "Fs/NativeDrive.h"
#include "Fs/OverlayDrive.h"
#include "Fs/RunfilesDrive.h"
#include "Fs/System.h"
#include "Log/Log.h"
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

class FsTestFixture : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Create temporary test directories
        testDir1 = std::filesystem::temp_directory_path() / "fs_test_1";
        testDir2 = std::filesystem::temp_directory_path() / "fs_test_2";
        testDir3 = std::filesystem::temp_directory_path() / "fs_test_3";

        std::filesystem::create_directories(testDir1);
        std::filesystem::create_directories(testDir2);
        std::filesystem::create_directories(testDir3);

        // Create test files
        testFile1 = testDir1 / "file.txt";
        testFile2 = testDir2 / "file.txt";
        
        std::ofstream(testFile1) << "test1";
        std::ofstream(testFile2) << "test2";
        
        // Create a file only in first directory for priority testing
        testFile1Only = testDir1 / "only_in_first.txt";
        std::ofstream(testFile1Only) << "only_first";
    }

    void TearDown() override
    {
        std::filesystem::remove_all(testDir1);
        std::filesystem::remove_all(testDir2);
        std::filesystem::remove_all(testDir3);
    }

    std::filesystem::path testDir1;
    std::filesystem::path testDir2;
    std::filesystem::path testDir3;
    std::filesystem::path testFile1;
    std::filesystem::path testFile2;
    std::filesystem::path testFile1Only;
};

TEST_F(FsTestFixture, DriveOverlay)
{
    Fs::NativeDrive drive({testDir1});

    const auto result = drive.GetNativePath("file.txt");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), testFile1);
}

TEST_F(FsTestFixture, SystemDefaultDrive)
{
    auto& drive = Fs::System::GetDefaultDrive();

    // Test with absolute path that we know exists (created in test setup)
    const auto result = drive.GetNativePath(testFile1);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), testFile1);
}

TEST(FsTest, RunfilesDrive)
{
    Fs::RunfilesDrive drive("tx-pkg-aux");

#if defined(__EMSCRIPTEN__) || defined(__ANDROID__)
    // Runfiles are not supported on WASM and Android platforms
    EXPECT_FALSE(drive.IsSupported());
#else
    // Desktop platforms should support runfiles
    EXPECT_TRUE(drive.IsSupported());

    auto result = drive.GetNativePath("test/pkg/fs/data/test.txt");
    if (result.has_value()) {
        Log::Debug("Native path success: {}", result.value().string());
    } else {
        Log::Debug("Native path error: {}", result.error().message());
    }
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result.value().empty());
#endif
}

TEST_F(FsTestFixture, OverlayDrive)
{
    Fs::NativeDrive drive1({testDir1});
    Fs::NativeDrive drive2({testDir2});
    Fs::NativeDrive drive3({testDir3});

    Fs::OverlayDrive overlay({&drive1, &drive2, &drive3});

    const auto result = overlay.GetNativePath("file.txt");
    ASSERT_TRUE(result.has_value());
    // Should use first drive
    EXPECT_EQ(result.value(), testFile1);
}

TEST_F(FsTestFixture, OverlayDriveWithRunfiles)
{
    Fs::RunfilesDrive runfiles("tx-pkg-aux");
    Fs::NativeDrive fallback({testDir1});

    Fs::OverlayDrive overlay({&runfiles, &fallback});

#if defined(__EMSCRIPTEN__) || defined(__ANDROID__)
    // On WASM and Android, runfiles not supported, should fall back to native drive
    auto result = overlay.GetNativePath("file.txt");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), testFile1);
#else
    // On desktop, should use runfiles if available
    if (runfiles.IsSupported())
    {
        auto result = overlay.GetNativePath("test/pkg/fs/data/test.txt");
        ASSERT_TRUE(result.has_value());
        EXPECT_FALSE(result.value().empty());
        EXPECT_NE(result.value(), testFile1);
    }
#endif
}

TEST_F(FsTestFixture, NativeDriveMultiplePrefixes)
{
    Fs::NativeDrive drive({testDir1, testDir2, testDir3});

    // Should search through all prefixes and return first match
    const auto result = drive.GetNativePath("file.txt");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), testFile1);

    // Test file that exists only in first directory
    const auto result2 = drive.GetNativePath("only_in_first.txt");
    ASSERT_TRUE(result2.has_value());
    EXPECT_EQ(result2.value(), testFile1Only);
}
