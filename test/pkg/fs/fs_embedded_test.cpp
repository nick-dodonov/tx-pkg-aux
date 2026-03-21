#include "App/Capy/Domain.h"
#include "App/Loop/Factory.h"
#include "Fs/Drive.h"
#include "Fs/System.h"
#include "Log/Log.h"

#include <boost/capy/buffers/string_dynamic_buffer.hpp>
#include <boost/capy/read.hpp>

#include <gtest/gtest.h>

/// ASSERT_TRUE equivalent for coroutine bodies (ASSERT_TRUE uses return, incompatible with co_return).
#define CO_ASSERT_TRUE(expr) \
    do { \
        EXPECT_TRUE(expr); \
        if (!(expr)) co_return; \
    } while (false)

static void RunCoroTest(Coro::Task<> task)
{
    auto wrapper = [](Coro::Task<> t) -> Coro::Task<int> {
        co_await std::move(t);
        co_return 0;
    };
    auto domain = std::make_shared<Coro::Domain>(wrapper(std::move(task)));
    auto runner = App::Loop::CreateTestRunner(domain);
    runner->Run();
}

class FsEmbeddedFixture : public ::testing::Test
{
protected:
    static constexpr auto TestFilePath = "test_data/test.txt";
    static constexpr auto ExpectedContent = "test content\n";
};

TEST_F(FsEmbeddedFixture, GetSize)
{
    auto& drive = Fs::System::GetDefaultDrive();
    auto result = drive.GetSize(TestFilePath);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), std::string_view(ExpectedContent).size());
}

TEST_F(FsEmbeddedFixture, ReadAllTo)
{
    auto& drive = Fs::System::GetDefaultDrive();

    auto sizeResult = drive.GetSize(TestFilePath);
    ASSERT_TRUE(sizeResult.has_value());

    std::vector<uint8_t> buf(*sizeResult);
    auto readResult = drive.ReadAllTo(TestFilePath, {buf.data(), buf.size()});
    ASSERT_TRUE(readResult.has_value());

    std::string_view content(reinterpret_cast<const char*>(buf.data()), *readResult);
    EXPECT_EQ(content, ExpectedContent);
}

TEST_F(FsEmbeddedFixture, ReadAllAsyncString)
{
    RunCoroTest([]() -> Coro::Task<> {
        auto& drive = Fs::System::GetDefaultDrive();
        auto result = co_await drive.ReadAllAsync<std::string>(TestFilePath);
        CO_ASSERT_TRUE(result.has_value());
        Log::Trace("{}:\n{}", TestFilePath, result.value());
        EXPECT_EQ(result.value(), ExpectedContent);
    }());
}

TEST_F(FsEmbeddedFixture, OpenAsyncAndRead)
{
    RunCoroTest([]() -> Coro::Task<> {
        auto& drive = Fs::System::GetDefaultDrive();
        auto openResult = co_await drive.OpenAsync(TestFilePath);
        CO_ASSERT_TRUE(openResult.has_value());

        auto& source = openResult.value();
        std::string content;
        auto [ec, bytesRead] = co_await boost::capy::read(source, boost::capy::string_dynamic_buffer(&content));
        EXPECT_FALSE(ec);
        EXPECT_GT(bytesRead, 0u);
        Log::Trace("{}:\n{}", TestFilePath, content);
        EXPECT_EQ(content, ExpectedContent);
    }());
}
