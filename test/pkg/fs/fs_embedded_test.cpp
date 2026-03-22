#include "Fs/Drive.h"
#include "Fs/System.h"
#include "Log/Log.h"

#include <gtest/gtest.h>

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
    auto readResult = drive.ReadAllTo(TestFilePath, buf);
    ASSERT_TRUE(readResult.has_value());

    std::string_view content(reinterpret_cast<const char*>(buf.data()), *readResult); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    Log::Trace("{}:\n{}", TestFilePath, content);
    EXPECT_EQ(content, ExpectedContent);
}
