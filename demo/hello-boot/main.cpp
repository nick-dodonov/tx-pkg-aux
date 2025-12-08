#include "Boot/Boot.h"
#include "Log/Log.h"

int main(const int argc, const char* argv[])
{
    Boot::LogHeader(argc, argv);

    Log::Info("Hello Boot!");
    return argc - 1;
}
