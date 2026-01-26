#include "Boot/Build.h"
#include "Boot/Boot.h"

#include <gtest/gtest.h>
#include <string>

TEST(BuildTest, BuildDescriptionNotEmpty)
{
    std::string desc = Build::BuildDescription();
    EXPECT_FALSE(desc.empty());
}

TEST(BuildTest, BuildDescriptionContainsRequiredInfo)
{
    std::string desc = Build::BuildDescription();
    
    // Should contain build configuration
    EXPECT_TRUE(desc.find("Debug") != std::string::npos || 
                desc.find("Release") != std::string::npos);
    
    // Should contain platform info
    EXPECT_TRUE(desc.find("macOS") != std::string::npos || 
                desc.find("Linux") != std::string::npos || 
                desc.find("Windows") != std::string::npos ||
                desc.find("WebAssembly") != std::string::npos);
    
    // Should contain compiler info
    EXPECT_TRUE(desc.find("Clang") != std::string::npos || 
                desc.find("GCC") != std::string::npos || 
                desc.find("MSVC") != std::string::npos);
    
    // Should contain C++ standard
    EXPECT_TRUE(desc.find("C++") != std::string::npos);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    Boot::DefaultInit(argc, const_cast<const char**>(argv));
    return RUN_ALL_TESTS();
}
