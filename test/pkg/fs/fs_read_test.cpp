#include "App/Loop/Factory.h"
#include "Coro/Domain.h"
#include "Fs/NativeDrive.h"
#include "Fs/OverlayDrive.h"

#include <boost/capy/read.hpp>
#include <boost/capy/buffers/string_dynamic_buffer.hpp>

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

class FsReadFixture : public ::testing::Test
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

static int RunCoroTest(Coro::Task<int> task)
{
    auto domain = std::make_shared<Coro::Domain>(std::move(task));
    auto runner = App::Loop::CreateTestRunner(domain);
    return runner->Run();
}

TEST_F(FsReadFixture, ReadAllAsyncDefaultBytes)
{
    auto dir = testDir1;
    auto exitCode = RunCoroTest([](std::filesystem::path dir) -> Coro::Task<int> {
        Fs::NativeDrive drive({dir});
        auto result = co_await drive.ReadAllAsync("file.txt");
        EXPECT_TRUE(result.has_value());
        if (!result) co_return 1;

        const auto& bytes = result.value();
        std::string_view content(reinterpret_cast<const char*>(bytes.data()), bytes.size());  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        EXPECT_EQ(content, "test1");
        co_return 0;
    }(dir));
    EXPECT_EQ(exitCode, 0);
}

TEST_F(FsReadFixture, ReadAllAsyncString)
{
    auto dir = testDir1;
    auto exitCode = RunCoroTest([](std::filesystem::path dir) -> Coro::Task<int> {
        Fs::NativeDrive drive({dir});
        auto result = co_await drive.ReadAllAsync<std::string>("file.txt");
        EXPECT_TRUE(result.has_value());
        if (!result) co_return 1;

        EXPECT_EQ(result.value(), "test1");
        co_return 0;
    }(dir));
    EXPECT_EQ(exitCode, 0);
}

TEST_F(FsReadFixture, OpenAsyncAndRead)
{
    auto dir = testDir1;
    auto exitCode = RunCoroTest([](std::filesystem::path dir) -> Coro::Task<int> {
        Fs::NativeDrive drive({dir});
        auto result = co_await drive.OpenAsync("file.txt");
        EXPECT_TRUE(result.has_value());
        if (!result) co_return 1;

        auto& source = result.value();
        std::string content;
        auto readResult = co_await boost::capy::read(source, boost::capy::string_dynamic_buffer(&content));
        auto [ec, bytesRead] = readResult;
        EXPECT_GT(bytesRead, 0u);
        EXPECT_EQ(content, "test1");
        co_return 0;
    }(dir));
    EXPECT_EQ(exitCode, 0);
}

TEST_F(FsReadFixture, ReadAllAsyncFileNotFound)
{
    auto dir = testDir1;
    auto exitCode = RunCoroTest([](std::filesystem::path dir) -> Coro::Task<int> {
        Fs::NativeDrive drive({dir});
        auto result = co_await drive.ReadAllAsync("nonexistent.txt");
        EXPECT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), std::make_error_code(std::errc::no_such_file_or_directory));
        co_return 0;
    }(dir));
    EXPECT_EQ(exitCode, 0);
}

TEST_F(FsReadFixture, OverlayDriveReadAllAsync)
{
    auto dir1 = testDir1;
    auto dir2 = testDir2;
    auto exitCode = RunCoroTest([](std::filesystem::path dir1, std::filesystem::path dir2) -> Coro::Task<int> {
        Fs::NativeDrive drive1({dir1});
        Fs::NativeDrive drive2({dir2});
        Fs::OverlayDrive overlay({&drive1, &drive2});

        auto result = co_await overlay.ReadAllAsync<std::string>("file.txt");
        EXPECT_TRUE(result.has_value());
        if (!result) co_return 1;

        // Should read from first drive
        EXPECT_EQ(result.value(), "test1");
        co_return 0;
    }(dir1, dir2));
    EXPECT_EQ(exitCode, 0);
}
