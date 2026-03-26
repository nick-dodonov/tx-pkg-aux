#include "Fs/NativeDrive.h"
#include "Fs/OverlayDrive.h"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

class FsReadFixture: public ::testing::Test
{
protected:
    void SetUp() override
    {
        testDir1 = std::filesystem::temp_directory_path() / "fs_read_test_1";
        testDir2 = std::filesystem::temp_directory_path() / "fs_read_test_2";

        std::filesystem::create_directories(testDir1);
        std::filesystem::create_directories(testDir2);

        std::ofstream(testDir1 / "file.txt") << "test1";
        std::ofstream(testDir2 / "file.txt") << "test2";
    }

    void TearDown() override
    {
        std::filesystem::remove_all(testDir1);
        std::filesystem::remove_all(testDir2);
    }

    std::filesystem::path testDir1;
    std::filesystem::path testDir2;
};

TEST_F(FsReadFixture, ReadAllToBytes)
{
    Fs::NativeDrive drive(testDir1);

    auto sizeResult = drive.GetSize("file.txt");
    ASSERT_TRUE(sizeResult.has_value());

    std::vector<uint8_t> buf(*sizeResult);
    auto readResult = drive.ReadAllTo("file.txt", buf);
    ASSERT_TRUE(readResult.has_value());

    std::string_view content(reinterpret_cast<const char*>(buf.data()), *readResult); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    EXPECT_EQ(content, "test1");
}

TEST_F(FsReadFixture, ReadAllToString)
{
    Fs::NativeDrive drive(testDir1);

    auto sizeResult = drive.GetSize("file.txt");
    ASSERT_TRUE(sizeResult.has_value());

    std::vector<uint8_t> buf(*sizeResult);
    auto readResult = drive.ReadAllTo("file.txt", buf);
    ASSERT_TRUE(readResult.has_value());

    buf.resize(*readResult);
    std::string content(reinterpret_cast<const char*>(buf.data()), buf.size()); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    EXPECT_EQ(content, "test1");
}

TEST_F(FsReadFixture, ReadAllToFileNotFound)
{
    Fs::NativeDrive drive(testDir1);

    auto sizeResult = drive.GetSize("nonexistent.txt");
    EXPECT_FALSE(sizeResult.has_value());
    EXPECT_EQ(sizeResult.error(), std::make_error_code(std::errc::no_such_file_or_directory));
}

TEST_F(FsReadFixture, OverlayDriveReadAllTo)
{
    auto drive1 = Fs::NativeDrive::Make(testDir1);
    auto drive2 = Fs::NativeDrive::Make(testDir2);
    Fs::OverlayDrive overlay(drive1, drive2);

    auto sizeResult = overlay.GetSize("file.txt");
    ASSERT_TRUE(sizeResult.has_value());

    std::vector<uint8_t> buf(*sizeResult);
    auto readResult = overlay.ReadAllTo("file.txt", buf);
    ASSERT_TRUE(readResult.has_value());

    std::string_view content(reinterpret_cast<const char*>(buf.data()), *readResult); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    EXPECT_EQ(content, "test1");
}
