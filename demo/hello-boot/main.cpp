#include "Boot/Boot.h"
#include "Log/Log.h"

int main(int argc, char* argv[])
{
    Boot::LogHeader(argc, argv);

    Log::Info("Hello Boot!");
    return argc - 1;
}
