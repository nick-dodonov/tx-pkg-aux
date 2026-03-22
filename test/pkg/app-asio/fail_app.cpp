#include "App/AsioDomain.h"
#include "App/Factory.h"
#include "Boot/Boot.h"

#include <gtest/gtest.h>

TEST(FailTest, AssertFail)
{
    ASSERT_TRUE(false);
}

TEST(FailTest, CoroFail)
{
    auto domain = std::make_shared<App::AsioDomain>();
    auto runner = App::CreateTestRunner(domain);

    auto coroMain = []() -> boost::asio::awaitable<int> { 
        co_return 11;
    };
    int exitCode = domain->RunCoroMain(runner, coroMain());
    EXPECT_EQ(exitCode, 0);
}

int main(int argc, char** argv)
{
    Boot::DefaultInit(argc, const_cast<const char**>(argv));
    ::testing::InitGoogleTest(&argc, argv);
    exit(RUN_ALL_TESTS()); // exit to workaround emscripten issue (read comment in TightRunner)
}
