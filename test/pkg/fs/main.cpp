#include "Boot/Boot.h"
#include <gtest/gtest.h>

int main(int argc, char** argv)
{
    Boot::DefaultInit(argc, const_cast<const char**>(argv));
    ::testing::InitGoogleTest(&argc, argv);
    exit(RUN_ALL_TESTS()); // exit to workaround emscripten issue (read comment in TightRunner)
}
