#include "Fs/NativeDrive.h"
#include "Fs/System.h"
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
