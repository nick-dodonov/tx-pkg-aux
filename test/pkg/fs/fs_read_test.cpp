#include "Coro/CapyDomain.h"
#include "App/Factory.h"
#include "Fs/NativeDrive.h"
#include "Fs/OverlayDrive.h"

#include <boost/capy/buffers/string_dynamic_buffer.hpp>
#include <boost/capy/read.hpp>

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

/// CO_ASSERT_TRUE equivalent for coroutine bodies (ASSERT_TRUE uses return, incompatible with co_return).
#define CO_ASSERT_TRUE(expr) \
    do { \
        EXPECT_TRUE(expr); \
        if (!(expr)) co_return; \
    } while (false)

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

static void RunCoroTest(Coro::Task<> task)
{
    auto wrapper = [](Coro::Task<> t) -> Coro::Task<int> {
        co_await std::move(t);
        co_return 0;
    };
    auto domain = std::make_shared<Coro::CapyDomain>(wrapper(std::move(task)));
    auto runner = App::CreateTestRunner(domain);
    runner->Run();
}

TEST_F(FsReadFixture, ReadAllAsyncDefaultBytes)
{
    RunCoroTest([this]() -> Coro::Task<> {
        Fs::NativeDrive drive({testDir1});
        auto result = co_await drive.ReadAllAsync("file.txt");
        CO_ASSERT_TRUE(result.has_value());

        const auto& bytes = result.value();
        std::string_view content(reinterpret_cast<const char*>(bytes.data()), bytes.size()); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        EXPECT_EQ(content, "test1");
    }());
}

TEST_F(FsReadFixture, ReadAllAsyncString)
{
    RunCoroTest([this]() -> Coro::Task<> {
        Fs::NativeDrive drive({testDir1});
        auto result = co_await drive.ReadAllAsync<std::string>("file.txt");
        CO_ASSERT_TRUE(result.has_value());

        EXPECT_EQ(result.value(), "test1");
    }());
}

TEST_F(FsReadFixture, OpenAsyncAndRead)
{
    RunCoroTest([this]() -> Coro::Task<> {
        Fs::NativeDrive drive({testDir1});
        auto result = co_await drive.OpenAsync("file.txt");
        CO_ASSERT_TRUE(result.has_value());

        auto& source = result.value();
        std::string content;
        auto readResult = co_await boost::capy::read(source, boost::capy::string_dynamic_buffer(&content));
        auto [ec, bytesRead] = readResult;
        EXPECT_GT(bytesRead, 0u);
        EXPECT_EQ(content, "test1");
    }());
}

TEST_F(FsReadFixture, ReadAllAsyncFileNotFound)
{
    RunCoroTest([this]() -> Coro::Task<> {
        Fs::NativeDrive drive({testDir1});
        auto result = co_await drive.ReadAllAsync("nonexistent.txt");
        EXPECT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), std::make_error_code(std::errc::no_such_file_or_directory));
    }());
}

TEST_F(FsReadFixture, OverlayDriveReadAllAsync)
{
    RunCoroTest([this]() -> Coro::Task<> {
        Fs::NativeDrive drive1({testDir1});
        Fs::NativeDrive drive2({testDir2});
        Fs::OverlayDrive overlay({&drive1, &drive2});

        auto result = co_await overlay.ReadAllAsync<std::string>("file.txt");
        CO_ASSERT_TRUE(result.has_value());

        EXPECT_EQ(result.value(), "test1");
    }());
}
