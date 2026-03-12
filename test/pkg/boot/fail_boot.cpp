#include "Boot/Boot.h"
#include <gtest/gtest.h>

TEST(FailTest, AssertFail)
{
    ASSERT_TRUE(false);
}

int main(int argc, char** argv)
{
    Boot::DefaultInit(argc, const_cast<const char**>(argv));
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
