#include "Boot/Build.h"
#include "Boot/Boot.h"

#include <gtest/gtest.h>
#include <string>

TEST(BuildTest, BuildDescriptionNotEmpty)
{
    const auto desc = Build::BuildDescription();
    EXPECT_FALSE(desc.empty());
}

TEST(BuildTest, BuildDescriptionContainsRequiredInfo)
{
    const auto desc = Build::BuildDescription();
    
    // Should contain build configuration
    EXPECT_TRUE(desc.contains("Debug") ||
                desc.contains("Release"));
    
    // Should contain platform info
    EXPECT_TRUE(desc.contains("macOS") ||
                desc.contains("Linux") ||
                desc.contains("Windows") ||
                desc.contains("WebAssembly"));
    
    // Should contain compiler info
    EXPECT_TRUE(desc.contains("Clang") ||
                desc.contains("GCC") ||
                desc.contains("MSVC"));
    
    // Should contain C++ standard
    EXPECT_TRUE(desc.contains("C++"));
}

int main(int argc, char** argv)
{
    Boot::DefaultInit(argc, const_cast<const char**>(argv));
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
